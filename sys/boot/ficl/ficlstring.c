/*******************************************************************
** f i c l s t r i n g . c
** Forth Inspired Command Language
** ANS STRING words plus ficl extras for c-string class
** Author: John Sadler (john_sadler@alum.mit.edu)
** Created: 2 June 2000
** 
*******************************************************************/

/* $FreeBSD$ */

#ifdef TESTMAIN
#include <ctype.h>
#else
#include <stand.h>
#endif
#include <string.h>
#include "ficl.h"


/**************************************************************************
                        f o r m a t
** ( params... fmt-addr fmt-u dest-addr dest-u -- dest-addr dest-u )
**************************************************************************/

void ficlStrFormat(FICL_VM *pVM)
{
	return;
}
