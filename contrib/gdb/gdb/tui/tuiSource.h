#ifndef _TUI_SOURCE_H
#define _TUI_SOURCE_H
/*
** This header file supports
*/


#include "defs.h"
#if 0
#include "symtab.h"
#include "breakpoint.h"
#endif

extern TuiStatus    tuiSetSourceContent PARAMS ((struct symtab *, int, int));
extern void         tuiShowSource PARAMS ((struct symtab *, Opaque, int));
extern void         tuiShowSourceAsIs PARAMS ((struct symtab *, Opaque, int));
extern int          tuiSourceIsDisplayed PARAMS ((char *));
extern void         tuiVerticalSourceScroll PARAMS ((TuiScrollDirection, int));


/*******************
** MACROS         **
*******************/
#define m_tuiShowSourceAsIs(s, line, noerror)    tuiUpdateSourceWindowAsIs(srcWin, s, line, noerror)


#endif /*_TUI_SOURCE_H*/
