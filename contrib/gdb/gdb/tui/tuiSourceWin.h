#ifndef _TUI_SOURCEWIN_H
#define _TUI_SOURCEWIN_H
/*
** This header file supports
*/


extern void         tuiDisplayMainFunction PARAMS ((void));
extern void         tuiUpdateSourceWindow PARAMS 
                    	((TuiWinInfoPtr, struct symtab *, Opaque, int));
extern void         tuiUpdateSourceWindowAsIs PARAMS 
                        ((TuiWinInfoPtr, struct symtab *, Opaque, int));
extern void         tuiUpdateSourceWindowsWithAddr PARAMS ((Opaque));
extern void         tui_vUpdateSourceWindowsWithAddr PARAMS ((va_list));
extern void         tuiUpdateSourceWindowsWithLine PARAMS ((struct symtab *, int));
extern void         tui_vUpdateSourceWindowsWithLine PARAMS ((va_list));
extern void         tuiUpdateSourceWindowsFromLocator PARAMS ((void));
extern void         tuiClearSourceContent PARAMS ((TuiWinInfoPtr, int));
extern void         tuiClearAllSourceWinsContent PARAMS ((int));
extern void         tuiEraseSourceContent PARAMS ((TuiWinInfoPtr, int));
extern void         tuiEraseAllSourceWinsContent PARAMS ((int));
extern void         tuiSetSourceContentNil PARAMS ((TuiWinInfoPtr, char *));
extern void         tuiShowSourceContent PARAMS ((TuiWinInfoPtr));
extern void         tuiShowAllSourceWinsContent PARAMS ((void));
extern void         tuiHorizontalSourceScroll PARAMS ((TuiWinInfoPtr, TuiScrollDirection, int));
extern void         tuiUpdateOnEnd PARAMS ((void));

extern TuiStatus    tuiSetExecInfoContent PARAMS ((TuiWinInfoPtr));
extern void         tuiShowExecInfoContent PARAMS ((TuiWinInfoPtr));
extern void         tuiShowAllExecInfosContent PARAMS ((void));
extern void         tuiEraseExecInfoContent PARAMS ((TuiWinInfoPtr));
extern void         tuiEraseAllExecInfosContent PARAMS ((void));
extern void         tuiClearExecInfoContent PARAMS ((TuiWinInfoPtr));
extern void         tuiClearAllExecInfosContent PARAMS ((void));
extern void         tuiUpdateExecInfo PARAMS ((TuiWinInfoPtr));
extern void         tuiUpdateAllExecInfos PARAMS ((void));

extern void         tuiSetIsExecPointAt PARAMS ((Opaque, TuiWinInfoPtr));
extern void         tuiSetHasBreakAt PARAMS ((struct breakpoint *, TuiWinInfoPtr, int));
extern void         tuiAllSetHasBreakAt PARAMS ((struct breakpoint *, int));
extern void         tui_vAllSetHasBreakAt PARAMS ((va_list));
extern TuiStatus    tuiAllocSourceBuffer PARAMS ((TuiWinInfoPtr));
extern int          tuiLineIsDisplayed PARAMS ((Opaque, TuiWinInfoPtr, int));


/*
** Constant definitions
*/
#define        SCROLL_THRESHOLD            2 /* threshold for lazy scroll */


/*
** Macros 
*/
#define    m_tuiSetBreakAt(bp, winInfo)       tuiSetHasBreakAt((bp, winInfo, TRUE)
#define    m_tuiClearBreakAt(bp, winInfo)     tuiSetHasBreakAt(bp, winInfo, FALSE)

#define    m_tuiAllSetBreakAt(bp)             tuiAllSetHasBreakAt(bp, TRUE)
#define    m_tuiAllClearBreakAt(bp)           tuiAllSetHasBreakAt(bp, FALSE)

#define    m_tuiSrcLineDisplayed(lineNo)      tuiLineIsDisplayed((Opaque)(lineNo), srcWin, FALSE)
#define    m_tuiSrcAddrDisplayed(addr)        tuiLineIsDisplayed((Opaque)(addr), disassemWin, FALSE)
#define    m_tuiSrcLineDisplayedWithinThreshold(lineNo) \
                                            tuiLineIsDisplayed((Opaque)(lineNo), srcWin, TRUE)
#define    m_tuiSrcAddrDisplayedWithinThreshold(addr) \
                                            tuiLineIsDisplayed((Opaque)(addr), disassemWin, TRUE)
#define m_tuiLineDisplayedWithinThreshold(winInfo, lineOrAddr)                                 \
                                    ( (winInfo == srcWin) ?                                    \
                                        m_tuiSrcLineDisplayedWithinThreshold(lineOrAddr) :    \
                                        m_tuiSrcAddrDisplayedWithinThreshold(lineOrAddr) )



#endif /*_TUI_SOURCEWIN_H */
