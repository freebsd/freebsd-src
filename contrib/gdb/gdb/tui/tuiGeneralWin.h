#ifndef TUI_GENERAL_WIN_H
#define TUI_GENERAL_WIN_H

/*
** Functions
*/
extern void             tuiClearWin PARAMS ((TuiGenWinInfoPtr));
extern void             unhighlightWin PARAMS ((TuiWinInfoPtr));
extern void             makeVisible PARAMS ((TuiGenWinInfoPtr, int));
extern void             makeAllVisible PARAMS ((int));
extern void             scrollWinForward PARAMS ((TuiGenWinInfoPtr, int));
extern void             scrollWinBackward PARAMS ((TuiGenWinInfoPtr, int));
extern void             makeWindow PARAMS ((TuiGenWinInfoPtr, int));
extern TuiWinInfoPtr    copyWin PARAMS ((TuiWinInfoPtr));
extern void             boxWin PARAMS ((TuiGenWinInfoPtr, int));
extern void             highlightWin PARAMS ((TuiWinInfoPtr));
extern void             checkAndDisplayHighlightIfNeeded PARAMS ((TuiWinInfoPtr));
extern void             refreshAll PARAMS ((TuiWinInfoPtr *));
extern void             tuiDelwin PARAMS ((WINDOW  *window));
extern void             tuiRefreshWin PARAMS ((TuiGenWinInfoPtr));

/*
** Macros
*/
#define    m_beVisible(winInfo)   makeVisible((TuiGenWinInfoPtr)(winInfo), TRUE)
#define    m_beInvisible(winInfo) \
                            makeVisible((TuiGenWinInfoPtr)(winInfo), FALSE)
#define    m_allBeVisible()       makeAllVisible(TRUE)
#define m_allBeInvisible()        makeAllVisible(FALSE)

#endif /*TUI_GENERAL_WIN_H*/
