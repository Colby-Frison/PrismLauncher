// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Prism Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "net/WorkScheduler.h"

WorkScheduler::WorkScheduler(SchedulerLimits limits, QObject* parent) : QObject(parent), m_limits(clampLimits(limits)) {}

QString WorkScheduler::hostKey(const QUrl& url)
{
    if (!url.isValid())
        return {};
    return url.host().toLower();
}

WorkScheduler::Ticket WorkScheduler::request(const QString& host, WorkPriority priority)
{
    const Ticket ticket = m_nextTicket++;
    Entry entry;
    entry.host = host;
    entry.priority = priority;

    m_entries.insert(ticket, entry);
    if (canFit(host)) {
        markGranted(ticket);
        return ticket;
    }

    if (priority == WorkPriority::Foreground)
        m_foregroundQueue.append(ticket);
    else
        m_backgroundQueue.append(ticket);
    return ticket;
}

bool WorkScheduler::release(Ticket ticket)
{
    auto it = m_entries.find(ticket);
    if (it == m_entries.end() || !it->granted)
        return false;

    const QString host = it->host;
    m_entries.erase(it);

    --m_runningCount;
    auto hostIt = m_runningByHost.find(host);
    if (hostIt != m_runningByHost.end()) {
        --(*hostIt);
        if (*hostIt <= 0)
            m_runningByHost.erase(hostIt);
    }

    tryGrantQueued();
    return true;
}

bool WorkScheduler::cancel(Ticket ticket)
{
    auto it = m_entries.find(ticket);
    if (it == m_entries.end() || it->granted)
        return false;

    QList<Ticket>& queue = (it->priority == WorkPriority::Foreground) ? m_foregroundQueue : m_backgroundQueue;
    queue.removeOne(ticket);
    m_entries.erase(it);
    return true;
}

void WorkScheduler::setLimits(SchedulerLimits limits)
{
    m_limits = clampLimits(limits);
    tryGrantQueued();
}

bool WorkScheduler::isGranted(Ticket ticket) const
{
    auto it = m_entries.constFind(ticket);
    return it != m_entries.cend() && it->granted;
}

int WorkScheduler::runningCount() const
{
    return m_runningCount;
}

int WorkScheduler::runningCount(const QString& host) const
{
    return m_runningByHost.value(host, 0);
}

int WorkScheduler::queuedCount() const
{
    return m_foregroundQueue.size() + m_backgroundQueue.size();
}

SchedulerLimits WorkScheduler::clampLimits(SchedulerLimits limits)
{
    if (limits.globalMax < 1)
        limits.globalMax = 1;
    if (limits.perHostMax < 1)
        limits.perHostMax = 1;
    for (auto it = limits.hostOverrides.begin(); it != limits.hostOverrides.end(); ++it) {
        if (it.value() < 1)
            it.value() = 1;
    }
    return limits;
}

int WorkScheduler::hostCap(const QString& host) const
{
    auto it = m_limits.hostOverrides.constFind(host);
    if (it != m_limits.hostOverrides.cend())
        return it.value();
    return m_limits.perHostMax;
}

bool WorkScheduler::canFit(const QString& host) const
{
    if (m_runningCount >= m_limits.globalMax)
        return false;
    if (m_runningByHost.value(host, 0) >= hostCap(host))
        return false;
    return true;
}

void WorkScheduler::markGranted(Ticket ticket)
{
    auto it = m_entries.find(ticket);
    Q_ASSERT(it != m_entries.end());
    it->granted = true;
    ++m_runningCount;
    ++m_runningByHost[it->host];
}

void WorkScheduler::tryGrantQueued()
{
    auto drain = [this](QList<Ticket>& queue) {
        for (int i = 0; i < queue.size();) {
            const Ticket ticket = queue.at(i);
            const QString& host = m_entries.value(ticket).host;
            if (!canFit(host)) {
                ++i;
                continue;
            }
            queue.removeAt(i);
            markGranted(ticket);
            emit granted(ticket);
        }
    };

    drain(m_foregroundQueue);
    drain(m_backgroundQueue);
}
