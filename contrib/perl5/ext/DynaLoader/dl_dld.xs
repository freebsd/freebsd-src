/*
 *    Written 3/1/94, Robert Sanders <Robert.Sanders@linux.org>
 *
 * based upon the file "dl.c", which is
 *    Copyright (c) 1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Date: 1994/03/07 00:21:43 $
 * $Source: /home/rsanders/src/perl5alpha6/RCS/dld_dl.c,v $
 * $Revision: 1.4 $
 * $State: Exp $
 *
 * $Log: dld_dl.c,v $
 * Removed implicit link against libc.  1994/09/14 William Setzer.
 *
 * Integrated other DynaLoader changes. 1994/06/08 Tim Bunce.
 *
 * rewrote dl_load_file, misc updates.  1994/09/03 William Setzer.
 *
 * Revision 1.4  1994/03/07  00:21:43  rsanders
 * added min symbol count for load_libs and switched order so system libs
 * are loaded after app-specified libs.
 *
 * Revision 1.3  1994/03/05  01:17:26  rsanders
 * added path searching.
 *
 * Revision 1.2  1994/03/05  00:52:39  rsanders
 * added package-specified libraries.
 *
 * Revision 1.1  1994/03/05  00:33:40  rsanders
 * Initial revision
 *
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <dld.h>	/* GNU DLD header file */
#include <unistd.h>

#include "dlutils.c"	/* for SaveError() etc */

static AV *dl_resolve_using   = Nullav;
static AV *dl_require_symbols = Nullav;

static void
dl_private_init()
{
    int dlderr;
    dl_generic_private_init();
    dl_resolve_using   = perl_get_av("DynaLoader::dl_resolve_using",   0x4);
    dl_require_symbols = perl_get_av("DynaLoader::dl_require_symbols", 0x4);
#ifdef __linux__
    dlderr = dld_init("/proc/self/exe");
    if (dlderr) {
#endif
        dlderr = dld_init(dld_find_executable(PL_origargv[0]));
        if (dlderr) {
            char *msg = dld_strerror(dlderr);
            SaveError("dld_init(%s) failed: %s", PL_origargv[0], msg);
            DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "%s", LastError));
        }
#ifdef __linux__
    }
#endif
}


MODULE = DynaLoader     PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init();


char *
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
    PREINIT:
    int dlderr,x,max;
    GV *gv;
    CODE:
    RETVAL = filename;
    DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dl_load_file(%s,%x):\n", filename,flags));
    if (flags & 0x01)
	croak("Can't make loaded symbols global on this platform while loading %s",filename);
    max = AvFILL(dl_require_symbols);
    for (x = 0; x <= max; x++) {
	char *sym = SvPVX(*av_fetch(dl_require_symbols, x, 0));
	DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dld_create_ref(%s)\n", sym));
	if (dlderr = dld_create_reference(sym)) {
	    SaveError("dld_create_reference(%s): %s", sym,
		      dld_strerror(dlderr));
	    goto haverror;
	}
    }

    DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dld_link(%s)\n", filename));
    if (dlderr = dld_link(filename)) {
	SaveError("dld_link(%s): %s", filename, dld_strerror(dlderr));
	goto haverror;
    }

    max = AvFILL(dl_resolve_using);
    for (x = 0; x <= max; x++) {
	char *sym = SvPVX(*av_fetch(dl_resolve_using, x, 0));
	DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dld_link(%s)\n", sym));
	if (dlderr = dld_link(sym)) {
	    SaveError("dld_link(%s): %s", sym, dld_strerror(dlderr));
	    goto haverror;
	}
    }
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "libref=%s\n", RETVAL));
haverror:
    ST(0) = sv_newmortal() ;
    if (dlderr == 0)
	sv_setiv(ST(0), (IV)RETVAL);


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "dl_find_symbol(handle=%x, symbol=%s)\n",
	    libhandle, symbolname));
    RETVAL = (void *)dld_get_func(symbolname);
    /* if RETVAL==NULL we should try looking for a non-function symbol */
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "  symbolref = %x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError("dl_find_symbol: Unable to find '%s' symbol", symbolname) ;
    else
	sv_setiv(ST(0), (IV)RETVAL);


void
dl_undef_symbols()
    PPCODE:
    if (dld_undefined_sym_count) {
	int x;
	char **undef_syms = dld_list_undefined_sym();
	EXTEND(SP, dld_undefined_sym_count);
	for (x=0; x < dld_undefined_sym_count; x++)
	    PUSHs(sv_2mortal(newSVpv(undef_syms[x]+1, 0)));
	free(undef_syms);
    }



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *	perl_name
    void *	symref 
    char *	filename
    CODE:
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "dl_install_xsub(name=%s, symref=%x)\n",
	    perl_name, symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)())symref, filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
