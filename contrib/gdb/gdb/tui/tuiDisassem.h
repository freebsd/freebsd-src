#ifndef _TUI_DISASSEM_H
#define _TUI_DISASSEM_H
/*
** This header file supports
*/

/*****************************************
** TYPE DEFINITIONS                        **
******************************************/



/*****************************************
** PUBLIC FUNCTION EXTERNAL DECLS        **
******************************************/
extern TuiStatus        tuiSetDisassemContent PARAMS ((struct symtab *, Opaque));
extern void             tuiShowDisassem PARAMS ((Opaque));
extern void             tuiShowDisassemAndUpdateSource PARAMS ((Opaque));
extern void             tuiVerticalDisassemScroll PARAMS ((TuiScrollDirection, int));
extern Opaque           tuiGetBeginAsmAddress PARAMS ((void));

#endif /*_TUI_DISASSEM_H*/
