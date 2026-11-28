// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"

namespace sms
{

/** All modules compiled into the application, in initialisation order.
    To add a feature, create a Module and append it here. */
std::vector<std::unique_ptr<Module>> createBuiltInModules();

// individual factories (one per module source file)
std::unique_ptr<Module> createCoreModule();
std::unique_ptr<Module> createTransportModule();
std::unique_ptr<Module> createBrowserModule();
std::unique_ptr<Module> createPanelsModule();
std::unique_ptr<Module> createMidiModule();
std::unique_ptr<Module> createSettingsModule();

} // namespace sms
