#ifndef _LEASH_DRAGLISTBOX
#define _LEASH_DRAGLISTBOX

/////////////////////////////////////////////////////////////////////////////
// CLeashDragListBox

//#include "AFXCMN.h"

class CLeashDragListBox : public CDragListBox
{
	//DECLARE_DYNAMIC(CDragListBoxCLeashDragListBox)

	CListBox* m_pOtherListBox;
	CPropertyPage* m_pPage;

// Constructors
public:
	CLeashDragListBox();
	void initOtherListbox(CPropertyPage* pPage, CListBox* pOtherListBox);

// Attributes
	//int ItemFromPt(CPoint pt, BOOL bAutoScroll = TRUE) const;

// Operations
	virtual void DrawInsert(int nItem);

// Overridables
	virtual BOOL BeginDrag(CPoint pt);
	virtual void CancelDrag(CPoint pt);
	virtual UINT Dragging(CPoint pt);
	virtual void Dropped(int nSrcIndex, CPoint pt);

// Implementation
public:
	int m_nLast;
	void DrawSingle(int nIndex);
	virtual void PreSubclassWindow();
	virtual ~CLeashDragListBox();
protected:
	//virtual BOOL OnChildNotify(UINT, WPARAM, LPARAM, LRESULT*);
};

//class CLeashDragListBox;

#endif // _LEASH_DRAGLISTBOX
