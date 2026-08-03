# 口算作业批改系统（Arithmetic Practice Grading System）

基于 **SFML 3 + OpenCV** 的 C++ 桌面应用，调用阿里云百炼 **Qwen-VL** 视觉模型，对学生口算作业进行拍照识别与自动批改，并提供答案生成、错题本、历史统计和口算练习等功能。

## 功能特性

- 摄像头拍照、文件路径导入、文件对话框选择图片三种方式录入题目
- AI 批改：识别算式与答案并自动判定正误（支持分数、带分数、涂抹标记）
- AI 答案生成：自动计算并生成答案
- 错题本：自动归档错题截图，按批查阅
- 历史记录：批改输入/输出自动归档，正确率趋势图与结果分布饼图
- 口算练习：加减乘除四则运算计时练习

## 技术栈

| 组件 | 版本 | 说明 |
| --- | --- | --- |
| C++ | C++20 | 桌面程序主体 |
| SFML | 3.1.0 | Graphics / Window / System / Audio（头文件与库已随仓库附带） |
| OpenCV | 4.12.0 | 摄像头采集（库已随仓库附带） |
| Python | 3.10+ | AI 批改脚本（开发环境使用 3.13） |
| 阿里云百炼 DashScope | - | Qwen-VL 视觉模型 API（需自备 API Key） |

Python 依赖（见 `requirement.txt`）：`openai`、`Pillow`、`matplotlib`。

> **注意**：仓库只附带 SFML/OpenCV 的头文件和 `.lib` 库，**运行时 DLL 未包含**，需要自行准备（见下文「构建与运行」）。

## 环境要求

- Windows 10/11 x64
- Visual Studio 2022 或更新版本（需安装「使用 C++ 的桌面开发」工作负载；解决方案为 `.slnx` 格式，仅支持 x64）
- Python 3.10+（需保证 `python` 命令在 PATH 中）
- 阿里云百炼 DashScope API Key（模型：`qwen-vl-max`）

## 快速开始

1. 克隆仓库：

   ```bash
   git clone https://github.com/MOSS-vZ/Calculator.git
   ```

2. 安装 Python 依赖（在仓库根目录执行）：

   ```bash
   pip install -r requirement.txt
   ```

3. 配置 API Key（见下文「配置 API Key」）。

4. 用 Visual Studio 2022+ 打开 `calculator.slnx`，选择 **Release | x64**，生成并运行。

5. 将 SFML 3.1 / OpenCV 4.12 的运行时 DLL 放入 `calculator/bin/` 或输出目录 `calculator/x64/Release/`（构建后事件会自动将 `bin/` 下的 DLL 复制到输出目录）。

### 构建与运行

工程文件已预先配置头文件路径（`..\include`）和库路径（`..\lib`），**无需手动修改**；Debug 配置链接调试库（`-d` 后缀），Release 配置链接发布库。

1. 用 Visual Studio 2022+ 打开 `calculator.slnx`
2. 工具栏切换为 **Release**、平台 **x64**
3. 菜单栏「生成 → 重新生成解决方案」
4. 运行 `calculator/x64/Release/calculator.exe`（或在 VS 中直接按 F5）

### 配置 API Key

AI 批改与答案生成依赖 DashScope API Key，脚本按以下优先级读取：

1. 环境变量 `DASHSCOPE_API_KEY`
2. 本地文件 `calculator/photo/api_key.txt`（文件内只写 Key 本身，不要引号；该文件已加入 `.gitignore`）

```bash
# 方式一：设置环境变量
setx DASHSCOPE_API_KEY "你的 Key"   # Windows 命令提示符
```

```text
# 方式二：创建本地密钥文件（推荐，不会进版本库）
calculator/photo/api_key.txt
```

### 使用流程

1. 主界面点击「拍照批改」进入拍摄/导入界面
2. 打开摄像头拍照，或在输入框中粘贴图片路径、点击「…」选择文件
3. 点击批改按钮，等待 AI 识别完成
4. 查看批改结果、历史记录与错题本

## 项目结构

```text
Calculator/
├─ calculator.slnx            解决方案文件（仅 x64）
├─ requirement.txt            Python 依赖清单
├─ 软件详细设计文档.docx       详细设计文档
└─ calculator/
   ├─ src/                    C++ 源码
   ├─ assets/
   │  ├─ images/              界面图片（按钮、返回图标等）
   │  └─ splash/              启动动画帧
   ├─ scripts/                Python AI 脚本（process.py 批改、answer.py 答案）
   ├─ include/                第三方头文件（SFML 3.1 / OpenCV 4.12）
   ├─ lib/                    第三方库（.lib）
   ├─ bin/                    运行时 DLL 存放位置（需自行准备，已 gitignore）
   └─ photo/                  运行时数据（gitignored）
      ├─ test.png             待批改图片
      ├─ in/ out/             批改输入/输出归档
      ├─ wrong/               错题本数据
      ├─ trend.png pie.png    统计图表
      ├─ api_key.txt          API Key（可选）
      └─ python.log           Python 脚本输出日志
```

## 常见问题

- **扫描后没有结果图 / 批改失败**：查看 `calculator/photo/python.log` 中的脚本输出；确认 API Key 已配置且网络可访问 DashScope。
- **exe 无法启动**：缺少 SFML/OpenCV 运行时 DLL，将 DLL 放入 `calculator/bin/` 或输出目录后重新构建。
- **photo/ 目录会被自动清理**：仅保留运行时文件（含 `api_key.txt`、`python.log`），请勿在其中存放其他数据。
- **摄像头打不开**：检查 Windows 相机隐私权限，或改用文件导入方式。

## 许可证

本项目以 MIT License 发布，详见 [LICENSE.txt](LICENSE.txt)。注意其中版权信息仍为占位符，发布前请补充实际作者与年份。
