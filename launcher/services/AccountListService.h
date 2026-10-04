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

#include "services/AccountService.h"

/**
 * AccountService wrapper over AccountList. Does not take ownership.
 */
class AccountListService : public AccountService {
   public:
    explicit AccountListService(AccountList* accounts);
    ~AccountListService() override = default;

    int count() const override;
    MinecraftAccountPtr at(int index) const override;

    void addAccount(MinecraftAccountPtr account) override;
    void removeAccount(QModelIndex index) override;

    int findAccountByProfileId(const QString& profileId) const override;
    MinecraftAccountPtr getAccountByProfileName(const QString& profileName) const override;
    QStringList profileNames() const override;

    MinecraftAccountPtr defaultAccount() const override;
    void setDefaultAccount(MinecraftAccountPtr account) override;
    bool anyAccountIsValid() override;

    void requestRefresh(QString accountId) override;
    void queueRefresh(QString accountId) override;

    AccountList* model() override;

   private:
    AccountList* m_accounts = nullptr;
};
