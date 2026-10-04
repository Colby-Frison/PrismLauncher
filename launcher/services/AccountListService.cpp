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

#include "services/AccountListService.h"

#include "minecraft/auth/AccountList.h"

AccountListService::AccountListService(AccountList* accounts) : m_accounts(accounts) {}

int AccountListService::count() const
{
    return m_accounts->count();
}

MinecraftAccountPtr AccountListService::at(int index) const
{
    return m_accounts->at(index);
}

void AccountListService::addAccount(MinecraftAccountPtr account)
{
    m_accounts->addAccount(account);
}

void AccountListService::removeAccount(QModelIndex index)
{
    m_accounts->removeAccount(index);
}

int AccountListService::findAccountByProfileId(const QString& profileId) const
{
    return m_accounts->findAccountByProfileId(profileId);
}

MinecraftAccountPtr AccountListService::getAccountByProfileName(const QString& profileName) const
{
    return m_accounts->getAccountByProfileName(profileName);
}

QStringList AccountListService::profileNames() const
{
    return m_accounts->profileNames();
}

MinecraftAccountPtr AccountListService::defaultAccount() const
{
    return m_accounts->defaultAccount();
}

void AccountListService::setDefaultAccount(MinecraftAccountPtr account)
{
    m_accounts->setDefaultAccount(account);
}

bool AccountListService::anyAccountIsValid()
{
    return m_accounts->anyAccountIsValid();
}

void AccountListService::requestRefresh(QString accountId)
{
    m_accounts->requestRefresh(accountId);
}

void AccountListService::queueRefresh(QString accountId)
{
    m_accounts->queueRefresh(accountId);
}

AccountList* AccountListService::model()
{
    return m_accounts;
}
