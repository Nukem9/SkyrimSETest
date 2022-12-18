#include "../../common.h"
#include <commdlg.h>
#include <CommCtrl.h>
#include "MainWindow.h"
#include "EditorUI.h"
#include "LogWindow.h"
#include "BSString.h"
#include "TESForm_CK.h"
#include "ObjectWindow.h"
#include "../../typeinfo/hk_rtti.h"
#include "../../typeinfo/ms_rtti.h"

namespace MainWindow
{
	HWND MainWindowHandle;
	HMENU ExtensionMenuHandle;
	WNDPROC OldWndProc;

	static Core::Classes::UI::CUIMainWindow MainWindow;

	HWND GetWindow()
	{
		return MainWindowHandle;
	}

	Core::Classes::UI::CUIMainWindow& FIXAPI GetWindowObj() {
		return MainWindow;
	}

	void Initialize()
	{
		// Does nothing. Kept for consistency.
	}

	bool GetFileVersion(LPCSTR pszFilePath, BSString& result)
	{
		DWORD dwSize = 0;
		BYTE* pbVersionInfo = NULL;
		VS_FIXEDFILEINFO* pFileInfo = NULL;
		UINT puLenFileInfo = 0;

		// Get the version information for the file requested
		dwSize = GetFileVersionInfoSize(pszFilePath, NULL);
		if (dwSize == 0)
			return false;

		pbVersionInfo = new BYTE[dwSize];
		if (!pbVersionInfo)
			return false;

		if (!GetFileVersionInfo(pszFilePath, 0, dwSize, pbVersionInfo)) {
			delete[] pbVersionInfo;
			return false;
		}

		if (!VerQueryValue(pbVersionInfo, TEXT("\\"), (LPVOID*)&pFileInfo, &puLenFileInfo)) {
			delete[] pbVersionInfo;
			return false;
		}

		char temp[100];
		sprintf_s(temp, "%d.%d", (pFileInfo->dwFileVersionMS >> 16) & 0xFF, (pFileInfo->dwFileVersionMS) & 0xFF);
		result = temp;

		delete[] pbVersionInfo;
		return true;
	}

	void CreateExtensionMenu(HWND MainWindow, HMENU MainMenu)
	{
		// Create extended menu options
		ExtensionMenuHandle = CreateMenu();

		auto insertItem = [](uint32_t Flags, uint32_t Id, const char *Text)
		{
			auto result = InsertMenu(ExtensionMenuHandle, -1, MF_BYPOSITION | Flags, static_cast<UINT_PTR>(Id), Text);
			AssertMsg(result, "Failed to register menu item");
		};

		insertItem(MF_STRING, UI_EXTMENU_SHOWLOG, "Show Log");
		insertItem(MF_STRING, UI_EXTMENU_CLEARLOG, "Clear Log");
		insertItem(MF_STRING | MF_CHECKED, UI_EXTMENU_AUTOSCROLL, "Autoscroll Log");
		insertItem(MF_SEPARATOR, UI_EXTMENU_SPACER, "");
		insertItem(MF_STRING, UI_EXTMENU_DUMPRTTI, "Dump RTTI Data");
		insertItem(MF_STRING, UI_EXTMENU_DUMPNIRTTI, "Dump NiRTTI Data");
		insertItem(MF_STRING, UI_EXTMENU_DUMPHAVOKRTTI, "Dump Havok RTTI Data");
		insertItem(MF_STRING, UI_EXTMENU_LOADEDESPINFO, "Dump Active Forms");
		insertItem(MF_SEPARATOR, UI_EXTMENU_SPACER, "");
		insertItem(MF_STRING, UI_EXTMENU_HARDCODEDFORMS, "Save Hardcoded Forms");

		MENUITEMINFO menuInfo
		{
			.cbSize = sizeof(MENUITEMINFO),
			.fMask = MIIM_SUBMENU | MIIM_ID | MIIM_STRING,
			.wID = UI_EXTMENU_ID,
			.hSubMenu = ExtensionMenuHandle,
			.dwTypeData = const_cast<LPSTR>("Extensions"),
			.cch = static_cast<uint32_t>(strlen(menuInfo.dwTypeData))
		};

		AssertMsg(InsertMenuItem(MainMenu, -1, TRUE, &menuInfo), "Failed to create extension submenu");
	}

	LRESULT CALLBACK WndProc(HWND Hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
	{
		Core::Classes::UI::CUIMenuItem MenuItem;

		if (Message == WM_CREATE)
		{
			auto createInfo = reinterpret_cast<const CREATESTRUCT *>(lParam);

			if (!strcmp(createInfo->lpszName, "Creation Kit SSE") && !strcmp(createInfo->lpszClass, "Creation Kit SSE"))
			{
				// Initialize the original window before adding anything
				LRESULT status = CallWindowProc(OldWndProc, Hwnd, Message, wParam, lParam);
				MainWindow = Hwnd;
				MainWindowHandle = Hwnd;

				// Grass is always enabled by default, make the UI buttons match
				CheckMenuItem(GetMenu(Hwnd), UI_EDITOR_TOGGLEGRASS, MF_CHECKED);
				SendMessageA(GetDlgItem(Hwnd, UI_EDITOR_TOOLBAR), TB_CHECKBUTTON, UI_EDITOR_TOGGLEGRASS_BUTTON, TRUE);

				// Same for fog
				CheckMenuItem(GetMenu(Hwnd), UI_EDITOR_TOGGLEFOG, *reinterpret_cast<bool*>(OFFSET(0x4F05728, 1530)) ? MF_CHECKED : MF_UNCHECKED);

				// Create custom menu controls
				MainWindow.MainMenu = createInfo->hMenu;
				CreateExtensionMenu(Hwnd, createInfo->hMenu);

				// All main menus change to uppercase letters
				for (UINT i = 0; i < MainWindow.MainMenu.Count(); i++) {
					MenuItem = MainWindow.MainMenu.GetItemByPos(i);
					MenuItem.Text = BSString(MenuItem.Text).UpperCase().c_str();
				}

				Core::Classes::UI::CUIMenu ViewMenu = MainWindow.MainMenu.GetSubMenuItem(2);

				// How annoying is this window Warnings, delete from the menu.
				ViewMenu.Remove(0xA043);

				// Fix show/hide object window
				MenuItem = ViewMenu.GetItemByPos(2);
				MenuItem.Checked = TRUE;

				// Fix display text hotkey toggle sound marker
				ViewMenu.GetItem(40677).Text = "Sound Marker\tCtrl-N";
				// Deprecated
				ViewMenu.GetItem(290).Enabled = FALSE;
				ViewMenu.GetItem(40032).Enabled = FALSE;

				return status;
			}
		}
		else if (Message == WM_SIZE && Hwnd == GetWindow())
		{
			// Scale the status bar segments to fit the window size
			auto scale = [&](int Width)
			{
				return static_cast<int>(Width * (LOWORD(lParam) / 1024.0f));
			};

			std::array<int, 4> spacing
			{
				scale(150),	// 150
				scale(300),	// 225
				scale(600),	// 500
				-1,			// -1
			};

			SendMessageA(GetDlgItem(Hwnd, UI_EDITOR_STATUSBAR), SB_SETPARTS, spacing.size(), reinterpret_cast<LPARAM>(spacing.data()));
		}
		else if (Message == WM_COMMAND && Hwnd == GetWindow())
		{
			const uint32_t param = LOWORD(wParam);

			switch (param)
			{
			case UI_EDITOR_TOGGLEFOG:
			{
				// Call the CTRL+F5 hotkey function directly
				((void(__fastcall *)())OFFSET(0x1319740, 1530))();
			}
			return 0;

			case UI_EDITOR_OPENFORMBYID:
			{
				auto form = TESForm_CK::GetFormByNumericID(static_cast<uint32_t>(lParam));

				if (form)
					(*(void(__fastcall **)(TESForm_CK*, HWND, __int64, __int64))(*(__int64*)form + 720i64))(form, Hwnd, 0, 1);
			}
			return 0;

			case UI_EXTMENU_SHOWLOG:
			{
				LogWindow::BringToFront();
			}
			return 0;

			case UI_EXTMENU_CLEARLOG:
			{
				LogWindow::Clear();
			}
			return 0;

			case UI_EDITOR_TOGGLEOBJECTWND:
			{
				ObjectWindow::OBJWNDS Wnds = ObjectWindow::GetAllWindowObj();

				for (auto Wnd : Wnds) {
					Wnd.second->ObjectWindow.Visible = !Wnd.second->ObjectWindow.Visible;
					if (Wnd.second->ObjectWindow.Visible)
						Wnd.second->ObjectWindow.Foreground();
				}

				// Change the checkbox
				MenuItem = MainWindow.MainMenu.GetItem(UI_EDITOR_TOGGLEOBJECTWND);
				MenuItem.Checked = !MenuItem.Checked;
			}
			return 0;

			case UI_EXTMENU_AUTOSCROLL:
			{
				MENUITEMINFO info
				{
					.cbSize = sizeof(MENUITEMINFO),
					.fMask = MIIM_STATE
				};

				GetMenuItemInfo(ExtensionMenuHandle, param, FALSE, &info);

				bool doCheck = !((info.fState & MFS_CHECKED) == MFS_CHECKED);

				if (doCheck)
					info.fState |= MFS_CHECKED;
				else
					info.fState &= ~MFS_CHECKED;

				LogWindow::EnableAutoscroll(doCheck);
				SetMenuItemInfo(ExtensionMenuHandle, param, FALSE, &info);
			}
			return 0;

			case UI_EXTMENU_DUMPRTTI:
			case UI_EXTMENU_DUMPNIRTTI:
			case UI_EXTMENU_DUMPHAVOKRTTI:
			case UI_EXTMENU_LOADEDESPINFO:
			{
				char filePath[MAX_PATH] = {};
				OPENFILENAME ofnData
				{
					.lStructSize = sizeof(OPENFILENAME),
					.lpstrFilter = "Text Files (*.txt)\0*.txt\0\0",
					.lpstrFile = filePath,
					.nMaxFile = ARRAYSIZE(filePath),
					.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR,
					.lpstrDefExt = "txt"
				};

				if (FILE *f; GetSaveFileName(&ofnData) && fopen_s(&f, filePath, "w") == 0)
				{
					if (param == UI_EXTMENU_DUMPRTTI)
						MSRTTI::Dump(f);
					//else if (param == UI_EXTMENU_DUMPNIRTTI)
					//	ExportTest(f);
					else if (param == UI_EXTMENU_DUMPHAVOKRTTI)
					{
						// Convert path to directory
						*strrchr(filePath, '\\') = '\0';
						HKRTTI::DumpReflectionData(filePath);
					}
					else if (param == UI_EXTMENU_LOADEDESPINFO)
					{
						struct VersionControlListItem
						{
							char *EditorId;
							uint32_t FileOffset;
							char Type[4];
							uint32_t FileLength;
							char GroupType[4];
							uint32_t FormId;
							uint32_t VersionControlId;
							char _pad0[0x8];
						};
						static_assert_offset(VersionControlListItem, EditorId, 0x0);
						static_assert_offset(VersionControlListItem, FileOffset, 0x8);
						static_assert_offset(VersionControlListItem, Type, 0xC);
						static_assert_offset(VersionControlListItem, FileLength, 0x10);
						static_assert_offset(VersionControlListItem, GroupType, 0x14);
						static_assert_offset(VersionControlListItem, FormId, 0x18);
						static_assert_offset(VersionControlListItem, VersionControlId, 0x1C);
						static_assert(sizeof(VersionControlListItem) == 0x28);

						static std::vector<VersionControlListItem> formList;

						// Invoke the dialog, building form list
						auto callback = +[](void *, int, VersionControlListItem *Data)
						{
							formList.push_back(*Data);
							formList.back().EditorId = _strdup(Data->EditorId);
						};

						XUtil::DetourCall(OFFSET(0x13E32B0, 1530), callback);
						CallWindowProcA(reinterpret_cast<WNDPROC>(OFFSET(0x13E6270, 1530)), Hwnd, WM_COMMAND, 1185, 0);

						// Sort by: type, editor id, form id, then file offset
						std::sort(formList.begin(), formList.end(),
							[](const auto& A, const auto& B) -> bool
						{
							int ret = memcmp(A.Type, B.Type, sizeof(VersionControlListItem::Type));

							if (ret != 0)
								return ret < 0;

							ret = _stricmp(A.EditorId, B.EditorId);

							if (ret != 0)
								return ret < 0;

							if (A.FormId != B.FormId)
								return A.FormId > B.FormId;

							return A.FileOffset > B.FileOffset;
						});

						// Dump it to the log
						fprintf(f, "Type, Editor Id, Form Id, File Offset, File Length, Version Control Id\n");

						for (auto& item : formList)
						{
							fprintf(f, "%c%c%c%c,\"%s\",%08X,%u,%u,-%08X-\n",
								item.Type[0], item.Type[1], item.Type[2], item.Type[3],
								item.EditorId,
								item.FormId,
								item.FileOffset,
								item.FileLength,
								item.VersionControlId);

							free(reinterpret_cast<void *>(item.EditorId));
						}

						formList.clear();
					}

					fclose(f);
				}
			}
			return 0;

			case UI_EXTMENU_HARDCODEDFORMS:
			{
				for (uint32_t i = 0; i < 2048; i++)
				{
					auto form = TESForm_CK::GetFormByNumericID(i);

					if (form)
					{
						(*(void(__fastcall **)(TESForm_CK*, __int64))(*(__int64*)form + 360))(form, 1);
						LogWindow::Log("SetFormModified(%08X)", i);
					}
				}

				// Fake the click on "Save"
				PostMessageA(Hwnd, WM_COMMAND, 40127, 0);
			}
			return 0;

			case WM_SHOWWINDOW: 
			{
				// Getting additional child Windows
				MainWindow.FindToolWindow();
			}
			return 0;

			case  WM_GETMINMAXINFO:
			{
				// https://social.msdn.microsoft.com/Forums/vstudio/en-US/fb4de52d-66d4-44da-907c-0357d6ba894c/swmaximize-is-same-as-fullscreen?forum=vcgeneral

				RECT WorkArea;
				SystemParametersInfoA(SPI_GETWORKAREA, 0, &WorkArea, 0);
				((MINMAXINFO*)lParam)->ptMaxSize.x = (WorkArea.right - WorkArea.left);
				((MINMAXINFO*)lParam)->ptMaxSize.y = (WorkArea.bottom - WorkArea.top);
				((MINMAXINFO*)lParam)->ptMaxPosition.x = WorkArea.left;
				((MINMAXINFO*)lParam)->ptMaxPosition.y = WorkArea.top;	
			}
			return 0;

			}
		}
		else if (Message == WM_SETTEXT && Hwnd == GetWindow())
		{
			// Continue normal execution but with a custom string
			char customTitle[1024];
			BSString versionApp;

			char modulePath[MAX_PATH];
			GetModuleFileNameA((HMODULE)g_hModule, modulePath, MAX_PATH);
			GetFileVersion(modulePath, versionApp);

			sprintf_s(customTitle, "%s [CK64Fixes Rev. %s: v%s]", (LPCSTR)lParam, g_GitVersion, versionApp.c_str());

			return CallWindowProc(OldWndProc, Hwnd, Message, wParam, reinterpret_cast<LPARAM>(customTitle));
		}

		return CallWindowProc(OldWndProc, Hwnd, Message, wParam, lParam);
	}
}