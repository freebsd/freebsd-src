#include "stdafx.h"
#include "CLeashDragListBox.h"
#include "leash.h"
#include "lglobals.h"

/////////////////////////////////////////////////////////////////////////////
// CLeashDragListBox

//IMPLEMENT_DYNAMIC(CLeashDragListBox, CDragListBox)

CLeashDragListBox::CLeashDragListBox()
 :CDragListBox()
{

}

CLeashDragListBox::~CLeashDragListBox()
{
	DestroyWindow();
}

void CLeashDragListBox::initOtherListbox(CPropertyPage* pPage, CListBox* pOtherListBox)
{
	m_pPage = pPage;
	m_pOtherListBox = pOtherListBox;
}


void CLeashDragListBox::PreSubclassWindow()
{
	ASSERT(::IsWindow(m_hWnd));
	ASSERT((GetStyle() & (LBS_MULTIPLESEL|LBS_SORT)) == 0);
	MakeDragList(m_hWnd);
}

BOOL CLeashDragListBox::BeginDrag(CPoint pt)
{
	m_nLast = -1;
	DrawInsert(ItemFromPt(pt));
	return TRUE;
}

void CLeashDragListBox::CancelDrag(CPoint)
{
	DrawInsert(-1);
}

UINT CLeashDragListBox::Dragging(CPoint pt)
{
	int nIndex = ItemFromPt(pt, FALSE); // don't allow scrolling just yet
	DrawInsert(nIndex);
	ItemFromPt(pt);
	return (nIndex == LB_ERR) ? DL_STOPCURSOR : DL_MOVECURSOR;
}

void CLeashDragListBox::Dropped(int nSrcIndex, CPoint pt)
{
	ASSERT(!(GetStyle() & (LBS_OWNERDRAWFIXED|LBS_OWNERDRAWVARIABLE)) ||
		(GetStyle() & LBS_HASSTRINGS));

	DrawInsert(-1);
	int nDestIndex = ItemFromPt(pt);

	if (nSrcIndex == -1 || nDestIndex == -1)
		return;
	if (nDestIndex == nSrcIndex || nDestIndex == nSrcIndex+1)
		return; //didn't move
	CString str1, str2;
	DWORD_PTR dwData;
	GetText(nSrcIndex, str1);
	GetText(nDestIndex, str2);
	dwData = GetItemData(nSrcIndex);
	DeleteString(nSrcIndex);
	if (nSrcIndex < nDestIndex)
		nDestIndex--;
	nDestIndex = InsertString(nDestIndex, str1);
	SetItemData(nDestIndex, dwData);
	SetCurSel(nDestIndex);

	// Save new order of items to profile linklist
	char theSection[REALM_SZ + 1];
	const char*  adminServer[] = {"realms", theSection, ADMIN_SERVER, NULL};
	const char* Section[] = {"realms", theSection, NULL};
	const char** adminServ = adminServer;
	const char** section = Section;
	const char* valueSection[] = {"realms", theSection, "kdc", NULL};
	const char** valueSec = valueSection;
	CString theValue;
	CHAR hostServer[MAX_HSTNM];

	if (LB_ERR == m_pOtherListBox->GetText(m_pOtherListBox->GetCurSel(), theSection))
      ASSERT(0);

	long retval = pprofile_rename_section(CLeashApp::m_krbv5_profile,
										   section, NULL);
	if (retval)
	{
		MessageBox("Dropped::There is on error, profile will not be saved!!!\
                   \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}

	retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
								    section, NULL);
	if (retval)
	{
		MessageBox("Dropped::There is on error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}

	for (INT maxItems = GetCount(), item = 0; item < maxItems; item++)
	{
		GetText(item, hostServer);
		//strcpy(hostServer, theValue);

		if (strstr(hostServer, ADMIN_SERVER))
		{
			char* pAdmin = strchr(hostServer, ' ');
			if (pAdmin)
			  *pAdmin = 0;
			else
			  ASSERT(0);

			retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
											adminServ, hostServer);
			if (retval)
			{
				MessageBox("Dropped::There is on error, profile will not be saved!!!\
                           \nIf this error persist, contact your administrator.",
						   "Leash", MB_OK);
				return;
			}
		}

		retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
										valueSec, hostServer);
		if (retval)
		{
			MessageBox("Dropped::There is on error, profile will not be saved!!!\
                       \nIf this error persist, contact your administrator.",
					   "Leash", MB_OK);
			return;
		}
	}

	m_pPage->SetModified(TRUE);
}

void CLeashDragListBox::DrawInsert(int nIndex)
{
	if (m_nLast != nIndex)
	{
		DrawSingle(m_nLast);
		DrawSingle(nIndex);
		m_nLast = nIndex;
	}
}

void CLeashDragListBox::DrawSingle(int nIndex)
{
	if (nIndex == -1)
		return;
	CBrush* pBrush = CDC::GetHalftoneBrush();
	CRect rect;
	GetClientRect(&rect);
	CRgn rgn;
	rgn.CreateRectRgnIndirect(&rect);

	CDC* pDC = GetDC();
	// prevent drawing outside of listbox
	// this can happen at the top of the listbox since the listbox's DC is the
	// parent's DC
	pDC->SelectClipRgn(&rgn);

	GetItemRect(nIndex, &rect);
	rect.bottom = rect.top+2;
	rect.top -= 2;
	CBrush* pBrushOld = pDC->SelectObject(pBrush);
	//draw main line
	pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);

	pDC->SelectObject(pBrushOld);
	ReleaseDC(pDC);
}

/*
BOOL CLeashDragListBox::OnChildNotify(UINT nMessage, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (nMessage != m_nMsgDragList)
		return CListBox::OnChildNotify(nMessage, wParam, lParam, pResult);

	ASSERT(pResult != NULL);
	LPDRAGLISTINFO pInfo = (LPDRAGLISTINFO)lParam;
	ASSERT(pInfo != NULL);
	switch (pInfo->uNotification)
	{
	case DL_BEGINDRAG:
		*pResult = BeginDrag(pInfo->ptCursor);
		break;
	case DL_CANCELDRAG:
		CancelDrag(pInfo->ptCursor);
		break;
	case DL_DRAGGING:
		*pResult = Dragging(pInfo->ptCursor);
		break;
	case DL_DROPPED:
		Dropped(GetCurSel(), pInfo->ptCursor);
		break;
	}
	return TRUE;
}
*/
