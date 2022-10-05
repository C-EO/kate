/***************************************************************************
 *   This file is part of Kate build plugin                                *
 *   SPDX-FileCopyrightText: 2014 Kåre Särs <kare.sars@iki.fi>                           *
 *                                                                         *
 *   SPDX-License-Identifier: LGPL-2.0-or-later
 ***************************************************************************/

#include "TargetModel.h"
#include <KLocalizedString>
#include <QDebug>
#include <QTimer>

TargetModel::TargetSet::TargetSet(const QString &_name, const QString &_dir)
    : name(_name)
    , workDir(_dir)
{
}

// Model data
// parent is the m_targets list index or InvalidIndex if it is a root element
// row is the m_targets.commands list index or m_targets index if it is a root element
// column 0 is command name or target-set name if it is a root element
// column 1 is the command or working directory if it is a root element

TargetModel::TargetModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}
TargetModel::~TargetModel()
{
}

void TargetModel::clear()
{
    beginResetModel();
    m_targets.clear();
    endResetModel();
}

void TargetModel::setDefaultCmd(int rootRow, const QString &defCmd)
{
    if (rootRow < 0 || rootRow >= m_targets.size()) {
        qDebug() << "rootRow not valid";
        return;
    }

    for (int i = 0; i < m_targets[rootRow].commands.size(); i++) {
        if (defCmd == m_targets[rootRow].commands[i].name) {
            m_targets[rootRow].defaultCmd = defCmd;
            return;
        }
    }
}

QModelIndex TargetModel::addTargetSet(const QString &setName, const QString &workDir)
{
    // make the name unique
    QString newName = setName;
    for (int i = 0; i < m_targets.count(); i++) {
        if (m_targets[i].name == newName) {
            newName += QStringLiteral(" 2");
            i = -1;
        }
    }

    beginInsertRows(QModelIndex(), m_targets.count(), m_targets.count());
    TargetModel::TargetSet targetSet(newName, workDir);
    m_targets << targetSet;
    endInsertRows();
    return index(m_targets.count() - 1, 0);
}

QModelIndex TargetModel::addCommand(const QModelIndex &parentIndex, const QString &cmdName, const QString &buildCmd, const QString &runCmd)
{
    int rootRow = parentIndex.row();
    if (rootRow < 0 || rootRow >= m_targets.size()) {
        qDebug() << "rootRow" << rootRow << "not valid" << m_targets.size();
        return QModelIndex();
    }

    // make the name unique
    QString newName = cmdName;
    for (int i = 0; i < m_targets[rootRow].commands.count(); i++) {
        if (m_targets[rootRow].commands[i].name == newName) {
            newName += QStringLiteral(" 2");
            i = -1;
        }
    }

    QModelIndex rootIndex = createIndex(rootRow, 0, InvalidIndex);
    beginInsertRows(rootIndex, m_targets[rootRow].commands.count(), m_targets[rootRow].commands.count());
    m_targets[rootRow].commands << Command{newName, buildCmd, runCmd};
    endInsertRows();
    return createIndex(m_targets[rootRow].commands.size() - 1, 0, rootRow);
}

QModelIndex TargetModel::copyTargetOrSet(const QModelIndex &index)
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    quint32 rootRow = index.internalId();
    if (rootRow == InvalidIndex) {
        rootRow = index.row();
        if (m_targets.count() <= static_cast<int>(rootRow)) {
            return QModelIndex();
        }

        beginInsertRows(QModelIndex(), m_targets.count(), m_targets.count());

        QString newName = m_targets[rootRow].name + QStringLiteral(" 2");
        for (int i = 0; i < m_targets.count(); i++) {
            if (m_targets[i].name == newName) {
                newName += QStringLiteral(" 2");
                i = -1;
            }
        }
        m_targets << m_targets[rootRow];
        m_targets.last().name = newName;
        endInsertRows();

        return createIndex(m_targets.count() - 1, 0, InvalidIndex);
        ;
    }

    if (m_targets.count() <= static_cast<int>(rootRow)) {
        return QModelIndex();
    }
    if (index.row() < 0) {
        return QModelIndex();
    }
    if (index.row() >= m_targets[rootRow].commands.count()) {
        return QModelIndex();
    }

    QModelIndex rootIndex = createIndex(rootRow, 0, InvalidIndex);
    beginInsertRows(rootIndex, m_targets[rootRow].commands.count(), m_targets[rootRow].commands.count());

    const auto cmd = m_targets[rootRow].commands[index.row()];
    QString newName = cmd.name + QStringLiteral(" 2");
    for (int i = 0; i < m_targets[rootRow].commands.count(); i++) {
        if (m_targets[rootRow].commands[i].name == newName) {
            newName += QStringLiteral(" 2");
            i = -1;
        }
    }
    m_targets[rootRow].commands << Command{newName, cmd.buildCmd, cmd.runCmd};

    endInsertRows();
    return createIndex(m_targets[rootRow].commands.count() - 1, 0, rootRow);
}

QModelIndex TargetModel::defaultTarget(const QModelIndex &targetSet)
{
    QModelIndex root = targetSet.sibling(targetSet.row(), 0);

    if (root.parent().isValid()) {
        // go to the parent so we know where we start
        root = root.parent();
    }

    // This is the target-set root
    auto model = root.model();
    if (!model) {
        qDebug() << "No model found";
        return QModelIndex();
    }
    QModelIndex defaultIndex;
    for (int i = 0; i < model->rowCount(root); ++i) {
        QModelIndex childIndex = model->index(i, 0, root);
        if (i == 0) {
            defaultIndex = childIndex;
        }
        if (childIndex.data(Qt::CheckStateRole) == Qt::Checked) {
            defaultIndex = childIndex;
            break;
        }
    }
    return defaultIndex;
}

void TargetModel::deleteItem(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    if (index.internalId() == InvalidIndex) {
        if (index.row() < 0 || index.row() >= m_targets.size()) {
            qWarning() << "Bad target-set row:" << index.row() << m_targets.size();
            return;
        }
        beginRemoveRows(index.parent(), index.row(), index.row());
        m_targets.removeAt(index.row());
        endRemoveRows();
    } else {
        int setRow = static_cast<int>(index.internalId());
        if (setRow >= m_targets.size()) {
            qWarning() << "Bad target-set row:" << index.internalId() << m_targets.size();
            return;
        }
        TargetSet &set = m_targets[setRow];
        if (index.row() < 0 || index.row() >= set.commands.size()) {
            qWarning() << "Bad command row:" << index.row() << set.commands.size();
            return;
        }
        beginRemoveRows(index.parent(), index.row(), index.row());
        set.commands.removeAt(index.row());
        endRemoveRows();
    }
}

void TargetModel::deleteTargetSet(const QString &targetSet)
{
    for (int i = 0; i < m_targets.count(); i++) {
        if (m_targets[i].name == targetSet) {
            beginRemoveRows(QModelIndex(), i, i);
            m_targets.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

const QString TargetModel::command(const QModelIndex &itemIndex)
{
    if (!itemIndex.isValid()) {
        return QString();
    }

    // take the item from the second column
    QModelIndex cmdIndex = itemIndex.siblingAtColumn(1);

    if (!itemIndex.parent().isValid()) {
        // This is the target-set root column
        // execute the default target (checked or first child)
        auto model = itemIndex.model();
        if (!model) {
            qDebug() << "No model found";
            return QString();
        }
        for (int i = 0; i < model->rowCount(itemIndex); ++i) {
            QModelIndex childIndex = model->index(i, 0, itemIndex);
            if (i == 0) {
                cmdIndex = childIndex.siblingAtColumn(1);
            }
            if (childIndex.data(Qt::CheckStateRole) == Qt::Checked) {
                cmdIndex = childIndex.siblingAtColumn(1);
                break;
            }
        }
    }
    return cmdIndex.data().toString();
}

const QString TargetModel::cmdName(const QModelIndex &itemIndex)
{
    if (!itemIndex.isValid()) {
        return QString();
    }

    // take the item from the first column
    QModelIndex nameIndex = itemIndex.sibling(itemIndex.row(), 0);

    if (!itemIndex.parent().isValid()) {
        // This is the target-set root column
        // execute the default target (checked or first child)
        auto model = itemIndex.model();
        if (!model) {
            qDebug() << "No model found";
            return QString();
        }
        for (int i = 0; i < model->rowCount(itemIndex); ++i) {
            QModelIndex childIndex = model->index(i, 0, itemIndex);
            if (i == 0) {
                nameIndex = childIndex.siblingAtColumn(0);
            }
            if (childIndex.data(Qt::CheckStateRole) == Qt::Checked) {
                nameIndex = childIndex.siblingAtColumn(0);
                break;
            }
        }
    }
    return nameIndex.data().toString();
}

const QString TargetModel::workDir(const QModelIndex &itemIndex)
{
    if (!itemIndex.isValid()) {
        return QString();
    }

    QModelIndex workDirIndex = itemIndex.sibling(itemIndex.row(), 1);

    if (itemIndex.parent().isValid()) {
        workDirIndex = itemIndex.parent().siblingAtColumn(1);
    }
    return workDirIndex.data().toString();
}

const QString TargetModel::targetName(const QModelIndex &itemIndex)
{
    if (!itemIndex.isValid()) {
        return QString();
    }
    QModelIndex targetNameIndex = itemIndex.sibling(itemIndex.row(), 0);

    if (itemIndex.parent().isValid()) {
        targetNameIndex = itemIndex.parent().siblingAtColumn(0);
    }
    return targetNameIndex.data().toString();
}

QVariant TargetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !hasIndex(index.row(), index.column(), index.parent())) {
        return QVariant();
    }

    // Tooltip
    if (role == Qt::ToolTipRole) {
        if (index.column() == 0 && index.parent().isValid()) {
            return i18n("Check the check-box to make the command the default for the target-set.");
        }
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::CheckStateRole) {
        return QVariant();
    }

    int row = index.row();

    if (index.internalId() == InvalidIndex) {
        if (row < 0 || row >= m_targets.size() || role == Qt::CheckStateRole) {
            return QVariant();
        }

        switch (index.column()) {
        case 0:
            return m_targets[row].name;
        case 1:
            return m_targets[row].workDir;
        }
    } else {
        int rootIndex = index.internalId();
        if (rootIndex < 0 || rootIndex >= m_targets.size()) {
            return QVariant();
        }
        if (row < 0 || row >= m_targets[rootIndex].commands.size()) {
            return QVariant();
        }

        if (role == Qt::CheckStateRole) {
            if (index.column() != 0) {
                return QVariant();
            }
            return m_targets[rootIndex].commands[row].buildCmd == m_targets[rootIndex].defaultCmd ? Qt::Checked : Qt::Unchecked;
        } else {
            switch (index.column()) {
            case 0:
                return m_targets[rootIndex].commands[row].name;
            case 1:
                return m_targets[rootIndex].commands[row].buildCmd;
            case 2:
                return m_targets[rootIndex].commands[row].runCmd;
            }
        }
    }

    return QVariant();
}

QVariant TargetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation != Qt::Horizontal) {
        return QVariant();
    }

    if (section == 0) {
        return i18n("Command/Target-set Name");
    }
    if (section == 1) {
        return i18n("Working Directory / Command");
    }
    if (section == 2) {
        return i18n("Run Command");
    }
    return QVariant();
}

bool TargetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    // FIXME
    if (role != Qt::EditRole && role != Qt::CheckStateRole) {
        return false;
    }
    if (!index.isValid() || !hasIndex(index.row(), index.column(), index.parent())) {
        return false;
    }

    int row = index.row();
    if (index.internalId() == InvalidIndex) {
        if (row < 0 || row >= m_targets.size()) {
            return false;
        }
        switch (index.column()) {
        case 0:
            m_targets[row].name = value.toString();
            return true;
        case 1:
            m_targets[row].workDir = value.toString();
            return true;
        }
    } else {
        int rootIndex = index.internalId();
        if (rootIndex < 0 || rootIndex >= m_targets.size()) {
            return false;
        }
        if (row < 0 || row >= m_targets[rootIndex].commands.size()) {
            return false;
        }

        if (role == Qt::CheckStateRole) {
            if (index.column() == 0) {
                m_targets[rootIndex].defaultCmd = m_targets[rootIndex].commands[row].buildCmd;
                Q_EMIT dataChanged(createIndex(0, 0, rootIndex), createIndex(m_targets[rootIndex].commands.size() - 1, 0, rootIndex));
            }
        } else {
            switch (index.column()) {
            case 0:
                m_targets[rootIndex].commands[row].name = value.toString();
                return true;
            case 1:
                m_targets[rootIndex].commands[row].buildCmd = value.toString();
                return true;
            case 2:
                m_targets[rootIndex].commands[row].runCmd = value.toString();
                return true;
            }
        }
    }
    return false;
}

Qt::ItemFlags TargetModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    // run command column for target set row
    if (index.column() == 2 && !index.parent().isValid()) {
        return Qt::NoItemFlags;
    }

    if (index.internalId() != InvalidIndex && index.column() == 0) {
        return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    }

    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int TargetModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_targets.size();
    }

    if (parent.internalId() != InvalidIndex) {
        return 0;
    }

    if (parent.column() != 0) {
        return 0;
    }

    int row = parent.row();
    if (row < 0 || row >= m_targets.size()) {
        return 0;
    }

    return m_targets[row].commands.size();
}

int TargetModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QModelIndex TargetModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0) {
        return QModelIndex();
    }

    quint32 rootIndex = InvalidIndex;
    if (parent.isValid() && parent.internalId() == InvalidIndex) {
        // This is a command (child of a root element)
        if (parent.column() != 0) {
            // Only root item column 0 can have children
            return QModelIndex();
        }
        rootIndex = parent.row();
        if (parent.row() >= m_targets.size() || row >= m_targets.at(parent.row()).commands.size()) {
            return QModelIndex();
        }
        return createIndex(row, column, rootIndex);
    }
    // This is a root item
    if (row >= m_targets.size()) {
        return QModelIndex();
    }
    return createIndex(row, column, rootIndex);
}

QModelIndex TargetModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }

    if (child.internalId() == InvalidIndex) {
        return QModelIndex();
    }

    int pRow = (int)child.internalId();

    if (pRow < 0 || pRow >= m_targets.size()) {
        return QModelIndex();
    }
    return createIndex(pRow, 0, InvalidIndex);
}
