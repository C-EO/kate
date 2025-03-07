/*
   SPDX-FileCopyrightText: 2010 Marco Mentasti <marcomentasti@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

struct OutputStyle;

#include "cachedsqlquerymodel.h"

/// provide colors and styles
class DataOutputModel : public CachedSqlQueryModel
{
public:
    explicit DataOutputModel(QObject *parent = nullptr);
    ~DataOutputModel() override;

    bool useSystemLocale() const;
    void setUseSystemLocale(bool useSystemLocale);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void clear() override;
    void readConfig();

private:
    QHash<QString, OutputStyle *> m_styles;
    bool m_useSystemLocale;
};
