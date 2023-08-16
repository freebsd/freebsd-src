
#ifndef TESTMAIN
#include <machine/cpufunc.h>

/* 
 * outb ( port# c -- )
 * Store a byte to I/O port number port#
 */
void
ficlOutb(FICL_VM *pVM)
{
	u_char c;
	uint32_t port;

	port=stackPopUNS(pVM->pStack);
	c=(u_char)stackPopINT(pVM->pStack);
	outb(port,c);
}

/*
 * inb ( port# -- c )
 * Fetch a byte from I/O port number port#
 */
void
ficlInb(FICL_VM *pVM)
{
	u_char c;
	uint32_t port;

	port=stackPopUNS(pVM->pStack);
	c=inb(port);
	stackPushINT(pVM->pStack,c);
}

/*
 * Glue function to add the appropriate forth words to access x86 special cpu
 * functionality.
 */
static void ficlCompileCpufunc(FICL_SYSTEM *pSys)
{
    FICL_DICT *dp = pSys->dp;
    assert (dp);

    dictAppendWord(dp, "outb",      ficlOutb,       FW_DEFAULT);
    dictAppendWord(dp, "inb",       ficlInb,        FW_DEFAULT);
}

FICL_COMPILE_SET(ficlCompileCpufunc);

#endif
