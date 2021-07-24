/*  SPDX-License-Identifier: LGPL-2.0-or-later

    SPDX-FileCopyrightText: 2005 Christoph Cullmann <cullmann@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __KATE_SESSION_MANAGER_H__
#define __KATE_SESSION_MANAGER_H__

#include "katesession.h"

#include <KDirWatch>

#include <QHash>
#include <QObject>

#include <memory>

typedef QList<KateSession::Ptr> KateSessionList;

/**
 * Takes care of session store, session activation and sesion defaults.
 *
 * There is a single implicit anonymous session that is always present, but not
 * exported outside the manager, hence not even visible to user. It's possible
 * to activate the session via KateSessionManager::activateAnonymousSession. For other
 * usecases the anonymous session behaves the same as regular one.
 *
 * The session defaults are stored in an implicit default session, which the users
 * can only overwrite via KateSessionManager::saveDefaults.
 *
 * If the client request new session (calling KateSessionManager::activateNewSession)
 * the manager will import stored default session config to new session and overwrite
 * the anonymous session with this newly created one.
 */
class KATE_TESTS_EXPORT KateSessionManager : public QObject
{
    Q_OBJECT

public:
    KateSessionManager(QObject *parent = nullptr, const QString &sessionsDir = QString());

    /**
     * allow access to the session list
     * kept up to date by watching the dir
     */
    KateSessionList sessionList();

    /**
     * activate session by \p name
     * first, it will look if a session with this name exists in list
     * if yes, it will use this session, else it will create a new session file
     * @param name name of the session to activate
     * @param closeAndSaveLast try to close and save last session or not?
     * @param loadNew load new session stuff?
     * @return false==session has been delegated, true==session has been activated in this distance
     */
    // bool activateSession(const QString &name, const bool closeAndSaveLast = true, const bool loadNew = true);
    bool activateSessionById(const QString &id, const bool closeAndSaveLast = true, const bool loadNew = true);
    bool activateSessionByName(const QString &name);

    /**
     * create and activate new session and store it as anonymous
     */
    void activateNewSession();

    /**
     * create and activate new session from another \p session as template
     */
    void activateNewSessionFrom(const KateSession::Ptr &session);

    /**
     * save current session
     * @param rememberAsLast remember this session as last used?
     * @return success
     */
    bool saveActiveSession(bool rememberAsLast = false);

    /**
     * return the current active session
     * sessionFile == empty means we have no session around for this instance of kate
     * @return session active atm
     */
    KateSession::Ptr activeSession()
    {
        return m_activeSession;
    }

    /**
     * @return true when @p session is active in any Kate instance, otherwise false
     */
    bool sessionIsActive(const KateSession::Ptr session);

    /**
     * session dir
     * @return global session dir
     */
    const QString &sessionsDir() const
    {
        return m_sessionsDir;
    }

    /**
     * initial session chooser, on app start
     * @return success, if false, app should exit
     */
    bool chooseSession();

public Q_SLOTS:
    /**
     * try to start a new session
     */
    void sessionNew();

    /**
     * try to save current session
     */
    void sessionSave();

    /**
     * try to save as current session
     */
    void sessionSaveAs();

    /**
     * show dialog to manage our sessions
     */
    void sessionManage();

Q_SIGNALS:
    /**
     * Emitted, whenever the session changes, e.g. when it was renamed.
     */
    void sessionChanged();

    /**
     * Emitted whenever the session list has changed.
     * @see sessionList()
     */
    void sessionListChanged();

    /**
     * module internal APIs
     */
public:

    /**
     * Try to delete the @p session and removes the session from sessions list
     * @param session the session to delete
     * @return true on success, false if @p session is currently in use
     */
    bool deleteSession(KateSession::Ptr session);

    /**
     * Try to copy the @p session to a new session @p newName.
     * Will ask by @c askForNewSessionName() for a different name when @p newName is already in use or is an
     * empty string.
     * @param session the session to copy
     * @param newName is wished name of the new session
     * @return the new session name on success, otherwise an empty string
     * @see askForNewSessionName()
     */
    QString copySession(const KateSession::Ptr &session, const QString &newName = QString());

    /**
     * Try to rename the @p session to @p newName.
     * Will ask by @c askForNewSessionName() for a different name when @p newName is already in use or is an
     * empty string.
     * @param session the session to rename
     * @param newName is wished new name of the session
     * @return the new session name on success, otherwise an empty string
     * @see askForNewSessionName()
     */
    QString renameSession(KateSession::Ptr session, const QString &newName = QString());

    /**
     * activate a session
     * first, it will look if a session with this name exists in list
     * if yes, it will use this session, else it will create a new session file
     * @param session session to activate
     * @param closeAndSaveLast try to close and save last session or not?
     * @param loadNew load new session stuff?
     * @return false==session has been delegated, true==session has been activated in this distance
     */
    bool activateSession(KateSession::Ptr session, const bool closeAndSaveLast = true, const bool loadNew = true);

private Q_SLOTS:
    /**
     * trigger update of session list
     */
    void updateSessionList();

private:
    /**
     * return session with given name
     * if no existing session matches, create new one with this name
     * @param name session name
     */
    KateSession::Ptr giveSessionX(const QString &id);

    /**
     * Create new session from defaults
     * @param name session name
     */
    KateSession::Ptr createSession();
    KateSession::Ptr createSessionFrom(KateSession::Ptr session);

    KateSession::Ptr createSession(KateSession::Ptr session);

    /**
     * Ask the user for a new session name, when needed.
     * @param session is the session to rename or copy
     * @param newName is a preset value. Is @p newName not already in use is nothing asked
     * @return a (currently) not used new session name or an empty string when
     * user aborted or when @p newName is the current session name.
     */
    QString askForNewSessionName(KateSession::Ptr session, const QString &newName = QString());

    KateSession::Ptr sessionByName(const QString &name) const;

    /**
     * Try to generate a new session name from @p target by a number suffix.
     * @param target is the base name
     * @return a (currently) not used session name or an empty string
     */
    QString suggestNewSessionName(const QString &target);

    /**
     * helper function to save the session to a given config object
     */
    void saveSessionTo(KConfig *sc);

    /**
     * restore sessions documents, windows, etc...
     */
    void loadSession(const KateSession::Ptr &session, bool loadDocs = true) const;

    /**
     * Writes sessions as jump list actions to the kate.desktop file
     */
    void updateJumpListActions(const QStringList &sessionList);

    /**
     * Given a config group name, determines if the group represents
     * a session ViewSpace group (i.e. "MainWindowN-ViewSpace N [doc url]")
     * and, if so,
     *  - returns TRUE if the document referred to has no views
     *            and the session is not anonymous;
     *  - returns FALSE otherwise.
     *
     *  A document does not have a view when it is opened in a named session
     *  until its tab is activated.
     */
    static bool isViewLessDocumentViewSpaceGroup(const QString &group);

    void writeSession(const KateSession::Ptr session);
    KateSession::Ptr readSession(const QString &id);

    /**
     * returns session config file according to policy
     */
    QString sessionFileForId(const QString &id) const;
    QString sessionFile(const KateSession::Ptr session) const;

private:
    /**
     * absolute path to dir in home dir where to store the sessions
     */
    QString m_sessionsDir;

    /**
     * list of current available sessions
     */
    QHash<QString, KateSession::Ptr> m_sessions;

    /**
     * current active session
     */
    KateSession::Ptr m_activeSession;

    std::unique_ptr<KDirWatch> m_dirWatch;
};

#endif
