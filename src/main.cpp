#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/SceneManager.hpp>
#include <chrono>
#include <map>
#include <vector>

using namespace geode::prelude;

struct KeyDef {
    const char* label;
    enumKeyCodes code;
    float width; // в «единицах» клавиши
};

// Раскладка. Максимальная ширина ряда — 12.5 единиц.
static const std::vector<std::vector<KeyDef>> LAYOUT = {
    {
        {"Esc", KEY_Escape, 1.2f},
        {"1", KEY_One, 1.f}, {"2", KEY_Two, 1.f}, {"3", KEY_Three, 1.f},
        {"4", KEY_Four, 1.f}, {"5", KEY_Five, 1.f}, {"6", KEY_Six, 1.f},
        {"7", KEY_Seven, 1.f}, {"8", KEY_Eight, 1.f}, {"9", KEY_Nine, 1.f},
        {"0", KEY_Zero, 1.f},
        {"<-", KEY_Backspace, 1.3f},
    },
    {
        {"Tab", KEY_Tab, 1.5f},
        {"Q", KEY_Q, 1.f}, {"W", KEY_W, 1.f}, {"E", KEY_E, 1.f},
        {"R", KEY_R, 1.f}, {"T", KEY_T, 1.f}, {"Y", KEY_Y, 1.f},
        {"U", KEY_U, 1.f}, {"I", KEY_I, 1.f}, {"O", KEY_O, 1.f},
        {"P", KEY_P, 1.f},
    },
    {
        {"A", KEY_A, 1.f}, {"S", KEY_S, 1.f}, {"D", KEY_D, 1.f},
        {"F", KEY_F, 1.f}, {"G", KEY_G, 1.f}, {"H", KEY_H, 1.f},
        {"J", KEY_J, 1.f}, {"K", KEY_K, 1.f}, {"L", KEY_L, 1.f},
        {"Enter", KEY_Enter, 2.f},
    },
    {
        {"Shift", KEY_Shift, 2.f},
        {"Z", KEY_Z, 1.f}, {"X", KEY_X, 1.f}, {"C", KEY_C, 1.f},
        {"V", KEY_V, 1.f}, {"B", KEY_B, 1.f}, {"N", KEY_N, 1.f},
        {"M", KEY_M, 1.f},
    },
    {
        {"Ctrl", KEY_Control, 1.5f},
        {"Space", KEY_Space, 6.f},
        {"<", KEY_Left, 1.f}, {"^", KEY_Up, 1.f},
        {"v", KEY_Down, 1.f}, {">", KEY_Right, 1.f},
    },
};

static double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

class KeyboardOverlay : public CCLayer {
protected:
    struct KeyView {
        CCRect rect;
        CCLayerColor* bg = nullptr;
        enumKeyCodes code = KEY_None;
        int holds = 0;
    };

    CCNode* m_panel = nullptr;
    CCRect m_panelRect = CCRectZero;
    CCNode* m_button = nullptr;

    std::vector<KeyView> m_keys;
    std::map<CCTouch*, size_t> m_active;

    CCTouch* m_btnTouch = nullptr;
    CCPoint m_dragStart = ccp(0, 0);
    CCPoint m_btnStart = ccp(0, 0);
    bool m_dragged = false;
    double m_lastTap = -100.0;

    bool init() override {
        if (!CCLayer::init()) return false;

        auto win = CCDirector::get()->getWinSize();

        m_panel = CCNode::create();
        m_panel->setVisible(false);
        this->addChild(m_panel);

        // Перетаскиваемая кнопка
        m_button = CCNode::create();
        auto spr = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
        spr->setScale(0.7f);
        spr->setOpacity(190);
        m_button->addChild(spr);
        auto lbl = CCLabelBMFont::create("KB", "bigFont.fnt");
        lbl->setScale(0.45f);
        m_button->addChild(lbl);
        this->addChild(m_button);

        float x = Mod::get()->getSavedValue<float>("btn-x", 32.f);
        float y = Mod::get()->getSavedValue<float>("btn-y", win.height * 0.5f);
        m_button->setPosition(clampToScreen(ccp(x, y)));

        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-600); // выше игры и меню
        this->setTouchEnabled(true);
        return true;
    }

    CCPoint clampToScreen(CCPoint p) {
        auto win = CCDirector::get()->getWinSize();
        const float r = 26.f;
        p.x = std::max(r, std::min(win.width - r, p.x));
        p.y = std::max(r, std::min(win.height - r, p.y));
        return p;
    }

    void sendKey(enumKeyCodes code, bool down) {
        CCDirector::get()->getKeyboardDispatcher()->dispatchKeyboardMSG(code, down, false, nowSeconds());
    }

    void pressKey(size_t idx) {
        auto& k = m_keys[idx];
        if (k.holds++ == 0) {
            sendKey(k.code, true);
            k.bg->setColor(ccc3(90, 170, 255));
        }
    }

    void releaseKey(size_t idx) {
        auto& k = m_keys[idx];
        if (k.holds > 0 && --k.holds == 0) {
            sendKey(k.code, false);
            k.bg->setColor(ccc3(255, 255, 255));
        }
    }

    void releaseAll() {
        for (auto& [touch, idx] : m_active) {
            if (idx < m_keys.size()) releaseKey(idx);
        }
        m_active.clear();
    }

    void buildPanel() {
        m_panel->removeAllChildren();
        m_keys.clear();

        auto win = CCDirector::get()->getWinSize();
        float scale = static_cast<float>(Mod::get()->getSettingValue<double>("scale"));
        float opacity = static_cast<float>(Mod::get()->getSettingValue<double>("opacity"));

        float u = win.width * 0.85f * scale / 12.5f; // размер одной клавиши
        float gap = u * 0.08f;
        float pad = u * 0.15f;
        float rows = static_cast<float>(LAYOUT.size());

        float W = 12.5f * u + pad * 2;
        float H = rows * u + pad * 2;
        float x0 = (win.width - W) / 2;
        float y0 = 4.f;

        m_panelRect = CCRect(x0, y0, W, H);

        auto back = CCLayerColor::create(ccc4(0, 0, 0, static_cast<GLubyte>(200 * opacity)), W, H);
        back->setPosition(ccp(x0, y0));
        m_panel->addChild(back);

        for (size_t r = 0; r < LAYOUT.size(); r++) {
            float rowUnits = 0;
            for (auto& k : LAYOUT[r]) rowUnits += k.width;

            float x = x0 + (W - rowUnits * u) / 2;
            float y = y0 + pad + (rows - 1 - r) * u;

            for (auto& k : LAYOUT[r]) {
                float kw = k.width * u - gap;
                float kh = u - gap;

                KeyView kv;
                kv.rect = CCRect(x, y, kw, kh);
                kv.code = k.code;
                kv.bg = CCLayerColor::create(ccc4(60, 60, 75, static_cast<GLubyte>(255 * opacity)), kw, kh);
                kv.bg->setPosition(ccp(x, y));
                m_panel->addChild(kv.bg);

                auto label = CCLabelBMFont::create(k.label, "bigFont.fnt");
                label->limitLabelWidth(kw * 0.8f, 0.6f, 0.1f);
                label->setPosition(ccp(x + kw / 2, y + kh / 2));
                m_panel->addChild(label);

                m_keys.push_back(kv);
                x += k.width * u;
            }
        }
    }

    void showKeyboard() {
        if (m_panel->isVisible()) return;
        buildPanel();
        m_panel->setVisible(true);
    }

    void hideKeyboard() {
        releaseAll();
        m_panel->setVisible(false);
    }

    void handleTap() {
        double now = nowSeconds();
        if (now - m_lastTap < 0.35) {
            // двойное нажатие — прячем
            hideKeyboard();
            m_lastTap = -100.0;
        } else {
            // одно нажатие — показываем
            showKeyboard();
            m_lastTap = now;
        }
    }

    // ---- касания ----

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto local = this->convertToNodeSpace(touch->getLocation());

        if (!m_btnTouch && ccpDistance(local, m_button->getPosition()) <= 28.f) {
            m_btnTouch = touch;
            m_dragStart = local;
            m_btnStart = m_button->getPosition();
            m_dragged = false;
            return true;
        }

        if (m_panel->isVisible() && m_panelRect.containsPoint(local)) {
            for (size_t i = 0; i < m_keys.size(); i++) {
                if (m_keys[i].rect.containsPoint(local)) {
                    m_active[touch] = i;
                    pressKey(i);
                    break;
                }
            }
            return true; // поглощаем касание, чтобы игра не прыгала
        }

        return false;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (touch != m_btnTouch) return;

        auto local = this->convertToNodeSpace(touch->getLocation());
        auto delta = local - m_dragStart;

        if (!m_dragged && ccpLength(delta) > 8.f) m_dragged = true;
        if (m_dragged) m_button->setPosition(clampToScreen(m_btnStart + delta));
    }

    void finishTouch(CCTouch* touch, bool cancelled) {
        if (touch == m_btnTouch) {
            m_btnTouch = nullptr;
            if (m_dragged) {
                auto p = m_button->getPosition();
                Mod::get()->setSavedValue<float>("btn-x", p.x);
                Mod::get()->setSavedValue<float>("btn-y", p.y);
            } else if (!cancelled) {
                handleTap();
            }
            return;
        }

        auto it = m_active.find(touch);
        if (it != m_active.end()) {
            if (it->second < m_keys.size()) releaseKey(it->second);
            m_active.erase(it);
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent*) override { finishTouch(touch, false); }
    void ccTouchCancelled(CCTouch* touch, CCEvent*) override { finishTouch(touch, true); }

public:
    static KeyboardOverlay* create() {
        auto ret = new KeyboardOverlay();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// Создаём оверлей один раз и оставляем его поверх всех сцен.
class $modify(MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        static bool created = false;
        if (!created) {
            created = true;
            auto overlay = KeyboardOverlay::create();
            if (overlay) geode::SceneManager::get()->keepAcrossScenes(overlay);
        }
        return true;
    }
};
