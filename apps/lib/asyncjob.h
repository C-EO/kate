/*
    SPDX-FileCopyrightText: 2026 Mark Nauwelaerts <mark.nauwelaerts@gmail.com>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include "kateprivate_export.h"

#include <QObject>
#include <QThreadPool>

#include <atomic>
#include <functional>

namespace Utils
{

/* The following is indeed much like std::stop_token and std::stop_source,
 * but in case of clang libc++ that appears to require -fexperimental-library,
 * so let's use a home-made construct instead that just covers what is needed.
 */

class stop_token
{
public:
    stop_token() noexcept = default;

    bool stop_requested() const
    {
        return m_stop && m_stop->test();
    }

    using stop_state = std::shared_ptr<std::atomic_flag>;

private:
    friend class stop_source;
    explicit stop_token(const stop_state &s) noexcept
        : m_stop(s)
    {
    }

    stop_state m_stop;
};

class stop_source
{
    stop_token::stop_state m_stop;

public:
    stop_source()
        : m_stop(std::make_shared<std::atomic_flag>())
    {
        m_stop->clear();
    }

    void request_stop() noexcept
    {
        m_stop->test_and_set();
    }

    bool stop_requested() const noexcept
    {
        return m_stop && m_stop->test();
    }

    stop_token get_token() const noexcept
    {
        return stop_token{m_stop};
    }
};

using JobFunction = std::function<void(const stop_token &)>;
using JobResult = std::function<void(bool cancel)>;

/* Runs func in a worker thread of tp, which can be requested to abort/stop using the return source
 * (though it obviously depends on func how it actually responds to that).
 * When finished, cb is called in mainloop thread, where cancel argument is false iff context is gone or stop requested.
 */
KATE_PRIVATE_EXPORT stop_source runAsyncJob(QThreadPool &tp, JobFunction func, const QObject *context, JobResult cb);
}
