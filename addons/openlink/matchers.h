/*
    SPDX-FileCopyrightText: 2023 Waqar Ahmed <waqar.17a@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFileInfo>
#include <QRegularExpression>

enum OpenLinkType {
    HttpLink,
    FileLink,
};

struct OpenLinkRange {
    int start = 0;
    int end = 0;
    OpenLinkType type;
};

static const QRegularExpression &httplinkRE()
{
    static const QRegularExpression re(
        QStringLiteral(R"re((https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*)))re"));
    return re;
}

static void adjustMDLink(const QString &line, int capturedStart, int &capturedEnd)
{
    if (capturedStart > 1) { // at least two chars before
        int i = capturedStart - 1;
        // for markdown [asd](google.com) style urls, make sure to strip last `)`
        bool isMD = line.at(i - 1) == QLatin1Char(']') && line.at(i) == QLatin1Char('(');
        if (isMD) {
            int f = line.lastIndexOf(QLatin1Char(')'), capturedEnd >= line.size() ? line.size() - 1 : capturedEnd);
            capturedEnd = f != -1 ? f : capturedEnd;
        }
    }
}

static void matchFilePaths(const QString &line, std::vector<OpenLinkRange> *outColumnRanges)
{
#ifdef Q_OS_WIN
    const auto isValidDriveLetter = [](QChar letter) {
        return (letter.isLetter() && letter.toUpper() >= u'A' && letter.toUpper() <= u'Z');
    };

    int s = 0;
    while (true) {
        const int from = s;
        s = line.indexOf(u'\\', from);
        s = s != -1 ? s : line.indexOf(u'/', from);
        if (s < 0) {
            break;
        }

        const bool isAbsoloutePath = s > 2 && line[s - 1] == u':' && isValidDriveLetter(line[s - 2]);
        const bool isRelativePath = s > 1 && line[s - 1] == u'.';
        if (isAbsoloutePath || isRelativePath) {
            const bool isDotDotRelativePath = isRelativePath && s > 2 && line[s - 2] == u'.';
            // move s back to actual start position
            const int orignalS = s;
            s = isAbsoloutePath ? s - 2 : s - 1;
            s = isDotDotRelativePath ? s - 1 : s;

            // must be preceded by a space or d-quote
            if (s != 0 && line[s - 1] != u'"' && line[s - 1] != u' ') {
                s = orignalS + 1;
                continue;
            }

            const bool matchNextQuote = s > 3 && line[s - 3] == u'"'; // last char is quote?
            int e = -1;
            if (!matchNextQuote) {
                e = line.indexOf(QLatin1String(" "), s);
                e = e == -1 ? line.size() : e;
            } else {
                e = line.indexOf(u'"', s);
                if (e == -1) {
                    break;
                }
            }

            if (isRelativePath) {
                // TODO support relative paths
                s = e;
                continue;
            }

            if (e != -1 && QFileInfo(line.mid(s, e - s)).isFile()) {
                outColumnRanges->push_back({s, e, FileLink});
            }
            s = e;
            continue;
        } else {
            s++;
        }
        // try find relative path
    }
#else
    int s = 0;
    while (true) {
        s = line.indexOf(QLatin1Char('/'), s);
        if (s == -1) {
            break;
        }
        // must be preceded by a space or d-quote
        if (s != 0 && line[s - 1] != u'"' && line[s - 1] != u' ') {
            s++;
            continue;
        }

        const bool matchNextQuote = s > 0 && line[s - 1] == u'"'; // last char is quote?
        int e = -1;
        if (!matchNextQuote) {
            e = line.indexOf(QLatin1String(" "), s);
            e = e == -1 ? line.size() : e;
        } else {
            e = line.indexOf(u'"', s);
            if (e == -1) {
                break;
            }
        }

        if (e != -1 && QFileInfo(line.mid(s, e - s)).isFile()) {
            outColumnRanges->push_back({s, e, FileLink});
        }
        s = e;
    }
#endif
}

[[maybe_unused]] static void matchLine(const QString &line, std::vector<OpenLinkRange> *outColumnRanges)
{
    outColumnRanges->clear();
    if (line.contains(QLatin1String("http://")) || line.contains(QLatin1String("https://"))) {
        QRegularExpressionMatchIterator it = httplinkRE().globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            if (match.hasMatch()) {
                int capturedEnd = match.capturedEnd();
                adjustMDLink(line, match.capturedStart(), capturedEnd);
                outColumnRanges->push_back({.start = (int)match.capturedStart(), .end = capturedEnd, .type = HttpLink});
            }
        }
    }

    matchFilePaths(line, outColumnRanges);
}
