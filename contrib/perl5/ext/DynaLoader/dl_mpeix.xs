/*
 * Author:  Mark Klein (mklein@dis.com)
 * Version: 2.1, 1996/07/25
 * Version: 2.2, 1997/09/25 Mark Bixby (markb@cccd.edu)
 * Version: 2.3, 1998/11/19 Mark Bixby (markb@cccd.edu)
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef __GNUC__
extern void HPGETPROCPLABEL(    int    parms,
                                char * procname,
                                int  * plabel,
                                int  * status,
                                char * firstfile,
                                int    casesensitive,
                                int    symboltype,
                                int  * datasize,
                                int    position,
                                int    searchpath,
                                int    binding);
#else
#pragma intrinsic HPGETPROCPLABEL
#endif
#include "dlutils.c"    /* for SaveError() etc */

typedef struct {
  char  filename[PATH_MAX + 3];
  } t_mpe_dld, *p_mpe_dld;

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
    char *      filename
    int         flags
    PREINIT:
    char                buf[PATH_MAX + 3];
    p_mpe_dld           obj = NULL;
    int                 i;
    CODE:
    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", filename,
flags));
    if (flags & 0x01)
        Perl_warn(aTHX_ "Can't make loaded symbols global on this platform while loading %s
",filename);
    obj = (p_mpe_dld) safemalloc(sizeof(t_mpe_dld));
    memzero(obj, sizeof(t_mpe_dld));
    if (filename[0] != '/')
        {
        getcwd(buf,sizeof(buf));
        sprintf(obj->filename," %s/%s ",buf,filename);
        }
    else
        sprintf(obj->filename," %s ",filename);

    DLDEBUG(2,PerlIO_printf(Perl_debug_log," libref=%x\n", obj));

    ST(0) = sv_newmortal() ;
    if (obj == NULL)
        SaveError(aTHX_"%s",Strerror(errno));
    else
        sv_setiv( ST(0), PTR2IV(obj) );

void *
dl_find_symbol(libhandle, symbolname)
    void *      libhandle
    char *      symbolname
    CODE:
    int       datalen;
    p_mpe_dld obj = (p_mpe_dld) libhandle;
    char      symname[PATH_MAX + 3];
    void *    symaddr = NULL;
    int       status;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"dl_find_symbol(handle=%x, symbol=%s)\n",
                libhandle, symbolname));
    ST(0) = sv_newmortal() ;
    errno = 0;

    sprintf(symname, " %s ", symbolname);
    HPGETPROCPLABEL(8, symname, &symaddr, &status, obj->filename, 1,
                    0, &datalen, 1, 0, 0);

    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"  symbolref(PROCEDURE) = %x, status=%x\n", symaddr, status));

    if (status != 0) {
        SaveError(aTHX_"%s",(errno) ? Strerror(errno) : "Symbol not found") ;
    } else {
        sv_setiv( ST(0), PTR2IV(symaddr) );
    }

void
dl_undef_symbols()
    PPCODE:

# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *      perl_name
    void *      symref
    char *      filename
    CODE:
    DLDEBUG(2,PerlIO_printf(Perl_debug_log,"dl_install_xsub(name=%s, symref=%x)\n",
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
