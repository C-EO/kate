/**
 * Description: Common interface for GDB and DAP clients
 *
 *  SPDX-FileCopyrightText: 2022 Héctor Mesa Jiménez <wmj.py@gmx.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "backend.h"
#include "dapbackend.h"

#include <KLocalizedString>
#include <KMessageBox>

Backend::Backend(QObject *parent)
    : BackendInterface(parent)
    , m_debugger(nullptr)
{
}

void Backend::runDebugger(const DAPTargetConf &conf)
{
    if (m_debugger && m_debugger->debuggerRunning()) {
        KMessageBox::error(nullptr, i18n("A debugging session is on course. Please, use re-run or stop the current session."));
        return;
    }

    DapBackend *dap;

    unbind();
    m_debugger = dap = new DapBackend(this);
    bind();

    dap->setPendingBreakpoints(m_breakpoints);
    dap->runDebugger(conf);

    if (m_displayQueryLocals) {
        dap->slotQueryLocals(*m_displayQueryLocals);
    }
}

void Backend::bind()
{
    connect(m_debugger, &BackendInterface::debugLocationChanged, this, &BackendInterface::debugLocationChanged);
    connect(m_debugger, &BackendInterface::breakPointSet, this, &BackendInterface::breakPointSet);
    connect(m_debugger, &BackendInterface::breakPointCleared, this, &BackendInterface::breakPointCleared);
    connect(m_debugger, &BackendInterface::clearBreakpointMarks, this, &BackendInterface::clearBreakpointMarks);
    connect(m_debugger, &BackendInterface::stackFrameInfo, this, &BackendInterface::stackFrameInfo);
    connect(m_debugger, &BackendInterface::stackFrameChanged, this, &BackendInterface::stackFrameChanged);
    connect(m_debugger, &BackendInterface::threads, this, &BackendInterface::threads);
    connect(m_debugger, &BackendInterface::threadUpdated, this, &BackendInterface::threadUpdated);

    connect(m_debugger, &BackendInterface::variableInfo, this, &BackendInterface::variableInfo);
    connect(m_debugger, &BackendInterface::variableScopeOpened, this, &BackendInterface::variableScopeOpened);
    connect(m_debugger, &BackendInterface::variableScopeClosed, this, &BackendInterface::variableScopeClosed);

    connect(m_debugger, &BackendInterface::outputText, this, &BackendInterface::outputText);
    connect(m_debugger, &BackendInterface::outputError, this, &BackendInterface::outputError);
    connect(m_debugger, &BackendInterface::readyForInput, this, &BackendInterface::readyForInput);
    connect(m_debugger, &BackendInterface::programEnded, this, &BackendInterface::programEnded);
    connect(m_debugger, &BackendInterface::gdbEnded, this, &BackendInterface::gdbEnded);
    connect(m_debugger, &BackendInterface::sourceFileNotFound, this, &BackendInterface::sourceFileNotFound);
    connect(m_debugger, &BackendInterface::scopesInfo, this, &BackendInterface::scopesInfo);

    connect(m_debugger, &BackendInterface::debuggerCapabilitiesChanged, this, &BackendInterface::debuggerCapabilitiesChanged);

    connect(m_debugger, &BackendInterface::debuggeeOutput, this, &BackendInterface::debuggeeOutput);
    connect(m_debugger, &BackendInterface::backendError, this, &BackendInterface::backendError);
    connect(m_debugger, &BackendInterface::debuggeeRequiresTerminal, this, &BackendInterface::debuggeeRequiresTerminal);
}

void Backend::unbind()
{
    if (!m_debugger)
        return;
    disconnect(m_debugger, nullptr, this, nullptr);
    delete m_debugger;
}

bool Backend::debuggerRunning() const
{
    return m_debugger && m_debugger->debuggerRunning();
}

bool Backend::debuggerBusy() const
{
    return !m_debugger || m_debugger->debuggerBusy();
}

bool Backend::hasBreakpoint(QUrl const &url, int line) const
{
    return m_debugger && m_debugger->hasBreakpoint(url, line);
}

bool Backend::supportsMovePC() const
{
    return m_debugger && m_debugger->supportsMovePC();
}

bool Backend::supportsRunToCursor() const
{
    return m_debugger && m_debugger->supportsRunToCursor();
}

bool Backend::canSetBreakpoints() const
{
    return m_debugger && m_debugger->canSetBreakpoints();
}

bool Backend::canMove() const
{
    return m_debugger && m_debugger->canMove();
}

bool Backend::canContinue() const
{
    return m_debugger && m_debugger->canContinue();
}

void Backend::toggleBreakpoint(QUrl const &url, int line, bool *added)
{
    if (m_debugger && m_debugger->debuggerRunning()) {
        // let the actual debugger handle it. It will emit
        // breakPointSet/breakPointCleared signal
        m_debugger->toggleBreakpoint(url, line);
    } else {
        // update the breakpoint in our storage
        auto it = m_breakpoints.find(url);
        if (it != m_breakpoints.end()) {
            auto &lines = *it;
            auto existing = lines.indexOf(line);
            if (existing != -1) {
                lines.remove(existing);
                *added = false;
            } else {
                it->push_back(line);
            }
        } else {
            m_breakpoints[url] = {line};
        }
    }
}

void Backend::saveBreakpoint(QUrl const &url, int line)
{
    auto it = m_breakpoints.find(url);
    if (it != m_breakpoints.end()) {
        auto &lines = *it;
        auto existing = lines.indexOf(line);
        if (existing == -1) {
            lines.push_back(line);
        }
    }
}

void Backend::removeSavedBreakpoint(QUrl const &url, int line)
{
    auto it = m_breakpoints.find(url);
    if (it != m_breakpoints.end()) {
        auto &lines = *it;
        auto existing = lines.indexOf(line);
        if (existing != -1) {
            lines.remove(existing);
        }
    }
}

bool Backend::canHotReload() const
{
    if (auto dap = qobject_cast<DapBackend *>(m_debugger)) {
        return dap->canHotReload();
    }
    return false;
}

bool Backend::canHotRestart() const
{
    if (auto dap = qobject_cast<DapBackend *>(m_debugger)) {
        return dap->canHotRestart();
    }
    return false;
}

void Backend::movePC(QUrl const &url, int line)
{
    if (m_debugger)
        m_debugger->movePC(url, line);
}

void Backend::runToCursor(QUrl const &url, int line)
{
    if (m_debugger)
        m_debugger->runToCursor(url, line);
}

void Backend::issueCommand(QString const &cmd)
{
    if (m_debugger)
        m_debugger->issueCommand(cmd);
}

QString Backend::targetName() const
{
    if (m_debugger)
        return m_debugger->targetName();
    return QString();
}

void Backend::setFileSearchPaths(const QStringList &paths)
{
    if (m_debugger)
        m_debugger->setFileSearchPaths(paths);
}

void Backend::slotInterrupt()
{
    if (m_debugger)
        m_debugger->slotInterrupt();
}

void Backend::slotStepInto()
{
    if (m_debugger)
        m_debugger->slotStepInto();
}

void Backend::slotStepOver()
{
    if (m_debugger)
        m_debugger->slotStepOver();
}

void Backend::slotStepOut()
{
    if (m_debugger)
        m_debugger->slotStepOut();
}

void Backend::slotContinue()
{
    if (m_debugger)
        m_debugger->slotContinue();
}

void Backend::slotKill()
{
    if (m_debugger)
        m_debugger->slotKill();
}

void Backend::slotReRun()
{
    if (m_debugger)
        m_debugger->slotReRun();
}

QString Backend::slotPrintVariable(const QString &variable)
{
    if (m_debugger)
        return m_debugger->slotPrintVariable(variable);
    return QString();
}

void Backend::slotHotReload()
{
    if (m_debugger) {
        if (auto dap = qobject_cast<DapBackend *>(m_debugger)) {
            dap->slotHotReload();
        }
    }
}

void Backend::slotHotRestart()
{
    if (m_debugger) {
        if (auto dap = qobject_cast<DapBackend *>(m_debugger)) {
            dap->slotHotRestart();
        }
    }
}

void Backend::slotQueryLocals(bool display)
{
    if (m_debugger) {
        m_debugger->slotQueryLocals(display);
        m_displayQueryLocals = std::nullopt;
    } else {
        m_displayQueryLocals = display;
    }
}

void Backend::changeStackFrame(int index)
{
    if (m_debugger)
        m_debugger->changeStackFrame(index);
}

void Backend::changeThread(int thread)
{
    if (m_debugger)
        m_debugger->changeThread(thread);
}

void Backend::changeScope(int scopeId)
{
    if (m_debugger)
        m_debugger->changeScope(scopeId);
}

void Backend::requestVariable(int variablesReference)
{
    if (m_debugger)
        m_debugger->requestVariable(variablesReference);
}

#include "moc_backend.cpp"
