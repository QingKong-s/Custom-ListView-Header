#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib,"UxTheme.lib")
#pragma comment(lib,"Comctl32.lib")

#include <Windows.h>

#include "CStdHeaderCustom.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HINSTANCE hInst;
HWND hwndMain;
CStdHeaderCustom* pCustom;
HFONT hMyFont;
HBRUSH hMyBrush;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    hInst = hInstance;

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = L"QK.WndClass.ListViewHeaderCustomTest.Main";

    if (!RegisterClassExW(&wcex))
    {
        MessageBoxW(NULL, L"注册窗口类失败", L"错误", MB_ICONERROR);
        return 1;
    }

	hwndMain = CreateWindowExW(0, wcex.lpszClassName, L"ListView表头自定义测试，by QingKong，QQ：639582106", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
    if (!hwndMain)
    {
        MessageBoxW(NULL, L"创建窗口失败", L"错误", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return (int)msg.wParam;
}

HDCDRET CALLBACK HeaderCustomDrawProc(HWND hWnd, HDCDMSG uMsg, HDCDPART uPart, int iIndex, HDC hDC, RECT* prc, LPARAM lParam)
{
	switch (uMsg)
	{
	case HDCDMSG::PREPAINT:
	{
		switch (uPart)
		{
		case HDCDPART::TEXT:
		{
			switch (iIndex)
			{
			case 1:
			{
				HGDIOBJ hOld = SelectObject(hDC, hMyFont);
				COLORREF crOld = SetTextColor(hDC, RGB(0, 0xFF, 0));
				DrawTextW(hDC, L"我是第2列", -1, prc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
				SetTextColor(hDC, crOld);
				SelectObject(hDC, hOld);
			}
			return HDCDRET::SKIPDEF;

			case 2:
			{
				int cxIcon = GetSystemMetrics(SM_CXICON);
				int cyIcon = GetSystemMetrics(SM_CYICON);

				HICON hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(IDI_INFORMATION));
				DrawIcon(
					hDC,
					prc->left + (prc->right - prc->left - cxIcon) / 2,
					prc->top + (prc->bottom - prc->top - cyIcon) / 2,
					hIcon);
			}
			return HDCDRET::SKIPDEF;
			}
		}
		break;

		case HDCDPART::BK:
		{
			switch (iIndex)
			{
			case 3:
			{
				if (lParam == HIS_HOT)
				{
					FillRect(hDC, prc, hMyBrush);
					return HDCDRET::SKIPDEF;
				}
			}
			return HDCDRET::DODEF;
			}
		}
		break;
		}
	}
	break;

	case HDCDMSG::SETDRAGGINGMARK:
	{
		static BOOL b = FALSE;
		if (iIndex == 2)// 在第3项前显示的换色
		{
			b = TRUE;
			pCustom->SetDraggingMarkColor(RGB(0, 0, 0xFF));
			InvalidateRect((HWND)lParam, NULL, FALSE);
			UpdateWindow((HWND)lParam);
		}
		else
		{
			if (b)
			{
				b = FALSE;
				pCustom->SetDraggingMarkColor(RGB(11, 238, 183));// 换回默认颜色
				InvalidateRect((HWND)lParam, NULL, FALSE);
				UpdateWindow((HWND)lParam);
			}
		}
	}
	break;
	}
	return HDCDRET::DODEF;
}

HFONT EzFont(PCWSTR pszFontName, int nPoint, int nWeight, BOOL IsItalic, BOOL IsUnderline, BOOL IsStrikeOut)
{
	HDC hDC = GetDC(NULL);
	int iSize;
	iSize = -MulDiv(nPoint, GetDeviceCaps(hDC, LOGPIXELSY), 72);
	ReleaseDC(NULL, hDC);
	return CreateFontW(iSize, 0, 0, 0, nWeight, IsItalic, IsUnderline, IsStrikeOut, 0, 0, 0, 0, 0, pszFontName);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		HWND hLV = CreateWindowExW(0, WC_LISTVIEWW, NULL, WS_CHILD | WS_BORDER | WS_VISIBLE, 0, 0, 1000, 700, hWnd, NULL, hInst, NULL);
		SetWindowTheme(hLV, L"Explorer", NULL);
		DWORD dw = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
		ListView_SetExtendedListViewStyleEx(hLV, dw, dw);
		ListView_SetView(hLV, LV_VIEW_DETAILS);

		WCHAR sz[36];

		LVCOLUMNW lc;
		lc.mask = LVCF_TEXT | LVCF_WIDTH;
		lc.pszText = sz;
		lc.cx = 200;
		for (int i = 0; i < 4; ++i)
		{
			wsprintfW(sz, L"%d 测试测试测试", i);
			ListView_InsertColumn(hLV, i, &lc);
		}

		LVITEMW li;
		li.iSubItem = 0;
		li.mask = LVIF_TEXT;
		li.pszText = sz;
		for (int i = 0; i < 40; ++i)
		{
			li.iItem = i + 1;
			wsprintfW(sz, L"%d 测试测试测试", i);
			ListView_InsertItem(hLV, &li);
		}

		hMyFont = EzFont(L"微软雅黑", 20, 400, FALSE, TRUE, FALSE);
		hMyBrush = CreateSolidBrush(RGB(0xFF, 0, 0));

		HWND hHeader = ListView_GetHeader(hLV);
		HDITEMW hi;
		hi.mask = HDI_FORMAT;
		hi.fmt = HDF_SORTUP;
		Header_SetItem(hHeader, 0, &hi);

		pCustom = new CStdHeaderCustom(hLV, HeaderCustomDrawProc);
		auto Ret = pCustom->GetErrCode();
		pCustom->SetHeaderHeight(70);
	}
	return 0;

	case WM_DESTROY:
		delete pCustom;
		DeleteObject(hMyFont);
		DeleteObject(hMyBrush);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}