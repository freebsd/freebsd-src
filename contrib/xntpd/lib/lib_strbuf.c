/* lib_strbuf.c,v 3.1 1993/07/06 01:08:27 jbj Exp
 * lib_strbuf - library string storage
 */

#include "lib_strbuf.h"

/*
 * Storage declarations
 */
char lib_stringbuf[LIB_NUMBUFS][LIB_BUFLENGTH];
int lib_nextbuf;


/*
 * initialization routine.  Might be needed if the code is ROMized.
 */
void
init_lib()
{
	lib_nextbuf = 0;
}
