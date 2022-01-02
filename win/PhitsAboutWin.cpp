// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#include "PIUI.h"
#include <Windows.h>
#include <commctrl.h>
#include <sstream>

#include <string>
#include "../common/PhitsVersion.h"
#include "phits-sym.h"

class PIPhitsAboutBox : public PIDialog
{
public:
    PIPhitsAboutBox() : PIDialog(), m_notice() { }
    ~PIPhitsAboutBox() { }
private:

    PIText m_notice;
    void Init() override
    {
        PIDialogPtr dialog = GetDialog();
        const std::string str1("Phits " + std::string(PHITS_VERSION_STRING) + "\r\n\r\n");
        const std::string str2("A Photoshop plug-in for reading and writing FITS files.\r\n\r\n");
        const std::string str3("Copyright 2021, Craig E Kolb");

        PIItem item = PIGetDialogItem(dialog, IDC_STATIC1);
        m_notice.SetItem(item);
        m_notice.SetText((str1 + str2 + str3).c_str());
    }
    void Notify(int32 item) override
    {
        if (item == IDC_BUTTON1)
        {
            sPSFileList->BrowseUrl("https://github.com/cek/phits");
        }
    }
    virtual void Message(UINT wMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (wMsg)
        {
            case WM_CHAR:
            {
                TCHAR chCharCode = (TCHAR)wParam;
                if (chCharCode == VK_ESCAPE || chCharCode == VK_RETURN)
                EndDialog(GetDialog(), 0);
                break;
            }
            case WM_LBUTTONUP:
                EndDialog(GetDialog(), 0);
                break;
        }
    }

};

void DoAbout(SPPluginRef plugin_ref)
{
    PIPhitsAboutBox aboutBox;
    (void)aboutBox.Modal(plugin_ref, "About Phits", IDD_ABOUT);
}
