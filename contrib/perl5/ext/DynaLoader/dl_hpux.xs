/*
 * Author: Jeff Okamoto (okamoto@corp.hp.com)
 * Version: 2.1, 1995/1/25
 */

/* o Added BIND_VERBOSE to dl_nonlazy condition to add names of missing
 *   symbols to stderr message on fatal error.
 *
 * o Added BIND_NONFATAL comment to default condition.
 *
 * Chuck Phillips (cdp@fc.hp.com)
 * Version: 2.2, 1997/5/4 */

#ifdef __hp9000s300
#define magic hpux_magic
#define MAGIC HPUX_MAGIC
#endif

#include <dl.h>
#ifdef __hp9000s300
#undef magic
#undef MAGIC
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


#include "dlutils.c"	/* for SaveError() etc */

static AV *dl_resolve_using = Nullav;


static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
    dl_resolve_using = get_av("DynaLoader::dl_resolve_using", GV_ADDMULTI);
}

MODULE = DynaLoader     PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);


void *
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
    PREINIT:
    shl_t obj = NULL;
    int	i, max, bind_type;
    CODE:
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", filename,flags));
    if (flags & 0x01)
	Perl_warn(aTHX_ "Can't make loaded symbols global on this platform while loading %s",filename);
    if (dl_nonlazy) {
      bind_type = BIND_IMMEDIATE|BIND_VERBOSE;
    } else {
      bind_type = BIND_DEFERRED;
      /* For certain libraries, like DCE, deferred binding often causes run
       * time problems.  Adding BIND_NONFATAL to BIND_IMMEDIATE still allows
       * unresolved references in situations like this.  */
      /* bind_type = BIND_IMMEDIATE|BIND_NONFATAL; */
    }
    /* BIND_NOSTART removed from bind_type because it causes the shared library's	*/
    /* initialisers not to be run.  This causes problems with all of the static objects */
    /* in the library.	   */
#ifdef DEBUGGING
    if (dl_debug)
	bind_type |= BIND_VERBOSE;
#endif /* DEBUGGING */

    max = AvFILL(dl_resolve_using);
    for (i = 0; i <= max; i++) {
	char *sym = SvPVX(*av_fetch(dl_resolve_using, i, 0));
	DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s) (dependent)\n", sym));
	obj = shl_load(sym, bind_type, 0L);
	if (obj == NULL) {
	    goto end;
	}
    }

    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s): ", filename));
    obj = shl_load(filename, bind_type, 0L);

    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " libref=%x\n", obj));
end:
    ST(0) = sv_newmortal() ;
    if (obj == NULL)
        SaveError(aTHX_ "%s",Strerror(errno));
    else
        sv_setiv( ST(0), PTR2IV(obj) );


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    shl_t obj = (shl_t) libhandle;
    void *symaddr = NULL;
    int status;
#ifdef __hp9000s300
    symbolname = Perl_form_nocontext("_%s", symbolname);
#endif
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));

    ST(0) = sv_newmortal() ;
    errno = 0;

    status = shl_findsym(&obj, symbolname, TYPE_PROCEDURE, &symaddr);
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, "  symbolref(PROCEDURE) = %x\n", symaddr));

    if (status == -1 && errno == 0) {	/* try TYPE_DATA instead */
	status = shl_findsym(&obj, symbolname, TYPE_DATA, &symaddr);
	DLDEBUG(2,PerlIO_printf(Perl_debug_log, "  symbolref(DATA) = %x\n", symaddr));
    }

    if (status == -1) {
	SaveError(aTHX_ "%s",(errno) ? Strerror(errno) : "Symbol not found") ;
    } else {
	sv_setiv( ST(0), PTR2IV(symaddr) );
    }


void
dl_undef_symbols()
    PPCODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *	perl_name
    void *	symref 
    char *	filename
    CODE:
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, "dl_install_xsub(name=%s, symref=%x)\n",
	    perl_name, symref));
    ST(0) = sv_2mortal(newRV((SV*)newXS(perl_name,
					(void(*)(pTHX_ CV *))symref,
					filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
