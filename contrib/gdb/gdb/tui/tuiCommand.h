#ifndef _TUI_COMMAND_H
#define _TUI_COMMAND_H
/*
** This header file supports
*/


/*****************************************
** TYPE DEFINITIONS                        **
******************************************/



/*****************************************
** PUBLIC FUNCTION EXTERNAL DECLS        **
******************************************/

extern unsigned int     tuiDispatchCtrlChar PARAMS ((unsigned int));
extern int              tuiIncrCommandCharCountBy PARAMS ((int));
extern int              tuiDecrCommandCharCountBy PARAMS ((int));
extern int              tuiSetCommandCharCountTo PARAMS ((int));
extern int              tuiClearCommandCharCount PARAMS ((void));

#endif /*_TUI_COMMAND_H*/
