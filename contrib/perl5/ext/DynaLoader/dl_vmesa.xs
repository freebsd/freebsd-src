/* dl_vmesa.xs
 *
 * Platform:	VM/ESA, possibly others which use dllload etc.
 * Author:	Neale Ferguson (neale@mailbox.tabnsw.com.au)
 * Created:	23rd Septemer, 1998
 *
 *
 */
 
/* Porting notes:
 
 
   Definition of VM/ESA dynamic Linking functions
   ==============================================
   In order to make this implementation easier to understand here is a
   quick definition of the VM/ESA Dynamic Linking functions which are
   used here.
 
   dlopen
   ------
     void *
     dlopen(const char *path)
 
     This function takes the name of a dynamic object file and returns
     a descriptor which can be used by dlsym later. It returns NULL on
     error.
 
 
   dllsym
   ------
     void *
     dlsym(void *handle, char *symbol)
 
     Takes the handle returned from dlopen and the name of a symbol to
     get the address of. If the symbol was found a pointer is
     returned.  It returns NULL on error.
 
   dlerror
   -------
     char * dlerror()
 
     Returns a null-terminated string which describes the last error
     that occurred with the other dll functions. After each call to
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
 
   In the case of VM/ESA the function dlerror returns the error message
   associated with the last dynamic link error. As the VM/ESA dynamic
   linker functions return NULL on error every call to a VM/ESA dynamic
   dynamic link routine is coded like this
 
	RETVAL = dlopen(filename) ;
	if (RETVAL == NULL)
	    SaveError(aTHX_ "%s",dlerror()) ;
 
   Note that SaveError() takes a printf format string. Use a "%s" as
   the first parameter if the error may contain and % characters.
 
*/
 
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <dll.h>
 
 
#include "dlutils.c"	/* SaveError() etc	*/
 
 
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
    CODE:
    if (flags & 0x01)
	Perl_warn(aTHX_ "Can't make loaded symbols global on this platform while loading %s",filename);
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", filename,flags));
    RETVAL = dlopen(filename) ;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " libref=%lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError(aTHX_ "%s",dlerror()) ;
    else
	sv_setiv( ST(0), PTR2IV(RETVAL) );
 
 
void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));
    RETVAL = dlsym(libhandle, symbolname);
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "  symbolref = %lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError(aTHX_ "%s",dlerror()) ;
    else
	sv_setiv( ST(0), PTR2IV(RETVAL) );
 
 
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
