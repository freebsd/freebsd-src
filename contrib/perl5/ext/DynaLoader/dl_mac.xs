/* dl_mac.xs
 * 
 * Platform:	Macintosh CFM
 * Author:	Matthias Neeracher <neeri@iis.ee.ethz.ch>
 *		Adapted from dl_dlopen.xs reference implementation by
 *              Paul Marquess (pmarquess@bfsec.bt.co.uk)
 * $Log: dl_mac.xs,v $
 * Revision 1.3  1998/04/07 01:47:24  neeri
 * MacPerl 5.2.0r4b1
 *
 * Revision 1.2  1997/08/08 16:39:18  neeri
 * MacPerl 5.1.4b1 + time() fix
 *
 * Revision 1.1  1997/04/07 20:48:23  neeri
 * Synchronized with MacPerl 5.1.4a1
 *
 */

#define MAC_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <CodeFragments.h>


#include "dlutils.c"	/* SaveError() etc	*/

typedef CFragConnectionID ConnectionID;

static ConnectionID **	connections;

static void terminate(void)
{
    int size = GetHandleSize((Handle) connections) / sizeof(ConnectionID);
    HLock((Handle) connections);
    while (size)
    	CloseConnection(*connections + --size);
    DisposeHandle((Handle) connections);
    connections = nil;
}

static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
}

MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);


ConnectionID
dl_load_file(filename, flags=0)
    char *		filename
    int			flags
    PREINIT:
    OSErr		err;
    FSSpec		spec;
    ConnectionID	connID;
    Ptr			mainAddr;
    Str255		errName;
    CODE:
    DLDEBUG(1,PerlIO_printf(Perl_debug_log,"dl_load_file(%s):\n", filename));
    err = GUSIPath2FSp(filename, &spec);
    if (!err)
    	err = 
	    GetDiskFragment(
	    	&spec, 0, 0, spec.name, kLoadCFrag, &connID, &mainAddr, errName);
    if (!err) {
    	if (!connections) {
	    connections = (ConnectionID **)NewHandle(0);
	    atexit(terminate);
    	}
        PtrAndHand((Ptr) &connID, (Handle) connections, sizeof(ConnectionID));
    	RETVAL = connID;
    } else
    	RETVAL = (ConnectionID) 0;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log," libref=%d\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (err)
    	SaveError(aTHX_ "DynaLoader error [%d, %#s]", err, errName) ;
    else
    	sv_setiv( ST(0), (IV)RETVAL);

void *
dl_find_symbol(connID, symbol)
    ConnectionID	connID
    Str255		symbol
    CODE:
    {
    	OSErr		    err;
    	Ptr		    symAddr;
    	CFragSymbolClass    symClass;
    	DLDEBUG(2,PerlIO_printf(Perl_debug_log,"dl_find_symbol(handle=%x, symbol=%#s)\n",
	    connID, symbol));
   	err = FindSymbol(connID, symbol, &symAddr, &symClass);
    	if (err)
    	    symAddr = (Ptr) 0;
    	RETVAL = (void *) symAddr;
    	DLDEBUG(2,PerlIO_printf(Perl_debug_log,"  symbolref = %x\n", RETVAL));
    	ST(0) = sv_newmortal() ;
    	if (err)
	    SaveError(aTHX_ "DynaLoader error [%d]!", err) ;
    	else
	    sv_setiv( ST(0), (IV)RETVAL);
    }

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
    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"dl_install_xsub(name=%s, symref=%x)\n",
		perl_name, symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)())symref, filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
