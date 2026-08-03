#!/usr/bin/env python3
"""
口算作业批改 —— 等宽分列 + 左对齐 + 垂直均匀分布 + 带分数数学显示 + 分数竖式
带输入/输出文件归档（时间戳命名），批改图先在当前目录生成 clean_rewrite.png，再复制到 out/
"""
import os, sys, base64, re, shutil, datetime

# Keep console output from crashing on GBK codepages (emoji in print statements).
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
import matplotlib
matplotlib.use('Agg')        # 必须在 import pyplot 之前，使用无 GUI 后端
import matplotlib.pyplot as plt
import glob
from fractions import Fraction
from openai import OpenAI
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# ==================== 用户配置 ====================
API_KEY = ""
IMAGE_PATH = "test.png"
MODEL_NAME = "qwen-vl-max"
OUTPUT_TXT = "批改结果.txt"           # 将不再使用，直接写入 out/
OUTPUT_IMG = "clean_rewrite.png"     # 固定当前目录的文件名
CHECK_PNG = "check.png"
CROSS_PNG = "cross.png"

BLUR_RADIUS = 36

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
def set_chinese_font():
    """配置 matplotlib 支持中文显示"""
    try:
        import matplotlib.pyplot as plt
        from matplotlib import font_manager
        # 按顺序尝试常见中文字体路径
        font_paths = [
            'C:/Windows/Fonts/msyh.ttc',        # 微软雅黑 (Windows)
            'C:/Windows/Fonts/simhei.ttf',      # 黑体 (Windows)
            '/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc',  # Ubuntu/Debian
            '/System/Library/Fonts/PingFang.ttc',                      # macOS
            '/System/Library/Fonts/STHeiti Light.ttc',                # macOS 备选
        ]
        font_found = False
        for fp in font_paths:
            if os.path.exists(fp):
                font_manager.fontManager.addfont(fp)
                prop = font_manager.FontProperties(fname=fp)
                plt.rcParams['font.sans-serif'] = [prop.get_name()]
                font_found = True
                break
        if not font_found:
            # 若未找到具体字体，指定常见中文字体名称（系统可能已注册）
            plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'WenQuanYi Micro Hei', 'Arial Unicode MS']
        plt.rcParams['axes.unicode_minus'] = False   # 解决负号显示问题
    except Exception as e:
        print(f"⚠️ 设置中文字体失败：{e}")

# ---------- 原有函数（未改动） ----------
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
    return (
        "你是一位严谨的小学数学老师。请仔细查看这张口算作业图片，找出每道完整的口算题（必须有等号）。\n"
        "特别注意事项：\n"
        "- 带分数格式：如 1又3/8 应识别为 '1又3/8'，输出时务必保留这种中文表示方式，不要转换为 11/8 等假分数。\n"
        "- 如果答案有涂改或模糊，用 @ 代替无法辨认的字符。\n\n"
        "对每道题，请按以下格式输出（一行一题，不要额外文字）：\n"
        "题号|算式=学生答案|正确/错误/涂抹| <box>x1 y1 x2 y2</box>\n"
        "其中 x1,y1 是等号区域**左上角**的归一化坐标（0‑1000），x2,y2 是右下角坐标。\n"
        "判断规则：只比较数值是否相等（忽略假分数、带分数、约分形式）。\n"
        "示例：\n"
        "1|7/8+1/2=1又3/8|正确| <box>268 62 334 138</box>\n"
    )

def parse_mixed_fraction(s: str) -> str:
    s = s.replace('又', '+')
    return s

def calc_fraction(expr: str) -> Fraction:
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

def parse_qwen_output(raw, img_w, img_h):
    box_pattern = re.compile(
        r'^(\d+)\|(.+?)\|(正确|错误|涂抹)\|\s*<box>\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*</box>'
    )
    problems = []
    for line in raw.strip().split('\n'):
        m = box_pattern.match(line.strip())
        if not m: continue
        num = int(m.group(1))
        expr = m.group(2)
        verdict = m.group(3)
        x1, y1, x2, y2 = map(int, m.groups()[3:7])
        cx = int((x1 + x2) / 2 * img_w / 1000)
        cy = int((y1 + y2) / 2 * img_h / 1000)

        left_part, right_part = (expr.split('=', 1) + [''])[:2]
        right_raw = right_part.strip()
        right_clean = parse_mixed_fraction(re.sub(r'@', '', right_raw)).strip()

        final_verdict = verdict
        if verdict != '涂抹':
            try:
                correct_val = calc_fraction(parse_mixed_fraction(left_part.strip()))
                if '@' in right_raw:
                    final_verdict = '涂抹'
                else:
                    student_val = calc_fraction(right_clean)
                    if correct_val == student_val:
                        final_verdict = '正确'
                    else:
                        final_verdict = '错误'
            except:
                pass

        problems.append({
            'num': num,
            'left': left_part.strip(),
            'right_raw': right_raw.strip(),
            'right_clean': right_clean,
            'verdict': final_verdict,
            'center': (cx, cy)
        })
    return sorted(problems, key=lambda x: x['num'])

def create_blurred_background(img_path, radius):
    orig = Image.open(img_path).convert("RGB")
    blurred = orig.filter(ImageFilter.GaussianBlur(radius=radius))
    return blurred

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

def draw_clean_rewrite(blurred_img, layouts, out_path, font_size, icon_size):
    img = blurred_img.copy()
    draw = ImageDraw.Draw(img)
    try: font_main = ImageFont.truetype(FONT_PATH, font_size)
    except: font_main = ImageFont.load_default()
    try:
        check_icon = Image.open(CHECK_PNG).convert("RGBA").resize((icon_size, icon_size), Image.LANCZOS)
        cross_icon = Image.open(CROSS_PNG).convert("RGBA").resize((icon_size, icon_size), Image.LANCZOS)
    except: check_icon = cross_icon = None

    bboxes = []  # 存储 (题号, (x0, y0, x1, y1))

    for col_layout in layouts:
        for item in col_layout:
            p = item['problem']; cy = item['y']
            left_x, eq_x, ans_x = item['left_x'], item['eq_x'], item['ans_x']

            # 初始化边界（用极大极小值）
            min_x = min_y = float('inf')
            max_x = max_y = -float('inf')

            # 1. 绘制左式并获取宽度（实际高度在 draw_expression 内不可知）
            left_w = draw_expression(draw, left_x, cy, p['left'], font_main, (0,0,0))
            # 使用字体度量估算垂直范围
            ascent, descent = font_main.getmetrics()
            left_top = cy - ascent
            left_bottom = cy + descent
            min_x = min(min_x, left_x)
            max_x = max(max_x, left_x + left_w)
            min_y = min(min_y, left_top)
            max_y = max(max_y, left_bottom)

            # 2. 等号
            eq_bbox = draw.textbbox((0,0), "=", font=font_main)
            eq_w, eq_h = eq_bbox[2]-eq_bbox[0], eq_bbox[3]-eq_bbox[1]
            draw.text((eq_x, cy - eq_h//2), "=", fill=(0,0,0), font=font_main)
            eq_left = eq_x
            eq_right = eq_x + eq_w
            eq_top = cy - eq_h//2
            eq_bottom = eq_top + eq_h
            min_x = min(min_x, eq_left)
            max_x = max(max_x, eq_right)
            min_y = min(min_y, eq_top)
            max_y = max(max_y, eq_bottom)

            # 3. 答案
            ans_text = p['right_raw'].replace('@', '')
            if p['verdict'] == '正确':
                color = (34,139,34)
            elif p['verdict'] == '错误':
                color = (220,20,60)
            else:
                ans_text = p['right_raw'].replace('@', '?')
                color = (128,128,128)
            ans_w = draw_expression(draw, ans_x, cy, ans_text, font_main, color)
            ans_left = ans_x
            ans_right = ans_x + ans_w
            ans_top = cy - ascent
            ans_bottom = cy + descent
            min_x = min(min_x, ans_left)
            max_x = max(max_x, ans_right)
            min_y = min(min_y, ans_top)
            max_y = max(max_y, ans_bottom)

            # 4. 图标
            icon = check_icon if p['verdict'] == '正确' else cross_icon
            if icon:
                iw, ih = icon.size
                ix = eq_x + eq_w//2 - iw//2
                iy = cy + eq_h//2 + 5   # 图标在等号下方
                ix = max(0, min(ix, img.width - iw))
                iy = max(0, min(iy, img.height - ih))
                img.paste(icon, (ix, iy), icon)
                icon_left = ix
                icon_right = ix + iw
                icon_top = iy
                icon_bottom = iy + ih
                min_x = min(min_x, icon_left)
                max_x = max(max_x, icon_right)
                min_y = min(min_y, icon_top)
                max_y = max(max_y, icon_bottom)

            # ----- 关键修复：增加垂直内边距，覆盖分数和图标 -----
            pad_x = 10                     # 水平内边距
            pad_y = int(font_size * 0.6)   # 垂直内边距（足够容纳分数分母和图标）
            bbox = (min_x - pad_x, min_y - pad_y,
                    max_x + pad_x, max_y + pad_y)
            bboxes.append((p['num'], bbox))

    img.save(out_path)
    print(f"✅ 批改图已生成（本地）：{out_path}")
    return bboxes

def parse_report(file_path):
    """解析单个报告文件，返回 (total, correct, wrong, smudge)"""
    total = correct = wrong = smudge = 0
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                # 格式：题号:表达式_状态
                parts = line.split('_')
                if len(parts) == 2:
                    status = parts[1].strip()
                    total += 1
                    if status == 'Y':
                        correct += 1
                    elif status == 'N':
                        wrong += 1
                    elif status == 'M':
                        smudge += 1
                    else:
                        # 兼容旧格式，将非Y视为错误
                        wrong += 1
    except Exception as e:
        print(f"⚠️ 读取报告 {file_path} 失败：{e}")
    return total, correct, wrong, smudge

def plot_trend(out_dir='out'):
    """绘制正确率历史趋势图（等间距分布）"""
    set_chinese_font()
    report_files = glob.glob(os.path.join(out_dir, 'output_*.txt'))
    if not report_files:
        print("⚠️ 未找到历史报告，跳过趋势图绘制。")
        return

    def extract_timestamp(fname):
        base = os.path.basename(fname)
        match = re.search(r'output_(\d{8}_\d{6})\.txt', base)
        return match.group(1) if match else ''

    report_files.sort(key=extract_timestamp)

    datetimes = []
    accuracies = []
    for fpath in report_files:
        total, correct, wrong, smudge = parse_report(fpath)
        if total == 0:
            continue
        acc = correct / total
        ts = extract_timestamp(fpath)
        if ts:
            try:
                datetimes.append(datetime.datetime.strptime(ts, "%Y%m%d_%H%M%S"))
            except:
                datetimes.append(None)
        else:
            datetimes.append(None)
        accuracies.append(acc)

    # 过滤掉无法解析时间的条目
    valid = [(dt, acc) for dt, acc in zip(datetimes, accuracies) if dt is not None]
    if len(valid) < 2:
        print("⚠️ 历史报告不足2份，无法绘制趋势图。")
        return
    datetimes, accuracies = zip(*valid)

    # ---- 改为等间距（忽略实际时间间隔） ----
    x = range(len(datetimes))   # 使用序号作为 x 轴
    plt.figure(figsize=(10, 6))
    plt.plot(x, accuracies, marker='o', linestyle='-', color='b')
    plt.xlabel('运行时间')
    plt.ylabel('正确率')
    plt.title('口算批改正确率历史趋势')
    plt.ylim(0, 1)
    # 设置刻度标签为格式化的时间字符串
    plt.xticks(x, [dt.strftime('%m-%d %H:%M') for dt in datetimes], rotation=45, ha='right')
    plt.tight_layout()
    plt.savefig('trend.png', dpi=150)
    plt.close()
    print("📈 趋势图已保存：trend.png")

def plot_pie(correct, wrong, smudge):
    """绘制本次批改结果扇形图"""
    set_chinese_font()   # 添加这一行
    labels = []
    sizes = []
    colors = []
    if correct > 0:
        labels.append('正确')
        sizes.append(correct)
        colors.append('#4CAF50')
    if wrong > 0:
        labels.append('错误')
        sizes.append(wrong)
        colors.append('#f44336')
    if smudge > 0:
        labels.append('涂抹')
        sizes.append(smudge)
        colors.append('#FF9800')

    if not sizes:
        print("⚠️ 没有有效数据，无法绘制扇形图。")
        return

    plt.figure(figsize=(6, 6))
    plt.pie(sizes, labels=labels, autopct='%1.1f%%', colors=colors, startangle=90, wedgeprops={'edgecolor': 'white'})
    plt.title('本次口算批改结果分布')
    plt.axis('equal')
    plt.savefig('pie.png', dpi=150)
    plt.close()
    print("📊 扇形图已保存：pie.png")

# ==================== 修改后的 main ====================
def main():
    # ---------- 时间戳归档 ----------
    in_dir = "in"
    out_dir = "out"
    os.makedirs(in_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    print(f"🕒 时间戳：{timestamp}")

    # 复制原始图片到 in/ 下
    src_ext = os.path.splitext(IMAGE_PATH)[1]
    new_input_path = os.path.join(in_dir, f'input_{timestamp}{src_ext}')
    shutil.copy2(IMAGE_PATH, new_input_path)
    print(f"📁 原始图片已存档：{new_input_path}")

    # 本地生成的批改图
    local_img = "clean_rewrite.png"
    # 输出文本直接放入 out/
    txt_out = os.path.join(out_dir, f'output_{timestamp}.txt')

    # ---------- 原有流程 ----------
    if not os.path.exists(IMAGE_PATH):
        print(f"❌ 图片 {IMAGE_PATH} 不存在"); sys.exit(1)

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

    for p in problems:
        if p['verdict'] == '错误':
            try: p['true_answer'] = str(calc_fraction(parse_mixed_fraction(p['left'])))
            except: p['true_answer'] = '?'

    col_problems = cluster_columns(problems, w)
    print(f"📊 检测到 {len(col_problems)} 列")

    font_size, icon_size = get_optimal_size(col_problems, w, h, FONT_PATH, FONT_SIZE_INIT, MIN_FONT_SIZE)
    print(f"✨ 使用字号：{font_size}")

    layouts = layout_columns(col_problems, w, h, font_size, icon_size)

    print("🎨 正在创建模糊背景...")
    blurred_bg = create_blurred_background(IMAGE_PATH, BLUR_RADIUS)
    bboxes = draw_clean_rewrite(blurred_bg, layouts, local_img, font_size, icon_size)

    # 复制到 out/ 按时间戳命名
    final_img = os.path.join(out_dir, f'output_{timestamp}.png')
    shutil.copy2(local_img, final_img)
    print(f"📸 批改图已存档（副本）：{final_img}")

    # ---------- 生成文本报告（包含状态分类） ----------
    correct = wrong = smudge = 0
    lines = []
    for p in problems:
        expr = f"{p['left']}={p['right_raw']}"
        expr_clean = re.sub(r'\s+', '', expr)
        # 状态：正确 Y，错误 N，涂抹 M
        if p['verdict'] == '正确':
            status = 'Y'
            correct += 1
        elif p['verdict'] == '涂抹':
            status = 'M'
            smudge += 1
        else:
            status = 'N'
            wrong += 1
        lines.append(f"{p['num']}:{expr_clean}_{status}")

    with open(txt_out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"📝 文本结果：{txt_out}")

    # ---------- 错题归档 ----------
    wrong_dir = "wrong"
    os.makedirs(wrong_dir, exist_ok=True)
    bbox_dict = {num: bbox for num, bbox in bboxes}
    wrong_info = []

    for p in problems:
        if p['verdict'] == '错误':
            bbox = bbox_dict.get(p['num'])
            if bbox:
                # 从本地批改图中裁剪错题区域
                try:
                    crop_img = Image.open(local_img).crop(bbox)
                    wrong_img_name = f"wrong_{timestamp}_{p['num']}.png"
                    wrong_img_path = os.path.join(wrong_dir, wrong_img_name)
                    crop_img.save(wrong_img_path)
                    wrong_info.append(f"{p['num']} | {p['right_raw']} | {p.get('true_answer', '?')} | {wrong_img_name}")
                except Exception as e:
                    print(f"⚠️ 裁剪错题 {p['num']} 失败：{e}")

    # 生成错题汇总文本
    if wrong_info:
        wrong_txt = os.path.join(wrong_dir, f"错题汇总_{timestamp}.txt")
        with open(wrong_txt, 'w', encoding='utf-8') as f:
            f.write("题号 | 学生答案 | 正确答案 | 图片文件名\n")
            for line in wrong_info:
                f.write(line + "\n")
        print(f"📝 错题汇总已保存：{wrong_txt}")
    else:
        print("🎉 本次无错题！")

    # ---------- 绘制图表（使用 matplotlib） ----------
    try:
        # 检查 matplotlib 是否可用
        import matplotlib
        # 绘制趋势图
        plot_trend(out_dir)
        # 绘制扇形图
        plot_pie(correct, wrong, smudge)
    except ImportError:
        print("⚠️ matplotlib 未安装，跳过图表绘制。")
        print("   如需图表，请运行: pip install matplotlib")
    except Exception as e:
        print(f"⚠️ 绘图失败：{e}")

if __name__ == '__main__':
    main()
