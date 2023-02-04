#include "CStdHeaderCustom.h"

#define HCCHELPER_SETERRCODEANDRET(ErrCode) { m_ErrCode = ErrCode; return; }


ATOM CStdHeaderCustom::m_atomDraggingMarkWnd = 0;

CStdHeaderCustom::CStdHeaderCustom(HWND hwndListView, HDCDPROC pProc)
{
	if (!IsWindow(hwndListView))
		HCCHELPER_SETERRCODEANDRET(HDERRCODE::INVALID_WINDOW);

	if (!m_atomDraggingMarkWnd)
	{
		WNDCLASSW wc = { sizeof(WNDCLASSW) };
		wc.lpszClassName = c_pszDraggingMarkWndClassName;
		wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW));
		wc.cbWndExtra = sizeof(void*);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc_DraggingMark;

		if (!(m_atomDraggingMarkWnd = RegisterClassW(&wc)))
			HCCHELPER_SETERRCODEANDRET(HDERRCODE::REGISTER_WNDCLASS_FAIL);
	}

	m_hwndHeader = ListView_GetHeader(hwndListView);
	if (!(m_hTheme = OpenThemeData(m_hwndHeader, L"Header")))
		HCCHELPER_SETERRCODEANDRET(HDERRCODE::OPEN_THEME_DATA_FAIL);

	m_hwndListView = hwndListView;
	m_pProc = pProc;

	m_hwndDraggingMark = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT, MAKEINTATOM(m_atomDraggingMarkWnd), NULL, 
		WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
	if (!m_hwndDraggingMark)
	{
		CloseThemeData(m_hTheme);
		HCCHELPER_SETERRCODEANDRET(HDERRCODE::CREATE_WND_FAIL);
	}
	SetLayeredWindowAttributes(m_hwndDraggingMark, 0, 0xFF, LWA_ALPHA);
	SetWindowLongPtrW(m_hwndDraggingMark, GWLP_HWNDPARENT, (LONG_PTR)m_hwndListView);
	SetWindowLongPtrW(m_hwndDraggingMark, 0, (LONG_PTR)this);

	m_hFont = (HFONT)SendMessageW(m_hwndHeader, WM_GETFONT, 0, 0);
	RECT rc;
	GetClientRect(m_hwndHeader, &rc);

	HDC hDC = GetDC(m_hwndHeader);
	m_iDpi = GetDeviceCaps(hDC, LOGPIXELSX);// Win10 1607以上的版本可以改用改用GetDpiForWindow
	m_hCDC = CreateCompatibleDC(hDC);
	m_hBitmap = CreateCompatibleBitmap(hDC, rc.right, rc.bottom);
	m_hOldBmp = SelectObject(m_hCDC, m_hBitmap);
	SelectObject(m_hCDC, m_hFont);
	ReleaseDC(m_hwndHeader, hDC);
	SetBkMode(m_hCDC, TRANSPARENT);

	SetDraggingMarkColor(RGB(11, 238, 183));

	SetWindowSubclass(m_hwndHeader, SubClassProc_Header, c_uSubclassIDHeader, (DWORD_PTR)this);
	SetWindowSubclass(m_hwndListView, SubClassProc_LV, c_uSubclassIDLV, (DWORD_PTR)this);
	InvalidateRect(m_hwndHeader, NULL, TRUE);
	HCCHELPER_SETERRCODEANDRET(HDERRCODE::OK);
}

CStdHeaderCustom::~CStdHeaderCustom()
{
	if (m_ErrCode == HDERRCODE::OK)
	{
		RemoveWindowSubclass(m_hwndHeader, SubClassProc_Header, c_uSubclassIDHeader);
		RemoveWindowSubclass(m_hwndListView, SubClassProc_LV, c_uSubclassIDLV);

		DestroyWindow(m_hwndDraggingMark);

		DeleteObject(SelectObject(m_hCDC, m_hOldBmp));
		DeleteDC(m_hCDC);
		DeleteObject(m_hbrDraggingMark);

		CloseThemeData(m_hTheme);
	}
}

void CStdHeaderCustom::SetHeaderHeight(int cy)
{
	m_cyHeader = cy;
	SendMessageW(m_hwndListView, LVM_UPDATE, 0, 0);
	RECT rc;
	GetClientRect(m_hwndHeader, &rc);
	SendMessageW(m_hwndHeader, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
}

void CStdHeaderCustom::SetDraggingMarkColor(COLORREF cr)
{
	DeleteObject(m_hbrDraggingMark);
	m_hbrDraggingMark = CreateSolidBrush(cr);
}

void CStdHeaderCustom::ResetDraggingMark()
{
	if (m_idxDragging < 0)
		return;

	RECT rc;
	int temp = DPI(4);
	if (m_pProc(m_hwndHeader, HDCDMSG::SETDRAGGINGMARK, HDCDPART::NONE, m_idxDragging, NULL, NULL, (LPARAM)m_hwndDraggingMark) == HDCDRET::DODEF)
	{
		if (m_idxDragging == DefSubclassProc(m_hwndHeader, HDM_GETITEMCOUNT, 0, 0))
		{
			DefSubclassProc(m_hwndHeader, HDM_GETITEMRECT, m_idxDragging - 1, (LPARAM)&rc);
			POINT pt = { rc.right - temp / 2,rc.top };
			ClientToScreen(m_hwndHeader, &pt);
			SetWindowPos(m_hwndDraggingMark, NULL, pt.x, pt.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
		}
		else
		{
			DefSubclassProc(m_hwndHeader, HDM_GETITEMRECT, m_idxDragging, (LPARAM)&rc);
			POINT pt = { rc.left - temp / 2,rc.top };
			ClientToScreen(m_hwndHeader, &pt);
			SetWindowPos(m_hwndDraggingMark, NULL, pt.x, pt.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
		}
	}

	if (!m_bShowDragging)
	{
		ShowWindow(m_hwndDraggingMark, SW_SHOWNOACTIVATE);
		m_bShowDragging = TRUE;
	}
}

void CStdHeaderCustom::PaintItem(int iIndex, BOOL bImmdShow, RECT* prcItem)
{
	RECT rcItem, rc;
	if (prcItem)
		rcItem = *prcItem;
	else
		DefSubclassProc(m_hwndHeader, HDM_GETITEMRECT, iIndex, (LPARAM)&rcItem);
	//
	// 画背景
	//
	int iState;
	if (m_idxPressItem == iIndex)
		iState = HIS_PRESSED;
	else if (m_idxHotItem == iIndex)
		iState = HIS_HOT;
	else
		iState = HIS_NORMAL;
	if (m_pProc(m_hwndHeader, HDCDMSG::PREPAINT, HDCDPART::BK, iIndex, m_hCDC, &rcItem, iState) == HDCDRET::DODEF)
	{
		DrawThemeBackground(m_hTheme, m_hCDC, HP_HEADERITEM, iState, &rcItem, NULL);
		m_pProc(m_hwndHeader, HDCDMSG::POSTPAINT, HDCDPART::BK, iIndex, m_hCDC, &rcItem, 0);
	}
	//
	// 画修饰元素（排序标记等）
	//
	HDITEMW hi;
	hi.mask = HDI_FORMAT;
	DefSubclassProc(m_hwndHeader, HDM_GETITEM, iIndex, (LPARAM)&hi);
	int iFmt = hi.fmt;
	if (m_pProc(m_hwndHeader, HDCDMSG::PREPAINT, HDCDPART::DECORATION, iIndex, m_hCDC, &rcItem, iFmt) == HDCDRET::DODEF)
	{
		SIZE size;
		if (hi.fmt & HDF_SORTUP)
		{
			GetThemePartSize(m_hTheme, m_hCDC, HP_HEADERSORTARROW, HSAS_SORTEDUP, NULL, TS_DRAW, &size);
			rc.left = ((rcItem.right - rcItem.left) - size.cx) / 2;
			rc.top = 0;
			rc.right = rc.left + size.cx;
			rc.bottom = rc.top + size.cy;
			DrawThemeBackground(m_hTheme, m_hCDC, HP_HEADERSORTARROW, HSAS_SORTEDUP, &rc, NULL);
		}
		else if (hi.fmt & HDF_SORTDOWN)
		{
			GetThemePartSize(m_hTheme, m_hCDC, HP_HEADERSORTARROW, HSAS_SORTEDDOWN, NULL, TS_DRAW, &size);
			rc.left = ((rcItem.right - rcItem.left) - size.cx) / 2;
			rc.top = 0;
			rc.right = rc.left + size.cx;
			rc.bottom = rc.top + size.cy;
			DrawThemeBackground(m_hTheme, m_hCDC, HP_HEADERSORTARROW, HSAS_SORTEDDOWN, &rc, NULL);
		}

		m_pProc(m_hwndHeader, HDCDMSG::POSTPAINT, HDCDPART::DECORATION, iIndex, m_hCDC, &rcItem, iFmt);
	}
	//
	// 画文本
	//
	if (m_pProc(m_hwndHeader, HDCDMSG::PREPAINT, HDCDPART::TEXT, iIndex, m_hCDC, &rcItem, 0) == HDCDRET::DODEF)
	{
		if (m_pProc(m_hwndHeader, HDCDMSG::GETTEXT, HDCDPART::NONE, iIndex, NULL, NULL, (WPARAM)&hi.pszText) == HDCDRET::DODEF)
		{
			hi.mask = HDI_TEXT;
			WCHAR sz[260 + 1];
			hi.pszText = sz;
			hi.cchTextMax = 260;// 这里到底包不包含结尾NULL？MSDN上说的奇奇怪怪的
			DefSubclassProc(m_hwndHeader, HDM_GETITEM, iIndex, (LPARAM)&hi);
		}

		UINT uDTFlag;
		if (iFmt & HDF_CENTER)
			uDTFlag = DT_CENTER;
		else if (iFmt & HDF_RIGHT)
			uDTFlag = DT_RIGHT;
		else
			uDTFlag = DT_LEFT;

		int temp = DPI(3);
		rc.left = rcItem.left + temp;
		rc.right = rcItem.right - temp;
		rc.top = rcItem.top;
		rc.bottom = rcItem.bottom;
		DrawTextW(m_hCDC, hi.pszText, -1, &rc, uDTFlag | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
		m_pProc(m_hwndHeader, HDCDMSG::POSTPAINT, HDCDPART::TEXT, iIndex, m_hCDC, &rc, 0);
	}
	//
	// 显示
	//
	if (bImmdShow)
	{
		HDC hDC = GetDC(m_hwndHeader);
		BitBlt(hDC, rcItem.left, rcItem.top, rcItem.right - rcItem.left, rcItem.bottom - rcItem.top,
			m_hCDC, rcItem.left, rcItem.top, SRCCOPY);
		ReleaseDC(m_hwndHeader, hDC);
	}
}

LRESULT CALLBACK CStdHeaderCustom::SubClassProc_Header(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto p = (CStdHeaderCustom*)dwRefData;
	switch (uMsg)
	{
	case WM_MOUSEMOVE:
	{
		HDHITTESTINFO hdhti;
		hdhti.pt = GET_PT_LPARAM(lParam);
		hdhti.iItem = -1;
		DefSubclassProc(hWnd, HDM_HITTEST, 0, (LPARAM)&hdhti);
		if (!p->m_bMoved)
			p->m_bMoved = TRUE;

		if (p->m_bDragging)
		{
			if (hdhti.iItem >= 0)
			{
				RECT rc;
				DefSubclassProc(hWnd, HDM_GETITEMRECT, hdhti.iItem, (LPARAM)&rc);
				int iOld = p->m_idxDragging;
				if (hdhti.pt.x < rc.left + (rc.right - rc.left) / 2)
					p->m_idxDragging = hdhti.iItem;
				else
				{
					HDITEMW hi;
					hi.mask = HDI_ORDER;
					DefSubclassProc(hWnd, HDM_GETITEM, hdhti.iItem, (LPARAM)&hi);
					int iCount = DefSubclassProc(hWnd, HDM_GETITEMCOUNT, 0, 0);
					auto piOrder = new int[iCount];
					DefSubclassProc(hWnd, HDM_GETORDERARRAY, iCount, (LPARAM)piOrder);

					if (hi.iOrder == iCount - 1)
						p->m_idxDragging = iCount;
					else
						p->m_idxDragging = piOrder[hi.iOrder + 1];

					delete[] piOrder;
				}
				if (iOld != p->m_idxDragging)
					p->ResetDraggingMark();
			}
		}
		else
		{
			p->m_idxHotItem = hdhti.iItem;
		}

		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = hWnd;
		TrackMouseEvent(&tme);
	}
	break;

	case WM_MOUSELEAVE:
	{
		p->m_idxHotItem = -1;
	}
	break;

	case WM_LBUTTONDOWN:
	{
		HDHITTESTINFO hdhti;
		hdhti.pt = GET_PT_LPARAM(lParam);
		hdhti.iItem = -1;
		DefSubclassProc(hWnd, HDM_HITTEST, 0, (LPARAM)&hdhti);

		p->m_idxPressItem = hdhti.iItem;
	}
	break;

	case WM_LBUTTONUP:
	{
		LRESULT lResult = DefSubclassProc(hWnd, uMsg, wParam, lParam);// 先call默认过程，因为控件内部可能要重排列

		p->m_idxPressItem = -1;
		p->m_idxDragging = -1;

		HDHITTESTINFO hdhti;
		hdhti.pt = GET_PT_LPARAM(lParam);
		hdhti.iItem = -1;
		DefSubclassProc(hWnd, HDM_HITTEST, 0, (LPARAM)&hdhti);
		p->m_idxHotItem = hdhti.iItem;

		if (p->m_bMoved)// 奶奶的不知道为什么有时候不重画，手动重画一下
		{
			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);
			p->m_bMoved = FALSE;
		}

		return lResult;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		int iCount = DefSubclassProc(hWnd, HDM_GETITEMCOUNT, 0, 0);
		RECT rc;
		FillRect(p->m_hCDC, &ps.rcPaint, GetSysColorBrush(COLOR_WINDOW));// 刷掉背景
		for (int i = 0; i < iCount; ++i)
		{
			DefSubclassProc(hWnd, HDM_GETITEMRECT, i, (LPARAM)&rc);
			if (RectVisible(ps.hdc, &rc))// 不在剪辑范围，不画
				p->PaintItem(i, FALSE, &rc);
		}
		BitBlt(
			ps.hdc,
			ps.rcPaint.left,
			ps.rcPaint.top,
			ps.rcPaint.right - ps.rcPaint.left,
			ps.rcPaint.bottom - ps.rcPaint.top,
			p->m_hCDC,
			ps.rcPaint.left,
			ps.rcPaint.top,
			SRCCOPY);// 贴图
		EndPaint(hWnd, &ps);
	}
	return 0;

	case WM_SIZE:
	{
		GET_SIZE_LPARAM(p->m_rcClient.right, p->m_rcClient.bottom, lParam);

		DeleteObject(SelectObject(p->m_hCDC, p->m_hOldBmp));
		DeleteDC(p->m_hCDC);

		HDC hDC = GetDC(hWnd);
		p->m_hCDC = CreateCompatibleDC(hDC);
		p->m_hBitmap = CreateCompatibleBitmap(hDC, p->m_rcClient.right, p->m_rcClient.bottom);
		p->m_hOldBmp = SelectObject(p->m_hCDC, p->m_hBitmap);
		SelectObject(p->m_hCDC, p->m_hFont);
		ReleaseDC(hWnd, hDC);
		SetBkMode(p->m_hCDC, TRANSPARENT);
	}
	break;

	case WM_THEMECHANGED:
	{
		CloseThemeData(p->m_hTheme);
		p->m_hTheme = OpenThemeData(hWnd, L"Header");
	}
	break;

	case WM_SETFONT:
	{
		p->m_hFont = (HFONT)wParam;
		SelectObject(p->m_hCDC, p->m_hFont);
	}
	break;

	case HDM_LAYOUT:
	{
		if (p->m_cyHeader != 0)
		{
			LRESULT lResult = DefSubclassProc(hWnd, uMsg, wParam, lParam);// call默认过程，让控件填充结构
			auto phdlo = (HDLAYOUT*)lParam;
			phdlo->prc->top = p->m_cyHeader;// 这个矩形是ListView工作区的矩形，就是表头矩形的补集
			phdlo->pwpos->cy = p->m_cyHeader;
			return lResult;
		}
	}
	break;

	case WM_DPICHANGED:
	{
		p->m_iDpi = HIWORD(wParam);
	}
	break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CStdHeaderCustom::SubClassProc_LV(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto p = (CStdHeaderCustom*)dwRefData;
	switch (uMsg)
	{
	case WM_NOTIFY:
	{
		UINT uCode = ((NMHDR*)lParam)->code;
		switch (uCode)
		{
		case HDN_BEGINDRAG:// 还是用这俩比较靠谱一点，处理鼠标消息感觉有点....
		{
			if (DefSubclassProc(hWnd, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0) & LVS_EX_HEADERDRAGDROP)
			{
				p->m_bDragging = TRUE;
				RECT rc;
				GetClientRect(p->m_hwndHeader, &rc);
				SetWindowPos(p->m_hwndDraggingMark, NULL, 0, 0, p->DPI(4), rc.bottom, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
			}
		}
		break;

		case HDN_ENDDRAG:
		{
			p->m_bDragging = FALSE;
			p->m_idxDragging = -1;
			p->m_bShowDragging = FALSE;
			ShowWindow(p->m_hwndDraggingMark, SW_HIDE);
		}
		break;
		}
	}
	break;
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CStdHeaderCustom::WndProc_DraggingMark(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto p = (CStdHeaderCustom*)GetWindowLongPtrW(hWnd, 0);
	if (p)
	{
		switch (uMsg)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			FillRect(ps.hdc, &ps.rcPaint, p->m_hbrDraggingMark);
			EndPaint(hWnd, &ps);
		}
		return 0;
		}
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}