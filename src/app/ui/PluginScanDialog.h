// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"

namespace sms
{

/** Starts the plug-in scan and shows a modal window with a progress bar and a
    cancel button. The rest of the application is blocked until the scan has
    finished or was cancelled. */
void startPluginScanWithProgress (AppContext&);

} // namespace sms
