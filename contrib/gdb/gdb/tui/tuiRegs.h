#ifndef _TUI_REGS_H
#define _TUI_REGS_H
/*
** This header file supports the display of registers in the data window.
*/

/*****************************************
** TYPE DEFINITIONS                        **
******************************************/



/*****************************************
** PUBLIC FUNCTION EXTERNAL DECLS        **
******************************************/
extern void     tuiCheckRegisterValues PARAMS ((struct frame_info *));
extern void     tuiShowRegisters PARAMS ((TuiRegisterDisplayType));
extern void     tuiDisplayRegistersFrom PARAMS ((int));
extern int      tuiDisplayRegistersFromLine PARAMS ((int, int));
extern int      tuiLastRegsLineNo PARAMS ((void));
extern int      tuiFirstRegElementInLine PARAMS ((int));
extern int      tuiLastRegElementInLine PARAMS ((int));
extern int      tuiLineFromRegElementNo PARAMS ((int));
extern void     tuiToggleFloatRegs PARAMS ((void));
extern int      tuiCalculateRegsColumnCount PARAMS ((TuiRegisterDisplayType));


#endif /*_TUI_REGS_H*/
