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

#include "Application.h"
#include "minecraft/auth/AuthProviderRegistry.h"

// Static registration of built-in auth providers (R3.4).
REGISTER_AUTH_PROVIDER(msa, [] {
    Auth::AuthProvider provider;
    provider.id = QStringLiteral("msa");
    provider.displayName = QStringLiteral("Microsoft");
    provider.capabilities = Auth::AuthCapability::InteractiveLogin | Auth::AuthCapability::SilentRefresh |
                            Auth::AuthCapability::Persistence | Auth::AuthCapability::AccountCreationUI;
    provider.isAvailable = [] { return (APPLICATION->capabilities() & Application::SupportsMSA) != 0U; };
    return provider;
}());

REGISTER_AUTH_PROVIDER(offline, [] {
    Auth::AuthProvider provider;
    provider.id = QStringLiteral("offline");
    provider.displayName = QStringLiteral("Offline");
    provider.capabilities =
        Auth::AuthCapability::OfflineSession | Auth::AuthCapability::Persistence | Auth::AuthCapability::AccountCreationUI;
    return provider;
}());
