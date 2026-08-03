#pragma once
#include "Screen.h"
#include "Button.h"
#include <memory>
#include <vector>

class MainScreen : public Screen {
public:
    MainScreen();

    void init() override;
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window) override;
    void update(float deltaTime) override;
    void draw(sf::RenderWindow& window) override;

private:
    sf::View m_mainView;

    // ========== 尺寸参数 ==========
    static constexpr float WINDOW_WIDTH = 2048.f;
    static constexpr float WINDOW_HEIGHT = 1536.f;

    static constexpr float COMPUTER_SCALE = 0.6f;
    static constexpr float WORD_SCALE = 0.35f;
    static constexpr float WORD2_SCALE = 0.35f;
    static constexpr float BUTTON_SCALE = 0.3f;
    static constexpr float BUTTON_HOVER_SCALE = 0.33f;

    // ---------- 侧边栏常量 ----------
    static constexpr float SIDEBAR_WIDTH = 500.f;
    static constexpr float SIDEBAR_ANIM_DURATION = 0.4f;
    static constexpr float SIDEBAR_BTN_SPACING = 150.f;
    static constexpr float MENU_BTN_SCALE = 0.04f;
    static constexpr float MENU_BTN_HOVER_SCALE = 0.044f;
    static constexpr float MENU_BTN_X_OFFSET = 60.f;

    static constexpr float SIDEBAR_X = 0.f;
    static constexpr float SIDEBAR_Y = 10.f;
    static constexpr float SIDEBAR_HEIGHT = WINDOW_HEIGHT;

    static constexpr float MENU_BTN_X = 10.f;
    static constexpr float MENU_BTN_Y = 10.f;

    static constexpr float SIDEBAR_BTN_X = 40.f;
    static constexpr float SIDEBAR_BTN_Y_START = 180.f;

    static constexpr float SIDEBAR_BTN_SCALE = 0.1f;
    static constexpr float SIDEBAR_BTN_HOVER_SCALE = 0.11f;

    // ===== 原有坐标 =====
    static constexpr float COMPUTER_X = 900.f;
    static constexpr float COMPUTER_Y = 300.f;
    static constexpr float WORD_X = 150.f;
    static constexpr float WORD_Y = 200.f;
    static constexpr float WORD2_X = 150.f;
    static constexpr float WORD2_Y = 400.f;
    static constexpr float BTN_START_X = 130.f;
    static constexpr float BTN_START_Y = 900.f;
    static constexpr float BTN_PHOTO_X = 135.f;
    static constexpr float BTN_PHOTO_Y = 590.f;

    // ========== 动画相关 ==========
    sf::Clock  m_animClock;
    float      m_animDuration = 2.0f;
    bool       m_animFinished = false;

    float easeOut(float t) const;
    float lerp(float a, float b, float t) const;
    float easeOutBack(float t) const;

    // ========== 背景花纹 ==========
    struct Pattern {
        sf::CircleShape shape;
        sf::Vector2f    velocity;
        float           phase;
        float           radiusSpeed;
    };
    std::vector<Pattern> m_patterns;
    void initPatterns();

    // ---------- 侧边栏专属装饰图案 ----------
    struct SidebarPattern {
        sf::CircleShape shape;
        sf::Vector2f    velocity;   // 在侧边栏内的局部速度
    };
    std::vector<SidebarPattern> m_sidebarPatterns;
    void initSidebarPatterns();

    // ========== 鼠标状态 ==========
    sf::Vector2f m_mousePos;
    bool         m_mousePressed = false;

    // ========== UI 元素 ==========
    std::unique_ptr<sf::Sprite> m_computer;
    std::unique_ptr<sf::Sprite> m_word;
    std::unique_ptr<sf::Sprite> m_word2;
    std::unique_ptr<Button> m_btnStartPractice;
    std::unique_ptr<Button> m_btnTakePhoto;
    sf::Vector2f m_computerCenter;

    // ---------- 侧边栏相关 ----------
    sf::RectangleShape m_sidebarBg;
    sf::RectangleShape m_sidebarTopBar;
    std::unique_ptr<sf::Text> m_sidebarTitle;
    sf::RectangleShape m_sidebarShadow;
    sf::RectangleShape m_separatorLine;
    std::unique_ptr<Button> m_btnMenu;
    std::unique_ptr<Button> m_btnHistory;
    std::unique_ptr<Button> m_btnWrong;

    // 标题
    sf::Font m_titleFont;
    std::unique_ptr<sf::Text> m_titleText;
    sf::Clock m_titleClock;

    // 侧边栏动画状态
    bool   m_sidebarOpen = false;
    bool   m_sidebarAnimating = false;
    float  m_sidebarOffset = 0.f;
    float  m_sidebarTarget = 0.f;
    float  m_sidebarAnimStart = 0.f;
    sf::Clock m_sidebarAnimClock;

    // 浮动动画
    sf::Clock m_floatClock;

    // 辅助函数
    void toggleSidebar();
    void closeSidebar();

    // 平滑缩放插值变量
    float m_currentMenuScale = MENU_BTN_SCALE;
    float m_currentStartScale = BUTTON_SCALE;
    float m_currentPhotoScale = BUTTON_SCALE;
    float m_currentHistScale = SIDEBAR_BTN_SCALE;
    float m_currentWrongScale = SIDEBAR_BTN_SCALE;
};
