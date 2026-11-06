#include "gamepad.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <SDL3/SDL.h>
#include <limits>

namespace {
constexpr qint64 RepeatDelayMs = 380;
constexpr qint64 RepeatRateMs = 140;
constexpr qint64 ExitHoldMs = 1000;
constexpr Sint16 StickThreshold = 18000;
}

void InputInterpreter::edge(bool now, bool &was, const char *action, QStringList &out) {
    if (now && !was)
        out << QLatin1String(action);
    was = now;
}

QStringList InputInterpreter::update(const InputFrame &frame, qint64 nowMs, bool gameRunning) {
    QStringList actions;

    // Buttons still held from the moment a game ended mustn't act on the
    // drawer, so the first frame after a game only records what's down.
    if (m_wasRunning && !gameRunning) {
        m_was = frame;
        // A direction still held waits to be let go before it counts again.
        m_heldDirection = frame.left == frame.right ? 0 : frame.left ? -1 : 1;
        m_nextRepeat = std::numeric_limits<qint64>::max();
        m_comboStarted = -1;
        m_comboFired = false;
        m_wasRunning = false;
        return actions;
    }
    m_wasRunning = gameRunning;

    if (gameRunning) {
        // Held, not tapped: PS1 and PS2 games use Select and Start themselves.
        if (frame.select && frame.start) {
            if (m_comboStarted < 0)
                m_comboStarted = nowMs;
            if (!m_comboFired && nowMs - m_comboStarted >= ExitHoldMs) {
                actions << "exitGame";
                m_comboFired = true;
            }
        } else {
            m_comboStarted = -1;
            m_comboFired = false;
        }
        m_was = frame;
        return actions;
    }

    const int direction = frame.left == frame.right ? 0 : frame.left ? -1 : 1;
    if (direction != m_heldDirection) {
        m_heldDirection = direction;
        if (direction != 0) {
            actions << (direction < 0 ? "left" : "right");
            m_nextRepeat = nowMs + RepeatDelayMs;
        }
    } else if (direction != 0 && nowMs >= m_nextRepeat) {
        actions << (direction < 0 ? "left" : "right");
        m_nextRepeat = nowMs + RepeatRateMs;
    }

    edge(frame.up, m_was.up, "shelfPrevious", actions);
    edge(frame.down, m_was.down, "shelfNext", actions);
    edge(frame.shoulderLeft, m_was.shoulderLeft, "shelfPrevious", actions);
    edge(frame.shoulderRight, m_was.shoulderRight, "shelfNext", actions);
    edge(frame.south, m_was.south, "play", actions);
    edge(frame.east, m_was.east, "back", actions);
    edge(frame.north, m_was.north, "music", actions);
    edge(frame.start, m_was.start, "help", actions);
    m_was.left = frame.left;
    m_was.right = frame.right;
    m_was.west = frame.west;
    m_was.select = frame.select;
    return actions;
}

// SDL renames pads after whatever they imitate, so an 8BitDo in Xbox mode reaches
// us as an "Xbox One S Controller". The kernel still has the name it reported.
static QString deviceName(void *handle) {
    auto *pad = static_cast<SDL_Gamepad *>(handle);
    if (const char *path = SDL_GetGamepadPath(pad)) {
        QFile file(QStringLiteral("/sys/class/input/%1/device/name").arg(QFileInfo(QString::fromUtf8(path)).fileName()));
        if (file.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(file.readAll()).trimmed();
            if (!name.isEmpty())
                return name;
        }
    }
    return QString::fromUtf8(SDL_GetGamepadName(pad));
}

bool Gamepad::usesNintendoLayout(const QString &deviceName) {
    return deviceName.contains("8bitdo", Qt::CaseInsensitive);
}

Gamepad::Gamepad(QObject *parent) : QObject(parent) {
    // HIDAPI would open the controller directly and can fight the emulator for
    // it. The plain evdev route lets both read the same pad side by side.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // Qt owns the process's signals and windows; SDL is only here for input.
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    m_available = SDL_Init(SDL_INIT_GAMEPAD);
    if (!m_available) {
        qWarning("Controller support is off: %s", SDL_GetError());
        return;
    }
    refreshPads();
    connect(&m_timer, &QTimer::timeout, this, &Gamepad::poll);
    m_timer.start(16);
}

Gamepad::~Gamepad() {
    for (void *pad : std::as_const(m_pads))
        SDL_CloseGamepad(static_cast<SDL_Gamepad *>(pad));
    if (m_available)
        SDL_Quit();
}

void Gamepad::setGameRunning(bool running) {
    if (running == m_gameRunning)
        return;
    m_gameRunning = running;
    emit gameRunningChanged();
}

void Gamepad::refreshPads() {
    for (void *pad : std::as_const(m_pads))
        SDL_CloseGamepad(static_cast<SDL_Gamepad *>(pad));
    m_pads.clear();

    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; ids && i < count; ++i)
        if (SDL_Gamepad *pad = SDL_OpenGamepad(ids[i]))
            m_pads << pad;
    SDL_free(ids);

    m_name = m_pads.isEmpty() ? QString() : deviceName(m_pads.first());
    m_nintendoLayout = usesNintendoLayout(m_name);
    emit connectedChanged();
}

void Gamepad::poll() {
    SDL_Event event;
    bool changed = false;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED)
            changed = true;
    if (changed)
        refreshPads();

    // Asleep behind another window, the drawer shouldn't respond at all, and
    // it can check far less often to spare the battery.
    const bool active = qGuiApp->applicationState() == Qt::ApplicationActive;
    const int interval = m_gameRunning ? 33 : active ? 16 : 250;
    if (m_timer.interval() != interval)
        m_timer.setInterval(interval);

    InputFrame frame;
    for (void *handle : std::as_const(m_pads)) {
        auto *pad = static_cast<SDL_Gamepad *>(handle);
        const auto button = [pad](SDL_GamepadButton which) { return SDL_GetGamepadButton(pad, which); };
        const Sint16 stickX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 stickY = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        frame.left |= button(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || stickX < -StickThreshold;
        frame.right |= button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || stickX > StickThreshold;
        frame.up |= button(SDL_GAMEPAD_BUTTON_DPAD_UP) || stickY < -StickThreshold;
        frame.down |= button(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || stickY > StickThreshold;
        frame.south |= button(m_nintendoLayout ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH);
        frame.east |= button(m_nintendoLayout ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST);
        frame.north |= button(SDL_GAMEPAD_BUTTON_NORTH);
        frame.west |= button(SDL_GAMEPAD_BUTTON_WEST);
        frame.shoulderLeft |= button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        frame.shoulderRight |= button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        frame.select |= button(SDL_GAMEPAD_BUTTON_BACK);
        frame.start |= button(SDL_GAMEPAD_BUTTON_START);
    }

    const auto actions = m_interpreter.update(frame, QDateTime::currentMSecsSinceEpoch(), m_gameRunning);
    if (!m_gameRunning && !active)
        return;
    for (const auto &name : actions)
        emit action(name);
}
