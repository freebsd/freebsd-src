/*
 * dl_beos.xs, by Tom Spindler
 * based on dl_dlopen.xs, by Paul Marquess
 * $Id:$
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <be/kernel/image.h>
#include <OS.h>
#include <stdlib.h>
#include <limits.h>

#define dlerror() strerror(errno)

#include "dlutils.c"	/* SaveError() etc	*/

static void
dl_private_init()
{
    (void)dl_generic_private_init();
}

MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init();


void *
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
    CODE:
{   image_id bogo;
    char *path;
    path = malloc(PATH_MAX);
    if (*filename != '/') {
      getcwd(path, PATH_MAX);
      strcat(path, "/");
      strcat(path, filename);
    } else {
      strcpy(path, filename);
    }

    DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dl_load_file(%s,%x):\n", path, flags));
    bogo = load_add_on(path);
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), " libref=%lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (bogo < 0) {
	SaveError("%s", strerror(bogo));
	PerlIO_printf(PerlIO_stderr(), "load_add_on(%s) : %d (%s)\n", path, bogo, strerror(bogo));
    } else {
	RETVAL = (void *) bogo;
	sv_setiv( ST(0), (IV)RETVAL);
    }
    free(path);
}

void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    status_t retcode;
    void *adr = 0;
#ifdef DLSYM_NEEDS_UNDERSCORE
    symbolname = form("_%s", symbolname);
#endif
    RETVAL = NULL;
    DLDEBUG(2, PerlIO_printf(PerlIO_stderr(),
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));
    retcode = get_image_symbol((image_id) libhandle, symbolname,
                               B_SYMBOL_TYPE_TEXT, (void **) &adr);
    RETVAL = adr;
    DLDEBUG(2, PerlIO_printf(PerlIO_stderr(),
			     "  symbolref = %lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL) {
	SaveError("%s", strerror(retcode)) ;
	PerlIO_printf(PerlIO_stderr(), "retcode = %p (%s)\n", retcode, strerror(retcode));
    } else
	sv_setiv( ST(0), (IV)RETVAL);


void
dl_undef_symbols()
    PPCODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *		perl_name
    void *		symref 
    char *		filename
    CODE:
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "dl_install_xsub(name=%s, symref=%lx)\n",
		perl_name, (unsigned long) symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)_((CV *)))symref, filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
