// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  R3 CONTRACT DOCUMENTATION (test-side only — not application code)
 *
 *  This header documents the interfaces that AuthProviderRegistry_test.cpp and
 *  R3RegistryIntegration_test.cpp assume. Implementers should place matching
 *  production headers under launcher/ and then enable the gated CMake tests.
 *
 *  Do not #include this file from production code.
 *
 * ---------------------------------------------------------------------------
 * ASSUMED: launcher/minecraft/auth/AuthProviderRegistry.h (+ .cpp)
 * ---------------------------------------------------------------------------
 *
 * namespace Auth {
 *
 * enum class AuthCapability : std::uint32_t {
 *     None = 0U,
 *     InteractiveLogin = 1U << 0U,   // MSA browser / device-code login UI
 *     SilentRefresh    = 1U << 1U,   // token refresh without user interaction
 *     OfflineSession   = 1U << 2U,   // offline play / offline account creation
 *     Persistence      = 1U << 3U,   // save/load account records
 *     AccountCreationUI = 1U << 4U,  // contributes account-list / wizard UI metadata
 * };
 * Q_DECLARE_FLAGS(AuthCapabilityFlags, AuthCapability)
 *
 * struct AuthProvider {
 *     QString id;              // stable ids: "msa", "offline"
 *     QString displayName;
 *     AuthCapabilityFlags capabilities = AuthCapability::None;
 *     std::function<bool()> isAvailable;  // optional; unset => available
 *
 *     bool hasCapability(AuthCapability c) const;
 *     bool available() const;             // !isAvailable || isAvailable()
 * };
 *
 * class AuthProviderRegistry {
 * public:
 *     static AuthProviderRegistry& instance();
 *     bool registerProvider(AuthProvider provider); // false if empty/duplicate id
 *     const AuthProvider* find(const QString& id) const; // nullptr if missing
 *     QList<const AuthProvider*> providers() const;
 *     QList<const AuthProvider*> providersWith(AuthCapabilityFlags required) const;
 *     void resetForTesting();
 * };
 *
 * #define REGISTER_AUTH_PROVIDER(UniqueName, ProviderExpression) \
 *     static const bool registered_auth_provider_##UniqueName = \
 *         ::Auth::AuthProviderRegistry::instance().registerProvider(ProviderExpression)
 *
 * }  // namespace Auth
 *
 * Production static registration (issue 3 completion) is expected to register at
 * least:
 *   - id "msa"     with InteractiveLogin | SilentRefresh | Persistence | AccountCreationUI
 *   - id "offline" with OfflineSession | Persistence | AccountCreationUI
 *
 * ---------------------------------------------------------------------------
 * ASSUMED: UI discovery (issue 3 completion)
 * ---------------------------------------------------------------------------
 *
 * NewInstanceDialog::getPages() must obtain mod-platform pages by iterating
 * ModPlatform::ModPlatformRegistry::instance().providersWith(
 *     ModPlatform::ProviderCapability::NewInstancePage)
 * (plus any non-provider pages such as Custom/Import that remain local), instead of
 * constructing FlamePage / ModrinthPage / … by hard-coded type.
 *
 * Resource-download entry points (Flame/Modrinth mod/resource/shader/data pack flows)
 * must select providers via
 * ModPlatform::ModPlatformRegistry::instance().providersWith(
 *     ModPlatform::capabilityForResourceType(type))
 * (and HasResourceAPI where an API is required), not via hard-coded provider switches.
 *
 * Auth account UI / AuthFlow must resolve providers via Auth::AuthProviderRegistry
 * (find("msa") / find("offline") / providersWith(...)), not only AccountType switches.
 *
 * Registration remains static (REGISTER_MOD_PLATFORM / REGISTER_AUTH_PROVIDER). No
 * dynamic plugin loader API is required for R3.
 */

#pragma once
// Intentionally empty of declarations — documentation header only.
