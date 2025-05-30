/*
 * SPDX-FileCopyrightText: 2022 Pablo Rauzy <r .at. uzy .dot. me>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <QString>

#include <KTextEditor/Command>
#include <KTextEditor/View>

#include "keyboardmacrosplugin.h"

/**
 * Plugin commands to manage named keyboard macros
 */
class KeyboardMacrosPluginCommands : public KTextEditor::Command
{
public:
    explicit KeyboardMacrosPluginCommands(KeyboardMacrosPlugin *plugin);
    bool exec(KTextEditor::View *view, const QString &cmd, QString &msg, const KTextEditor::Range &) override;
    bool help(KTextEditor::View *, const QString &cmd, QString &msg) override;

private:
    KeyboardMacrosPlugin *m_plugin;
};
