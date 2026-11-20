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

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

enum class WorkPriority { Foreground, Background };

struct SchedulerLimits {
    int globalMax = 6;                  // values < 1 are treated as 1
    int perHostMax = 3;                 // values < 1 are treated as 1
    QHash<QString, int> hostOverrides;  // host key -> per-host cap, replaces perHostMax for that host
};

class WorkScheduler : public QObject {
    Q_OBJECT
   public:
    using Ticket = quint64;  // 0 is never a valid ticket

    explicit WorkScheduler(SchedulerLimits limits, QObject* parent = nullptr);

    // Lowercases the host of the URL. Returns "" for an invalid URL or no host.
    static QString hostKey(const QUrl& url);

    // Asks for a slot. If the global cap and the host's cap both have room,
    // the ticket is granted at once (isGranted() is true and granted() is NOT
    // emitted). Otherwise it is queued.
    Ticket request(const QString& host, WorkPriority priority);

    // Frees a granted slot, then grants as many queued tickets as now fit,
    // emitting granted() once for each. Returns false (and changes nothing)
    // for an unknown, queued, or already-released ticket.
    bool release(Ticket ticket);

    // Removes a queued ticket so it is never granted. Returns false for a
    // granted, unknown, or already-cancelled ticket.
    bool cancel(Ticket ticket);

    // Applies new limits. Running tickets are never revoked. If the new limits
    // leave room, queued tickets are granted at once and granted() is emitted.
    void setLimits(SchedulerLimits limits);

    bool isGranted(Ticket ticket) const;
    int runningCount() const;
    int runningCount(const QString& host) const;
    int queuedCount() const;

   signals:
    void granted(WorkScheduler::Ticket ticket);  // only for tickets granted from the queue

   private:
    struct Entry {
        QString host;
        WorkPriority priority = WorkPriority::Background;
        bool granted = false;
    };

    static SchedulerLimits clampLimits(SchedulerLimits limits);
    int hostCap(const QString& host) const;
    bool canFit(const QString& host) const;
    void markGranted(Ticket ticket);
    void tryGrantQueued();

    SchedulerLimits m_limits;
    QHash<Ticket, Entry> m_entries;
    QList<Ticket> m_foregroundQueue;
    QList<Ticket> m_backgroundQueue;
    QHash<QString, int> m_runningByHost;
    int m_runningCount = 0;
    Ticket m_nextTicket = 1;
};
