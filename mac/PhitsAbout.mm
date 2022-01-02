// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#include <Cocoa/Cocoa.h>
#include "AppKit/NSAlert.h"
#include "AppKit/NSWindow.h"
#include "PIUI.h"
#include "../common/PhitsVersion.h"

void DoAbout(SPPluginRef plugin_ref)
{
    const NSAlert* alert = [[NSAlert alloc] init];

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"github.com/cek/phits"];

    [alert setMessageText:@"About Phits"];
    std::string str("Phits " + std::string(PHITS_VERSION_STRING) + "\nA Photoshop plug-in for reading and writing FITS files.\nCopyright 2021 Craig E Kolb");
    NSString* message = [NSString stringWithUTF8String:str.c_str()];
    [alert setInformativeText:message];
    [alert setAlertStyle:NSAlertStyleInformational];

    NSModalResponse buttonPressed = [alert runModal];
    if (buttonPressed == NSAlertSecondButtonReturn)
    {
        sPSFileList->BrowseUrl("https://github.com/cek/phits");
    }
    [alert release];
}
