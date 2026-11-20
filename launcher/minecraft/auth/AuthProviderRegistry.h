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

#pragma once

#include <QList>
#include <QString>

#include <memory>
#include <vector>

#include "minecraft/auth/AuthProvider.h"

namespace Auth {

/**
 * Process-wide static registry of authentication providers.
 *
 * Registration is compile-time / static-init only for this phase.
 * Dynamic plugin loading is intentionally not supported.
 *
 * Iteration order matches registration order.
 */
class AuthProviderRegistry {
   public:
    static AuthProviderRegistry& instance();

    AuthProviderRegistry(const AuthProviderRegistry&) = delete;
    AuthProviderRegistry& operator=(const AuthProviderRegistry&) = delete;

    /**
     * Register a provider descriptor.
     * \return false if \p provider.id is empty or already registered.
     */
    bool registerProvider(AuthProvider provider);

    /** Lookup by stable id, or nullptr. */
    const AuthProvider* find(const QString& id) const;

    /** All registered providers, in registration order. */
    QList<const AuthProvider*> providers() const;

    /**
     * Providers that advertise every flag in \p required and pass \ref AuthProvider::available.
     */
    QList<const AuthProvider*> providersWith(AuthCapabilityFlags required) const;

    /** Drop all registrations. Intended for unit tests only. */
    void resetForTesting();

   private:
    AuthProviderRegistry() = default;

    // unique_ptr keeps returned const AuthProvider* stable across appends.
    std::vector<std::unique_ptr<AuthProvider>> m_providers;
};

}  // namespace Auth

/**
 * Statically register an authentication provider.
 *
 * \param UniqueName  Unique token used only to form a static symbol (e.g. msa).
 * \param ProviderExpression  Expression yielding an Auth::AuthProvider.
 *
 * Example:
 * \code
 * REGISTER_AUTH_PROVIDER(msa, [] {
 *     Auth::AuthProvider provider;
 *     provider.id = QStringLiteral("msa");
 *     provider.displayName = QStringLiteral("Microsoft");
 *     provider.capabilities = Auth::AuthCapability::InteractiveLogin |
 *                             Auth::AuthCapability::SilentRefresh |
 *                             Auth::AuthCapability::Persistence |
 *                             Auth::AuthCapability::AccountCreationUI;
 *     return provider;
 * }());
 * \endcode
 */
#define REGISTER_AUTH_PROVIDER(UniqueName, ProviderExpression) \
    static const bool registered_auth_provider_##UniqueName =  \
        ::Auth::AuthProviderRegistry::instance().registerProvider(ProviderExpression)
