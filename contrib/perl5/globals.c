#include "INTERN.h"
#define PERL_IN_GLOBALS_C
#include "perl.h"

#ifdef PERL_OBJECT

#undef PERLVAR
#define PERLVAR(x, y)
#undef PERLVARA
#define PERLVARA(x, n, y)
#undef PERLVARI
#define PERLVARI(x, y, z) interp.x = z;
#undef PERLVARIC
#define PERLVARIC(x, y, z) interp.x = z;

CPerlObj::CPerlObj(IPerlMem* ipM, IPerlMem* ipMS, IPerlMem* ipMP,
		   IPerlEnv* ipE, IPerlStdIO* ipStd,
		   IPerlLIO* ipLIO, IPerlDir* ipD, IPerlSock* ipS,
		   IPerlProc* ipP)
{
    memset(((char*)this)+sizeof(void*), 0, sizeof(CPerlObj)-sizeof(void*));

#include "thrdvar.h"
#include "intrpvar.h"

    PL_Mem = ipM;
    PL_MemShared = ipMS;
    PL_MemParse = ipMP;
    PL_Env = ipE;
    PL_StdIO = ipStd;
    PL_LIO = ipLIO;
    PL_Dir = ipD;
    PL_Sock = ipS;
    PL_Proc = ipP;
}

void*
CPerlObj::operator new(size_t nSize, IPerlMem *pvtbl)
{
    if(pvtbl)
	return pvtbl->pMalloc(pvtbl, nSize);
#ifndef __MINGW32__
    /* operator new is supposed to throw std::bad_alloc */
    return NULL;
#endif
}

#ifndef __BORLANDC__
void
CPerlObj::operator delete(void *pPerl, IPerlMem *pvtbl)
{
    if(pvtbl)
	pvtbl->pFree(pvtbl, pPerl);
}
#endif

#ifdef WIN32		/* XXX why are these needed? */
bool
Perl_do_exec(char *cmd)
{
    return PerlProc_Cmd(cmd);
}

int
CPerlObj::do_aspawn(void *vreally, void **vmark, void **vsp)
{
    return PerlProc_aspawn(vreally, vmark, vsp);
}
#endif  /* WIN32 */

#endif   /* PERL_OBJECT */

int
Perl_fprintf_nocontext(PerlIO *stream, const char *format, ...)
{
    dTHX;
    va_list(arglist);
    va_start(arglist, format);
    return PerlIO_vprintf(stream, format, arglist);
}

int
Perl_printf_nocontext(const char *format, ...)
{
    dTHX;
    va_list(arglist);
    va_start(arglist, format);
    return PerlIO_vprintf(PerlIO_stdout(), format, arglist);
}

#include "perlapi.h"		/* bring in PL_force_link_funcs */
