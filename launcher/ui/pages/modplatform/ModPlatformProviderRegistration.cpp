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
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "modplatform/ModPlatformRegistry.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"

#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/pages/modplatform/atlauncher/AtlPage.h"
#include "ui/pages/modplatform/flame/FlamePage.h"
#include "ui/pages/modplatform/flame/FlameResourcePages.h"
#include "ui/pages/modplatform/ftb/FtbPage.h"
#include "ui/pages/modplatform/import_ftb/ImportFTBPage.h"
#include "ui/pages/modplatform/legacy_ftb/Page.h"
#include "ui/pages/modplatform/modrinth/ModrinthPage.h"
#include "ui/pages/modplatform/modrinth/ModrinthResourcePages.h"
#include "ui/pages/modplatform/technic/TechnicPage.h"

#include <memory>

namespace {

ModPlatform::ModLoaderTypes supportedLoaders(BaseInstance& instance)
{
    auto* minecraft = dynamic_cast<MinecraftInstance*>(&instance);
    if (!minecraft) {
        return ModPlatform::ModLoaderTypes(0);
    }
    return minecraft->getPackProfile()->getSupportedModLoaders().value_or(ModPlatform::ModLoaderTypes(0));
}

}  // namespace

// Registration order matches the historical NewInstanceDialog page order
// (after Custom / Import, which remain special-cased).
REGISTER_MOD_PLATFORM(atl, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("atl");
    provider.displayName = QStringLiteral("ATLauncher");
    provider.iconName = QStringLiteral("atlauncher");
    provider.cacheKey = QStringLiteral("ATLauncherPacks");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage;
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new AtlPage(dialog); };
    return provider;
}());

REGISTER_MOD_PLATFORM(flame, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("flame");
    provider.displayName = QStringLiteral("CurseForge");
    provider.iconName = QStringLiteral("flame");
    provider.cacheKey = QStringLiteral("FlameMods");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage | ModPlatform::ProviderCapability::ResourceMods |
                            ModPlatform::ProviderCapability::ResourcePacks | ModPlatform::ProviderCapability::TexturePacks |
                            ModPlatform::ProviderCapability::ShaderPacks | ModPlatform::ProviderCapability::DataPacks |
                            ModPlatform::ProviderCapability::HasResourceAPI | ModPlatform::ProviderCapability::RequiresApiKey;
    provider.isAvailable = [] { return (APPLICATION->capabilities() & Application::SupportsFlame) != 0U; };
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new FlamePage(dialog); };
    provider.createResourceAPI = [] { return std::unique_ptr<ResourceAPI>(std::make_unique<FlameAPI>()); };
    provider.createModPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        if (!FlameAPI::validateModLoaders(supportedLoaders(instance))) {
            return nullptr;
        }
        return ResourceDownload::Flame::createModPage(dialog, instance);
    };
    provider.createResourcePackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Flame::createResourcePackResourcePage(dialog, instance);
    };
    provider.createTexturePackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Flame::createTexturePackResourcePage(dialog, instance);
    };
    provider.createShaderPackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Flame::createShaderPackResourcePage(dialog, instance);
    };
    provider.createDataPackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Flame::createDataPackResourcePage(dialog, instance);
    };
    return provider;
}());

REGISTER_MOD_PLATFORM(ftb, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("ftb");
    provider.displayName = QStringLiteral("FTB");
    provider.iconName = QStringLiteral("ftb_logo");
    provider.cacheKey = QStringLiteral("FTBPacks");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage;
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new FtbPage(dialog); };
    return provider;
}());

REGISTER_MOD_PLATFORM(legacy_ftb, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("legacy_ftb");
    provider.displayName = QStringLiteral("FTB Legacy");
    provider.iconName = QStringLiteral("ftb_logo");
    provider.cacheKey = QStringLiteral("LegacyFTBPacks");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage;
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new LegacyFTB::Page(dialog); };
    return provider;
}());

REGISTER_MOD_PLATFORM(import_ftb, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("import_ftb");
    provider.displayName = QStringLiteral("FTB App Import");
    provider.iconName = QStringLiteral("ftb_logo");
    provider.cacheKey = QStringLiteral("ImportFTBPacks");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage;
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new FTBImportAPP::ImportFTBPage(dialog); };
    return provider;
}());

REGISTER_MOD_PLATFORM(modrinth, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("modrinth");
    provider.displayName = QStringLiteral("Modrinth");
    provider.iconName = QStringLiteral("modrinth");
    provider.cacheKey = QStringLiteral("ModrinthPacks");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage | ModPlatform::ProviderCapability::ResourceMods |
                            ModPlatform::ProviderCapability::ResourcePacks | ModPlatform::ProviderCapability::TexturePacks |
                            ModPlatform::ProviderCapability::ShaderPacks | ModPlatform::ProviderCapability::DataPacks |
                            ModPlatform::ProviderCapability::HasResourceAPI;
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new ModrinthPage(dialog); };
    provider.createResourceAPI = [] { return std::unique_ptr<ResourceAPI>(std::make_unique<ModrinthAPI>()); };
    provider.createModPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        if (!ModrinthAPI::validateModLoaders(supportedLoaders(instance))) {
            return nullptr;
        }
        return ResourceDownload::Modrinth::createModPage(dialog, instance);
    };
    provider.createResourcePackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Modrinth::createResourcePackResourcePage(dialog, instance);
    };
    provider.createTexturePackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Modrinth::createTexturePackResourcePage(dialog, instance);
    };
    provider.createShaderPackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Modrinth::createShaderPackResourcePage(dialog, instance);
    };
    provider.createDataPackPage = [](ResourceDownload::ResourceDownloadDialog* dialog, BaseInstance& instance) -> BasePage* {
        return ResourceDownload::Modrinth::createDataPackResourcePage(dialog, instance);
    };
    return provider;
}());

REGISTER_MOD_PLATFORM(technic, [] {
    ModPlatform::ModPlatformProvider provider;
    provider.id = QStringLiteral("technic");
    provider.displayName = QStringLiteral("Technic");
    provider.iconName = QStringLiteral("technic");
    provider.cacheKey = QStringLiteral("TechnicPacks");
    provider.capabilities = ModPlatform::ProviderCapability::NewInstancePage;
    provider.createNewInstancePage = [](NewInstanceDialog* dialog) -> BasePage* { return new TechnicPage(dialog); };
    return provider;
}());
