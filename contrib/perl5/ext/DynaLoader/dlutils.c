/* dlutils.c - handy functions and definitions for dl_*.xs files
 *
 * Currently this file is simply #included into dl_*.xs/.c files.
 * It should really be split into a dlutils.h and dlutils.c
 *
 */


/* pointer to allocated memory for last error message */
static char *LastError  = (char*)NULL;

/* flag for immediate rather than lazy linking (spots unresolved symbol) */
static int dl_nonlazy = 0;

#ifdef DL_LOADONCEONLY
static HV *dl_loaded_files = Nullhv;	/* only needed on a few systems */
#endif


#ifdef DEBUGGING
static int dl_debug = 0;	/* value copied from $DynaLoader::dl_error */
#define DLDEBUG(level,code)	if (dl_debug>=level) { code; }
#else
#define DLDEBUG(level,code)
#endif


static void
dl_generic_private_init(CPERLarg)	/* called by dl_*.xs dl_private_init() */
{
    char *perl_dl_nonlazy;
#ifdef DEBUGGING
    dl_debug = SvIV( perl_get_sv("DynaLoader::dl_debug", 0x04) );
#endif
    if ( (perl_dl_nonlazy = getenv("PERL_DL_NONLAZY")) != NULL )
	dl_nonlazy = atoi(perl_dl_nonlazy);
    if (dl_nonlazy)
	DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "DynaLoader bind mode is 'non-lazy'\n"));
#ifdef DL_LOADONCEONLY
    if (!dl_loaded_files)
	dl_loaded_files = newHV(); /* provide cache for dl_*.xs if needed */
#endif
}


/* SaveError() takes printf style args and saves the result in LastError */
static void
SaveError(CPERLarg_ char* pat, ...)
{
    va_list args;
    char *message;
    int len;

    /* This code is based on croak/warn, see mess() in util.c */

    va_start(args, pat);
    message = mess(pat, &args);
    va_end(args);

    len = strlen(message) + 1 ;	/* include terminating null char */

    /* Allocate some memory for the error message */
    if (LastError)
        LastError = (char*)saferealloc(LastError, len) ;
    else
        LastError = (char *) safemalloc(len) ;

    /* Copy message into LastError (including terminating null char)	*/
    strncpy(LastError, message, len) ;
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "DynaLoader: stored error msg '%s'\n",LastError));
}

