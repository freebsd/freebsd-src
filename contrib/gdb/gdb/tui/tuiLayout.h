#ifndef TUI_LAYOUT_H
#define TUI_LAYOUT_H

extern void             showLayout PARAMS ((TuiLayoutType));
extern void             tuiAddWinToLayout PARAMS ((TuiWinType));
extern void             tui_vAddWinToLayout PARAMS ((va_list));
extern int              tuiDefaultWinHeight 
                             PARAMS ((TuiWinType, TuiLayoutType));
extern int              tuiDefaultWinViewportHeight 
                             PARAMS ((TuiWinType, TuiLayoutType));
extern TuiStatus        tuiSetLayout 
                             PARAMS ((TuiLayoutType, TuiRegisterDisplayType));
extern TuiStatus        tui_vSetLayoutTo PARAMS ((va_list));

#endif /*TUI_LAYOUT_H*/
