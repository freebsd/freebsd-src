#ifndef _TUI_DATAWIN_H
#define _TUI_DATAWIN_H
/*
** This header file supports the display of registers/data in the data window.
*/


/*****************************************
** TYPE DEFINITIONS                        **
******************************************/



/*****************************************
** PUBLIC FUNCTION EXTERNAL DECLS        **
******************************************/
extern void     tuiEraseDataContent PARAMS ((char *));
extern void     tuiDisplayAllData PARAMS ((void));
extern void     tuiCheckDataValues PARAMS ((struct frame_info *));
extern void     tui_vCheckDataValues PARAMS ((va_list));
extern void     tuiDisplayDataFromLine PARAMS ((int));
extern int      tuiFirstDataItemDisplayed PARAMS ((void));
extern int      tuiFirstDataElementNoInLine PARAMS ((int));
extern void     tuiDeleteDataContentWindows PARAMS ((void));
extern void     tuiRefreshDataWin PARAMS ((void));
extern void     tuiDisplayDataFrom PARAMS ((int, int));
extern void     tuiVerticalDataScroll PARAMS ((TuiScrollDirection, int));

#endif /*_TUI_DATAWIN_H*/
