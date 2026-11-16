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

#include <QObject>
#include <QVariantMap>

#include "minecraft/auth/MinecraftAccount.h"
#include "services/AccountService.h"

/**
 * Owns account add / refresh / default / OAuth-callback sequencing without UI.
 *
 * UI gathers credentials (login dialogs, offline name) and delegates mutation
 * and policy here. Compatible with R1 AccountService injection.
 */
class AccountCoordinator : public QObject {
    Q_OBJECT
   public:
    enum class AddOutcome {
        Added,
        AddedAndSetDefault,
        Rejected,
        RejectedMissingMicrosoftAccount,
    };

    explicit AccountCoordinator(AccountService* accounts, QObject* parent = nullptr);

    /**
     * Add an already-authenticated account (e.g. after MSA login dialog).
     * Sets it as default when it becomes the only account, or when forceDefault.
     */
    AddOutcome addAccount(MinecraftAccountPtr account, bool forceDefault = false);

    /**
     * Add an offline account after the UI created it. Requires a valid Microsoft
     * account that owns Minecraft (same gate as the accounts page).
     */
    AddOutcome addOfflineAccount(MinecraftAccountPtr account);

    void refreshAccount(MinecraftAccountPtr account);
    void setDefaultAccount(MinecraftAccountPtr account);
    void clearDefaultAccount();
    void removeAccount(QModelIndex index);

    /**
     * Replace an existing account after re-authentication: remove the old entry
     * by profile id, add the new one, and restore default when the old was default.
     */
    AddOutcome replaceAccount(MinecraftAccountPtr oldAccount, MinecraftAccountPtr newAccount);

    /**
     * Deliver an OAuth callback payload (custom URL scheme) to listeners such as
     * MSA auth steps. Replaces embedding that emit in MainWindow URL handling.
     */
    void deliverOAuthReply(const QVariantMap& data);

   signals:
    void oauthReplyReceived(QVariantMap data);

   private:
    AccountService* m_accounts = nullptr;
};
