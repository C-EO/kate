/*
    SPDX-FileCopyrightText: 2020 Kåre Särs <kare.sars@iki.fi>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KateSearchCommand_h
#define KateSearchCommand_h

#include <KTextEditor/Command>
#include <QString>

namespace KTextEditor
{
class View;
class Range;
}

class KateSearchCommand : public KTextEditor::Command
{
    Q_OBJECT
public:
    KateSearchCommand(QObject *parent);

Q_SIGNALS:
    void setSearchPlace(int place);
    void setCurrentFolder();
    void setSearchString(const QString &pattern);
    void startSearch();
    void newTab();

    //
    // KTextEditor::Command
    //
public:
    bool exec(KTextEditor::View *view, const QString &cmd, QString &msg, const KTextEditor::Range &range = KTextEditor::Range::invalid()) override;
    bool help(KTextEditor::View *view, const QString &cmd, QString &msg) override;
};

#endif

// kate: space-indent on; indent-width 4; replace-tabs on;
