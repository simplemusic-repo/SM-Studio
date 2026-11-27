// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Modules.h"

namespace sms
{

std::vector<std::unique_ptr<Module>> createBuiltInModules()
{
    std::vector<std::unique_ptr<Module>> m;
    m.push_back (createCoreModule());      // track types, file/edit/track actions
    m.push_back (createTransportModule()); // play/stop/metronome actions
    m.push_back (createBrowserModule());   // browser tabs + drop handlers
    m.push_back (createPanelsModule());    // inspector, browser, console panels
    return m;
}

} // namespace sms
