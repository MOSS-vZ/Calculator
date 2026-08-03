#include "MainScreen.h"
#include "ResourceManager.h"
#include "CaptureScreen.h"
#include "PracticeScreen.h"
#include "HistoryScreen.h"
#include "WrongBookScreen.h"
#include "App.h"
#include <iostream>
#include <cmath>
#include <cstdint>
#include <ctime>

MainScreen::MainScreen()
    : m_computer(std::make_unique<sf::Sprite>(ResourceManager::getInstance().getTexture("computer.png"))),
    m_word(std::make_unique<sf::Sprite>(ResourceManager::getInstance().getTexture("word1.png"))),
    m_word2(std::make_unique<sf::Sprite>(ResourceManager::getInstance().getTexture("word2.png"))) {
    m_computer->setColor(sf::Color(255, 255, 255, 0));
    m_word->setColor(sf::Color(255, 255, 255, 0));
    m_word2->setColor(sf::Color(255, 255, 255, 0));
}

float MainScreen::easeOut(float t) const { return 1.f - (1.f - t) * (1.f - t); }
float MainScreen::lerp(float a, float b, float t) const { return a + (b - a) * t; }
float MainScreen::easeOutBack(float t) const {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
}

void MainScreen::initPatterns() {
    m_patterns.clear();
    struct ColorSpec { uint8_t r, g, b, a; float radius; };
    std::vector<ColorSpec> specs = {
        {130, 210, 255, 30, 600.f}, {220, 180, 255, 28, 450.f},
        {255, 210, 220, 30, 700.f}, {180, 255, 220, 25, 380.f},
        {255, 230, 180, 30, 500.f}, {200, 200, 255, 28, 650.f},
        {255, 190, 210, 25, 420.f}, {190, 240, 255, 28, 550.f},
        {250, 210, 230, 30, 480.f}, {210, 250, 200, 25, 620.f},
    };
    for (const auto& spec : specs) {
        Pattern p;
        float x = 100.f + static_cast<float>(rand() % 1800);
        float y = 100.f + static_cast<float>(rand() % 1300);
        p.shape.setPosition(sf::Vector2f(x, y));
        p.shape.setRadius(spec.radius);
        p.shape.setFillColor(sf::Color(spec.r, spec.g, spec.b, spec.a));
        p.shape.setOrigin(sf::Vector2f(spec.radius, spec.radius));
        float angle = static_cast<float>(rand()) / RAND_MAX * 6.2832f;
        float speed = 0.15f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
        p.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        p.phase = static_cast<float>(rand()) / RAND_MAX * 6.2832f;
        p.radiusSpeed = 0.15f + static_cast<float>(rand()) / RAND_MAX * 0.25f;
        m_patterns.push_back(std::move(p));
    }
}

// 侧边栏装饰图案初始化
void MainScreen::initSidebarPatterns() {
    m_sidebarPatterns.clear();
    // 5个不同颜色的大圆，随机分布在侧边栏区域内
    struct { uint8_t r, g, b, a; float radius; } specs[] = {
        {255, 180, 200, 40, 80.f},
        {180, 220, 255, 35, 70.f},
        {220, 255, 180, 35, 90.f},
        {255, 220, 150, 30, 60.f},
        {200, 180, 255, 40, 85.f},
    };
    for (auto& spec : specs) {
        SidebarPattern p;
        float x = static_cast<float>(rand() % (int)(SIDEBAR_WIDTH - spec.radius * 2)) + spec.radius;
        float y = static_cast<float>(rand() % (int)(SIDEBAR_HEIGHT - spec.radius * 2)) + spec.radius;
        p.shape.setRadius(spec.radius);
        p.shape.setFillColor(sf::Color(spec.r, spec.g, spec.b, spec.a));
        p.shape.setOrigin(sf::Vector2f(spec.radius, spec.radius));
        p.shape.setPosition(sf::Vector2f(x, y));
        float angle = static_cast<float>(rand()) / RAND_MAX * 6.2832f;
        float speed = 0.3f + static_cast<float>(rand()) / RAND_MAX * 0.4f;
        p.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        m_sidebarPatterns.push_back(p);
    }
}

void MainScreen::init() {
    auto& rm = ResourceManager::getInstance();
    m_titleFont.openFromFile("C:/Windows/Fonts/msyh.ttf") ||
        m_titleFont.openFromFile("C:/Windows/Fonts/arial.ttf");
    m_titleText = std::make_unique<sf::Text>(m_titleFont);
    m_titleText->setString("Arithmetic Practice System");
    m_titleText->setCharacterSize(72);
    m_titleText->setFillColor(sf::Color(60, 80, 180, 200));
    sf::FloatRect tb = m_titleText->getLocalBounds();
    m_titleText->setOrigin(sf::Vector2f(tb.size.x * 0.5f, tb.size.y * 0.5f));
    m_titleText->setPosition(sf::Vector2f(WINDOW_WIDTH / 2.f, 150.f));

    m_computer->setScale(sf::Vector2f(COMPUTER_SCALE, COMPUTER_SCALE));
    sf::FloatRect cb = m_computer->getLocalBounds();
    float halfW = cb.size.x * 0.5f * COMPUTER_SCALE;
    float halfH = cb.size.y * 0.5f * COMPUTER_SCALE;
    m_computer->setOrigin(sf::Vector2f(cb.size.x * 0.5f, cb.size.y * 0.5f));
    m_computerCenter = sf::Vector2f(COMPUTER_X + halfW, COMPUTER_Y + halfH);
    m_computer->setPosition(m_computerCenter);

    m_word->setPosition(sf::Vector2f(WORD_X, WORD_Y));
    m_word->setScale(sf::Vector2f(WORD_SCALE, WORD_SCALE));
    m_word2->setPosition(sf::Vector2f(WORD2_X, WORD2_Y));
    m_word2->setScale(sf::Vector2f(WORD2_SCALE, WORD2_SCALE));

    const sf::Texture& texStart = rm.getTexture("startpractice.png");
    const sf::Texture& texPhoto = rm.getTexture("takephoto.png");
    m_btnStartPractice = std::make_unique<Button>(texStart, BTN_START_X, BTN_START_Y);
    m_btnStartPractice->setScale(BUTTON_SCALE, BUTTON_SCALE);
    m_btnStartPractice->setColor(sf::Color(255, 255, 255, 0));
    m_btnTakePhoto = std::make_unique<Button>(texPhoto, BTN_PHOTO_X, BTN_PHOTO_Y);
    m_btnTakePhoto->setScale(BUTTON_SCALE, BUTTON_SCALE);
    m_btnTakePhoto->setColor(sf::Color(255, 255, 255, 0));

    const sf::Texture& texMenu = rm.getTexture("menu.png");
    m_btnMenu = std::make_unique<Button>(texMenu, MENU_BTN_X, MENU_BTN_Y);
    m_btnMenu->setScale(MENU_BTN_SCALE, MENU_BTN_SCALE);
    m_btnMenu->setColor(sf::Color(150, 150, 150, 255));

    // ---------- 侧边栏背景 ----------
    m_sidebarBg.setSize(sf::Vector2f(SIDEBAR_WIDTH, SIDEBAR_HEIGHT));
    m_sidebarBg.setFillColor(sf::Color(255, 255, 255, 230));  // 更通透的背景
    m_sidebarBg.setOutlineColor(sf::Color(200, 200, 210, 120));
    m_sidebarBg.setOutlineThickness(1.5f);
    m_sidebarBg.setPosition(sf::Vector2f(SIDEBAR_X - SIDEBAR_WIDTH, SIDEBAR_Y));

    m_sidebarShadow.setSize(sf::Vector2f(SIDEBAR_WIDTH + 14.f, SIDEBAR_HEIGHT));
    m_sidebarShadow.setFillColor(sf::Color(0, 0, 0, 35));
    m_sidebarShadow.setPosition(sf::Vector2f(SIDEBAR_X - SIDEBAR_WIDTH - 7.f, SIDEBAR_Y + 8.f));

    // 顶部标题栏
    m_sidebarTopBar.setSize(sf::Vector2f(SIDEBAR_WIDTH, 90.f));
    m_sidebarTopBar.setFillColor(sf::Color(230, 235, 245, 220));
    m_sidebarTopBar.setOutlineThickness(0.f);

    // 标题文字
    m_sidebarTitle = std::make_unique<sf::Text>(m_titleFont);
    m_sidebarTitle->setString("Menu");
    m_sidebarTitle->setCharacterSize(45);
    m_sidebarTitle->setFillColor(sf::Color(50, 50, 70, 220));
    sf::FloatRect titleRect = m_sidebarTitle->getLocalBounds();
    m_sidebarTitle->setOrigin(sf::Vector2f(titleRect.size.x * 0.5f, titleRect.size.y * 0.5f));

    // 分隔线
    m_separatorLine.setSize(sf::Vector2f(3.f, WINDOW_HEIGHT));
    m_separatorLine.setFillColor(sf::Color(180, 180, 200, 100));

    // 内部按钮
    const sf::Texture& texHist = rm.getTexture("history.png");
    const sf::Texture& texWrong = rm.getTexture("wrong.png");
    m_btnHistory = std::make_unique<Button>(texHist, SIDEBAR_BTN_X, SIDEBAR_BTN_Y_START);
    m_btnHistory->setScale(SIDEBAR_BTN_SCALE, SIDEBAR_BTN_SCALE);
    m_btnHistory->setColor(sf::Color::White);
    m_btnWrong = std::make_unique<Button>(texWrong, SIDEBAR_BTN_X, SIDEBAR_BTN_Y_START + SIDEBAR_BTN_SPACING);
    m_btnWrong->setScale(SIDEBAR_BTN_SCALE, SIDEBAR_BTN_SCALE);
    m_btnWrong->setColor(sf::Color::White);

    float initX = SIDEBAR_BTN_X - SIDEBAR_WIDTH;
    m_btnHistory->setPosition(initX, SIDEBAR_BTN_Y_START);
    m_btnWrong->setPosition(initX, SIDEBAR_BTN_Y_START + SIDEBAR_BTN_SPACING);

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    initPatterns();
    initSidebarPatterns();   // 初始化侧边栏装饰

    m_animClock.restart();
    m_floatClock.restart();
    m_animFinished = false;
    m_sidebarOpen = false; m_sidebarOffset = 0.f; m_sidebarTarget = 0.f; m_sidebarAnimating = false;
    m_currentMenuScale = MENU_BTN_SCALE;
    m_currentStartScale = BUTTON_SCALE;
    m_currentPhotoScale = BUTTON_SCALE;
    m_currentHistScale = SIDEBAR_BTN_SCALE;
    m_currentWrongScale = SIDEBAR_BTN_SCALE;
    std::cout << "MainScreen init done (SFML 3.x)" << std::endl;
}

void MainScreen::toggleSidebar() {
    if (m_sidebarAnimating) return;
    m_sidebarOpen = !m_sidebarOpen;
    m_sidebarAnimStart = m_sidebarOffset;
    m_sidebarTarget = m_sidebarOpen ? SIDEBAR_WIDTH : 0.f;
    m_sidebarAnimating = true;
    m_sidebarAnimClock.restart();
}
void MainScreen::closeSidebar() { if (m_sidebarOpen && !m_sidebarAnimating) toggleSidebar(); }

void MainScreen::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        m_mousePos = window.mapPixelToCoords(moved->position);
    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (pressed->button == sf::Mouse::Button::Left) {
            m_mousePressed = true;
            if (m_btnMenu && m_btnMenu->isHovered(m_mousePos)) { toggleSidebar(); return; }
            if (m_sidebarOpen || m_sidebarAnimating) {
                if (m_btnHistory && m_btnHistory->isHovered(m_mousePos)) {
                    if (m_app) m_app->switchScreen(std::make_unique<HistoryScreen>()); return;
                }
                if (m_btnWrong && m_btnWrong->isHovered(m_mousePos)) {
                    if (m_app) m_app->switchScreen(std::make_unique<WrongBookScreen>()); return;
                }
                sf::FloatRect sr = m_sidebarBg.getGlobalBounds();
                if (!sr.contains(m_mousePos)) closeSidebar();
            }
            if (m_btnStartPractice && m_btnStartPractice->isHovered(m_mousePos)) {
                if (m_app) m_app->switchScreen(std::make_unique<PracticeScreen>()); return;
            }
            if (m_btnTakePhoto && m_btnTakePhoto->isHovered(m_mousePos)) {
                if (m_app) m_app->switchScreen(std::make_unique<CaptureScreen>()); return;
            }
        }
    }
    if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
        if (released->button == sf::Mouse::Button::Left) m_mousePressed = false;
    if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        if (key->scancode == sf::Keyboard::Scancode::R) { m_animClock.restart(); m_animFinished = false; }
}

void MainScreen::update(float dt) {
    float elapsed = m_animClock.getElapsedTime().asSeconds();
    float progress = std::min(elapsed / m_animDuration, 1.0f);
    if (progress >= 1.0f && !m_animFinished) { m_animFinished = true; std::cout << "anim done\n"; }

    // ========== 淡入效果（非按钮） ==========
    if (!m_animFinished) {
        float fp = std::min(progress / 0.7f, 1.0f);
        uint8_t alpha = static_cast<uint8_t>(easeOut(fp) * 255.f);
        m_computer->setColor(sf::Color(255, 255, 255, alpha));
        m_word->setColor(sf::Color(255, 255, 255, alpha));
        m_word2->setColor(sf::Color(255, 255, 255, alpha));
    }
    else {
        m_computer->setColor(sf::Color::White); m_word->setColor(sf::Color::White);
        m_word2->setColor(sf::Color::White);
    }

    // ========== 按钮从下方飞入 ==========
    if (!m_animFinished) {
        float bp1 = std::max(0.f, std::min((progress - 0.4f) / 0.6f, 1.0f));
        float bp2 = std::max(0.f, std::min((progress - 0.2f) / 0.6f, 1.0f));
        float y1 = BTN_START_Y + 300.f * (1.f - easeOut(bp1));
        float y2 = BTN_PHOTO_Y + 300.f * (1.f - easeOut(bp2));
        m_btnStartPractice->setPosition(BTN_START_X, y1);
        m_btnTakePhoto->setPosition(BTN_PHOTO_X, y2);
        m_btnStartPractice->setColor(sf::Color(255, 255, 255, static_cast<uint8_t>(easeOut(bp1) * 255.f)));
        m_btnTakePhoto->setColor(sf::Color(255, 255, 255, static_cast<uint8_t>(easeOut(bp2) * 255.f)));
        m_btnStartPractice->setScale(BUTTON_SCALE, BUTTON_SCALE);
        m_btnTakePhoto->setScale(BUTTON_SCALE, BUTTON_SCALE);
    }
    else {
        m_btnStartPractice->setColor(sf::Color::White);
        m_btnTakePhoto->setColor(sf::Color::White);
    }

    // ========== 侧边栏动画 ==========
    if (m_sidebarAnimating) {
        float ae = m_sidebarAnimClock.getElapsedTime().asSeconds();
        float ap = std::min(ae / SIDEBAR_ANIM_DURATION, 1.0f);
        m_sidebarOffset = m_sidebarAnimStart + (m_sidebarTarget - m_sidebarAnimStart) * easeOut(ap);
        if (ap >= 1.0f) { m_sidebarAnimating = false; m_sidebarOffset = m_sidebarTarget; }
    }
    else m_sidebarOffset = m_sidebarTarget;

    // ========== 闪入动画 ==========
    float titleBaseX = WINDOW_WIDTH / 2.f;
    float word1BaseX = WORD_X;
    float word2BaseX = WORD2_X;
    if (!m_animFinished) {
        auto calcProgress = [&](float start, float duration) -> float {
            return std::max(0.f, std::min((progress - start) / duration, 1.f));
            };
        float pTitle = calcProgress(0.0f, 0.8f);
        float pWord1 = calcProgress(0.15f, 0.7f);
        float pWord2 = calcProgress(0.3f, 0.7f);
        float et = easeOutBack(pTitle);
        float ew1 = easeOutBack(pWord1);
        float ew2 = easeOutBack(pWord2);
        float titleStartX = -500.f;
        float wordStartX = -300.f;
        titleBaseX = titleStartX + (WINDOW_WIDTH / 2.f - titleStartX) * et;
        word1BaseX = wordStartX + (WORD_X - wordStartX) * ew1;
        word2BaseX = wordStartX + (WORD2_X - wordStartX) * ew2;
    }

    // ========== 坐标压缩 ==========
    float availableWidth = WINDOW_WIDTH - m_sidebarOffset;
    if (availableWidth < 100.f) availableWidth = 100.f;
    auto compressX = [&](float baseX) -> float {
        return m_sidebarOffset + (baseX / WINDOW_WIDTH) * availableWidth;
        };

    m_titleText->setPosition(sf::Vector2f(compressX(titleBaseX), 150.f));
    m_word->setPosition(sf::Vector2f(compressX(word1BaseX), WORD_Y));
    m_word2->setPosition(sf::Vector2f(compressX(word2BaseX), WORD2_Y));
    m_computer->setPosition(sf::Vector2f(compressX(m_computerCenter.x), m_computerCenter.y));

    if (!m_animFinished) {
        float startY = m_btnStartPractice->getPosition().y;
        float photoY = m_btnTakePhoto->getPosition().y;
        m_btnStartPractice->setPosition(compressX(BTN_START_X), startY);
        m_btnTakePhoto->setPosition(compressX(BTN_PHOTO_X), photoY);
    }
    else {
        m_btnStartPractice->setPosition(compressX(BTN_START_X), BTN_START_Y);
        m_btnTakePhoto->setPosition(compressX(BTN_PHOTO_X), BTN_PHOTO_Y);
    }

    // ========== 侧边栏所有子元素位置更新 ==========
    m_btnMenu->setPosition(SIDEBAR_X + m_sidebarOffset + MENU_BTN_X_OFFSET, MENU_BTN_Y);
    m_sidebarBg.setPosition(sf::Vector2f(SIDEBAR_X - SIDEBAR_WIDTH + m_sidebarOffset, SIDEBAR_Y));
    m_sidebarShadow.setPosition(sf::Vector2f(SIDEBAR_X - SIDEBAR_WIDTH - 7.f + m_sidebarOffset, SIDEBAR_Y + 8.f));
    m_separatorLine.setPosition(sf::Vector2f(SIDEBAR_X + m_sidebarOffset - 1.f, 0.f));
    m_sidebarTopBar.setPosition(sf::Vector2f(SIDEBAR_X - SIDEBAR_WIDTH + m_sidebarOffset, SIDEBAR_Y));

    float titleCenterX = SIDEBAR_X - SIDEBAR_WIDTH + SIDEBAR_WIDTH / 2.f + m_sidebarOffset;
    float titleY = 45.f;
    m_sidebarTitle->setPosition(sf::Vector2f(titleCenterX, titleY));

    float bx = SIDEBAR_BTN_X + (m_sidebarOffset - SIDEBAR_WIDTH);
    m_btnHistory->setPosition(bx, SIDEBAR_BTN_Y_START);
    m_btnWrong->setPosition(bx, SIDEBAR_BTN_Y_START + SIDEBAR_BTN_SPACING);

    // ========== 更新侧边栏装饰图案 ==========
    // 图案的基准位置 = 侧边栏背景的当前位置
    sf::Vector2f sidebarBase(SIDEBAR_X - SIDEBAR_WIDTH + m_sidebarOffset, SIDEBAR_Y);
    for (auto& p : m_sidebarPatterns) {
        sf::Vector2f localPos = p.shape.getPosition(); // 相对于侧边栏的坐标
        localPos += p.velocity * dt * 60.f;
        float r = p.shape.getRadius();
        // 边界反弹（限制在侧边栏内）
        if (localPos.x - r < 0.f) { localPos.x = r; p.velocity.x = -p.velocity.x; }
        if (localPos.x + r > SIDEBAR_WIDTH) { localPos.x = SIDEBAR_WIDTH - r; p.velocity.x = -p.velocity.x; }
        if (localPos.y - r < 0.f) { localPos.y = r; p.velocity.y = -p.velocity.y; }
        if (localPos.y + r > SIDEBAR_HEIGHT) { localPos.y = SIDEBAR_HEIGHT - r; p.velocity.y = -p.velocity.y; }
        p.shape.setPosition(localPos);
    }

    // ========== 电脑抖动 ==========
    if (!m_animFinished) {
        float shakeAngle = 12.f * std::sin(elapsed * 18.f) * (1.f - progress);
        m_computer->setRotation(sf::degrees(shakeAngle));
    }
    else {
        m_computer->setRotation(sf::Angle::Zero);
    }

    // 背景图案动画
    for (auto& p : m_patterns) {
        sf::Vector2f pos = p.shape.getPosition();
        pos.x += p.velocity.x * dt * 60.f; pos.y += p.velocity.y * dt * 60.f;
        float r = p.shape.getRadius();
        if (pos.x < r) { pos.x = r; p.velocity.x = -p.velocity.x; }
        if (pos.x > WINDOW_WIDTH - r) { pos.x = WINDOW_WIDTH - r; p.velocity.x = -p.velocity.x; }
        if (pos.y < r) { pos.y = r; p.velocity.y = -p.velocity.y; }
        if (pos.y > WINDOW_HEIGHT - r) { pos.y = WINDOW_HEIGHT - r; p.velocity.y = -p.velocity.y; }
        p.shape.setPosition(pos);
        p.phase += dt * p.radiusSpeed;
    }

    // ========== 悬停效果 ==========
    if (m_animFinished) {
        bool sh = m_btnStartPractice->isHovered(m_mousePos);
        m_currentStartScale = lerp(m_currentStartScale, sh ? BUTTON_HOVER_SCALE : BUTTON_SCALE, dt * 8.f);
        m_btnStartPractice->setScale(m_currentStartScale, m_currentStartScale);
        bool ph = m_btnTakePhoto->isHovered(m_mousePos);
        m_currentPhotoScale = lerp(m_currentPhotoScale, ph ? BUTTON_HOVER_SCALE : BUTTON_SCALE, dt * 8.f);
        m_btnTakePhoto->setScale(m_currentPhotoScale, m_currentPhotoScale);
        bool mh = m_btnMenu->isHovered(m_mousePos);
        m_currentMenuScale = lerp(m_currentMenuScale, mh ? MENU_BTN_HOVER_SCALE : MENU_BTN_SCALE, dt * 8.f);
        m_btnMenu->setScale(m_currentMenuScale, m_currentMenuScale);
        m_btnMenu->setColor(mh ? sf::Color(0, 0, 0, 255) : sf::Color(150, 150, 150, 255));

        if (m_sidebarOpen && !m_sidebarAnimating) {
            bool hh = m_btnHistory->isHovered(m_mousePos);
            m_currentHistScale = lerp(m_currentHistScale, hh ? SIDEBAR_BTN_HOVER_SCALE : SIDEBAR_BTN_SCALE, dt * 8.f);
            m_btnHistory->setScale(m_currentHistScale, m_currentHistScale);
            m_btnHistory->setColor(hh ? sf::Color(180, 180, 180, 255) : sf::Color::White);
            bool wh = m_btnWrong->isHovered(m_mousePos);
            m_currentWrongScale = lerp(m_currentWrongScale, wh ? SIDEBAR_BTN_HOVER_SCALE : SIDEBAR_BTN_SCALE, dt * 8.f);
            m_btnWrong->setScale(m_currentWrongScale, m_currentWrongScale);
            m_btnWrong->setColor(wh ? sf::Color(180, 180, 180, 255) : sf::Color::White);
        }
        else {
            m_currentHistScale = lerp(m_currentHistScale, SIDEBAR_BTN_SCALE, dt * 8.f);
            m_currentWrongScale = lerp(m_currentWrongScale, SIDEBAR_BTN_SCALE, dt * 8.f);
            m_btnHistory->setScale(m_currentHistScale, m_currentHistScale);
            m_btnWrong->setScale(m_currentWrongScale, m_currentWrongScale);
            m_btnHistory->setColor(sf::Color::White);
            m_btnWrong->setColor(sf::Color::White);
        }
    }
    else {
        m_btnMenu->setScale(MENU_BTN_SCALE, MENU_BTN_SCALE);
        m_btnHistory->setScale(SIDEBAR_BTN_SCALE, SIDEBAR_BTN_SCALE);
        m_btnWrong->setScale(SIDEBAR_BTN_SCALE, SIDEBAR_BTN_SCALE);
        m_btnMenu->setColor(sf::Color(150, 150, 150, 255));
    }

    // 标题颜色呼吸
    float t = m_titleClock.getElapsedTime().asSeconds();
    uint8_t r = static_cast<uint8_t>(60 + 40 * std::sin(t * 0.8f));
    uint8_t g = static_cast<uint8_t>(80 + 40 * std::sin(t * 0.8f + 2.0f));
    uint8_t b = static_cast<uint8_t>(180 + 40 * std::sin(t * 0.8f + 4.0f));
    m_titleText->setFillColor(sf::Color(r, g, b, 200));
}

void MainScreen::draw(sf::RenderWindow& window) {
    window.clear(sf::Color(250, 252, 255));
    for (const auto& p : m_patterns) window.draw(p.shape);
    window.draw(*m_titleText);
    window.draw(*m_computer);
    window.draw(*m_word);
    window.draw(*m_word2);

    // 主按钮（阴影在动画结束后才显示）
    if (m_btnStartPractice) {
        const sf::Sprite& orig = m_btnStartPractice->getSprite();
        if (m_animFinished) {
            sf::Sprite shadow = orig;
            sf::Vector2f shadowOffset(4.f, 4.f);
            shadow.setPosition(orig.getPosition() + shadowOffset);
            shadow.setColor(sf::Color(0, 0, 0, 40));
            window.draw(shadow);
        }
        float floatOffset = 0.f;
        if (m_animFinished) {
            float time = m_floatClock.getElapsedTime().asSeconds();
            floatOffset = std::sin(time * 2.5f) * 2.0f;
        }
        sf::Sprite mainSprite = orig;
        mainSprite.setPosition(orig.getPosition() + sf::Vector2f(0.f, floatOffset));
        window.draw(mainSprite);
    }
    if (m_btnTakePhoto) {
        const sf::Sprite& orig = m_btnTakePhoto->getSprite();
        if (m_animFinished) {
            sf::Sprite shadow = orig;
            sf::Vector2f shadowOffset(4.f, 4.f);
            shadow.setPosition(orig.getPosition() + shadowOffset);
            shadow.setColor(sf::Color(0, 0, 0, 40));
            window.draw(shadow);
        }
        float floatOffset = 0.f;
        if (m_animFinished) {
            float time = m_floatClock.getElapsedTime().asSeconds();
            floatOffset = std::sin(time * 2.5f + 1.5f) * 2.0f;
        }
        sf::Sprite mainSprite = orig;
        mainSprite.setPosition(orig.getPosition() + sf::Vector2f(0.f, floatOffset));
        window.draw(mainSprite);
    }

    // 绘制侧边栏（仅在可见时）
    if (m_sidebarOffset > 0.f) {
        // 分隔线
        window.draw(m_separatorLine);
        // 阴影
        window.draw(m_sidebarShadow);
        // 背景
        window.draw(m_sidebarBg);

        // 侧边栏内部装饰图案（相对于侧边栏位置绘制）
        sf::Vector2f sidebarBase(SIDEBAR_X - SIDEBAR_WIDTH + m_sidebarOffset, SIDEBAR_Y);
        for (const auto& p : m_sidebarPatterns) {
            sf::CircleShape drawCircle = p.shape;
            drawCircle.setPosition(p.shape.getPosition() + sidebarBase);
            window.draw(drawCircle);
        }

        // 顶部标题栏
        window.draw(m_sidebarTopBar);
        // 标题文字
        window.draw(*m_sidebarTitle);
        // 内部按钮
        if (m_btnHistory) m_btnHistory->draw(window);
        if (m_btnWrong)   m_btnWrong->draw(window);
    }
    if (m_btnMenu) m_btnMenu->draw(window);
}
