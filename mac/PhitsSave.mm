// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#include <Cocoa/Cocoa.h>
#include "AppKit/NSAlert.h"
#include "AppKit/NSWindow.h"
#include "PIUI.h"
#include "../common/PhitsMetadata.h"
#include <vector>
#include <string>

using namespace std;

bool DoSaveWarn(SPPluginRef pluginRef, const string& warnString)
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"Cancel"];
    [alert addButtonWithTitle:@"Save"];
    [alert setMessageText:@"Warning"];
    NSArray* buttons = [alert buttons];
    [[buttons objectAtIndex:0] setKeyEquivalent: @"\r"];
    [[buttons objectAtIndex:1] setKeyEquivalent: @""];

    NSString* informText = [NSString stringWithUTF8String:warnString.c_str()];
    [alert setInformativeText:informText];
    [alert setAlertStyle:NSAlertStyleWarning];
    int buttonPressed = [alert runModal];
    [alert release];
    return buttonPressed == NSAlertSecondButtonReturn;
}
