/*    deb.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Didst thou think that the eyes of the White Tower were blind?  Nay, I
 * have seen more than thou knowest, Gray Fool."  --Denethor
 */

#include "EXTERN.h"
#define PERL_IN_DEB_C
#include "perl.h"

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_deb_nocontext(const char *pat, ...)
{
#ifdef DEBUGGING
    dTHX;
    va_list args;
    va_start(args, pat);
    vdeb(pat, &args);
    va_end(args);
#endif /* DEBUGGING */
}
#endif

void
Perl_deb(pTHX_ const char *pat, ...)
{
#ifdef DEBUGGING
    va_list args;
    va_start(args, pat);
    vdeb(pat, &args);
    va_end(args);
#endif /* DEBUGGING */
}

void
Perl_vdeb(pTHX_ const char *pat, va_list *args)
{
#ifdef DEBUGGING
    char* file = CopFILE(PL_curcop);

#ifdef USE_THREADS
    PerlIO_printf(Perl_debug_log, "0x%"UVxf" (%s:%ld)\t",
		  PTR2UV(thr),
		  (file ? file : "<free>"),
		  (long)CopLINE(PL_curcop));
#else
    PerlIO_printf(Perl_debug_log, "(%s:%ld)\t", (file ? file : "<free>"),
		  (long)CopLINE(PL_curcop));
#endif /* USE_THREADS */
    (void) PerlIO_vprintf(Perl_debug_log, pat, *args);
#endif /* DEBUGGING */
}

I32
Perl_debstackptrs(pTHX)
{
#ifdef DEBUGGING
    PerlIO_printf(Perl_debug_log,
		  "%8"UVxf" %8"UVxf" %8"IVdf" %8"IVdf" %8"IVdf"\n",
		  PTR2UV(PL_curstack), PTR2UV(PL_stack_base),
		  (IV)*PL_markstack_ptr, (IV)(PL_stack_sp-PL_stack_base),
		  (IV)(PL_stack_max-PL_stack_base));
    PerlIO_printf(Perl_debug_log,
		  "%8"UVxf" %8"UVxf" %8"UVuf" %8"UVuf" %8"UVuf"\n",
		  PTR2UV(PL_mainstack), PTR2UV(AvARRAY(PL_curstack)),
		  PTR2UV(PL_mainstack), PTR2UV(AvFILLp(PL_curstack)),
		  PTR2UV(AvMAX(PL_curstack)));
#endif /* DEBUGGING */
    return 0;
}

I32
Perl_debstack(pTHX)
{
#ifdef DEBUGGING
    I32 top = PL_stack_sp - PL_stack_base;
    register I32 i = top - 30;
    I32 *markscan = PL_markstack + PL_curstackinfo->si_markoff;

    if (i < 0)
	i = 0;
    
    while (++markscan <= PL_markstack_ptr)
	if (*markscan >= i)
	    break;

#ifdef USE_THREADS
    PerlIO_printf(Perl_debug_log,
		  i ? "0x%"UVxf"    =>  ...  " : "0x%lx    =>  ",
		  PTR2UV(thr));
#else
    PerlIO_printf(Perl_debug_log, i ? "    =>  ...  " : "    =>  ");
#endif /* USE_THREADS */
    if (PL_stack_base[0] != &PL_sv_undef || PL_stack_sp < PL_stack_base)
	PerlIO_printf(Perl_debug_log, " [STACK UNDERFLOW!!!]\n");
    do {
	++i;
	if (markscan <= PL_markstack_ptr && *markscan < i) {
	    do {
		++markscan;
		PerlIO_putc(Perl_debug_log, '*');
	    }
	    while (markscan <= PL_markstack_ptr && *markscan < i);
	    PerlIO_printf(Perl_debug_log, "  ");
	}
	if (i > top)
	    break;
	PerlIO_printf(Perl_debug_log, "%-4s  ", SvPEEK(PL_stack_base[i]));
    }
    while (1);
    PerlIO_printf(Perl_debug_log, "\n");
#endif /* DEBUGGING */
    return 0;
}
