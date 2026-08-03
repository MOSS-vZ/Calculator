#include "CaptureScreen.h"
#include "App.h"
#include "ResultScreen.h"
#include "MainScreen.h"
#include "ResourceManager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cmath>
#include <random>
#include <regex>
#include <windows.h>
#include <commdlg.h>

namespace fs = std::filesystem;

CaptureScreen::CaptureScreen() {}
CaptureScreen::~CaptureScreen() { if (m_cap.isOpened()) m_cap.release(); }

// ---------- 清空 photo 文件夹 ----------
void CaptureScreen::clearPhotoFolder() {
    try {
        fs::path photoDir = "photo";
        if (!fs::exists(photoDir)) { fs::create_directory(photoDir); return; }
        std::vector<std::string> keepFiles = { "process.py", "answer.py", "check.png", "cross.png",
                                               "captured.jpg", "test.png", "clean_rewrite.png", "in", "out",
                                                "trend.png", "pie.png", "api_key.txt", "python.log"};
        for (const auto& entry : fs::directory_iterator(photoDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                bool shouldKeep = false;
                for (const auto& keep : keepFiles) {
                    if (filename == keep) { shouldKeep = true; break; }
                }
                if (!shouldKeep) fs::remove(entry.path());
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error clearing photo folder: " << e.what() << std::endl;
    }
}

// ---------- OpenCV Mat → SFML Texture ----------
std::unique_ptr<sf::Texture> CaptureScreen::matToTexture(const cv::Mat& mat) {
    auto texture = std::make_unique<sf::Texture>();
    if (mat.empty()) return texture;
    cv::Mat rgba;
    cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
    sf::Image image(sf::Vector2u(rgba.cols, rgba.rows));
    for (int y = 0; y < rgba.rows; ++y)
        for (int x = 0; x < rgba.cols; ++x) {
            cv::Vec4b pixel = rgba.at<cv::Vec4b>(y, x);
            image.setPixel(sf::Vector2u(x, y), sf::Color(pixel[0], pixel[1], pixel[2], pixel[3]));
        }
    texture->loadFromImage(image);
    return texture;
}

// ---------- 更新摄像头帧 ----------
void CaptureScreen::updateFrame() {
    if (m_cameraReady && m_cap.isOpened()) {
        cv::Mat frame;
        m_cap >> frame;
        if (!frame.empty()) {
            m_frameMat = frame;
            m_frameTexture = matToTexture(frame);
            if (m_frameTexture && m_frameSprite) m_frameSprite->setTexture(*m_frameTexture, true);
        }
    }
}

// ---------- 复制文件到 test.png ----------
bool CaptureScreen::copyFileToTest(const std::string& sourcePath) {
    try { fs::copy(sourcePath, "photo/test.png", fs::copy_options::overwrite_existing); return true; }
    catch (const std::exception& e) { return false; }
}

// ---------- 无窗口运行 Python ----------
static int runPythonSilent(const std::string& script, const std::wstring& workDir = L"") {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::wstring cmd = L"python " + std::wstring(script.begin(), script.end());
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    std::vector<wchar_t> cwdBuf(workDir.begin(), workDir.end());
    cwdBuf.push_back(L'\0');
    LPCWSTR cwd = workDir.empty() ? NULL : cwdBuf.data();
    // Redirect Python output to photo/python.log so failures are visible.
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE logFile = CreateFileW(L"photo/python.log", GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (logFile != INVALID_HANDLE_VALUE) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = logFile;
        si.hStdError = logFile;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    if (CreateProcessW(NULL, buf.data(), NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, cwd, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (logFile != INVALID_HANDLE_VALUE) CloseHandle(logFile);
        return static_cast<int>(code);
    }
    if (logFile != INVALID_HANDLE_VALUE) CloseHandle(logFile);
    return -1;
}

bool CaptureScreen::runPythonScript() {
    if (!fs::exists("scripts/process.py")) return false;
    int ret = runPythonSilent("../scripts/process.py", L"photo");
    return (ret == 0);
}

// ---------- 加载结果图片 ----------
sf::Image CaptureScreen::loadResultImage() {
    sf::Image img;
    img.loadFromFile("photo/clean_rewrite.png");
    return img;
}

// ---------- 加载图片到精灵 ----------
bool CaptureScreen::loadImageToSprite(const std::string& imagePath) {
    if (!fs::exists(imagePath)) return false;
    sf::Image img;
    if (!img.loadFromFile(imagePath)) return false;
    m_originalImage = img;
    m_frameTexture = std::make_unique<sf::Texture>();
    m_frameTexture->loadFromImage(img);
    m_frameSprite = std::make_unique<sf::Sprite>(*m_frameTexture);
    sf::Vector2u sz = img.getSize();
    float scale = std::min(1200.f / sz.x, 800.f / sz.y);
    m_frameSprite->setScale(sf::Vector2f(scale, scale));
    m_frameSprite->setPosition(sf::Vector2f(100.f, 100.f));
    return true;
}

// ---------- 启动异步批改 ----------
void CaptureScreen::processImageFromPath(const std::string& imagePath) {
    if (!fs::exists(imagePath)) return;
    m_isProcessing = true;
    m_scanLineY = 0.f;
    m_futureResult = std::async(std::launch::async, [this, imagePath]() -> sf::Image {
        clearPhotoFolder();
        if (!copyFileToTest(imagePath)) return sf::Image();
        if (!runPythonScript()) return sf::Image();
        return loadResultImage();
        });
}

// ==================== 初始化界面 ====================
void CaptureScreen::init() {
    m_font.openFromFile("C:/Windows/Fonts/msyh.ttf") || m_font.openFromFile("C:/Windows/Fonts/arial.ttf");
    clearPhotoFolder();

    // ========== 白色背景 + 漂浮花纹（与主界面一致） ==========
    m_backgroundGradient = sf::VertexArray(sf::PrimitiveType::TriangleStrip, 4);
    m_backgroundGradient[0] = sf::Vertex(sf::Vector2f(0.f, 0.f), sf::Color(250, 252, 255));
    m_backgroundGradient[1] = sf::Vertex(sf::Vector2f(WINDOW_WIDTH, 0.f), sf::Color(250, 252, 255));
    m_backgroundGradient[2] = sf::Vertex(sf::Vector2f(0.f, WINDOW_HEIGHT), sf::Color(250, 252, 255));
    m_backgroundGradient[3] = sf::Vertex(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT), sf::Color(250, 252, 255));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> xDist(100.f, WINDOW_WIDTH - 100.f);
    std::uniform_real_distribution<float> yDist(100.f, WINDOW_HEIGHT - 100.f);
    std::uniform_real_distribution<float> radiusDist(80.f, 300.f);
    std::uniform_real_distribution<float> speedDist(10.f, 40.f);
    std::uniform_int_distribution<int> alphaDist(20, 50);

    m_patterns.clear();
    struct ColorSpec { uint8_t r, g, b, a; };
    std::vector<ColorSpec> specs = {
        {130, 210, 255, 30}, {220, 180, 255, 28}, {255, 210, 220, 30},
        {180, 255, 220, 25}, {255, 230, 180, 30}, {200, 200, 255, 28},
        {255, 190, 210, 25}, {190, 240, 255, 28}, {250, 210, 230, 30},
        {210, 250, 200, 25}
    };
    for (const auto& spec : specs) {
        Pattern p;
        float radius = radiusDist(gen);
        p.shape.setRadius(radius);
        p.shape.setFillColor(sf::Color(spec.r, spec.g, spec.b, static_cast<uint8_t>(alphaDist(gen))));
        p.shape.setOrigin(sf::Vector2f(radius, radius));
        p.shape.setPosition(sf::Vector2f(xDist(gen), yDist(gen)));
        float angle = static_cast<float>(gen()) / std::mt19937::max() * 6.2832f;
        float speed = speedDist(gen);
        p.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        p.phase = static_cast<float>(gen()) / std::mt19937::max() * 6.2832f;
        p.radiusSpeed = 0.2f + static_cast<float>(gen()) / std::mt19937::max() * 0.3f;
        m_patterns.push_back(p);
    }

    // ========== 摄像头预览区（淡蓝灰底色） ==========
    cv::Mat infoMat(480, 640, CV_8UC3, cv::Scalar(230, 235, 245));
    cv::putText(infoMat, "Click capture to open camera", cv::Point(120, 240), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(60, 60, 70), 2);
    m_frameMat = infoMat;
    m_frameTexture = matToTexture(m_frameMat);
    m_frameSprite = std::make_unique<sf::Sprite>(*m_frameTexture);
    float scale = std::min(1200.f / m_frameMat.cols, 800.f / m_frameMat.rows);
    m_frameSprite->setScale(sf::Vector2f(scale, scale));
    m_frameSprite->setPosition(sf::Vector2f(100.f, 100.f));

    // ---- 返回按钮（自定义贴图，带备用） ----
    std::cout << "Current working directory: " << fs::current_path() << std::endl;
    bool loaded = false;
    if (ResourceManager::loadTextureWithFallback(m_backTexture, "return.png")) {
        m_backSprite = std::make_unique<sf::Sprite>(m_backTexture);
        float backScale = 80.f / static_cast<float>(m_backTexture.getSize().x);
        m_backSprite->setScale(sf::Vector2f(backScale, backScale));
        m_backSprite->setPosition(sf::Vector2f(20.f, 20.f));
        m_usingCustomBack = true;
        loaded = true;
    }
    if (!loaded) {
        m_usingCustomBack = false;
        m_backButtonFallback.setRadius(36.f);
        m_backButtonFallback.setFillColor(sf::Color(50, 50, 60, 180));
        m_backButtonFallback.setOutlineColor(sf::Color(220, 220, 230, 200));
        m_backButtonFallback.setOutlineThickness(2.5f);
        m_backButtonFallback.setPosition(sf::Vector2f(22.f, 22.f));
    }

    // ---- 摄像头开关按钮 ----
    const float btnW = 160.f, btnH = 50.f;
    m_cameraToggleBtnShadow.setSize(sf::Vector2f(btnW, btnH));
    m_cameraToggleBtnShadow.setFillColor(sf::Color(0, 0, 0, 50));
    m_cameraToggleBtnShadow.setPosition(sf::Vector2f(1610.f + 40.f - 80.f + 3.f, 350.f + 40.f - 120.f - 25.f + 3.f));
    m_cameraToggleBtn.setSize(sf::Vector2f(btnW, btnH));
    m_cameraToggleBtn.setFillColor(sf::Color(90, 150, 210));
    m_cameraToggleBtn.setOutlineColor(sf::Color(200, 210, 230));
    m_cameraToggleBtn.setOutlineThickness(2.f);
    float pcx = 1610.f + 40.f, pcy = 350.f + 40.f;
    m_cameraToggleBtn.setPosition(sf::Vector2f(pcx - 80.f, pcy - 120.f - 25.f));
    m_cameraToggleText = std::make_unique<sf::Text>(m_font);
    m_cameraToggleText->setString("Open Camera");
    m_cameraToggleText->setCharacterSize(24);
    m_cameraToggleText->setFillColor(sf::Color::White);
    sf::FloatRect btnBounds = m_cameraToggleBtn.getGlobalBounds();
    sf::FloatRect textBounds = m_cameraToggleText->getLocalBounds();
    m_cameraToggleText->setPosition(sf::Vector2f(
        btnBounds.position.x + (btnBounds.size.x - textBounds.size.x) / 2.f,
        btnBounds.position.y + (btnBounds.size.y - textBounds.size.y) / 2.f - textBounds.position.y
    ));

    // ---- 拍照按钮（带阴影） ----
    m_captureButtonShadow.setRadius(42.f);
    m_captureButtonShadow.setFillColor(sf::Color(0, 0, 0, 70));
    m_captureButtonShadow.setPosition(sf::Vector2f(1608.f, 348.f));
    m_captureButton.setRadius(40.f);
    m_captureButton.setFillColor(sf::Color(230, 60, 60));
    m_captureButton.setOutlineColor(sf::Color(255, 180, 180));
    m_captureButton.setOutlineThickness(3.f);
    m_captureButton.setPosition(sf::Vector2f(1610.f, 350.f));

    // ---- 答案按钮 (below camera area, always visible) ----
    if (ResourceManager::loadTextureWithFallback(m_answerBtnTexture, "new.png")) {
        m_answerBtnSprite = std::make_unique<sf::Sprite>(m_answerBtnTexture);
        float ansScale = 80.f / m_answerBtnTexture.getSize().x;
        m_answerBtnSprite->setScale(sf::Vector2f(ansScale, ansScale));
        m_answerBtnSprite->setPosition(sf::Vector2f(980.f, 930.f));
    }

    // ---- Answer overlay (hidden initially) ----
    m_ansOverlay.setSize(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    m_ansOverlay.setFillColor(sf::Color(0, 0, 0, 160));
    m_ansOverlay.setPosition(sf::Vector2f(0.f, 0.f));

    m_ansCloseBtn.setRadius(28.f);
    m_ansCloseBtn.setFillColor(sf::Color(80, 80, 80, 200));
    m_ansCloseBtn.setOutlineColor(sf::Color(200, 200, 200, 180));
    m_ansCloseBtn.setOutlineThickness(2.f);

    m_ansCloseX = std::make_unique<sf::Text>(m_font);
    m_ansCloseX->setString("X");
    m_ansCloseX->setCharacterSize(32);
    m_ansCloseX->setFillColor(sf::Color(255, 255, 255, 230));

    m_ansTitleInput = std::make_unique<sf::Text>(m_font);
    m_ansTitleInput->setCharacterSize(28);
    m_ansTitleInput->setFillColor(sf::Color(255, 255, 255, 230));

    m_ansTitleOutput = std::make_unique<sf::Text>(m_font);
    m_ansTitleOutput->setCharacterSize(28);
    m_ansTitleOutput->setFillColor(sf::Color(255, 255, 255, 230));

    // ---- 输入框 ----
    m_inputBox.setSize(sf::Vector2f(500.f, 50.f));
    m_inputBox.setFillColor(sf::Color(255, 255, 255, 220));
    m_inputBox.setOutlineColor(sf::Color(150, 150, 160));
    m_inputBox.setOutlineThickness(2.f);
    m_inputBox.setPosition(sf::Vector2f(1400.f, 600.f));
    m_inputText = std::make_unique<sf::Text>(m_font);
    m_inputText->setCharacterSize(30);
    m_inputText->setFillColor(sf::Color(40, 40, 60));
    m_inputText->setPosition(sf::Vector2f(1415.f, 610.f));
    m_inputText->setString("Enter image path...");

    // ---- 浏览按钮 (...) ----
    m_browseBtn.setSize(sf::Vector2f(48.f, 50.f));
    m_browseBtn.setFillColor(sf::Color(200, 200, 210, 220));
    m_browseBtn.setOutlineColor(sf::Color(150, 150, 160));
    m_browseBtn.setOutlineThickness(2.f);
    m_browseBtn.setPosition(sf::Vector2f(1908.f, 600.f));
    m_browseBtnText = std::make_unique<sf::Text>(m_font);
    m_browseBtnText->setString("...");
    m_browseBtnText->setCharacterSize(28);
    m_browseBtnText->setFillColor(sf::Color(60, 60, 80));
    sf::FloatRect bbg = m_browseBtn.getGlobalBounds();
    sf::FloatRect btg = m_browseBtnText->getLocalBounds();
    m_browseBtnText->setPosition(sf::Vector2f(
        bbg.position.x + (bbg.size.x - btg.size.x) / 2.f,
        bbg.position.y + (bbg.size.y - btg.size.y) / 2.f - btg.position.y));

    // ---- 处理按钮 ----
    m_processButton.setSize(sf::Vector2f(200.f, 60.f));
    m_processButton.setFillColor(sf::Color(70, 130, 220));
    m_processButton.setOutlineColor(sf::Color::White);
    m_processButton.setOutlineThickness(2.f);
    m_processButton.setPosition(sf::Vector2f(1550.f, 680.f));
    m_processButtonText = std::make_unique<sf::Text>(m_font);
    m_processButtonText->setString("Process");
    m_processButtonText->setCharacterSize(30);
    m_processButtonText->setFillColor(sf::Color::White);
    sf::FloatRect br = m_processButton.getGlobalBounds();
    sf::FloatRect btr = m_processButtonText->getLocalBounds();
    m_processButtonText->setPosition(sf::Vector2f(
        br.position.x + (br.size.x - btr.size.x) / 2.f,
        br.position.y + (br.size.y - btr.size.y) / 2.f - btr.position.y
    ));

    std::cout << "CaptureScreen init done" << std::endl;
}

// ==================== 事件处理（完整） ====================
void CaptureScreen::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        m_mousePos = window.mapPixelToCoords(moved->position);

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (pressed->button == sf::Mouse::Button::Left) {
            m_mousePressed = true;

            // 返回按钮
            bool backClicked = false;
            if (m_usingCustomBack && m_backSprite)
                backClicked = m_backSprite->getGlobalBounds().contains(m_mousePos);
            else if (!m_usingCustomBack) {
                sf::Vector2f c = m_backButtonFallback.getPosition() + sf::Vector2f(36.f, 36.f);
                backClicked = (std::hypot(m_mousePos.x - c.x, m_mousePos.y - c.y) <= 36.f);
            }
            if (backClicked) {
                if (m_app) { if (m_cap.isOpened()) m_cap.release(); m_app->switchScreen(std::make_unique<MainScreen>()); }
                return;
            }

            // 摄像头开关按钮
            if (m_cameraToggleBtn.getGlobalBounds().contains(m_mousePos)) {
                if (!m_cameraOpened) {
                    m_cap.open(0);
                    if (m_cap.isOpened()) { m_cameraOpened = true; m_cameraReady = true; m_cameraToggleBtn.setFillColor(sf::Color(220, 50, 50)); m_cameraToggleText->setString("Close Camera"); updateFrame(); }
                }
                else {
                    m_cap.release(); m_cameraOpened = false; m_cameraReady = false; m_cameraToggleBtn.setFillColor(sf::Color(70, 180, 70)); m_cameraToggleText->setString("Open Camera");
                    cv::Mat info(480, 640, CV_8UC3, cv::Scalar(50, 60, 80));
                    cv::putText(info, "Camera is off", { 215,230 }, cv::FONT_HERSHEY_SIMPLEX, 0.8, { 255,255,255 }, 2);
                    m_frameMat = info; m_frameTexture = matToTexture(info); m_frameSprite->setTexture(*m_frameTexture, true);
                    float s = std::min(1200.f / info.cols, 800.f / info.rows); m_frameSprite->setScale({ s,s }); m_frameSprite->setPosition({ 100,100 });
                }
                return;
            }

            // 拍照按钮
            sf::Vector2f capC = m_captureButton.getPosition() + sf::Vector2f(40.f, 40.f);
            if (m_cameraOpened && std::hypot(m_mousePos.x - capC.x, m_mousePos.y - capC.y) <= m_captureButton.getRadius()) {
                cv::Mat frame; m_cap >> frame;
                if (!frame.empty()) { cv::imwrite("photo/captured.jpg", frame); if (loadImageToSprite("photo/captured.jpg")) { m_pendingImagePath = "photo/captured.jpg"; m_pendingFrames = 1; } }
                return;
            }

            // 答案按钮
            if (m_answerBtnSprite && m_answerBtnSprite->getGlobalBounds().contains(m_mousePos)) {
                if (!m_showingAnswer && !m_isAnswerProcessing && !m_isProcessing) runAnswerScript();
                return;
            }

            // Answer overlay: close button or click outside → dismiss
            if (m_showingAnswer) {
                sf::FloatRect cb = m_ansCloseBtn.getGlobalBounds();
                bool onClose = cb.contains(m_mousePos) || std::hypot(
                    m_mousePos.x - (m_ansCloseBtn.getPosition().x + 28.f),
                    m_mousePos.y - (m_ansCloseBtn.getPosition().y + 28.f))
                    <= m_ansCloseBtn.getRadius() + 4.f;
                if (onClose) { m_showingAnswer = false; return; }
                // Click outside both images → dismiss
                bool onInput  = m_ansInputSprite  && m_ansInputSprite->getGlobalBounds().contains(m_mousePos);
                bool onOutput = m_ansOutputSprite && m_ansOutputSprite->getGlobalBounds().contains(m_mousePos);
                if (!onInput && !onOutput) { m_showingAnswer = false; return; }
                return;
            }

            // 浏览按钮
            if (m_browseBtn.getGlobalBounds().contains(m_mousePos)) { openFileDialog(); return; }

            // 点击输入框
            if (m_inputBox.getGlobalBounds().contains(m_mousePos)) { m_isInputActive = true; m_cursorPos = m_inputString.length(); clearSelection(); updateInputDisplay(); return; }

            // 处理按钮
            if (m_processButton.getGlobalBounds().contains(m_mousePos)) {
                if (!m_isProcessing && !m_isAnswerProcessing &&
                    !m_inputString.empty() && loadImageToSprite(m_inputString)) { m_pendingImagePath = m_inputString; m_pendingFrames = 1; }
                return;
            }
            m_isInputActive = false;
        }
    }
    if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>()) { if (released->button == sf::Mouse::Button::Left) m_mousePressed = false; }

    // 键盘输入（粘贴、方向键等）
    if (m_isInputActive) {
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
            bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::RControl);
            bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::RShift);
            if (ctrl && keyPressed->scancode == sf::Keyboard::Scancode::V) {
                sf::String clip = sf::Clipboard::getString(); std::string p = clip.toAnsiString();
                while (!p.empty() && (p.front() == '"' || p.front() == '\'')) p.erase(0, 1); while (!p.empty() && (p.back() == '"' || p.back() == '\'')) p.pop_back();
                if (!p.empty()) { if (m_hasSelection) deleteSelection(); m_inputString.insert(m_cursorPos, p); m_cursorPos += p.length(); clearSelection(); updateInputDisplay(); } return;
            }
            if (ctrl && keyPressed->scancode == sf::Keyboard::Scancode::C) { std::string cp = m_hasSelection ? getSelectedText() : m_inputString; if (!cp.empty()) sf::Clipboard::setString(cp); return; }
            if (ctrl && keyPressed->scancode == sf::Keyboard::Scancode::X) { std::string ct = m_hasSelection ? getSelectedText() : m_inputString; if (!ct.empty()) { sf::Clipboard::setString(ct); if (m_hasSelection) deleteSelection(); else { m_inputString.clear(); m_cursorPos = 0; } clearSelection(); updateInputDisplay(); } return; }
            if (ctrl && keyPressed->scancode == sf::Keyboard::Scancode::A) { m_selectionStart = 0; m_cursorPos = m_inputString.length(); m_hasSelection = true; updateInputDisplay(); return; }

            switch (keyPressed->scancode) {
            case sf::Keyboard::Scancode::Left:
                if (shift) { if (!m_hasSelection) { m_selectionStart = m_cursorPos; m_hasSelection = true; } }
                else clearSelection();
                if (m_cursorPos > 0) m_cursorPos--;
                updateInputDisplay(); break;
            case sf::Keyboard::Scancode::Right:
                if (shift) { if (!m_hasSelection) { m_selectionStart = m_cursorPos; m_hasSelection = true; } }
                else clearSelection();
                if (m_cursorPos < m_inputString.length()) m_cursorPos++;
                updateInputDisplay(); break;
            case sf::Keyboard::Scancode::Home:
                if (shift) { if (!m_hasSelection) { m_selectionStart = m_cursorPos; m_hasSelection = true; } }
                else clearSelection();
                m_cursorPos = 0; updateInputDisplay(); break;
            case sf::Keyboard::Scancode::End:
                if (shift) { if (!m_hasSelection) { m_selectionStart = m_cursorPos; m_hasSelection = true; } }
                else clearSelection();
                m_cursorPos = m_inputString.length(); updateInputDisplay(); break;
            case sf::Keyboard::Scancode::Delete:
                if (m_hasSelection) deleteSelection();
                else if (m_cursorPos < m_inputString.length()) m_inputString.erase(m_cursorPos, 1);
                clearSelection(); updateInputDisplay(); break;
            default: break;
            }
        }
    }
    if (m_isInputActive) {
        if (const auto* textEntered = event.getIf<sf::Event::TextEntered>()) {
            if (textEntered->unicode == 8) { // Backspace
                if (m_hasSelection) deleteSelection();
                else if (m_cursorPos > 0) { m_cursorPos--; m_inputString.erase(m_cursorPos, 1); }
            }
            else if (textEntered->unicode == 13) { // Enter
                m_isInputActive = false; clearSelection();
                if (!m_isProcessing && !m_isAnswerProcessing &&
                    !m_inputString.empty() && loadImageToSprite(m_inputString)) { m_pendingImagePath = m_inputString; m_pendingFrames = 1; }
                return;
            }
            else if (textEntered->unicode >= 32 && textEntered->unicode < 128) {
                if (m_hasSelection) deleteSelection();
                m_inputString.insert(m_cursorPos, 1, static_cast<char>(textEntered->unicode));
                m_cursorPos++; clearSelection();
            }
            updateInputDisplay();
        }
    }
}

// ==================== 更新循环 ====================
void CaptureScreen::update(float deltaTime) {
    // 异步批改任务检查
    if (m_isProcessing && m_futureResult.valid()) {
        if (m_futureResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            m_resultImage = m_futureResult.get(); m_isProcessing = false;
            if (m_resultImage.getSize().x > 0 && m_app) {
                sf::Image pie, trend;
                pie.loadFromFile("photo/pie.png"); trend.loadFromFile("photo/trend.png");
                m_app->switchScreen(std::make_unique<ResultScreen>(m_originalImage, m_resultImage, pie, trend));
                return;
            }
        }
    }

    // 异步答案任务检查
    if (m_isAnswerProcessing && m_futureAnswer.valid()) {
        if (m_futureAnswer.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            m_futureAnswer.get(); m_isAnswerProcessing = false;
            loadAnswerResult();
        }
    }
    // 延迟处理
    if (m_pendingFrames > 0) m_pendingFrames--;
    else if (m_pendingFrames == 0 && !m_isProcessing && !m_isAnswerProcessing) { m_pendingFrames = -1; processImageFromPath(m_pendingImagePath); }
    // 摄像头开启时更新帧
    if (m_cameraOpened && m_cap.isOpened() && m_pendingFrames < 0 && !m_isProcessing && !m_isAnswerProcessing) updateFrame();

    // 扫描动画（批改 + 答案共用）
    if ((m_isProcessing || m_isAnswerProcessing) && m_frameSprite) {
        sf::FloatRect b = m_frameSprite->getGlobalBounds(); float h = b.size.y;
        m_scanLineY += m_scanSpeed * deltaTime;
        while (m_scanLineY >= b.position.y + h) m_scanLineY -= h;
        while (m_scanLineY < b.position.y) m_scanLineY += h;
    }

    // 背景花纹动画
    for (auto& p : m_patterns) {
        sf::Vector2f pos = p.shape.getPosition();
        pos += p.velocity * deltaTime;
        float r = p.shape.getRadius();
        if (pos.x < r) { pos.x = r; p.velocity.x = -p.velocity.x; }
        if (pos.x > WINDOW_WIDTH - r) { pos.x = WINDOW_WIDTH - r; p.velocity.x = -p.velocity.x; }
        if (pos.y < r) { pos.y = r; p.velocity.y = -p.velocity.y; }
        if (pos.y > WINDOW_HEIGHT - r) { pos.y = WINDOW_HEIGHT - r; p.velocity.y = -p.velocity.y; }
        p.shape.setPosition(pos);
        p.phase += deltaTime * p.radiusSpeed;
    }

    // ========== 按钮悬停动态效果 ==========
    // Process 按钮
    if (m_processButton.getGlobalBounds().contains(m_mousePos))
        m_processButton.setFillColor(sf::Color(100, 160, 240));
    else
        m_processButton.setFillColor(sf::Color(70, 130, 220));

    // 浏览按钮
    if (m_browseBtn.getGlobalBounds().contains(m_mousePos))
        m_browseBtn.setFillColor(sf::Color(170, 170, 190, 240));
    else
        m_browseBtn.setFillColor(sf::Color(200, 200, 210, 220));

    // 摄像头开关按钮
    if (m_cameraToggleBtn.getGlobalBounds().contains(m_mousePos)) {
        if (m_cameraOpened)
            m_cameraToggleBtn.setFillColor(sf::Color(255, 80, 80));          // 打开时悬停更红
        else
            m_cameraToggleBtn.setFillColor(sf::Color(120, 180, 240));        // 关闭时悬停更亮
    }
    else {
        if (m_cameraOpened)
            m_cameraToggleBtn.setFillColor(sf::Color(220, 50, 50));
        else
            m_cameraToggleBtn.setFillColor(sf::Color(90, 150, 210));
    }

    // 拍照按钮（圆形）
    sf::Vector2f capCenter = m_captureButton.getPosition() + sf::Vector2f(40.f, 40.f);
    bool captureHover = (std::hypot(m_mousePos.x - capCenter.x, m_mousePos.y - capCenter.y) <= 40.f);
    if (captureHover) {
        m_captureButton.setFillColor(sf::Color(255, 100, 100));    // 悬停变亮
        m_captureButton.setOutlineColor(sf::Color(255, 200, 200));
    }
    else {
        m_captureButton.setFillColor(sf::Color(230, 60, 60));
        m_captureButton.setOutlineColor(sf::Color(255, 180, 180));
    }

    // 返回按钮悬停效果已在 draw 中实现（通过图片变色）
    // 备用圆形按钮悬停也在 draw 中处理

    // 光标闪烁
    static float cursorTimer = 0;
    cursorTimer += deltaTime;
    if (cursorTimer >= 0.5f) { cursorTimer = 0; m_showCursor = !m_showCursor; if (m_isInputActive) updateInputDisplay(); }
}

// ==================== 绘制 ====================
void CaptureScreen::draw(sf::RenderWindow& window) {
    window.clear(sf::Color(250, 252, 255));

    // 1. 背景花纹
    for (const auto& p : m_patterns)
        window.draw(p.shape);

    // 2. 摄像头/图片
    if (m_frameSprite) window.draw(*m_frameSprite);

    // 3. 返回按钮
    if (m_usingCustomBack && m_backSprite) {
        bool hover = m_backSprite->getGlobalBounds().contains(m_mousePos);
        m_backSprite->setColor(hover ? sf::Color(200, 200, 200, 255) : sf::Color(255, 255, 255, 255));
        window.draw(*m_backSprite);
    }
    else if (!m_usingCustomBack) {
        sf::Vector2f btnC = m_backButtonFallback.getPosition() + sf::Vector2f(36.f, 36.f);
        bool hover = (std::hypot(m_mousePos.x - btnC.x, m_mousePos.y - btnC.y) <= 36.f);
        sf::CircleShape shadow(36.f); shadow.setPosition(m_backButtonFallback.getPosition() + sf::Vector2f(3, 3)); shadow.setFillColor(sf::Color(0, 0, 0, 60)); window.draw(shadow);
        m_backButtonFallback.setFillColor(hover ? sf::Color(80, 80, 90, 200) : sf::Color(50, 50, 60, 180));
        m_backButtonFallback.setOutlineColor(hover ? sf::Color(255, 255, 255, 220) : sf::Color(220, 220, 230, 180)); window.draw(m_backButtonFallback);
        sf::ConvexShape arrow; arrow.setPointCount(3); arrow.setPoint(0, btnC + sf::Vector2f(-14, -18)); arrow.setPoint(1, btnC + sf::Vector2f(-14, 18)); arrow.setPoint(2, btnC + sf::Vector2f(16, 0)); arrow.setFillColor(hover ? sf::Color(255, 255, 255, 240) : sf::Color(220, 220, 230, 220)); window.draw(arrow);
    }

    // 4. 摄像头开关按钮（带阴影）
    window.draw(m_cameraToggleBtnShadow);
    window.draw(m_cameraToggleBtn);
    if (m_cameraToggleText) window.draw(*m_cameraToggleText);

    // 5. 拍照按钮（带阴影）
    window.draw(m_captureButtonShadow);
    window.draw(m_captureButton);
    sf::CircleShape inner(14.f); inner.setFillColor(sf::Color::White);
    inner.setPosition(m_captureButton.getPosition() + sf::Vector2f(26.f, 26.f));
    window.draw(inner);

    // 5b. 答案按钮 (always visible)
    if (m_answerBtnSprite) {
        bool ansHover = m_answerBtnSprite->getGlobalBounds().contains(m_mousePos);
        m_answerBtnSprite->setColor(ansHover ? sf::Color(200, 200, 200, 255) : sf::Color(255, 255, 255, 255));
        window.draw(*m_answerBtnSprite);
    }

    // 6. 输入框和处理按钮
    window.draw(m_inputBox); if (m_inputText) window.draw(*m_inputText);
    window.draw(m_browseBtn); if (m_browseBtnText) window.draw(*m_browseBtnText);
    window.draw(m_processButton); if (m_processButtonText) window.draw(*m_processButtonText);

    // 7. 扫描动画（批改 + 答案共用）
    if ((m_isProcessing || m_isAnswerProcessing) && m_frameSprite) {
        sf::FloatRect r = m_frameSprite->getGlobalBounds(); float x = r.position.x, w = r.size.x, h = r.size.y; int strips = 200; float stripH = h / strips;
        sf::RectangleShape dark({ w,h }); dark.setPosition({ x,r.position.y }); dark.setFillColor({ 0,0,0,25 }); window.draw(dark);
        for (int i = 0; i < strips; ++i) {
            float y = r.position.y + i * stripH; float cy = y + stripH / 2.f; float dist = std::abs(cy - m_scanLineY) / h; if (dist > 0.5f) dist = 1.0f - dist; float t = dist * 2.f; if (t > 1.f) t = 1.f;
            sf::Color c(static_cast<uint8_t>(255 * (1 - t) + 80 * t), static_cast<uint8_t>(80 * (1 - t) + 160 * t), static_cast<uint8_t>(120 * (1 - t) + 255 * t), 110);
            sf::RectangleShape strip({ w,stripH + 1.f }); strip.setPosition({ x,y }); strip.setFillColor(c); window.draw(strip);
        }
    }

    // 8. Answer overlay
    if (m_showingAnswer) {
        window.draw(m_ansOverlay);
        if (m_ansInputSprite)  window.draw(*m_ansInputSprite);
        if (m_ansOutputSprite) window.draw(*m_ansOutputSprite);
        window.draw(m_ansCloseBtn);
        window.draw(*m_ansCloseX);
        if (m_ansTitleInput)  window.draw(*m_ansTitleInput);
        if (m_ansTitleOutput) window.draw(*m_ansTitleOutput);
    }
}

// ========== 输入框辅助函数 ==========
void CaptureScreen::updateInputDisplay() {
    if (!m_inputText) return;
    if (!m_isInputActive) {
        if (m_inputString.empty()) { m_inputText->setString("Enter image path..."); m_inputText->setFillColor(sf::Color(120, 120, 140)); }
        else { m_inputText->setString(m_inputString); m_inputText->setFillColor(sf::Color(40, 40, 60)); }
        return;
    }
    m_inputText->setFillColor(sf::Color(40, 40, 60));
    if (m_inputString.empty()) { m_inputText->setString(m_showCursor ? "_" : ""); return; }
    std::string before = m_inputString.substr(0, m_cursorPos);
    std::string after = m_inputString.substr(m_cursorPos);
    std::string cursor = m_showCursor ? "|" : " ";
    m_inputText->setString(before + cursor + after);
}

void CaptureScreen::clearSelection() { m_hasSelection = false; }
void CaptureScreen::deleteSelection() {
    if (!m_hasSelection) return;
    size_t start = std::min(m_cursorPos, m_selectionStart);
    size_t end = std::max(m_cursorPos, m_selectionStart);
    m_inputString.erase(start, end - start);
    m_cursorPos = start;
    m_hasSelection = false;
}
std::string CaptureScreen::getSelectedText() const {
    if (!m_hasSelection) return "";
    size_t start = std::min(m_cursorPos, m_selectionStart);
    size_t end = std::max(m_cursorPos, m_selectionStart);
    return m_inputString.substr(start, end - start);
}

// ========== Answer script ==========
void CaptureScreen::runAnswerScript() {
    // Take photo first if camera is open
    if (m_cameraOpened && m_cap.isOpened()) {
        cv::Mat frame;
        m_cap >> frame;
        if (!frame.empty()) {
            cv::imwrite("photo/captured.jpg", frame);
            loadImageToSprite("photo/captured.jpg");
        }
    }

    if (!fs::exists("photo/captured.jpg")) return;

    m_isAnswerProcessing = true;
    m_futureAnswer = std::async(std::launch::async, []() -> bool {
        try {
            fs::copy_file("photo/captured.jpg", "photo/test.png",
                          fs::copy_options::overwrite_existing);
        } catch (...) { return false; }

        int ret = runPythonSilent("../scripts/answer.py", L"photo");
        return (ret == 0);
    });
}

void CaptureScreen::loadAnswerResult() {
    std::string inputPath  = "photo/captured.jpg";
    std::string outputPath = "photo/clean_rewrite.png";

    if (!fs::exists(outputPath)) {
        std::cout << "Answer: output not found: " << outputPath << std::endl;
        return;
    }

    float maxW = WINDOW_WIDTH * 0.42f;
    float maxH = WINDOW_HEIGHT * 0.65f;

    // Input (captured photo)
    if (fs::exists(inputPath) && m_ansInputTex.loadFromFile(inputPath)) {
        m_ansInputSprite = std::make_unique<sf::Sprite>(m_ansInputTex);
        float iw = static_cast<float>(m_ansInputTex.getSize().x);
        float ih = static_cast<float>(m_ansInputTex.getSize().y);
        float sc = std::min(maxW / iw, maxH / ih);
        m_ansInputSprite->setScale(sf::Vector2f(sc, sc));
        float imgW = iw * sc, imgH = ih * sc;
        float imgX = (WINDOW_WIDTH - imgW) * 0.5f - maxW * 0.55f;
        float imgY = (WINDOW_HEIGHT - imgH) * 0.5f;
        m_ansInputSprite->setPosition(sf::Vector2f(imgX, imgY));
        m_ansTitleInput->setString("Original");
        m_ansTitleInput->setPosition(sf::Vector2f(imgX, imgY - 36.f));
    }

    // Output (answer image)
    if (m_ansOutputTex.loadFromFile(outputPath)) {
        m_ansOutputSprite = std::make_unique<sf::Sprite>(m_ansOutputTex);
        float ow = static_cast<float>(m_ansOutputTex.getSize().x);
        float oh = static_cast<float>(m_ansOutputTex.getSize().y);
        float sc = std::min(maxW / ow, maxH / oh);
        m_ansOutputSprite->setScale(sf::Vector2f(sc, sc));
        float imgW = ow * sc, imgH = oh * sc;
        float imgX = (WINDOW_WIDTH - imgW) * 0.5f + maxW * 0.55f;
        float imgY = (WINDOW_HEIGHT - imgH) * 0.5f;
        m_ansOutputSprite->setPosition(sf::Vector2f(imgX, imgY));
        m_ansTitleOutput->setString("Answer");
        m_ansTitleOutput->setPosition(sf::Vector2f(imgX, imgY - 36.f));

        // Close button at top-right of answer image
        float cx = imgX + imgW;
        float cy = imgY;
        m_ansCloseBtn.setPosition(sf::Vector2f(cx - 28.f, cy - 28.f));
        sf::FloatRect xb = m_ansCloseX->getLocalBounds();
        m_ansCloseX->setOrigin(sf::Vector2f(xb.size.x * 0.5f, xb.size.y * 0.5f));
        m_ansCloseX->setPosition(sf::Vector2f(cx, cy));
    }

    m_showingAnswer = true;
    std::cout << "Answer overlay shown" << std::endl;
}

// ========== file dialog ==========
void CaptureScreen::openFileDialog() {
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
        m_inputString = std::string(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, &m_inputString[0], len, NULL, NULL);
        while (!m_inputString.empty() && m_inputString.back() == '\0') m_inputString.pop_back();
        m_cursorPos = m_inputString.length();
        m_isInputActive = false;
        updateInputDisplay();
    }
}
