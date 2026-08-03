#include "PracticeScreen.h"
#include "App.h"
#include "MainScreen.h"
#include "ResourceManager.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <ctime>

PracticeScreen::PracticeScreen() {}
PracticeScreen::~PracticeScreen() {}

void PracticeScreen::initPatterns() {
    m_patterns.clear();
    struct ColorSpec { std::uint8_t r, g, b, a; float radius; };
    std::vector<ColorSpec> specs = {
        {180,210,255,30,500.f}, {220,180,255,25,400.f},
        {255,210,220,28,600.f}, {180,255,220,22,350.f},
        {255,230,180,25,450.f}, {200,200,255,20,550.f}
    };
    for (const auto& spec : specs) {
        Pattern p;
        float x = 100.f + rand() % 1800;
        float y = 100.f + rand() % 1300;
        p.shape.setPosition({ x,y });
        p.shape.setRadius(spec.radius);
        p.shape.setFillColor(sf::Color(spec.r, spec.g, spec.b, spec.a));
        p.shape.setOrigin({ spec.radius,spec.radius });
        float angle = static_cast<float>(rand()) / RAND_MAX * 6.2832f;
        float speed = 0.2f + static_cast<float>(rand()) / RAND_MAX * 0.6f;
        p.velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
        p.phase = static_cast<float>(rand()) / RAND_MAX * 6.2832f;
        p.radiusSpeed = 0.2f + static_cast<float>(rand()) / RAND_MAX * 0.3f;
        m_patterns.push_back(std::move(p));
    }
}

void PracticeScreen::init() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    // ---- 加载字体（优先支持 ×÷ 的字体） ----
    bool loaded = false;
    if (m_font.openFromFile("C:/Windows/Fonts/msyh.ttf")) loaded = true;      // 微软雅黑
    if (!loaded && m_font.openFromFile("C:/Windows/Fonts/simsun.ttc")) loaded = true; // 宋体
    if (!loaded && m_font.openFromFile("C:/Windows/Fonts/arial.ttf")) loaded = true;   // 备用
    if (!loaded) std::cerr << "Warning: No font loaded, symbols may not render." << std::endl;

    // ---- 返回按钮 ----
    ResourceManager::loadTextureWithFallback(m_backTexture, "return.png");
    m_backSprite = std::make_unique<sf::Sprite>(m_backTexture);
    float backScale = 80.f / m_backTexture.getSize().x;
    m_backSprite->setScale(sf::Vector2f(backScale, backScale));
    m_backSprite->setPosition(sf::Vector2f(20.f, 20.f));

    // ---- 模式选择按钮（四个运算符） ----
    const float btnWidth = 180.f, btnHeight = 100.f;
    const float gap = 40.f;
    const float totalWidth = btnWidth * 4 + gap * 3;
    const float startX = (WINDOW_WIDTH - totalWidth) / 2.f;
    const float startY = (WINDOW_HEIGHT - btnHeight) / 2.f - 100.f;

    std::vector<std::string> ops = { "+", "-", "*", "/" };  // UTF-8 编码，需 /utf-8 编译
    for (int i = 0; i < 4; ++i) {
        ModeButton btn;
        btn.rect.setSize({ btnWidth, btnHeight });
        btn.rect.setFillColor(sf::Color(200, 220, 240, 200));
        btn.rect.setOutlineColor(sf::Color::White);
        btn.rect.setOutlineThickness(2.f);
        btn.rect.setPosition({ startX + i * (btnWidth + gap), startY });
        btn.op = ops[i];
        m_modeButtons.push_back(std::move(btn));
    }

    // ---- 标题 ----
    m_titleText = std::make_unique<sf::Text>(m_font);
    m_titleText->setString("Choose an operator");
    m_titleText->setCharacterSize(60);
    m_titleText->setFillColor(sf::Color(40, 40, 60));
    sf::FloatRect titleBounds = m_titleText->getLocalBounds();
    m_titleText->setPosition({ (WINDOW_WIDTH - titleBounds.size.x) / 2.f, 200.f });

    // ---- 计分 ----
    m_scoreText = std::make_unique<sf::Text>(m_font);
    m_scoreText->setCharacterSize(40);
    m_scoreText->setFillColor(sf::Color(40, 40, 60));
    m_scoreText->setPosition({ 100.f,100.f });
    m_scoreText->setString("Score: 0");

    // ---- 计时 ----
    m_timerText = std::make_unique<sf::Text>(m_font);
    m_timerText->setCharacterSize(40);
    m_timerText->setFillColor(sf::Color(200, 50, 50));
    m_timerText->setPosition({ WINDOW_WIDTH - 200.f, 100.f });

    // ---- 题目 ----
    m_questionText = std::make_unique<sf::Text>(m_font);
    m_questionText->setCharacterSize(70);
    m_questionText->setFillColor(sf::Color(40, 40, 60));
    m_questionText->setPosition({ (WINDOW_WIDTH - 400.f) / 2.f, 400.f });

    // ---- 选项按钮（4个） ----
    const float optWidth = 300.f, optHeight = 80.f;
    const float optGap = 40.f;
    const float optTotal = optWidth * 4 + optGap * 3;
    const float optStartX = (WINDOW_WIDTH - optTotal) / 2.f;
    const float optStartY = 700.f;
    for (int i = 0; i < 4; ++i) {
        OptionButton btn;
        btn.rect.setSize({ optWidth, optHeight });
        btn.rect.setFillColor(sf::Color(230, 240, 250, 200));
        btn.rect.setOutlineColor(sf::Color::White);
        btn.rect.setOutlineThickness(2.f);
        btn.rect.setPosition({ optStartX + i * (optWidth + optGap), optStartY });
        m_optionButtons.push_back(std::move(btn));

        // 选项标签（用 unique_ptr）
        auto label = std::make_unique<sf::Text>(m_font);
        label->setCharacterSize(40);
        label->setFillColor(sf::Color(40, 40, 60));
        m_optionLabels.push_back(std::move(label));
    }

    // ---- "OK" 按钮 ----
    m_okButton.setSize({ 250.f,80.f });
    m_okButton.setFillColor(sf::Color(100, 200, 100));
    m_okButton.setOutlineColor(sf::Color::White);
    m_okButton.setOutlineThickness(2.f);
    m_okButton.setPosition({ (WINDOW_WIDTH - 250.f) / 2.f, WINDOW_HEIGHT / 2.f + 100.f });

    m_okText = std::make_unique<sf::Text>(m_font);
    m_okText->setString("Continue");
    m_okText->setCharacterSize(40);
    m_okText->setFillColor(sf::Color::White);
    sf::FloatRect okRect = m_okButton.getGlobalBounds();
    sf::FloatRect okTextRect = m_okText->getLocalBounds();
    m_okText->setPosition({
        okRect.position.x + (okRect.size.x - okTextRect.size.x) / 2.f,
        okRect.position.y + (okRect.size.y - okTextRect.size.y) / 2.f - okTextRect.position.y
        });

    // ---- 错误文字 ----
    m_wrongText = std::make_unique<sf::Text>(m_font);
    m_wrongText->setCharacterSize(60);
    m_wrongText->setFillColor(sf::Color::Red);
    m_wrongText->setPosition({ (WINDOW_WIDTH - 300.f) / 2.f, WINDOW_HEIGHT / 2.f - 100.f });

    // ---- 花纹 ----
    initPatterns();

    // ---- 状态 ----
    m_state = State::ModeSelect;
    m_score = 0;
    m_remainingTime = m_timeLimit;
    m_timerClock.restart();

    std::cout << "PracticeScreen init done" << std::endl;
}

void PracticeScreen::generateQuestion() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> numDist(10, 99);

    m_left = numDist(gen);
    m_right = numDist(gen);

    if (m_operator == "+") m_answer = m_left + m_right;
    else if (m_operator == "-") m_answer = m_left - m_right;
    else if (m_operator == "*") m_answer = m_left * m_right;
    else if (m_operator == "/") {
        m_right = numDist(gen) % 20 + 1;
        m_left = m_right * (numDist(gen) % 20 + 1);
        m_answer = m_left / m_right;
    }

    std::string q = std::to_string(m_left) + " " + m_operator + " " + std::to_string(m_right) + " = ?";
    m_questionText->setString(q);
    sf::FloatRect qBounds = m_questionText->getLocalBounds();
    m_questionText->setPosition({ (WINDOW_WIDTH - qBounds.size.x) / 2.f, 400.f });

    generateOptions();
}

void PracticeScreen::generateOptions() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> offsetDist(-15, 15);

    m_options.clear();
    m_options.push_back(m_answer);
    int attempts = 0;
    while (m_options.size() < 4 && attempts < 100) {
        int offset = offsetDist(gen);
        if (offset == 0) continue;
        int candidate = m_answer + offset;
        if (candidate >= 0 && candidate <= 200 &&
            std::find(m_options.begin(), m_options.end(), candidate) == m_options.end()) {
            m_options.push_back(candidate);
        }
        attempts++;
    }
    while (m_options.size() < 4) {
        int candidate = rand() % 200;
        if (std::find(m_options.begin(), m_options.end(), candidate) == m_options.end()) {
            m_options.push_back(candidate);
        }
    }

    std::shuffle(m_options.begin(), m_options.end(), gen);

    for (int i = 0; i < 4; ++i) {
        if (m_options[i] == m_answer) {
            m_correctOptionIndex = i;
            break;
        }
    }

    // 更新选项文字
    for (int i = 0; i < 4; ++i) {
        std::string label(1, char('A' + i));
        label += ": " + std::to_string(m_options[i]);
        m_optionLabels[i]->setString(label);
        m_optionLabels[i]->setFont(m_font); // 确保字体绑定
        sf::FloatRect rect = m_optionButtons[i].rect.getGlobalBounds();
        sf::FloatRect textRect = m_optionLabels[i]->getLocalBounds();
        m_optionLabels[i]->setPosition({
            rect.position.x + (rect.size.x - textRect.size.x) / 2.f,
            rect.position.y + (rect.size.y - textRect.size.y) / 2.f - textRect.position.y
            });
        m_optionButtons[i].rect.setFillColor(sf::Color(230, 240, 250, 200));
    }

    m_remainingTime = m_timeLimit;
    m_timerClock.restart();
}

void PracticeScreen::checkAnswer(int selectedIndex) {
    if (selectedIndex == m_correctOptionIndex) {
        m_score++;
        m_scoreText->setString("Score: " + std::to_string(m_score));
        generateQuestion();
    }
    else {
        m_state = State::WrongShow;
        m_wrongText->setString("Wrong! Score: " + std::to_string(m_score));
        sf::FloatRect wBounds = m_wrongText->getLocalBounds();
        m_wrongText->setPosition({
            (WINDOW_WIDTH - wBounds.size.x) / 2.f,
            WINDOW_HEIGHT / 2.f - 150.f
            });
    }
}

void PracticeScreen::returnToMainMenu() {
    if (m_app) {
        m_app->switchScreen(std::make_unique<MainScreen>());
    }
}

void PracticeScreen::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        m_mousePos = window.mapPixelToCoords(moved->position);
    }

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (pressed->button == sf::Mouse::Button::Left) {
            m_mousePressed = true;

            if (m_backSprite->getGlobalBounds().contains(m_mousePos)) {
                returnToMainMenu();
                return;
            }

            if (m_state == State::ModeSelect) {
                for (const auto& btn : m_modeButtons) {
                    if (btn.rect.getGlobalBounds().contains(m_mousePos)) {
                        m_operator = btn.op;
                        m_state = State::Playing;
                        m_score = 0;
                        m_scoreText->setString("Score: 0");
                        generateQuestion();
                        return;
                    }
                }
            }
            else if (m_state == State::Playing) {
                for (int i = 0; i < 4; ++i) {
                    if (m_optionButtons[i].rect.getGlobalBounds().contains(m_mousePos)) {
                        checkAnswer(i);
                        return;
                    }
                }
            }
            else if (m_state == State::WrongShow) {
                if (m_okButton.getGlobalBounds().contains(m_mousePos)) {
                    returnToMainMenu();
                    return;
                }
            }
        }
    }

    if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (released->button == sf::Mouse::Button::Left) {
            m_mousePressed = false;
        }
    }
}

void PracticeScreen::update(float deltaTime) {
    for (auto& p : m_patterns) {
        sf::Vector2f pos = p.shape.getPosition();
        pos.x += p.velocity.x * deltaTime * 60.f;
        pos.y += p.velocity.y * deltaTime * 60.f;
        float r = p.shape.getRadius();
        if (pos.x < r) { pos.x = r; p.velocity.x = -p.velocity.x; }
        if (pos.x > WINDOW_WIDTH - r) { pos.x = WINDOW_WIDTH - r; p.velocity.x = -p.velocity.x; }
        if (pos.y < r) { pos.y = r; p.velocity.y = -p.velocity.y; }
        if (pos.y > WINDOW_HEIGHT - r) { pos.y = WINDOW_HEIGHT - r; p.velocity.y = -p.velocity.y; }
        p.shape.setPosition(pos);

        p.phase += deltaTime * p.radiusSpeed;
        float radiusVar = std::sin(p.phase) * 20.f;
        float newRadius = std::max(30.f, p.shape.getRadius() + radiusVar);
        p.shape.setRadius(newRadius);
        p.shape.setOrigin({ newRadius, newRadius });
    }

    if (m_state == State::Playing) {
        m_remainingTime -= deltaTime;
        m_timerText->setString("Time: " + std::to_string(static_cast<int>(std::max(0.f, m_remainingTime))) + "s");
        if (m_remainingTime <= 0) {
            m_state = State::WrongShow;
            m_wrongText->setString("Time's up! Score: " + std::to_string(m_score));
            sf::FloatRect wBounds = m_wrongText->getLocalBounds();
            m_wrongText->setPosition({
                (WINDOW_WIDTH - wBounds.size.x) / 2.f,
                WINDOW_HEIGHT / 2.f - 150.f
                });
        }
    }

    // 悬停效果
    for (auto& btn : m_modeButtons) {
        btn.hovered = btn.rect.getGlobalBounds().contains(m_mousePos);
        btn.rect.setFillColor(btn.hovered ? sf::Color(160, 200, 240, 220) : sf::Color(200, 220, 240, 200));
    }
    for (size_t i = 0; i < m_optionButtons.size(); ++i) {
        m_optionButtons[i].hovered = m_optionButtons[i].rect.getGlobalBounds().contains(m_mousePos);
        m_optionButtons[i].rect.setFillColor(m_optionButtons[i].hovered ?
            sf::Color(200, 220, 250, 220) : sf::Color(230, 240, 250, 200));
    }
}

void PracticeScreen::draw(sf::RenderWindow& window) {
    window.clear(sf::Color::White);

    for (const auto& p : m_patterns) window.draw(p.shape);

    window.draw(*m_backSprite);

    window.draw(*m_scoreText);

    if (m_state == State::ModeSelect) {
        window.draw(*m_titleText);
        for (const auto& btn : m_modeButtons) {
            window.draw(btn.rect);
            // 绘制模式按钮上的文字（临时构造 sf::Text）
            sf::Text opText(m_font);
            opText.setString(btn.op);
            opText.setCharacterSize(60);
            opText.setFillColor(sf::Color(50, 50, 80));
            sf::FloatRect rect = btn.rect.getGlobalBounds();
            sf::FloatRect textRect = opText.getLocalBounds();
            opText.setPosition({
                rect.position.x + (rect.size.x - textRect.size.x) / 2.f,
                rect.position.y + (rect.size.y - textRect.size.y) / 2.f - textRect.position.y
                });
            window.draw(opText);
        }
    }
    else if (m_state == State::Playing) {
        window.draw(*m_timerText);
        window.draw(*m_questionText);
        for (size_t i = 0; i < m_optionButtons.size(); ++i) {
            window.draw(m_optionButtons[i].rect);
            window.draw(*m_optionLabels[i]);
        }
    }
    else if (m_state == State::WrongShow) {
        window.draw(*m_wrongText);
        window.draw(m_okButton);
        window.draw(*m_okText);
    }
}
