/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
 * unctrl.h
 *
 * Display a printable version of a control character.
 * Control characters are displayed in caret notation (^x), DELETE is displayed
 * as ^?. Printable characters are displayed as is.
 *
 * The returned pointer points to a static buffer which gets overwritten by
 * each call. Therefore, you must copy the resulting string to a safe place
 * before calling unctrl() again.
 *
 */

#ifndef _UNCTRL_H
#define _UNCTRL_H	1

#undef  NCURSES_VERSION
#define NCURSES_VERSION "1.8.6/ache"

#ifdef __cplusplus
extern "C" {
#endif

#include <ncurses.h>

extern char *unctrl(chtype);

#ifdef __cplusplus
}
#endif

#endif /* _UNCTRL_H */
