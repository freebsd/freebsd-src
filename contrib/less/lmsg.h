/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Define the LM_* symbols.
 * */
#undef  M
#define M(sym,msg) LM_##sym,
typedef enum lessmsg_id {
	LM_NULL = 0,
#include "lessmsg.inc"
	LM_HELP
} lessmsg_id;

/*
 * Use LM(sym) to get the message string for the symbol LM_sym.
 */
#define LM(sym)  lmsg(LM_##sym)
