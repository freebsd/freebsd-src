/* dlutils.c - handy functions and definitions for dl_*.xs files
 *
 * Currently this file is simply #included into dl_*.xs/.c files.
 * It should really be split into a dlutils.h and dlutils.c
 *
 * Modified:
 * 29th Feburary 2000 - Alan Burlison: Added functionality to close dlopen'd
 *                      files when the interpreter exits
 */


/* pointer to allocated memory for last error message */
static char *LastError  = (char*)NULL;

/* flag for immediate rather than lazy linking (spots unresolved symbol) */
static int dl_nonlazy = 0;

#ifdef DL_LOADONCEONLY
static HV *dl_loaded_files = Nullhv;	/* only needed on a few systems */
#endif


#ifdef DEBUGGING
static int dl_debug = 0;	/* value copied from $DynaLoader::dl_debug */
#define DLDEBUG(level,code)	if (dl_debug>=level) { code; }
#else
#define DLDEBUG(level,code)
#endif


/* Close all dlopen'd files */
static void
dl_unload_all_files(pTHXo_ void *unused)
{
    CV *sub;
    AV *dl_librefs;
    SV *dl_libref;

    if ((sub = get_cv("DynaLoader::dl_unload_file", FALSE)) != NULL) {
        dl_librefs = get_av("DynaLoader::dl_librefs", FALSE);
        while ((dl_libref = av_pop(dl_librefs)) != &PL_sv_undef) {
           dSP;
           ENTER;
           SAVETMPS;
           PUSHMARK(SP);
           XPUSHs(sv_2mortal(dl_libref));
           PUTBACK;
           call_sv((SV*)sub, G_DISCARD | G_NODEBUG);
           FREETMPS;
           LEAVE;
        }
    }
}


static void
dl_generic_private_init(pTHXo)	/* called by dl_*.xs dl_private_init() */
{
    char *perl_dl_nonlazy;
#ifdef DEBUGGING
    SV *sv = get_sv("DynaLoader::dl_debug", 0);
    dl_debug = sv ? SvIV(sv) : 0;
#endif
    if ( (perl_dl_nonlazy = getenv("PERL_DL_NONLAZY")) != NULL )
	dl_nonlazy = atoi(perl_dl_nonlazy);
    if (dl_nonlazy)
	DLDEBUG(1,PerlIO_printf(Perl_debug_log, "DynaLoader bind mode is 'non-lazy'\n"));
#ifdef DL_LOADONCEONLY
    if (!dl_loaded_files)
	dl_loaded_files = newHV(); /* provide cache for dl_*.xs if needed */
#endif
#ifdef DL_UNLOAD_ALL_AT_EXIT
    call_atexit(&dl_unload_all_files, (void*)0);
#endif
}


/* SaveError() takes printf style args and saves the result in LastError */
static void
SaveError(pTHXo_ char* pat, ...)
{
    va_list args;
    SV *msv;
    char *message;
    STRLEN len;

    /* This code is based on croak/warn, see mess() in util.c */

    va_start(args, pat);
    msv = vmess(pat, &args);
    va_end(args);

    message = SvPV(msv,len);
    len++;		/* include terminating null char */

    /* Allocate some memory for the error message */
    if (LastError)
        LastError = (char*)saferealloc(LastError, len) ;
    else
        LastError = (char *) safemalloc(len) ;

    /* Copy message into LastError (including terminating null char)	*/
    strncpy(LastError, message, len) ;
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, "DynaLoader: stored error msg '%s'\n",LastError));
}

