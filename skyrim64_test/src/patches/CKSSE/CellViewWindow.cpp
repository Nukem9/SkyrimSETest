#include "../../common.h"
#include <CommCtrl.h>
#include "CellViewWindow.h"
#include "EditorUI.h"
#include "TESForm_CK.h"
#include "UIBaseWindow.h"

#include "../../../resource.h"

namespace CellViewWindow
{
	DLGPROC OldCellViewProc;

	INT_PTR CALLBACK CellViewProc(HWND DialogHwnd, UINT Message, WPARAM wParam, LPARAM lParam)
	{
		Core::Classes::UI::CUICustomWindow CellViewWindow;

		if (Message == WM_INITDIALOG)
		{
			CellViewWindow = DialogHwnd;

			// Eliminate the flicker when changing cells
			ListView_SetExtendedListViewStyleEx(GetDlgItem(DialogHwnd, 1155), LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
			ListView_SetExtendedListViewStyleEx(GetDlgItem(DialogHwnd, 1156), LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);

			ShowWindow(GetDlgItem(DialogHwnd, 1007), SW_HIDE);
		}
		else if (Message == WM_SIZE)
		{
			auto labelRect = reinterpret_cast<RECT*>(OFFSET(0x3AFB570, 1530));

			auto handle = GetDlgItem(DialogHwnd, 1164);

			// Fix the "World Space" label positioning on window resize
			RECT label;
			GetClientRect(handle, &label);

			RECT rect;
			GetClientRect(GetDlgItem(DialogHwnd, 2083), &rect);

			int ddMid = rect.left + ((rect.right - rect.left) / 2);
			int labelMid = (label.right - label.left) / 2;

			SetWindowPos(handle, nullptr, ddMid - (labelMid / 2), labelRect->top, 0, 0, SWP_NOSIZE);

			// Force the dropdown to extend the full length of the column
			labelRect->right = 0;

			auto result = OldCellViewProc(DialogHwnd, Message, wParam, lParam);

			Core::Classes::UI::CUIBaseControl editSearchObjs = GetDlgItem(DialogHwnd, 2581);
			Core::Classes::UI::CUIBaseControl listObjs = GetDlgItem(DialogHwnd, 1156);
			Core::Classes::UI::CUIBaseControl listCheckActiveObjs = GetDlgItem(DialogHwnd, IDC_CELL_VIEW_CHECK_ACTIVE_CELL_OBJECTS);
			
			auto left = listObjs.Left;
			auto width = listObjs.Width;
			listCheckActiveObjs.Left = left;
			listCheckActiveObjs.Width = width;
			editSearchObjs.Left = left;
			editSearchObjs.Width = width;

			return result;
		}
		else if (Message == WM_COMMAND)
		{
			const uint32_t param = LOWORD(wParam);

			if (param == UI_CELL_VIEW_ACTIVE_CELLS_CHECKBOX)
			{
				bool enableFilter = SendMessage(reinterpret_cast<HWND>(lParam), BM_GETCHECK, 0, 0) == BST_CHECKED;
				SetPropA(DialogHwnd, "ActiveCellsOnly", reinterpret_cast<HANDLE>(enableFilter));

				// Fake the dropdown list being activated
				SendMessageA(DialogHwnd, WM_COMMAND, MAKEWPARAM(2083, 1), 0);
				return 1;
			}
			else if (param == UI_CELL_VIEW_ACTIVE_CELL_OBJECTS_CHECKBOX)
			{
				bool enableFilter = SendMessage(reinterpret_cast<HWND>(lParam), BM_GETCHECK, 0, 0) == BST_CHECKED;
				SetPropA(DialogHwnd, "ActiveObjectsOnly", reinterpret_cast<HANDLE>(enableFilter));

				// Fake a filter text box change
				SendMessageA(DialogHwnd, WM_COMMAND, MAKEWPARAM(2581, EN_CHANGE), 0);
				return 1;
			}
		}
		else if (Message == UI_CELL_VIEW_ADD_CELL_ITEM)
		{
			auto form = reinterpret_cast<const TESForm_CK *>(wParam);
			auto allowInsert = reinterpret_cast<bool *>(lParam);

			*allowInsert = true;

			// Skip the entry if "Show only active cells" is checked
			if (static_cast<bool>(GetPropA(DialogHwnd, "ActiveCellsOnly")))
			{
				if (form && !form->GetActive())
					*allowInsert = false;
			}

			return 1;
		}
		else if (Message == UI_CELL_VIEW_ADD_CELL_OBJECT_ITEM)
		{
			auto form = reinterpret_cast<const TESForm_CK *>(wParam);
			auto allowInsert = reinterpret_cast<bool *>(lParam);

			*allowInsert = true;

			// Skip the entry if "Show only active objects" is checked
			if (static_cast<bool>(GetPropA(DialogHwnd, "ActiveObjectsOnly")))
			{
				if (form && !form->GetActive())
					*allowInsert = false;
			}

			return 1;
		}
		// Don't let us reduce the window too much
		else if (Message == WM_GETMINMAXINFO) {
			if (lParam) {
				LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
				lpMMI->ptMinTrackSize.x = 738;
				lpMMI->ptMinTrackSize.y = 315;
			}

			return S_OK;
		}

		return OldCellViewProc(DialogHwnd, Message, wParam, lParam);
	}

	void UpdateCellList(void *Thisptr, HWND ControlHandle, __int64 Unknown)
	{
		SendMessage(ControlHandle, WM_SETREDRAW, FALSE, 0);
		((void(__fastcall *)(void *, HWND, __int64))OFFSET(0x147FA70, 1530))(Thisptr, ControlHandle, Unknown);
		SendMessage(ControlHandle, WM_SETREDRAW, TRUE, 0);
		RedrawWindow(ControlHandle, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_NOCHILDREN);
	}

	void UpdateObjectList(void *Thisptr, HWND *ControlHandle)
	{
		SendMessage(*ControlHandle, WM_SETREDRAW, FALSE, 0);
		((void(__fastcall *)(void *, HWND *))OFFSET(0x13E0CE0, 1530))(Thisptr, ControlHandle);
		SendMessage(*ControlHandle, WM_SETREDRAW, TRUE, 0);
		RedrawWindow(*ControlHandle, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_NOCHILDREN);
	}
}