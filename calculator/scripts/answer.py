#!/usr/bin/env python3
"""
口算作业答案填充 —— 自动计算答案，绿色重绘
基于原批改脚本修改，保留等宽分列、左对齐、分数竖式等特性。
"""
import os, sys, base64, re, shutil, datetime

# Keep console output from crashing on GBK codepages (emoji in print statements).
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
from fractions import Fraction
from openai import OpenAI
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# ==================== 用户配置 ====================
API_KEY = ""
IMAGE_PATH = "test.png"         # 输入图片（仅有题目，无答案）
MODEL_NAME = "qwen-vl-max"

# 输出图片（本地临时文件）
OUTPUT_IMG = "clean_rewrite.png"   # 最终带答案的图片
# 归档文件夹
OUT_DIR = "out_capture"
IN_DIR = "in_capture"

BLUR_RADIUS = 36          # 高斯模糊强度

FONT_PATH = "C:/Windows/Fonts/msyh.ttc"
if not os.path.exists(FONT_PATH):
    FONT_PATH = "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"

FONT_SIZE_INIT = 48
MIN_FONT_SIZE = 14

TOP_MARGIN = 60
BOTTOM_MARGIN = 60
COL_PADDING = 10
FRAC_GAP = 10
LINE_EXTRA = 6
# ================================================

def get_api_key():
    key = os.getenv("DASHSCOPE_API_KEY")
    if key: return key
    if API_KEY and API_KEY.strip(): return API_KEY
    # Fallback: read the key from a local gitignored file so it never has to live in source code.
    key_file_candidates = [
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "photo", "api_key.txt"),
        os.path.join(os.getcwd(), "api_key.txt"),
    ]
    for _kf in key_file_candidates:
        if os.path.isfile(_kf):
            # utf-8-sig strips a BOM that Notepad/PowerShell may add to UTF-8 files.
            with open(_kf, "r", encoding="utf-8-sig") as _f:
                _k = _f.read().strip()
            if _k:
                return _k
    raise RuntimeError("请填写 API_KEY 或设置环境变量")

def encode_image(path):
    with open(path, "rb") as f:
        b64 = base64.b64encode(f.read()).decode()
    ext = os.path.splitext(path)[1].lower()
    mime = {"jpg":"jpeg","jpeg":"jpeg","png":"png","webp":"webp"}.get(ext[1:], "jpeg")
    return f"data:image/{mime};base64,{b64}"

def build_prompt():
    """
    提示模型识别算式，答案部分可能为空，用 @ 代替。
    要求返回等号区域的归一化坐标，方便我们定位重绘。
    """
    return (
        "你是一位严谨的小学数学老师。请仔细查看这张口算作业图片，找出每道完整的口算题（必须有等号）。\n"
        "图片中可能只有题目，没有填写答案。如果答案位置是空白的，请用 @ 代替学生答案。\n"
        "对每道题，请严格按以下格式输出（一行一题，不要额外文字）：\n"
        "题号|算式=学生答案|正确/错误/涂抹| <box>x1 y1 x2 y2</box>\n"
        "其中 x1,y1 是等号区域**左上角**的归一化坐标（0‑1000），x2,y2 是右下角坐标。\n"
        "判断规则：不进行批改，请统一在「正确/错误/涂抹」栏写「正确」。\n"
        "示例：\n"
        "1|7/8+1/2=@|正确| <box>268 62 334 138</box>\n"
    )

def calc_fraction(expr: str) -> Fraction:
    """精确计算表达式，支持分数、括号、小数"""
    expr = expr.strip().replace(' ', '').replace('×', '*').replace('÷', '/')
    m = re.match(r'^(\d+(?:/\d+)?)\s*([+\-*/])\s*(\d+(?:/\d+)?)$', expr)
    if m:
        a, op, b = Fraction(m.group(1)), m.group(2), Fraction(m.group(3))
        if op == '+': return a + b
        if op == '-': return a - b
        if op == '*': return a * b
        if op == '/': return a / b
    if re.match(r'^[\d\.+\-*/()]+$', expr):
        val = eval(expr, {"__builtins__": {}})
        return Fraction(val).limit_denominator()
    raise ValueError(f"无法计算: {expr}")

def parse_mixed_fraction(s: str) -> str:
    return s.replace('又', '+')

def parse_qwen_output(raw, img_w, img_h):
    """解析模型回复，提取所有题目的等号位置，并本地计算正确答案"""
    box_pattern = re.compile(
        r'^(\d+)\|(.+?)\|(正确|错误|涂抹)\|\s*<box>\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*</box>'
    )
    problems = []
    for line in raw.strip().split('\n'):
        m = box_pattern.match(line.strip())
        if not m: continue
        num = int(m.group(1))
        expr = m.group(2)
        # verdict 在本题中无关紧要，我们只用坐标
        x1, y1, x2, y2 = map(int, m.groups()[3:7])
        cx = int((x1 + x2) / 2 * img_w / 1000)
        cy = int((y1 + y2) / 2 * img_h / 1000)

        left_part, right_part = (expr.split('=', 1) + [''])[:2]
        left_expr = left_part.strip()
        # 计算正确答案（忽略右侧的任何内容）
        try:
            correct_val = calc_fraction(parse_mixed_fraction(left_expr))
        except Exception:
            correct_val = None
        answer_text = str(correct_val) if correct_val is not None else '?'

        problems.append({
            'num': num,
            'left': left_expr,
            'right_raw': answer_text,   # 答案文本（绿色打印用）
            'verdict': '正确',               # 固定为正确
            'center': (cx, cy),
            'true_answer': answer_text
        })
    return sorted(problems, key=lambda x: x['num'])

def create_blurred_background(img_path, radius):
    orig = Image.open(img_path).convert("RGB")
    blurred = orig.filter(ImageFilter.GaussianBlur(radius=radius))
    return blurred

# ---------- 列聚类、布局、绘制函数（与原版完全相同，但仅使用正确答案绘制） ----------
def cluster_columns(problems, img_w):
    if not problems: return []
    sorted_probs = sorted(problems, key=lambda p: p['center'][0])
    cols = []
    current_col = [sorted_probs[0]]
    threshold = img_w * 0.15
    for i in range(1, len(sorted_probs)):
        prev_x = sorted_probs[i-1]['center'][0]
        cur_x = sorted_probs[i]['center'][0]
        if cur_x - prev_x > threshold:
            cols.append(current_col)
            current_col = [sorted_probs[i]]
        else:
            current_col.append(sorted_probs[i])
    cols.append(current_col)
    cols.sort(key=lambda col: min(p['center'][0] for p in col))
    return cols

def layout_columns(col_problems, img_w, img_h, font_size, icon_size):
    num_cols = len(col_problems)
    if num_cols == 0: return []
    col_w = img_w // num_cols
    try:
        font = ImageFont.truetype(FONT_PATH, font_size)
    except:
        font = ImageFont.load_default()
    layouts = []
    for col_idx, probs in enumerate(col_problems):
        col_left = col_idx * col_w
        left_x = col_left + COL_PADDING
        max_left_w = max(estimate_width(p['left'], font) for p in probs)
        eq_x = left_x + max_left_w + 5
        eq_w = font.getbbox("=")[2] - font.getbbox("=")[0]
        ans_x = eq_x + eq_w + 10

        if len(probs) >= 2:
            ys = [p['center'][1] for p in probs]
            min_y, max_y = min(ys), max(ys)
            target_min, target_max = TOP_MARGIN, img_h - BOTTOM_MARGIN
            if max_y > min_y:
                new_ys = [int(target_min + (y - min_y) * (target_max - target_min) / (max_y - min_y)) for y in ys]
            else:
                # All problems share the same y: spread them evenly instead of dividing by zero.
                new_ys = [int(target_min + (target_max - target_min) * (i + 1) / (len(probs) + 1)) for i in range(len(probs))]
        else:
            new_ys = [img_h // 2]

        col_layout = [{'problem': p, 'left_x': left_x, 'eq_x': eq_x, 'ans_x': ans_x, 'y': y} for p, y in zip(probs, new_ys)]
        layouts.append(col_layout)
    return layouts

def is_fraction_token(s: str) -> bool:
    return re.fullmatch(r'-?\d+/\d+', s) is not None

def draw_fraction(draw, x, y_line, numerator, denominator, font, color, line_extra=LINE_EXTRA):
    num_box = font.getbbox(numerator)
    den_box = font.getbbox(denominator)
    num_w = num_box[2] - num_box[0]
    den_w = den_box[2] - den_box[0]
    max_w = max(num_w, den_w) + line_extra
    ascent = -num_box[1]
    descent = num_box[3]
    total_height = ascent + descent
    half_char = total_height // 2
    num_bottom = y_line - 2 - half_char
    num_top = num_bottom - total_height
    den_top = y_line + 2

    draw.line([(x, y_line), (x + max_w, y_line)], fill=color, width=2)
    num_x = x + (max_w - num_w) // 2
    den_x = x + (max_w - den_w) // 2
    draw.text((num_x, num_top), numerator, fill=color, font=font)
    draw.text((den_x, den_top), denominator, fill=color, font=font)
    return max_w, total_height

def draw_expression(draw, x, y_line, expr, font, color, frac_gap=FRAC_GAP):
    tokens = re.findall(r'-?\d+/\d+|\d+又\d+/\d+|[+\-*/×÷()]|\d+', expr)
    cur_x = x
    char_h = draw.textbbox((0,0), "0", font=font)[3] - draw.textbbox((0,0), "0", font=font)[1]
    text_y = y_line - char_h // 2

    for tok in tokens:
        if '又' in tok:
            int_part, frac = tok.split('又')
            draw.text((cur_x, text_y), int_part, fill=color, font=font)
            int_w = draw.textbbox((cur_x, text_y), int_part, font=font)[2] - cur_x
            cur_x += int_w
            num, den = frac.split('/')
            fw, _ = draw_fraction(draw, cur_x, y_line, num, den, font, color)
            cur_x += fw + frac_gap
        elif is_fraction_token(tok):
            num, den = tok.split('/')
            fw, _ = draw_fraction(draw, cur_x, y_line, num, den, font, color)
            cur_x += fw + frac_gap
        else:
            draw.text((cur_x, text_y), tok, fill=color, font=font)
            tbox = draw.textbbox((cur_x, text_y), tok, font=font)
            cur_x += (tbox[2] - cur_x)
    return cur_x - x

def estimate_width(expr, font, frac_gap=FRAC_GAP):
    tokens = re.findall(r'-?\d+/\d+|\d+又\d+/\d+|[+\-*/×÷()]|\d+', expr)
    total = 0
    for tok in tokens:
        if '又' in tok:
            int_part, frac = tok.split('又')
            total += (font.getbbox(int_part)[2] - font.getbbox(int_part)[0])
            num, den = frac.split('/')
            num_w = font.getbbox(num)[2] - font.getbbox(num)[0]
            den_w = font.getbbox(den)[2] - font.getbbox(den)[0]
            total += max(num_w, den_w) + LINE_EXTRA + frac_gap
        elif is_fraction_token(tok):
            num, den = tok.split('/')
            num_w = font.getbbox(num)[2] - font.getbbox(num)[0]
            den_w = font.getbbox(den)[2] - font.getbbox(den)[0]
            total += max(num_w, den_w) + LINE_EXTRA + frac_gap
        else:
            total += (font.getbbox(tok)[2] - font.getbbox(tok)[0])
    return total

def get_optimal_size(col_problems, img_w, img_h, font_path, init_size, min_size):
    best_size = min_size
    num_cols = len(col_problems)
    if num_cols == 0: return best_size, best_size
    col_w = img_w // num_cols
    avail_w = col_w - 2 * COL_PADDING
    for size in range(init_size, min_size - 1, -1):
        try: font = ImageFont.truetype(font_path, size)
        except: font = ImageFont.load_default()
        ok = True
        for probs in col_problems:
            max_left = max(estimate_width(p['left'], font) for p in probs)
            max_right = max(estimate_width(p['right_raw'].replace('@', ''), font) for p in probs)
            eq_w = font.getbbox("=")[2] - font.getbbox("=")[0]
            needed = max_left + 5 + eq_w + 10 + max_right
            if needed > avail_w:
                ok = False; break
        if ok:
            best_size = size; break
    return best_size, best_size

def draw_clean_rewrite(blurred_img, layouts, out_path, font_size, icon_size=None):
    """绘制最终图片，答案统一用绿色"""
    img = blurred_img.copy()
    draw = ImageDraw.Draw(img)
    try: font_main = ImageFont.truetype(FONT_PATH, font_size)
    except: font_main = ImageFont.load_default()

    for col_layout in layouts:
        for item in col_layout:
            p = item['problem']; cy = item['y']
            left_x, eq_x, ans_x = item['left_x'], item['eq_x'], item['ans_x']

            # 左式（黑色）
            draw_expression(draw, left_x, cy, p['left'], font_main, (0,0,0))
            # 等号（黑色）
            eq_bbox = draw.textbbox((0,0), "=", font=font_main)
            eq_w, eq_h = eq_bbox[2]-eq_bbox[0], eq_bbox[3]-eq_bbox[1]
            draw.text((eq_x, cy - eq_h//2), "=", fill=(0,0,0), font=font_main)
            # 答案（绿色）
            ans_text = p['right_raw'].replace('@', '')
            # 对于分数答案，可能需要特殊处理？答案已经是计算后的最简形式字符串，可以直接绘制
            draw_expression(draw, ans_x, cy, ans_text, font_main, (34,139,34))
            # 不需要图标（因为不是批改）

    img.save(out_path)
    print(f"✅ 答案填充图已保存：{out_path}")

# ==================== 主流程 ====================
def main():
    if not os.path.exists(IMAGE_PATH):
        print(f"❌ 图片 {IMAGE_PATH} 不存在"); sys.exit(1)

    # 归档原始图片
    os.makedirs(IN_DIR, exist_ok=True)
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    src_ext = os.path.splitext(IMAGE_PATH)[1]
    new_input = os.path.join(IN_DIR, f'input_{timestamp}{src_ext}')
    shutil.copy2(IMAGE_PATH, new_input)
    print(f"📁 原始图片已存档：{new_input}")

    api_key = get_api_key()
    client = OpenAI(api_key=api_key, base_url="https://dashscope.aliyuncs.com/compatible-mode/v1")
    data_url = encode_image(IMAGE_PATH)
    print("🤖 正在请求视觉模型...")
    try:
        completion = client.chat.completions.create(
            model=MODEL_NAME,
            messages=[{"role":"user","content":[
                {"type":"image_url","image_url":{"url":data_url}},
                {"type":"text","text":build_prompt()}
            ]}],
            temperature=0.1
        )
        raw = completion.choices[0].message.content
    except Exception as e:
        print(f"❌ API 请求出错：{e}"); sys.exit(1)

    print("📝 模型回复：\n" + raw + "\n")

    with Image.open(IMAGE_PATH) as img:
        w, h = img.size
    problems = parse_qwen_output(raw, w, h)
    if not problems:
        print("❌ 未能解析到题目"); sys.exit(1)

    col_problems = cluster_columns(problems, w)
    print(f"📊 检测到 {len(col_problems)} 列")

    font_size, icon_size = get_optimal_size(col_problems, w, h, FONT_PATH, FONT_SIZE_INIT, MIN_FONT_SIZE)
    print(f"✨ 使用字号：{font_size}")

    layouts = layout_columns(col_problems, w, h, font_size, icon_size)

    print("🎨 正在创建模糊背景...")
    blurred_bg = create_blurred_background(IMAGE_PATH, BLUR_RADIUS)
    draw_clean_rewrite(blurred_bg, layouts, OUTPUT_IMG, font_size)

    # 复制结果到 out/
    os.makedirs(OUT_DIR, exist_ok=True)
    final_img = os.path.join(OUT_DIR, f'answer_{timestamp}.png')
    shutil.copy2(OUTPUT_IMG, final_img)
    print(f"📸 最终答案图片已归档：{final_img}")

    # 生成简单的文本报告
    txt_out = os.path.join(OUT_DIR, f'answers_{timestamp}.txt')
    lines = []
    for p in problems:
        expr = f"{p['left']}={p['true_answer']}"
        lines.append(expr)
    with open(txt_out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(f"📝 文本答案已保存：{txt_out}")

if __name__ == '__main__':
    main()
