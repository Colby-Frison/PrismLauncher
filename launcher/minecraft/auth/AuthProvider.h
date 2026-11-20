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

#include <QFlags>
#include <QString>
#include <cstdint>
#include <functional>

namespace Auth {

/**
 * Capability flags describing what a registered authentication provider can do.
 *
 * Account UI and auth workflow code should query these instead of hard-coding
 * account-type branches when deciding which login options to offer.
 */
enum class AuthCapability : std::uint32_t {
    None = 0U,

    /** Interactive browser / device-code login UI (MSA). */
    InteractiveLogin = 1U << 0U,

    /** Token refresh without user interaction. */
    SilentRefresh = 1U << 1U,

    /** Offline play / offline account creation. */
    OfflineSession = 1U << 2U,

    /** Save/load account records. */
    Persistence = 1U << 3U,

    /** Contributes account-list / wizard UI entry points. */
    AccountCreationUI = 1U << 4U,
};
Q_DECLARE_FLAGS(AuthCapabilityFlags, AuthCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(AuthCapabilityFlags)

/**
 * Descriptor for a statically registered authentication provider.
 */
struct AuthProvider {
    /** Stable serialized identifier (e.g. "msa", "offline"). */
    QString id;

    /** Human-readable name shown in the UI. */
    QString displayName;

    AuthCapabilityFlags capabilities = AuthCapability::None;

    /**
     * Optional runtime gate. When unset, the provider is treated as available.
     * Use for SupportsMSA-style checks that are not compile-time.
     */
    std::function<bool()> isAvailable;

    bool hasCapability(AuthCapability capability) const { return capabilities.testFlag(capability); }

    bool available() const { return !isAvailable || isAvailable(); }
};

}  // namespace Auth

// Static registration: see REGISTER_AUTH_PROVIDER in minecraft/auth/AuthProviderRegistry.h
