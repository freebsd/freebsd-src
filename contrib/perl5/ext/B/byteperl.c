#include "EXTERN.h"
#include "perl.h"
#ifndef PATCHLEVEL
#include "patchlevel.h"
#endif

static void xs_init _((void));
static PerlInterpreter *my_perl;

int
#ifndef CAN_PROTOTYPE
main(argc, argv, env)
int argc;
char **argv;
char **env;
#else  /* def(CAN_PROTOTYPE) */
main(int argc, char **argv, char **env)
#endif  /* def(CAN_PROTOTYPE) */
{
    int exitstatus;
    int i;
    char **fakeargv;
    FILE *fp;
#ifdef INDIRECT_BGET_MACROS
    struct bytestream bs;
#endif /* INDIRECT_BGET_MACROS */

    INIT_SPECIALSV_LIST;
    PERL_SYS_INIT(&argc,&argv);
 
#if PATCHLEVEL > 3 || (PATCHLEVEL == 3 && SUBVERSION >= 1)
    perl_init_i18nl10n(1);
#else
    perl_init_i18nl14n(1);
#endif

    if (!PL_do_undump) {
	my_perl = perl_alloc();
	if (!my_perl)
#ifdef VMS
	    exit(vaxc$errno);
#else
	    exit(1);
#endif
	perl_construct( my_perl );
    }

#ifdef CSH
    if (!PL_cshlen) 
      PL_cshlen = strlen(PL_cshname);
#endif

    if (argc < 2)
	fp = stdin;
    else {
#ifdef WIN32
	fp = fopen(argv[1], "rb");
#else
	fp = fopen(argv[1], "r");
#endif
	if (!fp) {
	    perror(argv[1]);
#ifdef VMS
	    exit(vaxc$errno);
#else
	    exit(1);
#endif
	}
	argv++;
	argc--;
    }
    New(666, fakeargv, argc + 4, char *);
    fakeargv[0] = argv[0];
    fakeargv[1] = "-e";
    fakeargv[2] = "";
    fakeargv[3] = "--";
    for (i = 1; i < argc; i++)
	fakeargv[i + 3] = argv[i];
    fakeargv[argc + 3] = 0;
    
    exitstatus = perl_parse(my_perl, xs_init, argc + 3, fakeargv, NULL);
    if (exitstatus)
	exit( exitstatus );

    sv_setpv(GvSV(gv_fetchpv("0", TRUE, SVt_PV)), argv[0]);
    PL_main_cv = PL_compcv;
    PL_compcv = 0;

#ifdef INDIRECT_BGET_MACROS
    bs.data = fp;
    bs.fgetc = (int(*) _((void*)))fgetc;
    bs.fread = (int(*) _((char*,size_t,size_t,void*)))fread;
    bs.freadpv = freadpv;
    byterun(bs);
#else    
    byterun(fp);
#endif /* INDIRECT_BGET_MACROS */
    
    exitstatus = perl_run( my_perl );

    perl_destruct( my_perl );
    perl_free( my_perl );

    exit( exitstatus );
}

static void
xs_init()
{
}
