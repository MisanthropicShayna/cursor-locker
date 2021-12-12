#ifndef QDEBUGCONSOLE_HPP
#define QDEBUGCONSOLE_HPP

#include <QScrollBar>
#include <QTextEdit>
#include <QObject>
#include <QWidget>

// Context menu widgets
#include <QAction>
#include <QMenu>

class QDebugConsole : public QTextEdit {
Q_OBJECT
private:
    QMenu*      contextMenu;             //
    QAction*    cmActionClearConsole;    //
    QMenu*      cmSubmenuLogLevels;      //

public:
    enum LOG_LEVEL {
        LL_INFO         = 0,
        LL_WARNING      = 1,
        LL_ERROR        = 2,
        LL_EXCEPTION    = 3
    } MinimumLogLevel = LL_INFO;

    QList<QPair<QPair<LOG_LEVEL, QString>, QString>> LogMessages;
    QString ConsoleContext;

signals:
    void MessageWasLogged(const QPair<QPair<LOG_LEVEL, QString>, QString>);

public slots:
    void log(const QString&,         LOG_LEVEL ll = LL_INFO);
    void log(const QList<QString>&,  LOG_LEVEL ll = LL_INFO);
    void log(const char*,            LOG_LEVEL ll = LL_INFO);

    void InsertIntoTextEdit(const QPair<QPair<LOG_LEVEL, QString>, QString>);
    void RefreshAllLogMessages();
    void ClearConsole();

    void HandleContextMenuActionTriggered(QAction*);
    void HandleContextMenuRequested(const QPoint&);

public:
    QDebugConsole(QWidget* parent);
};

class QDebugConsoleContext : public QObject {
protected:
    QString consoleContext;
    QString previousContext;

public:
    const QString& ConsoleContext;
    QDebugConsole* Console;

    QDebugConsoleContext(QDebugConsole*, const QString&);
    virtual ~QDebugConsoleContext() override;
};

#endif // QDEBUGCONSOLE_HPP
