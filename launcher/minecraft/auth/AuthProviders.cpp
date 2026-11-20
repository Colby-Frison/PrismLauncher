// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
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

#include "minecraft/auth/AuthProviders.h"

#include "Application.h"
#include "minecraft/auth/AuthProviderRegistry.h"

namespace {

void registerIfMissing(Auth::AuthProvider provider)
{
    auto& registry = Auth::AuthProviderRegistry::instance();
    if (registry.find(provider.id) != nullptr) {
        return;
    }
    registry.registerProvider(std::move(provider));
}

}  // namespace

namespace Auth {

void ensureBuiltInProvidersRegistered()
{
    {
        AuthProvider provider;
        provider.id = QStringLiteral("msa");
        provider.displayName = QStringLiteral("Microsoft");
        provider.capabilities = AuthCapability::InteractiveLogin | AuthCapability::SilentRefresh | AuthCapability::Persistence |
                                AuthCapability::AccountCreationUI;
        provider.isAvailable = [] {
            return APPLICATION != nullptr && (APPLICATION->capabilities() & Application::SupportsMSA) != 0U;
        };
        registerIfMissing(std::move(provider));
    }
    {
        AuthProvider provider;
        provider.id = QStringLiteral("offline");
        provider.displayName = QStringLiteral("Offline");
        provider.capabilities = AuthCapability::OfflineSession | AuthCapability::Persistence | AuthCapability::AccountCreationUI;
        registerIfMissing(std::move(provider));
    }
}

}  // namespace Auth

// Static registration entry: runs when this TU is linked into the process.
static const bool registered_auth_providers = [] {
    Auth::ensureBuiltInProvidersRegistered();
    return true;
}();
