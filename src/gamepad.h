#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>

struct InputFrame {
    bool left = false, right = false, up = false, down = false;
    bool south = false, east = false, north = false, west = false;
    bool shoulderLeft = false, shoulderRight = false;
    bool select = false, start = false;
};

// Turns raw button states into the drawer's actions. Kept apart from SDL so
// repeat timing and the exit combo can be tested without a controller.
class InputInterpreter {
public:
    QStringList update(const InputFrame &frame, qint64 nowMs, bool gameRunning);

private:
    void edge(bool now, bool &was, const char *action, QStringList &out);

    InputFrame m_was;
    int m_heldDirection = 0;
    qint64 m_nextRepeat = 0;
    qint64 m_comboStarted = -1;
    bool m_comboFired = false;
    bool m_wasRunning = false;
};

// The controller, through SDL. While a game runs it watches for nothing but
// the exit combo, which works the same in every emulator.
class Gamepad : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString name READ name NOTIFY connectedChanged)
    Q_PROPERTY(bool gameRunning READ gameRunning WRITE setGameRunning NOTIFY gameRunningChanged)
    Q_PROPERTY(bool nintendoLayout READ nintendoLayout NOTIFY connectedChanged)

public:
    explicit Gamepad(QObject *parent = nullptr);
    ~Gamepad() override;

    bool connected() const { return !m_pads.isEmpty(); }
    QString name() const { return m_name; }
    bool gameRunning() const { return m_gameRunning; }
    void setGameRunning(bool running);
    bool nintendoLayout() const { return m_nintendoLayout; }

    // 8BitDo pads are labelled like a Super Nintendo's: A on the right, B at the
    // bottom. Even in Xbox mode that holds, so "confirm" belongs on the right.
    static bool usesNintendoLayout(const QString &deviceName);

signals:
    void action(const QString &name);
    void connectedChanged();
    void gameRunningChanged();

private:
    void poll();
    void refreshPads();

    QList<void *> m_pads;
    QString m_name;
    bool m_available = false;
    bool m_nintendoLayout = false;
    bool m_gameRunning = false;
    QTimer m_timer;
    InputInterpreter m_interpreter;
};
