/*
    SPDX-FileCopyrightText: 2019 Mark Nauwelaerts <mark.nauwelaerts@gmail.com>
    SPDX-FileCopyrightText: 2019 Christoph Cullmann <cullmann@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "lspclientsymbolview.h"

#include <KFuzzyMatcher>
#include <KLineEdit>
#include <KLocalizedString>
#include <QSortFilterProxyModel>

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIdentityProxyModel>
#include <QMenu>
#include <QPointer>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include <drawing_utils.h>
#include <memory>
#include <utility>

class MenuButtonHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit MenuButtonHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QHeaderView(orientation, parent)
    {
    }

private:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override
    {
        const int w = 16 + 8;
        const int h = 16 + 8;

        QStyleOptionHeader optHeader;
        initStyleOption(&optHeader);
        initStyleOptionForIndex(&optHeader, logicalIndex);
        optHeader.rect = rect;
        painter->save();
        style()->drawControl(QStyle::CE_Header, &optHeader, painter, this);
        painter->restore();

        if (logicalIndex == 0) {
            QStyleOptionToolButton option;
            option.toolButtonStyle = Qt::ToolButtonIconOnly;
            option.rect = QRect(0, 0, w, h);
            option.rect = QStyle::alignedRect(layoutDirection(), Qt::AlignVCenter, option.rect.size(), rect);
            option.rect.moveLeft(rect.left());
            option.state = QStyle::State_Enabled;
            option.state.setFlag(QStyle::State_Sunken, m_clicked);
            option.state.setFlag(QStyle::State_AutoRaise);
            option.state.setFlag(QStyle::State_MouseOver, m_hovered);
            option.icon = QIcon::fromTheme(QStringLiteral("application-menu"));
            option.iconSize = QSize(16, 16);
            painter->save();
            this->style()->drawComplexControl(QStyle::CC_ToolButton, &option, painter, nullptr);
            painter->restore();
        }
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (!isPosOnCheckBox(event->pos())) {
            QHeaderView::mousePressEvent(event);
            return;
        }

        viewport()->update();
        QPoint p(rect().left() + 4, rect().bottom() - 4);
        m_clicked = true;
        Q_EMIT buttonClicked(viewport()->mapToGlobal(p));
        m_clicked = false;
        viewport()->update();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        bool wasHovering = m_hovered;
        m_hovered = isPosOnCheckBox(e->pos());
        if (m_hovered) {
            viewport()->update();
            e->accept();
            return;
        } else if (wasHovering) {
            viewport()->update();
        }
        QHeaderView::mouseMoveEvent(e);
    }

    void leaveEvent(QEvent *e) override
    {
        if (m_hovered) {
            m_hovered = false;
        }
        QHeaderView::leaveEvent(e);
    }

    bool isPosOnCheckBox(QPoint p)
    {
        const int pos = sectionPosition(0);
        const int w = 16 + 8;
        const int h = 16 + 8;
        QRect rect = QStyle::alignedRect(layoutDirection(), Qt::AlignVCenter, {w, h}, this->rect());
        rect.moveLeft(pos);
        return rect.contains(p);
    }

    bool m_hovered = false;
    bool m_clicked = false;

Q_SIGNALS:
    void buttonClicked(QPoint globalPos);
};

// TODO: Make this globally available in shared/
enum SymbolViewRoles {
    SymbolRange = Qt::UserRole,
    ScoreRole,
    IsPlaceholder
};

class LSPClientViewTrackerImpl : public LSPClientViewTracker
{
    Q_OBJECT

    typedef LSPClientViewTrackerImpl self_type;

    LSPClientPlugin *m_plugin;
    KTextEditor::MainWindow *m_mainWindow;
    // timers to delay some todo's
    QTimer m_changeTimer;
    int m_change;
    QTimer m_motionTimer;
    int m_motion;
    int m_oldCursorLine = -1;

public:
    LSPClientViewTrackerImpl(LSPClientPlugin *plugin, KTextEditor::MainWindow *mainWin, int change_ms, int motion_ms)
        : m_plugin(plugin)
        , m_mainWindow(mainWin)
        , m_change(change_ms)
        , m_motion(motion_ms)
    {
        Q_UNUSED(m_plugin);
        // get updated
        m_changeTimer.setSingleShot(true);
        auto ch = [this]() {
            Q_EMIT newState(m_mainWindow->activeView(), TextChanged);
        };
        connect(&m_changeTimer, &QTimer::timeout, this, ch);

        m_motionTimer.setSingleShot(true);
        auto mh = [this]() {
            Q_EMIT newState(m_mainWindow->activeView(), LineChanged);
        };
        connect(&m_motionTimer, &QTimer::timeout, this, mh);

        // track views
        connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &self_type::viewChanged);
    }

    void viewChanged(KTextEditor::View *view)
    {
        m_motionTimer.stop();
        m_changeTimer.stop();

        if (view) {
            if (m_motion) {
                connect(view, &KTextEditor::View::cursorPositionChanged, this, &self_type::cursorPositionChanged, Qt::UniqueConnection);
            }
            if (m_change > 0 && view->document()) {
                connect(view->document(), &KTextEditor::Document::textChanged, this, &self_type::textChanged, Qt::UniqueConnection);
            }
            Q_EMIT newState(view, ViewChanged);
            m_oldCursorLine = view->cursorPosition().line();
        }
    }

    void textChanged()
    {
        m_motionTimer.stop();
        m_changeTimer.start(m_change);
    }

    void cursorPositionChanged(KTextEditor::View *view, const KTextEditor::Cursor &newPosition)
    {
        if (m_changeTimer.isActive()) {
            // change trumps motion
            return;
        }

        if (view && newPosition.line() != m_oldCursorLine) {
            m_oldCursorLine = newPosition.line();
            m_motionTimer.start(m_motion);
        }
    }
};

LSPClientViewTracker *LSPClientViewTracker::new_(LSPClientPlugin *plugin, KTextEditor::MainWindow *mainWin, int change_ms, int motion_ms)
{
    return new LSPClientViewTrackerImpl(plugin, mainWin, change_ms, motion_ms);
}

class LSPClientSymbolViewFilterProxyModel : public QSortFilterProxyModel
{
public:
    LSPClientSymbolViewFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    void setFilterString(const QString &string)
    {
        beginResetModel();
        m_pattern = string;
        endResetModel();
    }

protected:
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override
    {
        // make sure to honour configured sort-order (if no scoring applies)
        if (m_pattern.isEmpty()) {
            return QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
        }

        const int l = sourceLeft.data(SymbolViewRoles::ScoreRole).toInt();
        const int r = sourceRight.data(SymbolViewRoles::ScoreRole).toInt();
        return l < r;
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (m_pattern.isEmpty()) {
            return true;
        }

        const auto idx = sourceModel()->index(sourceRow, 0, sourceParent);
        const QString symbol = idx.data().toString();
        const auto res = KFuzzyMatcher::match(m_pattern, symbol);
        sourceModel()->setData(idx, res.score, SymbolViewRoles::ScoreRole);
        return res.matched;
    }

private:
    QString m_pattern;
};

class SymbolViewProxyModel : public QIdentityProxyModel
{
    Q_OBJECT
public:
    using QIdentityProxyModel::QIdentityProxyModel;

    int columnCount(const QModelIndex &) const override
    {
        return 1;
    }
};

/*
 * Instantiates and manages the symbol outline toolview.
 */
class LSPClientSymbolViewImpl : public QObject, public LSPClientSymbolView
{
    Q_OBJECT

    typedef LSPClientSymbolViewImpl self_type;

    LSPClientPlugin *m_plugin;
    KTextEditor::MainWindow *m_mainWindow;
    std::shared_ptr<LSPClientServerManager> m_serverManager;
    std::unique_ptr<QWidget> m_toolview;
    // parent ownership
    QPointer<QTreeView> m_symbols;
    QPointer<KLineEdit> m_filter;
    std::unique_ptr<QMenu> m_popup;
    // initialized/updated from plugin settings
    // managed by context menu later on
    // parent ownership
    QAction *m_detailsOn;
    QAction *m_expandOn;
    QAction *m_treeOn;
    QAction *m_sortOn;
    // view tracking
    std::unique_ptr<LSPClientViewTracker> m_viewTracker;
    // outstanding request
    LSPClientServer::RequestHandle m_handle;
    // magic request tracking cookie
    int m_requestCnt = 0;
    // cached outline models
    struct ModelData {
        QPointer<KTextEditor::Document> document;
        qint64 revision;
        std::shared_ptr<QStandardItemModel> model;
    };
    QList<ModelData> m_models;
    // max number to cache
    static constexpr int MAX_MODELS = 10;
    // last outline model we constructed
    std::shared_ptr<QStandardItemModel> m_outline;
    // filter model, setup once
    LSPClientSymbolViewFilterProxyModel m_filterModel;

    SymbolViewProxyModel *m_identityModel;

    // cached icons for model
    bool m_iconsInitialized = false;
    QIcon m_icon_pkg;
    QIcon m_icon_class;
    QIcon m_icon_typedef;
    QIcon m_icon_function;
    QIcon m_icon_var;

public:
    LSPClientSymbolViewImpl(LSPClientPlugin *plugin, KTextEditor::MainWindow *mainWin, std::shared_ptr<LSPClientServerManager> manager)
        : m_plugin(plugin)
        , m_mainWindow(mainWin)
        , m_serverManager(std::move(manager))
        , m_outline(new QStandardItemModel())
        , m_identityModel(new SymbolViewProxyModel(this))
    {
        m_toolview.reset(m_mainWindow->createToolView(plugin,
                                                      QStringLiteral("lspclient_symbol_outline"),
                                                      KTextEditor::MainWindow::Left,
                                                      QIcon::fromTheme(QStringLiteral("quickopen-class")),
                                                      i18n("Symbol Outline")));

        m_symbols = new QTreeView(m_toolview.get());
        m_symbols->setFocusPolicy(Qt::NoFocus);
        m_symbols->setLayoutDirection(Qt::LeftToRight);
        auto header = new MenuButtonHeaderView(Qt::Horizontal, m_symbols);
        connect(header, &MenuButtonHeaderView::buttonClicked, this, [this](QPoint pos) {
            m_popup->exec(pos);
        });
        m_symbols->setHeader(header);
        m_symbols->header()->setSectionResizeMode(QHeaderView::Stretch);
        m_toolview->layout()->setContentsMargins(0, 0, 0, 0);

        m_toolview->layout()->addWidget(m_symbols);
        m_toolview->layout()->setSpacing(0);

        // setup filter line edit
        m_filter = new KLineEdit(m_toolview.get());
        m_toolview->layout()->addWidget(m_filter);
        m_filter->setPlaceholderText(i18n("Filter…"));
        m_filter->setClearButtonEnabled(true);
        m_filter->setProperty("_breeze_borders_sides", QVariant::fromValue(QFlags{Qt::TopEdge}));
        connect(m_filter, &KLineEdit::textChanged, this, &self_type::filterTextChanged);

        m_symbols->setContextMenuPolicy(Qt::CustomContextMenu);
        m_symbols->setIndentation(10);
        m_symbols->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_symbols->setAllColumnsShowFocus(true);

        // init filter model once, later we only swap the source model!
        QItemSelectionModel *m = m_symbols->selectionModel();
        m_filterModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_filterModel.setSortCaseSensitivity(Qt::CaseInsensitive);
        m_filterModel.setSourceModel(m_outline.get());
        m_filterModel.setRecursiveFilteringEnabled(true);
        m_symbols->setModel(&m_filterModel);
        delete m;

        m_identityModel->setSourceModel(m_outline.get());

        connect(m_symbols, &QTreeView::customContextMenuRequested, this, &self_type::showContextMenu);
        connect(m_symbols, &QTreeView::activated, this, &self_type::goToSymbol);
        connect(m_symbols, &QTreeView::clicked, this, &self_type::goToSymbol);

        // context menu
        m_popup.reset(new QMenu(m_symbols));
        m_treeOn = m_popup->addAction(i18n("Tree Mode"), this, &self_type::displayOptionChanged);
        m_treeOn->setCheckable(true);
        m_expandOn = m_popup->addAction(i18n("Automatically Expand Tree"), this, &self_type::displayOptionChanged);
        m_expandOn->setCheckable(true);
        m_sortOn = m_popup->addAction(i18n("Sort Alphabetically"), this, &self_type::displayOptionChanged);
        m_sortOn->setCheckable(true);
        m_detailsOn = m_popup->addAction(i18n("Show Details"), this, &self_type::displayOptionChanged);
        m_detailsOn->setCheckable(true);
        m_popup->addSeparator();
        m_popup->addAction(i18n("Expand All"), m_symbols.data(), &QTreeView::expandAll);
        m_popup->addAction(i18n("Collapse All"), m_symbols.data(), &QTreeView::collapseAll);

        // sync with plugin settings if updated
        connect(m_plugin, &LSPClientPlugin::update, this, &self_type::configUpdated);

        // get updated
        m_viewTracker.reset(LSPClientViewTracker::new_(plugin, mainWin, 500, 100));
        connect(m_viewTracker.get(), &LSPClientViewTracker::newState, this, &self_type::onViewState);
        connect(m_serverManager.get(), &LSPClientServerManager::serverChanged, this, [this]() {
            refresh(false, false);
        });

        // limit cached models; will not go beyond capacity set here
        m_models.reserve(MAX_MODELS + 1);

        // recolor icons
        QObject::connect(KTextEditor::Editor::instance(), &KTextEditor::Editor::configChanged, this, [this](KTextEditor::Editor *e) {
            colorIcons(e);
        });

        // initial trigger of symbols view update
        configUpdated();
    }

    void displayOptionChanged()
    {
        m_expandOn->setEnabled(m_treeOn->isChecked());
        refresh(false, false);
    }

    void configUpdated()
    {
        m_treeOn->setChecked(m_plugin->m_symbolTree);
        m_detailsOn->setChecked(m_plugin->m_symbolDetails);
        m_expandOn->setChecked(m_plugin->m_symbolExpand);
        m_sortOn->setChecked(m_plugin->m_symbolSort);
        displayOptionChanged();
    }

    void showContextMenu(const QPoint &pos)
    {
        m_popup->popup(m_symbols->viewport()->mapToGlobal(pos), m_treeOn);
    }

    void onViewState(KTextEditor::View *, LSPClientViewTracker::State newState)
    {
        switch (newState) {
        case LSPClientViewTracker::ViewChanged:
            refresh(true);
            break;
        case LSPClientViewTracker::TextChanged:
            refresh(false);
            break;
        case LSPClientViewTracker::LineChanged:
            updateCurrentTreeItem();
            break;
        }
    }

    void makeNodes(const std::list<LSPSymbolInformation> &symbols, bool tree, bool show_detail, QStandardItemModel *model, QStandardItem *parent, bool &details)
    {
        initIcons();

        const QIcon *icon = nullptr;
        for (const auto &symbol : symbols) {
            switch (symbol.kind) {
            case LSPSymbolKind::File:
            case LSPSymbolKind::Module:
            case LSPSymbolKind::Namespace:
            case LSPSymbolKind::Package:
                if (symbol.children.empty()) {
                    continue;
                }
                icon = &m_icon_pkg;
                break;
            case LSPSymbolKind::Class:
            case LSPSymbolKind::Interface:
                icon = &m_icon_class;
                break;
            case LSPSymbolKind::Enum:
                icon = &m_icon_typedef;
                break;
            case LSPSymbolKind::Method:
            case LSPSymbolKind::Function:
            case LSPSymbolKind::Constructor:
                icon = &m_icon_function;
                break;
            // all others considered/assumed Variable
            case LSPSymbolKind::Variable:
            case LSPSymbolKind::Constant:
            case LSPSymbolKind::String:
            case LSPSymbolKind::Number:
            case LSPSymbolKind::Property:
            case LSPSymbolKind::Field:
            default:
                // skip local variable
                // property, field, etc unlikely in such case anyway
                if (parent && parent->icon().cacheKey() == m_icon_function.cacheKey()) {
                    continue;
                }
                icon = &m_icon_var;
            }

            auto node = new QStandardItem();
            auto line = new QStandardItem();
            if (parent && tree) {
                parent->appendRow({node, line});
            } else {
                model->appendRow({node, line});
            }

            if (!symbol.detail.isEmpty()) {
                details = true;
            }
            auto detail = show_detail && !symbol.detail.isEmpty() ? QStringLiteral(" [%1]").arg(symbol.detail) : QString();
            node->setText(symbol.name + detail);
            node->setIcon(*icon);
            node->setData(QVariant::fromValue<KTextEditor::Range>(symbol.range), SymbolViewRoles::SymbolRange);
            static const QChar prefix = QChar::fromLatin1('0');
            line->setText(QStringLiteral("%1").arg(symbol.range.start().line(), 7, 10, prefix));
            // recurse children
            makeNodes(symbol.children, tree, show_detail, model, node, details);
        }
    }

    void onDocumentSymbols(const std::list<LSPSymbolInformation> &outline)
    {
        onDocumentSymbolsOrProblem(outline, QString(), true);
    }

    void onDocumentSymbolsOrProblem(const std::list<LSPSymbolInformation> &outline, const QString &problem = QString(), bool cache = false)
    {
        if (!m_symbols) {
            return;
        }

        // construct new model for data
        auto newModel = std::make_shared<QStandardItemModel>();

        // if we have some problem, just report that, else construct model
        bool details = false;
        if (problem.isEmpty()) {
            makeNodes(outline, m_treeOn->isChecked(), m_detailsOn->isChecked(), newModel.get(), nullptr, details);
            if (cache) {
                // last request has been placed at head of model list
                Q_ASSERT(!m_models.isEmpty());
                m_models[0].model = newModel;
            }
        } else {
            auto item = new QStandardItem(problem);
            item->setData(true, SymbolViewRoles::IsPlaceholder);
            newModel->appendRow(item);
        }

        // cache detail info with model
        newModel->invisibleRootItem()->setData(details);

        // fixup headers
        QStringList headers{i18n("Symbols")};
        newModel->setHorizontalHeaderLabels(headers);

        setModel(newModel);
    }

    void setModel(const std::shared_ptr<QStandardItemModel> &newModel)
    {
        Q_ASSERT(newModel);

        // update filter model, do this before the assignment below deletes the old model!
        m_filterModel.setSourceModel(newModel.get());

        // delete old outline if there, keep our new one alive
        m_outline = newModel;

        // fixup sorting
        if (m_sortOn->isChecked()) {
            m_symbols->setSortingEnabled(true);
            m_symbols->sortByColumn(0, Qt::AscendingOrder);
            m_symbols->header()->setSectionsClickable(true);
        } else {
            // most servers provide items in reasonable file/input order
            // however sadly not all, so let's sort by hidden line number column to make sure
            m_symbols->setSortingEnabled(true);
            m_symbols->sortByColumn(1, Qt::AscendingOrder);
            m_symbols->header()->setSectionsClickable(false);
        }
        // no need to show internal info
        m_symbols->setColumnHidden(1, true);

        // handle auto-expansion
        if (m_expandOn->isChecked()) {
            m_symbols->expandAll();
        }

        // recover detail info from model data
        bool details = newModel->invisibleRootItem()->data().toBool();

        // disable detail setting if no such info available
        // (as an indication there is nothing to show anyway)
        m_detailsOn->setEnabled(details);

        // current item tracking
        updateCurrentTreeItem();

        m_identityModel->setSourceModel(m_outline.get());
    }

    void refresh(bool clear, bool allow_cache = true, int retry = 0)
    {
        // cancel old request!
        m_handle.cancel();

        // check if we have some server for the current view => trigger request
        auto view = m_mainWindow->activeView();
        if (auto server = m_serverManager->findServer(view)) {
            // check if server supports what we need
            if (!server->capabilities().documentSymbolProvider) {
                // inform that support is missing
                onDocumentSymbolsOrProblem({}, i18n("LSP server does not support symbol outline."));
                return;
            }

            // clear current model in any case
            // this avoids that we show stuff not matching the current view
            // but let's only do it if needed, e.g. when changing view
            // so as to avoid unhealthy flickering in other cases
            if (clear) {
                onDocumentSymbolsOrProblem({}, QString(), false);
            }

            // check (valid) cache
            auto doc = view->document();
            auto revision = m_serverManager->revision(doc);
            auto it = m_models.begin();
            for (; it != m_models.end();) {
                if (it->document == doc) {
                    break;
                }
                if (!it->document) {
                    it = m_models.erase(it);
                    continue;
                }
                ++it;
            }
            if (it != m_models.end()) {
                // move to most recently used head
                m_models.move(it - m_models.begin(), 0);
                auto &model = m_models.front();
                // re-use if possible
                // reloaded document recycles revision number, so avoid stale cache
                // (clear := view switch)
                if (revision == model.revision && model.model && (clear || revision > 0) && allow_cache) {
                    setModel(model.model);
                    return;
                }
                it->revision = revision;
            } else {
                m_models.insert(0, {.document = doc, .revision = revision, .model = nullptr});
                if (m_models.size() > MAX_MODELS) {
                    m_models.pop_back();
                }
            }

            // a cancelled request or modified content might result in error response,
            // so arrange to process it as an error rather than an empty result,
            // since the latter would (temporarily) clear the symbol outline
            // and lead to flicker until the next/final request has a proper result again
            auto oldRequestCnt = ++m_requestCnt;
            auto eh = [this, clear, retry, oldRequestCnt](const LSPResponseError &err) {
                switch (err.code) {
                case LSPErrorCode::ContentModified:
                case LSPErrorCode::RequestCancelled:
                    break;
                default:
                    // also try to avoid flicker here
                    // never mind the request if another one has already been launched
                    // but if this is the last request standing, go for retry
                    if (m_requestCnt == oldRequestCnt) {
                        if (retry < 4) {
                            // if we got here, cache was not used
                            refresh(clear, false, retry + 1);
                        } else {
                            // clear old/stale situation and show that the server has lost track
                            onDocumentSymbols({});
                        }
                    }
                    break;
                }
            };

            m_handle = server->documentSymbols(doc->url(), this, utils::mem_fun(&self_type::onDocumentSymbols, this), eh);

            return;
        }

        // else: inform that no server is there
        onDocumentSymbolsOrProblem({}, i18n("No LSP server for this document."));
    }

    // returns (covering item, closest child of covering item after line)
    // if distance non-null, then (output) *distance:
    //  = irrelevant if first element of return value is non-null
    //  < 0 if line is after item range
    //  > 0 if line is before item range = distance from line to item range start
    std::pair<QStandardItem *, QStandardItem *> getCurrentItem(QStandardItem *item, int line, int *distance = nullptr)
    {
        // first traverse the child items to have deepest match!
        // only do this if our stuff is expanded
        QStandardItem *minItem = nullptr;
        if (item == m_outline->invisibleRootItem() || m_symbols->isExpanded(m_filterModel.mapFromSource(m_outline->indexFromItem(item)))) {
            int minDistance = std::numeric_limits<int>::max();
            for (int i = 0; i < item->rowCount(); i++) {
                int dist = 0;
                auto child = item->child(i);
                auto citem = getCurrentItem(child, line, &dist);
                if (citem.first) {
                    return citem;
                } else if (dist > 0 && dist < minDistance) {
                    minDistance = dist;
                    minItem = child;
                }
            }
        }

        // does the line match our item?
        auto range = item->data(SymbolViewRoles::SymbolRange).value<KTextEditor::Range>();
        if (range.overlapsLine(line)) {
            return {item, minItem};
        } else {
            if (distance) {
                auto startline = range.start().line();
                *distance = line < startline ? startline - line : -1;
            }
            return {nullptr, minItem};
        }
    }

    void updateCurrentTreeItem()
    {
        KTextEditor::View *editView = m_mainWindow->activeView();
        if (!editView || !m_symbols) {
            return;
        }

        /**
         * get item if any
         */
        auto items = getCurrentItem(m_outline->invisibleRootItem(), editView->cursorPositionVirtual().line());
        if (!items.first) {
            return;
        }

        /**
         * select it;
         * however, in a typical (e.g. class/namespace) case, the areas between (e.g. method) children
         * select the parent (class) as item.  To aid navigation in such case, the closest (following)
         * child is also considered (as the parent item may in fact be quite far away in the symbol tree).
         */
        QModelIndex coverIndex = m_filterModel.mapFromSource(m_outline->indexFromItem(items.first));
        QModelIndex closestIndex = m_filterModel.mapFromSource(m_outline->indexFromItem(items.second ? items.second : items.first));
        // select covering (parent) item
        m_symbols->selectionModel()->setCurrentIndex(coverIndex, QItemSelectionModel::Clear | QItemSelectionModel::Select);
        // add child to selection and move view there
        m_symbols->selectionModel()->setCurrentIndex(closestIndex, QItemSelectionModel::Select);
        m_symbols->scrollTo(closestIndex);
    }

    void goToSymbol(const QModelIndex &index)
    {
        KTextEditor::View *kv = m_mainWindow->activeView();
        const auto range = index.data(SymbolViewRoles::SymbolRange).value<KTextEditor::Range>();
        if (kv && range.isValid()) {
            kv->setCursorPosition(range.start());
        }
    }

    QAbstractItemModel *documentSymbolsModel() override
    {
        return m_identityModel;
    }

private:
    void colorIcons(KTextEditor::Editor *e)
    {
        using KSyntaxHighlighting::Theme;
        auto theme = e->theme();
        auto varColor = QColor::fromRgba(theme.textColor(Theme::Variable));
        m_icon_var = Utils::colorIcon(QIcon::fromTheme(QStringLiteral("code-variable")), varColor);

        auto typeColor = QColor::fromRgba(theme.textColor(Theme::DataType));
        m_icon_class = Utils::colorIcon(QIcon::fromTheme(QStringLiteral("code-class")), typeColor);

        auto enColor = QColor::fromRgba(theme.textColor(Theme::Constant));
        m_icon_typedef = Utils::colorIcon(QIcon::fromTheme(QStringLiteral("code-typedef")), enColor);

        auto funcColor = QColor::fromRgba(theme.textColor(Theme::Function));
        m_icon_function = Utils::colorIcon(QIcon::fromTheme(QStringLiteral("code-function")), funcColor);

        auto blockColor = QColor::fromRgba(theme.textColor(Theme::Import));
        m_icon_pkg = Utils::colorIcon(QIcon::fromTheme(QStringLiteral("code-block")), blockColor);
    }

    void initIcons()
    {
        if (!m_iconsInitialized) {
            colorIcons(KTextEditor::Editor::instance());
            m_iconsInitialized = true;
        }
    }

private Q_SLOTS:
    /**
     * React on filter change
     * @param filterText new filter text
     */
    void filterTextChanged(const QString &filterText)
    {
        if (!m_symbols) {
            return;
        }

        /**
         * filter
         */
        m_filterModel.setFilterString(filterText);

        /**
         * expand
         */
        if (!filterText.isEmpty()) {
            QTimer::singleShot(100, m_symbols, &QTreeView::expandAll);
        }
    }
};

LSPClientSymbolView *LSPClientSymbolView::new_(LSPClientPlugin *plugin, KTextEditor::MainWindow *mainWin, std::shared_ptr<LSPClientServerManager> manager)
{
    return new LSPClientSymbolViewImpl(plugin, mainWin, std::move(manager));
}

LSPClientSymbolView::~LSPClientSymbolView() = default;

#include "lspclientsymbolview.moc"
#include "moc_lspclientsymbolview.cpp"
