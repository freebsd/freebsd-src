//	**************************************************************************************
//	File:			LeashFrame.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for LeashFrame.cpp. Contains variables and functions
//					for Leash
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#ifndef _LEASH_PERSISTENT_FRAME
#define _LEASH_PERSISTENT_FRAME

class CLeashFrame : public CFrameWndEx
{ // remembers where it was on the desktop
	DECLARE_DYNAMIC(CLeashFrame)
    static const CRect s_rectDefault;
	static const char s_profileHeading[];
	static const char s_profileRect[];
	static const char s_profileIcon[];
	static const char s_profileMax[];
	static const char s_profileTool[];
	static const char s_profileStatus[];

  private:
	BOOL m_bFirstTime;

  protected: // Create from serialization only
	CLeashFrame();
	~CLeashFrame();

	//{{AFX_VIRTUAL(CLeashFrame)
	public:
	virtual void ActivateFrame(int nCmdShow = -1);
	protected:
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CLeashFrame)
	afx_msg void OnDestroy();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

#endif // _LEASH_PERSISTENT_FRAME
