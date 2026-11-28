// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"

namespace sms
{

/** Starts the plug-in scan and shows a modal window with a progress bar and a
    cancel button. The rest of the application is blocked until the scan has
    finished or was cancelled. */
/** Starts the plug-in scan with a modal progress window.
    full = true: check every plug-in again ("Plug-ins suchen"),
    full = false: only new or changed ones ("Aktualisieren"). */
void startPluginScanWithProgress (AppContext&, bool full = false);

} // namespace sms
