/**
 * Description: Common interface for GDB and DAP clients
 *
 *  SPDX-FileCopyrightText: 2022 Héctor Mesa Jiménez <wmj.py@gmx.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <QObject>
#include <QUrl>
#include <ktexteditor/message.h>
#include <optional>

#include "dap/client.h"
#include "dap/entities.h"

class BackendInterface : public QObject
{
    Q_OBJECT
public:
    BackendInterface(QObject *parent);
    ~BackendInterface() override = default;

    enum ThreadState {
        Started,
        Running,
        Stopped,
        Exited,
        Unknown,
    };

    /**
     * true if debugger is running
     */
    virtual bool debuggerRunning() const = 0;
    /**
     * true if debugger is running and busy executing commands
     */
    virtual bool debuggerBusy() const = 0;
    /**
     * true if a breakpoint exists at url:line
     */
    virtual bool hasBreakpoint(QUrl const &url, int line) const = 0;
    /**
     * true if debugger supports move program counter
     */
    virtual bool supportsMovePC() const = 0;
    /**
     * true if debugger supports run to cursor
     */
    virtual bool supportsRunToCursor() const = 0;
    /**
     * true if breakpoints can be set/unset
     */
    virtual bool canSetBreakpoints() const = 0;
    /**
     * true if execution can continue
     */
    virtual bool canContinue() const = 0;
    /**
     * true if basic movement actions can be used
     */
    virtual bool canMove() const = 0;
    /**
     * toggle breakpoint at url:line
     * whether the breakpoint was added or not
     */
    virtual void toggleBreakpoint(QUrl const &url, int line, bool *added = nullptr) = 0;
    /**
     * move PC to url:line
     */
    virtual void movePC(QUrl const &url, int line) = 0;
    /**
     * run to url:line
     */
    virtual void runToCursor(QUrl const &url, int line) = 0;

    virtual void issueCommand(QString const &cmd) = 0;

    virtual QString targetName() const = 0;
    virtual void setFileSearchPaths(const QStringList &paths) = 0;

public Q_SLOTS:
    /**
     * interrupt debugger process
     */
    virtual void slotInterrupt() = 0;
    /**
     * step into command
     */
    virtual void slotStepInto() = 0;
    /**
     * step over command
     */
    virtual void slotStepOver() = 0;
    /**
     * step out command
     */
    virtual void slotStepOut() = 0;
    /**
     * continue command
     */
    virtual void slotContinue() = 0;
    /**
     * kill debuggee command
     */
    virtual void slotKill() = 0;
    /**
     * re-run program
     */
    virtual void slotReRun() = 0;

    virtual QString slotPrintVariable(const QString &variable) = 0;

    /**
     * query information
     *
     * stack
     * frame
     * arguments
     * this
     * locals
     * thread
     */
    virtual void slotQueryLocals(bool display) = 0;

    virtual void changeStackFrame(int index) = 0;
    virtual void changeThread(int thread) = 0;
    virtual void changeScope(int scopeId) = 0;
    virtual void requestVariable(int variablesReference) = 0;

Q_SIGNALS:
    void debugLocationChanged(const QUrl &file, int lineNum);
    void breakPointSet(const QUrl &file, int lineNum);
    void breakPointCleared(const QUrl &file, int lineNum);
    void clearBreakpointMarks();
    void stackFrameInfo(const QList<dap::StackFrame> &frames);
    void stackFrameChanged(int level);
    void threads(const QList<dap::Thread> &thread);
    void threadUpdated(const dap::Thread &thread, ThreadState state, bool isActive);

    void variableInfo(int parentId, const dap::Variable &variable);
    void variableScopeOpened();
    void variableScopeClosed();
    void scopesInfo(const QList<dap::Scope> &scopes, std::optional<int> activeId);

    void outputText(const QString &text);
    void outputError(const QString &text);
    void debuggeeOutput(const dap::Output &output);
    void readyForInput(bool ready);
    void programEnded();
    void gdbEnded();
    void sourceFileNotFound(const QString &fileName);
    void backendError(const QString &message, KTextEditor::Message::MessageType level);
    void debuggeeRequiresTerminal(const dap::RunInTerminalRequestArguments &args, const dap::Client::ProcessInTerminal &notifyProcessCreation);

    void debuggerCapabilitiesChanged();
};
