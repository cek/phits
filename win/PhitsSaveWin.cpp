// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2022, Craig Kolb
 * Licensed under the Apache License, Version 2.0
 */
#include "PIUI.h"
#include <Windows.h>
#include <commctrl.h>
#include <sstream>
#include <vector>

#include <string>

#include "phits-sym.h"

using namespace std;

class PIPhitsSaveWarnBox : public PIDialog
{
public:
    PIPhitsSaveWarnBox(const string& warnStr) : PIDialog(), m_warnStr(warnStr) { }
    ~PIPhitsSaveWarnBox() { }
private:

    PIText m_notice;
    string m_warnStr;

    void Init() override
    {
        PIDialogPtr dialog = GetDialog();

        PIItem item = PIGetDialogItem(dialog, IDC_STATIC2);
        m_notice.SetItem(item);
        m_notice.SetText(m_warnStr.c_str());

        // FIXME: Why does specifying IDSAVE make the IDNOSAVE button the default?
        SetDialogDefaultItem(dialog, IDSAVE);
    }

    void Notify(int32 item) override
    {
        if (item == IDSAVE || item == IDNOSAVE)
        {
            EndDialog(GetDialog(), item);
        }
    }

    void Message(UINT wMsg, WPARAM wParam, LPARAM lParam) override
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

bool DoSaveWarn(SPPluginRef plugin_ref, const string& warnString)
{
    PIPhitsSaveWarnBox warnBox(warnString);
    int result = warnBox.Modal(plugin_ref, "About Phits", IDD_SAVEWARN);
    return result == IDSAVE;
}
