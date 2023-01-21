﻿/*  This file is part of the Kate project.
 *
 *  SPDX-FileCopyrightText: 2012 Christoph Cullmann <cullmann@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kateprojectview.h"
#include "branchcheckoutdialog.h"
#include "currentgitbranchbutton.h"
#include "gitprocess.h"
#include "gitwidget.h"
#include "kateprojectfiltermodel.h"
#include "kateprojectplugin.h"
#include "kateprojectpluginview.h"
#include "ktexteditor_utils.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <KAcceleratorManager>
#include <KActionCollection>
#include <KLineEdit>
#include <KLocalizedString>

#include <QPushButton>
#include <QVBoxLayout>

KateProjectView::KateProjectView(KateProjectPluginView *pluginView, KateProject *project, KTextEditor::MainWindow *mainWindow)
    : m_pluginView(pluginView)
    , m_project(project)
    , m_treeView(new KateProjectViewTree(pluginView, project))
    , m_filter(new KLineEdit())
    , m_branchBtn(new CurrentGitBranchButton(mainWindow, this))
{
    /**
     * layout tree view and co.
     */
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);
    layout->addWidget(m_filter);
    setLayout(layout);

    /**
     * Setup checkout stuff, git branch button in statusbar
     */
    auto chckbrAct = pluginView->actionCollection()->addAction(QStringLiteral("checkout_branch"), this, [mainWindow, this] {
        BranchCheckoutDialog bd(mainWindow->window(), m_pluginView, m_project->baseDir());
        bd.openDialog();
    });
    m_branchBtn->setDefaultAction(chckbrAct);
    chckbrAct->setIcon(QIcon::fromTheme(QStringLiteral("vcs-branch")));
    chckbrAct->setText(i18n("Checkout Git Branch"));
    Utils::insertWidgetInStatusbar(m_branchBtn, mainWindow);

    // let tree get focus for keyboard selection of file to open
    setFocusProxy(m_treeView);

    m_filterStartTimer.setSingleShot(true);
    m_filterStartTimer.setInterval(400);
    m_filterStartTimer.callOnTimeout(this, &KateProjectView::filterTextChanged);

    /**
     * setup filter line edit
     */
    m_filter->setPlaceholderText(i18n("Filter..."));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &KLineEdit::textChanged, this, [this] {
        m_filterStartTimer.start();
    });

    checkAndRefreshGit();

    connect(m_project, &KateProject::modelChanged, this, &KateProjectView::checkAndRefreshGit);
    connect(&m_pluginView->plugin()->fileWatcher(), &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (m_branchChangedWatcherFile == path) {
            m_project->reload(true);
        }
    });
}

KateProjectView::~KateProjectView()
{
    if (!m_branchChangedWatcherFile.isEmpty()) {
        m_pluginView->plugin()->fileWatcher().removePath(m_branchChangedWatcherFile);
    }
}

void KateProjectView::selectFile(const QString &file)
{
    m_treeView->selectFile(file);
}

void KateProjectView::openSelectedDocument()
{
    m_treeView->openSelectedDocument();
}

void KateProjectView::filterTextChanged()
{
    const auto filterText = m_filter->text();
    /**
     * filter
     */
    static_cast<KateProjectFilterProxyModel *>(m_treeView->model())->setFilterString(filterText);

    /**
     * expand
     */
    if (!filterText.isEmpty()) {
        QTimer::singleShot(100, m_treeView, &QTreeView::expandAll);
    }
}

void KateProjectView::checkAndRefreshGit()
{
    const auto dotGitPath = getRepoBasePath(m_project->baseDir());
    /**
     * Not in a git repo or git was removed
     */
    if (!dotGitPath.has_value()) {
        if (!m_branchChangedWatcherFile.isEmpty()) {
            m_pluginView->plugin()->fileWatcher().removePath(m_branchChangedWatcherFile);
            m_branchChangedWatcherFile.clear();
        }
    } else {
        const QString fileToWatch = dotGitPath.value() + QStringLiteral(".git/HEAD");
        // fileToWatch == m_branchChangedWatcherFile can be true, but doesn't matter. We MUST always
        // re add the file otherwise it will not work.

        if (!m_branchChangedWatcherFile.isEmpty()) {
            m_pluginView->plugin()->fileWatcher().removePath(m_branchChangedWatcherFile);
            m_branchChangedWatcherFile.clear();
        }
        if (QFileInfo::exists(fileToWatch)) {
            m_branchChangedWatcherFile = fileToWatch;
            m_pluginView->plugin()->fileWatcher().addPath(m_branchChangedWatcherFile);
        }
    }
    static_cast<CurrentGitBranchButton *>(m_branchBtn)->refresh();
}
