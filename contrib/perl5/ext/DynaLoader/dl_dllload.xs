/* dl_dllload.xs
 *
 * Platform:	OS/390, possibly others that use dllload(),dllfree() (VM/ESA?).
 * Authors:	John Goodyear && Peter Prymmer
 * Created:     28 October 2000
 * Modified:
 * 16 January 2001 - based loosely on dl_dlopen.xs.
 */
 
/* Porting notes:

   OS/390 Dynamic Loading functions: 

   dllload
   -------
     dllhandle * dllload(const char *dllName)

     This function takes the name of a dynamic object file and returns
     a descriptor which can be used by dlllqueryfn() and/or dllqueryvar() 
     later.  If dllName contains a slash, it is used to locate the dll.
     If not then the LIBPATH environment variable is used to
     search for the requested dll (at least within the HFS).
     It returns NULL on error and sets errno.

   dllfree
   -------
     int dllfree(dllhandle *handle);

     dllfree() decrements the load count for the dll and frees
     it if the count is 0.  It returns zero on success, and 
     non-zero on failure.

   dllqueryfn && dllqueryvar
   -------------------------
     void (* dllqueryfn(dllhandle *handle, const char *function))();
     void * dllqueryvar(dllhandle *handle, const char *symbol);

     dllqueryfn() takes the handle returned from dllload() and the name 
     of a function to get the address of.  If the function was found 
     a pointer is returned, otherwise NULL is returned.

     dllqueryvar() takes the handle returned from dllload() and the name 
     of a symbol to get the address of.  If the variable was found a 
     pointer is returned, otherwise NULL is returned.

     The XS dl_find_symbol() first calls dllqueryfn().  If it fails
     dlqueryvar() is then called.

   strerror
   --------
     char * strerror(int errno)

     Returns a null-terminated string which describes the last error
     that occurred with other functions (not necessarily unique to
     dll loading).

   Return Types
   ============
   In this implementation the two functions, dl_load_file() &&
   dl_find_symbol(), return (void *).  This is primarily because the 
   dlopen() && dlsym() style dynamic linker calls return (void *).
   We suspect that casting to (void *) may be easier than teaching XS
   typemaps about the (dllhandle *) type.

   Dealing with Error Messages
   ===========================
   In order to make the handling of dynamic linking errors as generic as
   possible you should store any error messages associated with your
   implementation with the StoreError function.

   In the case of OS/390 the function strerror(errno) returns the error 
   message associated with the last dynamic link error.  As the S/390 
   dynamic linker functions dllload() && dllqueryvar() both return NULL 
   on error every call to an S/390 dynamic link routine is coded 
   like this:

	RETVAL = dllload(filename) ;
	if (RETVAL == NULL)
	    SaveError("%s",strerror(errno)) ;

   Note that SaveError() takes a printf format string. Use a "%s" as
   the first parameter if the error may contain any % characters.

   Other comments within the dl_dlopen.xs file may be helpful as well.
*/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <dll.h>	/* the dynamic linker include file for S/390 */
#include <errno.h>	/* strerror() and friends */

#include "dlutils.c"	/* SaveError() etc */

static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
}

MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);


void *
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
  PREINIT:
    int mode = 0;
  CODE:
{
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", filename,flags));
    /* add a (void *) dllload(filename) ; cast if needed */
    RETVAL = dllload(filename) ;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " libref=%lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError(aTHX_ "%s",strerror(errno)) ;
    else
	sv_setiv( ST(0), PTR2IV(RETVAL));
}


int
dl_unload_file(libref)
    void *	libref
  CODE:
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_unload_file(%lx):\n", PTR2ul(libref)));
    /* RETVAL = (dllfree((dllhandle *)libref) == 0 ? 1 : 0); */
    RETVAL = (dllfree(libref) == 0 ? 1 : 0);
    if (!RETVAL)
        SaveError(aTHX_ "%s", strerror(errno)) ;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " retval = %d\n", RETVAL));
  OUTPUT:
    RETVAL


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));
    if((RETVAL = (void*)dllqueryfn(libhandle, symbolname)) == NULL)
    RETVAL = dllqueryvar(libhandle, symbolname);
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "  symbolref = %lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError(aTHX_ "%s",strerror(errno)) ;
    else
	sv_setiv( ST(0), PTR2IV(RETVAL));


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
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, "dl_install_xsub(name=%s, symref=%lx)\n",
		perl_name, (unsigned long) symref));
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
