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

#include "AccountCoordinator.h"

AccountCoordinator::AccountCoordinator(AccountService* accounts, QObject* parent)
    : QObject(parent), m_accounts(accounts)
{
}

AccountCoordinator::AddOutcome AccountCoordinator::addAccount(MinecraftAccountPtr account, bool forceDefault)
{
    if (!m_accounts || !account) {
        return AddOutcome::Rejected;
    }

    m_accounts->addAccount(account);

    const bool makeDefault = forceDefault || m_accounts->count() == 1;
    if (makeDefault) {
        m_accounts->setDefaultAccount(account);
        return AddOutcome::AddedAndSetDefault;
    }
    return AddOutcome::Added;
}

AccountCoordinator::AddOutcome AccountCoordinator::addOfflineAccount(MinecraftAccountPtr account)
{
    if (!m_accounts || !account) {
        return AddOutcome::Rejected;
    }
    if (!m_accounts->anyAccountIsValid()) {
        return AddOutcome::RejectedMissingMicrosoftAccount;
    }
    return addAccount(account, false);
}

void AccountCoordinator::refreshAccount(MinecraftAccountPtr account)
{
    if (!m_accounts || !account) {
        return;
    }
    m_accounts->requestRefresh(account->internalId());
}

void AccountCoordinator::setDefaultAccount(MinecraftAccountPtr account)
{
    if (!m_accounts) {
        return;
    }
    m_accounts->setDefaultAccount(account);
}

void AccountCoordinator::clearDefaultAccount()
{
    if (!m_accounts) {
        return;
    }
    m_accounts->setDefaultAccount(nullptr);
}

void AccountCoordinator::removeAccount(QModelIndex index)
{
    if (!m_accounts) {
        return;
    }
    m_accounts->removeAccount(index);
}

AccountCoordinator::AddOutcome AccountCoordinator::replaceAccount(MinecraftAccountPtr oldAccount, MinecraftAccountPtr newAccount)
{
    if (!m_accounts || !oldAccount || !newAccount) {
        return AddOutcome::Rejected;
    }

    const bool wasDefault = m_accounts->defaultAccount() == oldAccount;
    int row = -1;
    for (int i = 0; i < m_accounts->count(); ++i) {
        if (m_accounts->at(i) == oldAccount) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        row = m_accounts->findAccountByProfileId(oldAccount->profileId());
    }
    if (row < 0) {
        return AddOutcome::Rejected;
    }

    m_accounts->removeAccountAt(row);
    return addAccount(newAccount, wasDefault);
}

void AccountCoordinator::deliverOAuthReply(const QVariantMap& data)
{
    emit oauthReplyReceived(data);
}
