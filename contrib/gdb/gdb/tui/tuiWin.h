#ifndef _TUI_WIN_H
#define _TUI_WIN_H
/*
** This header file supports
*/

/*****************************************
** TYPE DEFINITIONS                        **
******************************************/



/*****************************************
** PUBLIC FUNCTION EXTERNAL DECLS        **
******************************************/
extern void             tuiScrollForward PARAMS ((TuiWinInfoPtr, int));
extern void             tuiScrollBackward PARAMS ((TuiWinInfoPtr, int));
extern void             tuiScrollLeft PARAMS ((TuiWinInfoPtr, int));
extern void             tuiScrollRight PARAMS ((TuiWinInfoPtr, int));
extern void             tui_vScroll PARAMS ((va_list));
extern void             tuiSetWinFocusTo PARAMS ((TuiWinInfoPtr));
extern void             tuiClearWinFocusFrom PARAMS ((TuiWinInfoPtr));
extern void             tuiClearWinFocus PARAMS ((void));
extern void             tuiResizeAll PARAMS ((void));
extern void             tuiRefreshAll PARAMS ((void));
extern void             tuiSigwinchHandler PARAMS ((int));

#endif /*_TUI_WIN_H*/
