///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/tooltip.cpp
// Purpose:     wxToolTip class implementation for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     31.01.99
// Copyright:   (c) 1999 Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"


#if wxUSE_TOOLTIPS

#include "wx/tooltip.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/app.h"
    #include "wx/control.h"
    #include "wx/module.h"
    #include "wx/toplevel.h"
#endif

#include "wx/tokenzr.h"
#include "wx/vector.h"
#include "wx/msw/private.h"

// Bricsys added
#include "wx/dynlib.h"

#include <windowsx.h>
// End Bricsys added

#ifndef TTTOOLINFO_V1_SIZE
    #define TTTOOLINFO_V1_SIZE 0x28
#endif

#ifndef TTF_TRANSPARENT
    #define TTF_TRANSPARENT 0x0100
#endif

// VZ: normally, the trick with subclassing the tooltip control and processing
//     TTM_WINDOWFROMPOINT should work but, somehow, it doesn't. I leave the
//     code here for now (but it's not compiled) in case we need it later.
//
//     For now I use an ugly workaround and process TTN_NEEDTEXT directly in
//     radio button wnd proc - fixing TTM_WINDOWFROMPOINT code would be nice
//     because it would then work for all controls, not only radioboxes but for
//     now I don't understand what's wrong with it...
#define wxUSE_TTM_WINDOWFROMPOINT   0

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

// the tooltip parent window
WXHWND wxToolTip::ms_hwndTT = (WXHWND)NULL;

// new tooltip maximum width, default value is set on first call to wxToolTip::Add()
int wxToolTip::ms_maxWidth = 0;

#if wxUSE_TTM_WINDOWFROMPOINT

// the tooltip window proc
static WNDPROC gs_wndprocToolTip = (WNDPROC)NULL;

#endif // wxUSE_TTM_WINDOWFROMPOINT

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// This is simply a wrapper for vector<HWND> but defined as a class to hide the
// details from the public header.
class wxToolTipOtherWindows : public wxVector<WXHWND>
{
};

// a wrapper around TOOLINFO Win32 structure
#ifdef __VISUALC__
    #pragma warning( disable : 4097 ) // we inherit from a typedef - so what?
#endif

class wxToolInfo : public TOOLINFO
{
public:
    wxToolInfo(HWND hwndOwner, unsigned int id, const wxRect& rc)
    {
        // initialize all members
        wxZeroMemory(*this);

        // the structure TOOLINFO has been extended with a 4 byte field in
        // version 4.70 of comctl32.dll and another one in 5.01 but we don't
        // use these extended fields so use the old struct size to ensure that
        // the tooltips work on old (Windows 95) systems too
        cbSize = TTTOOLINFO_V1_SIZE;

        hwnd = hwndOwner;

        if (rc.IsEmpty())
        {
            uFlags = TTF_IDISHWND;
            uId = (UINT_PTR)hwndOwner;
        }
        else
        {
            // this tooltip must be shown only if the mouse hovers a specific rect
            // of the hwnd parameter!
            rect.left = rc.GetLeft();
            rect.right = rc.GetRight();
            rect.top = rc.GetTop();
            rect.bottom = rc.GetBottom();

            // note that not setting TTF_IDISHWND from the uFlags member means that the
            // ti.uId field should not contain the HWND but rather as MSDN says an
            // "Application-defined identifier of the tool"; this is used internally by
            // Windows to distinguish the different tooltips attached to the same window
            uId = id;
        }

        // we use TTF_TRANSPARENT to fix a problem which arises at least with
        // the text controls but may presumably happen with other controls
        // which display the tooltip at mouse position: it can start flashing
        // then as the control gets "focus lost" events and dismisses the
        // tooltip which then reappears because mouse remains hovering over the
        // control, see SF patch 1821229
        uFlags |= TTF_TRANSPARENT;
    }
};

// Takes care of deleting ToolTip control window when shutting down the library.
class wxToolTipModule : public wxModule
{
public:
    wxToolTipModule()
    {
    }

    virtual bool OnInit() wxOVERRIDE
    {
        return true;
    }

    virtual void OnExit() wxOVERRIDE
    {
        wxToolTip::DeleteToolTipCtrl();
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxToolTipModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxToolTipModule, wxModule);

#ifdef __VISUALC__
    #pragma warning( default : 4097 )
#endif

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// send a message to the tooltip control if it exists
//
// NB: wParam is always 0 for the TTM_XXX messages we use
static inline LRESULT SendTooltipMessage(WXHWND hwnd, UINT msg, void *lParam)
{
    return hwnd ? ::SendMessage((HWND)hwnd, msg, 0, (LPARAM)lParam) : 0;
}

// send a message to all existing tooltip controls
static inline void
SendTooltipMessageToAll(WXHWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if ( hwnd )
        ::SendMessage((HWND)hwnd, msg, wParam, lParam);
}

// Bricsys added
// The shared tooltip control is created without an owner. In a system DPI
// aware process with monitors using different DPIs, Windows may then map its
// position using the scale of a monitor other than the one of the hovered
// tool, so the tooltip appears far away from it. Making the tooltip owned by
// the top-level window of the hovered tool avoids this. As Windows destroys
// owned windows together with their owner, a CBT hook detaches the tooltip
// before its owner gets destroyed. The owner may meanwhile have been
// re-parented into another window, which DestroyWindow() then destroys without
// a hook notification for the owner, so destroying any of its ancestors
// detaches the tooltip too.
static HWND gs_hwndOwnedToolTip = NULL;
static HHOOK gs_hookToolTipOwner = NULL;

static LRESULT CALLBACK
wxToolTipOwnerCBTProc(int code, WPARAM wParam, LPARAM lParam)
{
    if ( code == HCBT_DESTROYWND && gs_hwndOwnedToolTip )
    {
        const HWND hwndDestroyed = (HWND)wParam;
        for ( HWND hwnd = ::GetWindow(gs_hwndOwnedToolTip, GW_OWNER);
              hwnd;
              hwnd = ::GetWindow(hwnd, GW_OWNER) )
        {
            if ( hwnd == hwndDestroyed || ::IsChild(hwndDestroyed, hwnd) )
            {
                ::SetWindowLongPtr(gs_hwndOwnedToolTip, GWLP_HWNDPARENT, 0);
                break;
            }
        }
    }

    return ::CallNextHookEx(gs_hookToolTipOwner, code, wParam, lParam);
}

// Windows doesn't scale per monitor aware windows, so a per monitor aware
// tooltip is placed correctly without an owner. Check the tooltip window
// itself: wx temporarily makes the thread system DPI aware around some common
// dialogs, so the process awareness doesn't tell the one of the tooltip.
static bool IsToolTipPerMonitorDPIAware(HWND hwndTT)
{
#if wxUSE_DYNLIB_CLASS
    // DPI_AWARENESS_PER_MONITOR_AWARE, which older SDKs don't define.
    const int wxDPI_AWARENESS_PER_MONITOR_AWARE = 2;

    typedef WXDPI_AWARENESS_CONTEXT
            (WINAPI *GetWindowDpiAwarenessContext_t)(HWND);
    typedef int
            (WINAPI *GetAwarenessFromDpiAwarenessContext_t)(WXDPI_AWARENESS_CONTEXT);
    static GetWindowDpiAwarenessContext_t
        s_pfnGetWindowDpiAwarenessContext = NULL;
    static GetAwarenessFromDpiAwarenessContext_t
        s_pfnGetAwarenessFromDpiAwarenessContext = NULL;
    static bool s_initDone = false;

    if ( !s_initDone )
    {
        wxLoadedDLL dllUser32("user32.dll");
        wxDL_INIT_FUNC(s_pfn, GetWindowDpiAwarenessContext, dllUser32);
        wxDL_INIT_FUNC(s_pfn, GetAwarenessFromDpiAwarenessContext, dllUser32);
        s_initDone = true;
    }

    // Before Windows 10 1607 there is no per window awareness; keep owning
    // the tooltip there.
    if ( s_pfnGetWindowDpiAwarenessContext &&
         s_pfnGetAwarenessFromDpiAwarenessContext )
    {
        const int awareness = s_pfnGetAwarenessFromDpiAwarenessContext(
                                s_pfnGetWindowDpiAwarenessContext(hwndTT));
        return awareness == wxDPI_AWARENESS_PER_MONITOR_AWARE;
    }
#else
    wxUnusedVar(hwndTT);
#endif // wxUSE_DYNLIB_CLASS

    return false;
}

static void SetToolTipOwner(HWND hwndTT, HWND hwndOwner)
{
    if ( !hwndOwner || ::GetWindow(hwndTT, GW_OWNER) == hwndOwner )
        return;

    if ( IsToolTipPerMonitorDPIAware(hwndTT) )
        return;

    // The hook only sees the windows of the current thread.
    const DWORD threadId = ::GetCurrentThreadId();
    if ( ::GetWindowThreadProcessId(hwndTT, NULL) != threadId ||
         ::GetWindowThreadProcessId(hwndOwner, NULL) != threadId )
        return;

    if ( !gs_hookToolTipOwner )
    {
        gs_hookToolTipOwner = ::SetWindowsHookEx(WH_CBT, wxToolTipOwnerCBTProc,
                                                 NULL, threadId);
        if ( !gs_hookToolTipOwner )
        {
            wxLogLastError(wxT("SetWindowsHookEx(WH_CBT)"));
            return;
        }
    }

    gs_hwndOwnedToolTip = hwndTT;
    ::SetWindowLongPtr(hwndTT, GWLP_HWNDPARENT, (LONG_PTR)hwndOwner);
}

static void ResetToolTipOwner()
{
    if ( gs_hookToolTipOwner )
    {
        ::UnhookWindowsHookEx(gs_hookToolTipOwner);
        gs_hookToolTipOwner = NULL;
    }

    gs_hwndOwnedToolTip = NULL;
}
// End Bricsys added

// ============================================================================
// implementation
// ============================================================================

#if wxUSE_TTM_WINDOWFROMPOINT

// ----------------------------------------------------------------------------
// window proc for our tooltip control
// ----------------------------------------------------------------------------

LRESULT APIENTRY wxToolTipWndProc(HWND hwndTT,
                                  UINT msg,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
    if ( msg == TTM_WINDOWFROMPOINT )
    {
        LPPOINT ppt = (LPPOINT)lParam;

        // the window on which event occurred
        HWND hwnd = ::WindowFromPoint(*ppt);

        OutputDebugString("TTM_WINDOWFROMPOINT: ");
        OutputDebugString(wxString::Format("0x%08x => ", hwnd));

        // return a HWND corresponding to a wxWindow because only wxWidgets are
        // associated with tooltips using TTM_ADDTOOL
        wxWindow *win = wxGetWindowFromHWND((WXHWND)hwnd);

        if ( win )
        {
            hwnd = GetHwndOf(win);
            OutputDebugString(wxString::Format("0x%08x\r\n", hwnd));

#if 0
            // modify the point too!
            RECT rect;
            GetWindowRect(hwnd, &rect);

            ppt->x = (rect.right - rect.left) / 2;
            ppt->y = (rect.bottom - rect.top) / 2;
#endif // 0
            return (LRESULT)hwnd;
        }
        else
        {
            OutputDebugString("no window\r\n");
        }
    }

    return ::CallWindowProc(CASTWNDPROC gs_wndprocToolTip, hwndTT, msg, wParam, lParam);
}

#endif // wxUSE_TTM_WINDOWFROMPOINT

// ----------------------------------------------------------------------------
// static functions
// ----------------------------------------------------------------------------

void wxToolTip::Enable(bool flag)
{
    // Make sure the tooltip has been created
    (void) GetToolTipCtrl();

    SendTooltipMessageToAll(ms_hwndTT, TTM_ACTIVATE, flag, 0);
}

void wxToolTip::SetDelay(long milliseconds)
{
    // Make sure the tooltip has been created
    (void) GetToolTipCtrl();

    SendTooltipMessageToAll(ms_hwndTT, TTM_SETDELAYTIME,
                            TTDT_INITIAL, milliseconds);
}

void wxToolTip::SetAutoPop(long milliseconds)
{
    SendTooltipMessageToAll(ms_hwndTT, TTM_SETDELAYTIME,
                            TTDT_AUTOPOP, milliseconds);
}

void wxToolTip::SetReshow(long milliseconds)
{
    SendTooltipMessageToAll(ms_hwndTT, TTM_SETDELAYTIME,
                            TTDT_RESHOW, milliseconds);
}

void wxToolTip::SetMaxWidth(int width)
{
    wxASSERT_MSG( width == -1 || width >= 0, wxT("invalid width value") );

    ms_maxWidth = width;
}

void wxToolTip::DeleteToolTipCtrl()
{
// Bricsys added
    ResetToolTipOwner();
// End Bricsys added

    if ( ms_hwndTT )
    {
        ::DestroyWindow((HWND)ms_hwndTT);
        ms_hwndTT = (WXHWND)NULL;
    }
}

// ---------------------------------------------------------------------------
// implementation helpers
// ---------------------------------------------------------------------------

// create the tooltip ctrl for our parent frame if it doesn't exist yet
/* static */
WXHWND wxToolTip::GetToolTipCtrl()
{
// Bricsys added
    // Windows destroys the tooltip with its owner if the CBT hook missed the
    // owner destruction, e.g. if another hook didn't call CallNextHookEx().
    // Don't keep using a dead, possibly reused, HWND. The registered tools are
    // lost with it, but tooltips added afterwards work again.
    if ( ms_hwndTT && !::IsWindow((HWND)ms_hwndTT) )
    {
        wxLogDebug(wxT("Tooltip control destroyed together with its owner."));
        ResetToolTipOwner();
        ms_hwndTT = (WXHWND)NULL;
    }
// End Bricsys added

    if ( !ms_hwndTT )
    {
        WXDWORD exflags = 0;
// Bricsys added
        exflags |= WS_EX_TRANSPARENT;
// End Bricsys added
        if ( wxApp::MSWGetDefaultLayout() == wxLayout_RightToLeft )
        {
            exflags |= WS_EX_LAYOUTRTL;
        }

        // we want to show the tooltips always (even when the window is not
        // active) and we don't want to strip "&"s from them
        ms_hwndTT = (WXHWND)::CreateWindowEx(exflags,
                                             TOOLTIPS_CLASS,
                                             (LPCTSTR)NULL,
                                             TTS_ALWAYSTIP | TTS_NOPREFIX,
                                             CW_USEDEFAULT, CW_USEDEFAULT,
                                             CW_USEDEFAULT, CW_USEDEFAULT,
                                             NULL, (HMENU)NULL,
                                             wxGetInstance(),
                                             NULL);
       if ( ms_hwndTT )
       {
           HWND hwnd = (HWND)ms_hwndTT;
           SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

#if wxUSE_TTM_WINDOWFROMPOINT
           // subclass the newly created control
           gs_wndprocToolTip = wxSetWindowProc(hwnd, wxToolTipWndProc);
#endif // wxUSE_TTM_WINDOWFROMPOINT
       }
    }

    return ms_hwndTT;
}

/* static */
void wxToolTip::UpdateVisibility()
{
    wxToolInfo ti(NULL, 0, wxRect());
    ti.uFlags = 0;

    if ( !SendTooltipMessage(ms_hwndTT, TTM_GETCURRENTTOOL, &ti) )
        return;

    wxWindow* const associatedWindow = wxFindWinFromHandle(ti.hwnd);
    if ( !associatedWindow )
        return;

    bool hideTT = false;
    if ( !associatedWindow->IsShownOnScreen() )
    {
        // If the associated window or its parent is hidden, the tooltip
        // shouldn't remain shown.
        hideTT = true;
    }
    else
    {
        // Even if it's not hidden, it could also be iconized.
        wxTopLevelWindow* const
            frame = wxDynamicCast(wxGetTopLevelParent(associatedWindow), wxTopLevelWindow);

        if ( frame && frame->IsIconized() )
            hideTT = true;
    }

    if ( hideTT )
        ::ShowWindow(ms_hwndTT, SW_HIDE);
}

/* static */
void wxToolTip::RelayEvent(WXMSG *msg)
{
// Bricsys change
    const HWND hwndTT = (HWND)GetToolTipCtrl();
    (void)SendTooltipMessage(hwndTT, TTM_RELAYEVENT, msg);

    if ( hwndTT && msg->message == WM_MOUSEMOVE && msg->hwnd )
    {
        TTHITTESTINFO hti;
        wxZeroMemory(hti);
        hti.hwnd = msg->hwnd;
        hti.pt.x = GET_X_LPARAM(msg->lParam);
        hti.pt.y = GET_Y_LPARAM(msg->lParam);
        hti.ti.cbSize = TTTOOLINFO_V1_SIZE;

        if ( SendTooltipMessage(hwndTT, TTM_HITTEST, &hti) )
            SetToolTipOwner(hwndTT, ::GetAncestor(msg->hwnd, GA_ROOT));
    }
// End Bricsys change
}

// ----------------------------------------------------------------------------
// ctor & dtor
// ----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxToolTip, wxObject);

wxToolTip::wxToolTip(const wxString &tip)
         : m_text(tip)
{
    m_window = NULL;
    m_others = NULL;

    // make sure m_rect.IsEmpty() == true
    m_rect.SetWidth(0);
    m_rect.SetHeight(0);

    // since m_rect is not valid, m_id is ignored by wxToolInfo ctor...
    m_id = 0;
}

wxToolTip::wxToolTip(wxWindow* win, unsigned int id, const wxString &tip, const wxRect& rc)
         : m_text(tip), m_rect(rc), m_id(id)
{
    m_window = NULL;
    m_others = NULL;

    SetWindow(win);
}

wxToolTip::~wxToolTip()
{
    // the tooltip has to be removed before deleting. Otherwise, if it is visible
    // while being deleted, there will be a delay before it goes away.
    Remove();

    delete m_others;
}

// ----------------------------------------------------------------------------
// others
// ----------------------------------------------------------------------------

/* static */
void wxToolTip::Remove(WXHWND hWnd, unsigned int id, const wxRect& rc)
{
    wxToolInfo ti((HWND)hWnd, id, rc);

    (void)SendTooltipMessage(GetToolTipCtrl(), TTM_DELTOOL, &ti);
}

void wxToolTip::DoRemove(WXHWND hWnd)
{
    if ( m_window && hWnd == m_window->GetHWND() )
    {
        // Remove the tooltip from the main window.
        Remove(hWnd, m_id, m_rect);
    }
    else
    {
        // Not really sure what to pass to remove in this case...
        Remove(hWnd, 0, wxRect());
    }
}

void wxToolTip::Remove()
{
    DoForAllWindows(&wxToolTip::DoRemove);
}

void wxToolTip::AddOtherWindow(WXHWND hWnd)
{
    if ( !m_others )
        m_others = new wxToolTipOtherWindows;

    m_others->push_back(hWnd);

    DoAddHWND(hWnd);
}

void wxToolTip::DoAddHWND(WXHWND hWnd)
{
    HWND hwnd = (HWND)hWnd;

    wxToolInfo ti(hwnd, m_id, m_rect);

    // another possibility would be to specify LPSTR_TEXTCALLBACK here as we
    // store the tooltip text ourselves anyhow, and provide it in response to
    // TTN_NEEDTEXT (sent via WM_NOTIFY), but then we would be limited to 79
    // character tooltips as this is the size of the szText buffer in
    // NMTTDISPINFO struct -- and setting the tooltip here we can have tooltips
    // of any length
    ti.hwnd = hwnd;
    ti.lpszText = wxMSW_CONV_LPTSTR(m_text);

    if ( !SendTooltipMessage(GetToolTipCtrl(), TTM_ADDTOOL, &ti) )
    {
        wxLogDebug(wxT("Failed to create the tooltip '%s'"), m_text);

        return;
    }

#ifdef TTM_SETMAXTIPWIDTH
    if ( !AdjustMaxWidth() )
#endif // TTM_SETMAXTIPWIDTH
    {
        // replace the '\n's with spaces because otherwise they appear as
        // unprintable characters in the tooltip string
        m_text.Replace(wxT("\n"), wxT(" "));
        ti.lpszText = wxMSW_CONV_LPTSTR(m_text);

        if ( !SendTooltipMessage(GetToolTipCtrl(), TTM_ADDTOOL, &ti) )
        {
            wxLogDebug(wxT("Failed to create the tooltip '%s'"), m_text);
        }
    }
}

void wxToolTip::SetWindow(wxWindow *win)
{
    Remove();

    m_window = win;

    // add the window itself
    if ( m_window )
    {
        DoAddHWND(m_window->GetHWND());
    }
#if !defined(__WXUNIVERSAL__)
    // and all of its subcontrols (e.g. radio buttons in a radiobox) as well
    wxControl *control = wxDynamicCast(m_window, wxControl);
    if ( control )
    {
        const wxArrayLong& subcontrols = control->GetSubcontrols();
        size_t count = subcontrols.GetCount();
        for ( size_t n = 0; n < count; n++ )
        {
            int id = subcontrols[n];
            HWND hwnd = GetDlgItem(GetHwndOf(m_window), id);
            if ( !hwnd )
            {
                // maybe it's a child of parent of the control, in fact?
                // (radiobuttons are subcontrols, i.e. children of the radiobox
                // for wxWidgets but are its siblings at Windows level)
                hwnd = GetDlgItem(GetHwndOf(m_window->GetParent()), id);
            }

            // must have it by now!
            wxASSERT_MSG( hwnd, wxT("no hwnd for subcontrol?") );

            AddOtherWindow((WXHWND)hwnd);
        }
    }
#endif // !defined(__WXUNIVERSAL__)
}

void wxToolTip::SetRect(const wxRect& rc)
{
    m_rect = rc;

    if ( m_window )
    {
        wxToolInfo ti(GetHwndOf(m_window), m_id, m_rect);
        (void)SendTooltipMessage(GetToolTipCtrl(), TTM_NEWTOOLRECT, &ti);
    }
}

void wxToolTip::SetTip(const wxString& tip)
{
    m_text = tip;

#ifdef TTM_SETMAXTIPWIDTH
    if ( !AdjustMaxWidth() )
#endif // TTM_SETMAXTIPWIDTH
    {
        // replace the '\n's with spaces because otherwise they appear as
        // unprintable characters in the tooltip string
        m_text.Replace(wxT("\n"), wxT(" "));
    }

    DoForAllWindows(&wxToolTip::DoSetTip);
}

void wxToolTip::DoSetTip(WXHWND hWnd)
{
    // update the tip text shown by the control
    wxToolInfo ti((HWND)hWnd, m_id, m_rect);

    // for some reason, changing the tooltip text directly results in
    // repaint of the controls under it, see #10520 -- but this doesn't
    // happen if we reset it first
    ti.lpszText = const_cast<wxChar *>(wxT(""));
    (void)SendTooltipMessage(GetToolTipCtrl(), TTM_UPDATETIPTEXT, &ti);

    ti.lpszText = wxMSW_CONV_LPTSTR(m_text);
    (void)SendTooltipMessage(GetToolTipCtrl(), TTM_UPDATETIPTEXT, &ti);
}

bool wxToolTip::AdjustMaxWidth()
{
    // use TTM_SETMAXTIPWIDTH to make tooltip multiline using the
    // extent of its first line as max value
    HFONT hfont = (HFONT)
        SendTooltipMessage(GetToolTipCtrl(), WM_GETFONT, 0);

    if ( !hfont )
    {
        hfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if ( !hfont )
        {
            wxLogLastError(wxT("GetStockObject(DEFAULT_GUI_FONT)"));
        }
    }

    MemoryHDC hdc;
    if ( !hdc )
    {
        wxLogLastError(wxT("CreateCompatibleDC(NULL)"));
    }

    if ( !SelectObject(hdc, hfont) )
    {
        wxLogLastError(wxT("SelectObject(hfont)"));
    }

    // find the width of the widest line
    int maxWidth = 0;
    wxStringTokenizer tokenizer(m_text, wxT("\n"));
    while ( tokenizer.HasMoreTokens() )
    {
        const wxString token = tokenizer.GetNextToken();

        SIZE sz;
        if ( !::GetTextExtentPoint32(hdc, token.t_str(),
                                     token.length(), &sz) )
        {
            wxLogLastError(wxT("GetTextExtentPoint32"));
        }

        if ( sz.cx > maxWidth )
            maxWidth = sz.cx;
    }

    // limit size to ms_maxWidth, if set
    if ( ms_maxWidth == 0 )
    {
        // this is more or less arbitrary but seems to work well
        static const int DEFAULT_MAX_WIDTH = 400;

        ms_maxWidth = wxGetClientDisplayRect().width / 2;

        if ( ms_maxWidth > DEFAULT_MAX_WIDTH )
            ms_maxWidth = DEFAULT_MAX_WIDTH;
    }

    if ( ms_maxWidth != -1 && maxWidth > ms_maxWidth )
        maxWidth = ms_maxWidth;

    // only set a new width if it is bigger than the current setting:
    // otherwise adding a tooltip with shorter line(s) than a previous
    // one would result in breaking the longer lines unnecessarily as
    // all our tooltips share the same maximal width
    if ( maxWidth > SendTooltipMessage(GetToolTipCtrl(),
                                       TTM_GETMAXTIPWIDTH, 0) )
    {
        SendTooltipMessage(GetToolTipCtrl(), TTM_SETMAXTIPWIDTH,
                           wxUIntToPtr(maxWidth));
    }

    return true;
}

void wxToolTip::DoForAllWindows(void (wxToolTip::*func)(WXHWND))
{
    if ( m_window )
    {
        (this->*func)(m_window->GetHWND());
    }

    if ( m_others )
    {
        for ( wxToolTipOtherWindows::const_iterator it = m_others->begin();
              it != m_others->end();
              ++it )
        {
            (this->*func)(*it);
        }
    }
}

#endif // wxUSE_TOOLTIPS
