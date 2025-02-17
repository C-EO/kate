//
// configview.h
//
// Description: View for configuring the set of targets to be used with the debugger
//
//
// SPDX-FileCopyrightText: 2010 Ian Wakeling <ian.wakeling@ntlworld.com>
// SPDX-FileCopyrightText: 2012 Kåre Särs <kare.sars@iki.fi>
//
//  SPDX-License-Identifier: LGPL-2.0-only

#pragma once

#include <QJsonObject>
#include <QWidget>

#include <optional>

class AdvancedGDBSettings;
class QPushButton;
class QComboBox;
class QFrame;
class QSpinBox;
class QResizeEvent;
class QBoxLayout;
class QCheckBox;
class QLineEdit;
class QLabel;
class KSelectAction;
class QToolButton;
class KActionCollection;
class KConfigGroup;
class KatePluginGDB;
namespace DebugPluginSessionConfig
{
struct ConfigData;
}

namespace KTextEditor
{
class MainWindow;
}

struct GDBTargetConf {
    QString targetName;
    QString executable;
    QString workDir;
    QString arguments;
    QString gdbCmd;
    QStringList customInit;
    QStringList srcPaths;
};

struct DAPAdapterSettings {
    int index{};
    QJsonObject settings;
    QStringList variables;
};

struct DAPTargetConf {
    QString targetName;
    QString debugger;
    QString debuggerProfile;
    QVariantHash variables;
    std::optional<DAPAdapterSettings> dapSettings;
};

class ConfigView : public QWidget
{
    Q_OBJECT
public:
    ConfigView(QWidget *parent, KTextEditor::MainWindow *mainWin, KatePluginGDB *plugin, KSelectAction *targetsAction);
    ~ConfigView() override;

public:
    void readConfig(const DebugPluginSessionConfig::ConfigData &config);
    void writeConfig(DebugPluginSessionConfig::ConfigData &config);

    const GDBTargetConf currentGDBTarget() const;
    /**
     * When `full` is true, a DAP launch request will be computed.
     */
    const DAPTargetConf currentDAPTarget(bool full = false) const;
    bool takeFocusAlways() const;
    bool showIOTab() const;
    bool debuggerIsGDB() const;
    QUrl dapConfigPath;

Q_SIGNALS:
    void showIO(bool show);

    void configChanged();

private Q_SLOTS:
    void slotTargetEdited(const QString &newText);
    void slotTargetSelected(int index);
    void slotAddTarget();
    void slotCopyTarget();
    void slotDeleteTarget();
    void slotAdvancedClicked();
    void slotBrowseExec();
    void slotBrowseDir();
    void readTargetsFromLaunchJson();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void saveCurrentToIndex(int index);
    int loadFromIndex(int index);
    void setAdvancedOptions();
    struct Field {
        QLabel *label;
        QLineEdit *input;
    };
    Field &getDapField(const QString &fieldName);
    void refreshUI();
    void readDAPSettings();
    void initProjectPlugin();
    void setTargetsAction(KSelectAction *action);

private:
    KTextEditor::MainWindow *m_mainWindow;

    QComboBox *m_clientCombo;

    QComboBox *m_targetCombo;
    int m_currentTarget = 0;
    QToolButton *m_addTarget;
    QToolButton *m_copyTarget;
    QToolButton *m_deleteTarget;
    QFrame *m_line;

    QLineEdit *m_executable;
    QToolButton *m_browseExe;

    QLineEdit *m_workingDirectory;
    QToolButton *m_browseDir;
    QSpinBox *m_processId;

    QLineEdit *m_arguments;

    QCheckBox *m_takeFocus;
    QCheckBox *m_redirectTerminal;
    QPushButton *m_advancedSettings;
    QBoxLayout *m_checBoxLayout;

    bool m_useBottomLayout;
    QLabel *m_execLabel;
    QLabel *m_workDirLabel;
    QLabel *m_argumentsLabel;
    QLabel *m_processIdLabel;
    KSelectAction *m_targetSelectAction = nullptr;

    QHash<QString, Field> m_dapFields;
    QHash<QString, QHash<QString, DAPAdapterSettings>> m_dapAdapterSettings;

    AdvancedGDBSettings *m_advanced;
    QUrl m_dapConfigPath;
};
