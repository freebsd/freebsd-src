#ifndef _TUI_STACK_H
#define _TUI_STACK_H
/*
** This header file supports
*/

extern void         tuiSetLocatorInfo PARAMS ((char *, char *, int, Opaque, TuiLocatorElementPtr));
extern void         tuiUpdateLocatorFilename PARAMS ((char *));
extern void         tui_vUpdateLocatorFilename PARAMS ((va_list));
extern void         tuiUpdateLocatorInfoFromFrame 
                        PARAMS ((struct frame_info *, TuiLocatorElementPtr));
extern void         tuiUpdateLocatorDisplay PARAMS ((struct frame_info *));
extern void         tuiSetLocatorContent PARAMS ((struct frame_info *));
extern void         tuiShowLocatorContent PARAMS ((void));
extern void         tuiClearLocatorContent PARAMS ((void));
extern void         tuiSwitchFilename PARAMS ((char *));
extern void         tuiShowFrameInfo PARAMS ((struct frame_info *));
extern void         tui_vShowFrameInfo PARAMS ((va_list));
extern void         tuiGetLocatorFilename PARAMS ((TuiGenWinInfoPtr, char **));


#endif /*_TUI_STACK_H*/
