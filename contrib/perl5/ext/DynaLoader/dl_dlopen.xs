/* dl_dlopen.xs
 * 
 * Platform:	SunOS/Solaris, possibly others which use dlopen.
 * Author:	Paul Marquess (pmarquess@bfsec.bt.co.uk)
 * Created:	10th July 1994
 *
 * Modified:
 * 15th July 1994   - Added code to explicitly save any error messages.
 * 3rd August 1994  - Upgraded to v3 spec.
 * 9th August 1994  - Changed to use IV
 * 10th August 1994 - Tim Bunce: Added RTLD_LAZY, switchable debugging,
 *                    basic FreeBSD support, removed ClearError
 *
 */

/* Porting notes:


   Definition of Sunos dynamic Linking functions
   =============================================
   In order to make this implementation easier to understand here is a
   quick definition of the SunOS Dynamic Linking functions which are
   used here.

   dlopen
   ------
     void *
     dlopen(path, mode)
     char * path; 
     int    mode;

     This function takes the name of a dynamic object file and returns
     a descriptor which can be used by dlsym later. It returns NULL on
     error.

     The mode parameter must be set to 1 for Solaris 1 and to
     RTLD_LAZY (==2) on Solaris 2.


   dlsym
   ------
     void *
     dlsym(handle, symbol)
     void * handle; 
     char * symbol;

     Takes the handle returned from dlopen and the name of a symbol to
     get the address of. If the symbol was found a pointer is
     returned.  It returns NULL on error. If DL_PREPEND_UNDERSCORE is
     defined an underscore will be added to the start of symbol. This
     is required on some platforms (freebsd).

   dlerror
   ------
     char * dlerror()

     Returns a null-terminated string which describes the last error
     that occurred with either dlopen or dlsym. After each call to
     dlerror the error message will be reset to a null pointer. The
     SaveError function is used to save the error as soo as it happens.


   Return Types
   ============
   In this implementation the two functions, dl_load_file &
   dl_find_symbol, return void *. This is because the underlying SunOS
   dynamic linker calls also return void *.  This is not necessarily
   the case for all architectures. For example, some implementation
   will want to return a char * for dl_load_file.

   If void * is not appropriate for your architecture, you will have to
   change the void * to whatever you require. If you are not certain of
   how Perl handles C data types, I suggest you start by consulting	
   Dean Roerich's Perl 5 API document. Also, have a look in the typemap 
   file (in the ext directory) for a fairly comprehensive list of types 
   that are already supported. If you are completely stuck, I suggest you
   post a message to perl5-porters, comp.lang.perl.misc or if you are really 
   desperate to me.

   Remember when you are making any changes that the return value from 
   dl_load_file is used as a parameter in the dl_find_symbol 
   function. Also the return value from find_symbol is used as a parameter 
   to install_xsub.


   Dealing with Error Messages
   ============================
   In order to make the handling of dynamic linking errors as generic as
   possible you should store any error messages associated with your
   implementation with the StoreError function.

   In the case of SunOS the function dlerror returns the error message 
   associated with the last dynamic link error. As the SunOS dynamic 
   linker functions dlopen & dlsym both return NULL on error every call 
   to a SunOS dynamic link routine is coded like this

	RETVAL = dlopen(filename, 1) ;
	if (RETVAL == NULL)
	    SaveError("%s",dlerror()) ;

   Note that SaveError() takes a printf format string. Use a "%s" as
   the first parameter if the error may contain and % characters.

*/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_DLFCN
#include <dlfcn.h>	/* the dynamic linker include file for Sunos/Solaris */
#else
#include <nlist.h>
#include <link.h>
#endif

#ifndef RTLD_LAZY
# define RTLD_LAZY 1	/* Solaris 1 */
#endif

#ifndef HAS_DLERROR
# ifdef __NetBSD__
#  define dlerror() strerror(errno)
# else
#  define dlerror() "Unknown error - dlerror() not implemented"
# endif
#endif


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
    PREINIT:
    int mode = RTLD_LAZY;
    CODE:
#ifdef RTLD_NOW
    if (dl_nonlazy)
	mode = RTLD_NOW;
#endif
    if (flags & 0x01)
#ifdef RTLD_GLOBAL
	mode |= RTLD_GLOBAL;
#else
	warn("Can't make loaded symbols global on this platform while loading %s",filename);
#endif
    DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dl_load_file(%s,%x):\n", filename,flags));
    RETVAL = dlopen(filename, mode) ;
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), " libref=%lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError("%s",dlerror()) ;
    else
	sv_setiv( ST(0), (IV)RETVAL);


void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
#ifdef DLSYM_NEEDS_UNDERSCORE
    symbolname = form("_%s", symbolname);
#endif
    DLDEBUG(2, PerlIO_printf(PerlIO_stderr(),
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));
    RETVAL = dlsym(libhandle, symbolname);
    DLDEBUG(2, PerlIO_printf(PerlIO_stderr(),
			     "  symbolref = %lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError("%s",dlerror()) ;
    else
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
