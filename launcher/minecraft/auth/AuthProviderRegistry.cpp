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

#include "minecraft/auth/AuthProviderRegistry.h"

#include <QDebug>

namespace Auth {

AuthProviderRegistry& AuthProviderRegistry::instance()
{
    static AuthProviderRegistry registry;
    return registry;
}

bool AuthProviderRegistry::registerProvider(AuthProvider provider)
{
    if (provider.id.isEmpty()) {
        qWarning() << "AuthProviderRegistry: refusing to register provider with empty id";
        return false;
    }
    if (find(provider.id) != nullptr) {
        qWarning() << "AuthProviderRegistry: provider already registered:" << provider.id;
        return false;
    }
    m_providers.push_back(std::make_unique<AuthProvider>(std::move(provider)));
    return true;
}

const AuthProvider* AuthProviderRegistry::find(const QString& id) const
{
    for (const auto& provider : m_providers) {
        if (provider->id == id) {
            return provider.get();
        }
    }
    return nullptr;
}

QList<const AuthProvider*> AuthProviderRegistry::providers() const
{
    QList<const AuthProvider*> result;
    result.reserve(static_cast<int>(m_providers.size()));
    for (const auto& provider : m_providers) {
        result.append(provider.get());
    }
    return result;
}

QList<const AuthProvider*> AuthProviderRegistry::providersWith(AuthCapabilityFlags required) const
{
    QList<const AuthProvider*> result;
    for (const auto& provider : m_providers) {
        if ((provider->capabilities & required) != required) {
            continue;
        }
        if (!provider->available()) {
            continue;
        }
        result.append(provider.get());
    }
    return result;
}

void AuthProviderRegistry::resetForTesting()
{
    m_providers.clear();
}

}  // namespace Auth
