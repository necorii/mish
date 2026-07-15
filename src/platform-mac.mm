#ifdef __APPLE__

#include "platform.h"
#include <Cocoa/Cocoa.h>

std::string openFolderDialog()
{
    NSOpenPanel* panel = [NSOpenPanel openPanel];

    [panel setCanChooseDirectories:YES];
    [panel setCanChooseFiles:NO];
    [panel setAllowsMultipleSelection:NO];

    NSInteger result = [panel runModal];

    if(result == NSModalResponseOK)
    {
        NSURL* url = [[panel URLs] firstObject];

        return std::string(
            [[url path] UTF8String]
        );
    }

    return "";
}

#endif