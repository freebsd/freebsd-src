/* External/Public TUI Header File */

#ifndef TUI_H
#define TUI_H
#include <curses.h>

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ansidecl.h"

#if defined(reg)
#undef reg
#endif
#if defined(chtype)
#undef chtype
#endif

/* Opaque data type */
typedef char *Opaque;
typedef Opaque (*OpaqueFuncPtr) PARAMS ((va_list));
typedef char **OpaqueList;
typedef OpaqueList OpaquePtr;

/* Generic function pointer */
typedef     void              (*TuiVoidFuncPtr) PARAMS ((va_list));
typedef     int               (*TuiIntFuncPtr) PARAMS ((va_list));
/*
typedef     Opaque            (*TuiOpaqueFuncPtr) PARAMS ((va_list));
*/
typedef     OpaqueFuncPtr     TuiOpaqueFuncPtr;

extern Opaque vcatch_errors PARAMS ((OpaqueFuncPtr, ...));
extern Opaque va_catch_errors PARAMS ((OpaqueFuncPtr, va_list));

extern void strcat_to_buf PARAMS ((char *, int, char *));
extern void strcat_to_buf_with_fmt PARAMS ((char *, int, char *, ...));

/* Types of error returns */
typedef enum {
    TUI_SUCCESS,
    TUI_FAILURE
} TuiStatus, *TuiStatusPtr;

/* Types of windows */
typedef enum {
    SRC_WIN = 0,
    DISASSEM_WIN,
    DATA_WIN,
    CMD_WIN,
    /* This must ALWAYS be AFTER the major windows last */
    MAX_MAJOR_WINDOWS,
    /* auxillary windows */
    LOCATOR_WIN,
    EXEC_INFO_WIN,
    DATA_ITEM_WIN,
    /* This must ALWAYS be next to last */
    MAX_WINDOWS,
    UNDEFINED_WIN /* LAST */
} TuiWinType, *TuiWinTypePtr;

/* This is a point definition */
typedef struct _TuiPoint {
    int x, y;
} TuiPoint, *TuiPointPtr;

/* Generic window information */
typedef struct _TuiGenWinInfo {
    WINDOW          *handle; /* window handle */
    TuiWinType      type; /* type of window */
    int             width; /* window width */
    int             height; /* window height */
    TuiPoint        origin; /* origin of window */
    OpaquePtr       content; /* content of window */
    int             contentSize; /* Size of content (# of elements) */
    int             contentInUse; /* Can it be used, or is it already used? */
    int             viewportHeight; /* viewport height */
    int             lastVisibleLine; /* index of last visible line */
    int             isVisible; /* whether the window is visible or not */
} TuiGenWinInfo, *TuiGenWinInfoPtr;

/* GENERAL TUI FUNCTIONS */
/* tui.c */
extern void          tuiInit PARAMS ((char *argv0));
extern void          tuiInitWindows PARAMS ((void));
extern void          tuiResetScreen PARAMS ((void));
extern void          tuiCleanUp PARAMS ((void));
extern void          tuiError PARAMS ((char *, int));
extern void          tui_vError PARAMS ((va_list));
extern void          tuiFree PARAMS ((char *));
extern Opaque        tuiDo PARAMS ((TuiOpaqueFuncPtr, ...));
extern Opaque        tuiDoAndReturnToTop PARAMS ((TuiOpaqueFuncPtr, ...));
extern Opaque        tuiGetLowDisassemblyAddress PARAMS ((Opaque, Opaque));
extern Opaque        tui_vGetLowDisassemblyAddress PARAMS ((va_list));
extern void          tui_vSelectSourceSymtab PARAMS ((va_list));

/* tuiDataWin.c */
extern void		tui_vCheckDataValues PARAMS ((va_list));

/* tuiIO.c */
extern void		tui_vStartNewLines PARAMS ((va_list));

/* tuiLayout.c */
extern void		tui_vAddWinToLayout PARAMS ((va_list));
extern TuiStatus	tui_vSetLayoutTo PARAMS ((va_list));

/* tuiSourceWin.c */
extern void		tuiDisplayMainFunction PARAMS ((void));
extern void		tuiUpdateAllExecInfos PARAMS ((void));
extern void		tuiUpdateOnEnd PARAMS ((void));
extern void		tui_vAllSetHasBreakAt PARAMS ((va_list));
extern void		tui_vUpdateSourceWindowsWithAddr PARAMS ((va_list));

/* tuiStack.c */
extern void		tui_vShowFrameInfo PARAMS ((va_list));
extern void		tui_vUpdateLocatorFilename PARAMS ((va_list));
#endif /* TUI_H */
