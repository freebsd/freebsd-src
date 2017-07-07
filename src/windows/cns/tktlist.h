/* windows/cns/tktlist.h */
/*
 * Copyright 1994 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

/* Handle all actions of the Kerberos ticket list. */

/* Only one time, please */
#ifndef	TKTLIST_DEFS
#define TKTLIST_DEFS

/*
 * Prototypes
 */
BOOL ticket_init_list(HWND);

void ticket_destroy(HWND);

void ticket_measureitem(HWND, MEASUREITEMSTRUCT *);

void ticket_drawitem(HWND, const DRAWITEMSTRUCT *);

#endif
