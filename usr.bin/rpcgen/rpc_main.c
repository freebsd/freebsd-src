/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */


#ident	"@(#)rpc_main.c	1.21	94/04/25 SMI"

#ifndef lint
static char sccsid[] = "@(#)rpc_main.c 1.30 89/03/30 (C) 1987 SMI";
#endif

/*
 * rpc_main.c, Top level of the RPC protocol compiler.
 * Copyright (C) 1987, Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "rpc_parse.h"
#include "rpc_util.h"
#include "rpc_scan.h"

extern void write_sample_svc __P(( definition * ));
extern int write_sample_clnt __P(( definition * ));
extern void write_sample_clnt_main __P(( void ));
extern void add_sample_msg __P(( void ));
static void c_output __P(( char *, char *, int, char * ));
static void h_output __P(( char *, char *, int, char * ));
static void l_output __P(( char *, char *, int, char * ));
static void t_output __P(( char *, char *, int, char * ));
static void clnt_output __P(( char *, char *, int, char * ));

void c_initialize __P(( void ));

#ifndef __FreeBSD__
char * rindex();
#endif

static void usage __P(( void ));
static void options_usage __P (( void ));
static int do_registers __P(( int, char ** ));
static int parseargs __P(( int, char **, struct commandline * ));
static void svc_output __P(( char *, char *, int, char * ));
static void mkfile_output __P(( struct commandline * ));
static void s_output __P(( int, char **, char *, char *, int, char *, int, int ));

#define	EXTEND	1		/* alias for TRUE */
#define	DONT_EXTEND	0		/* alias for FALSE */

#define	SVR4_CPP "/usr/ccs/lib/cpp"
#ifdef __FreeBSD__
#define	SUNOS_CPP "/usr/libexec/cpp"
#else
#define	SUNOS_CPP "/usr/lib/cpp"
#endif

static int cppDefined = 0;	/* explicit path for C preprocessor */


static char *cmdname;

static char *svcclosetime = "120";
static char *CPP = SVR4_CPP;
static char CPPFLAGS[] = "-C";
static char pathbuf[MAXPATHLEN + 1];
static char *allv[] = {
	"rpcgen", "-s", "udp", "-s", "tcp",
};
static int allc = sizeof (allv)/sizeof (allv[0]);
static char *allnv[] = {
	"rpcgen", "-s", "netpath",
};
static int allnc = sizeof (allnv)/sizeof (allnv[0]);

/*
 * machinations for handling expanding argument list
 */
static void addarg();		/* add another argument to the list */
static void putarg();		/* put argument at specified location  */
static void clear_args();	/* clear argument list */
static void checkfiles();	/* check if out file already exists */



#define	ARGLISTLEN	20
#define	FIXEDARGS	2

static char *arglist[ARGLISTLEN];
static int argcount = FIXEDARGS;


int nonfatalerrors;	/* errors */
#ifdef __FreeBSD__
int inetdflag = 0;	/* Support for inetd  is now the default */
#else
int inetdflag;	/* Support for inetd  is now the default */
#endif
int pmflag;		/* Support for port monitors */
int logflag;		/* Use syslog instead of fprintf for errors */
int tblflag;		/* Support for dispatch table file */
int mtflag = 0;		/* Support for MT */
#ifdef __FreeBSD__
#define INLINE 0
#else
#define	INLINE 5
#endif
/* length at which to start doing an inline */

int inline = INLINE;
/*
 * Length at which to start doing an inline. INLINE = default
 * if 0, no xdr_inline code
 */

int indefinitewait;	/* If started by port monitors, hang till it wants */
int exitnow;		/* If started by port monitors, exit after the call */
int timerflag;		/* TRUE if !indefinite && !exitnow */
int newstyle;		/* newstyle of passing arguments (by value) */
int Cflag = 0;		/* ANSI C syntax */
int CCflag = 0;		/* C++ files */
static int allfiles;   /* generate all files */
#ifdef __FreeBSD__
int tirpcflag = 0;    /* generating code for tirpc, by default */
#else
int tirpcflag = 1;    /* generating code for tirpc, by default */
#endif
xdrfunc *xdrfunc_head = NULL; /* xdr function list */
xdrfunc *xdrfunc_tail = NULL; /* xdr function list */
pid_t childpid;


int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct commandline cmd;

	(void) memset((char *)&cmd, 0, sizeof (struct commandline));
	clear_args();
	if (!parseargs(argc, argv, &cmd))
		usage();
	/*
	 * Only the client and server side stubs are likely to be customized,
	 *  so in that case only, check if the outfile exists, and if so,
	 *  print an error message and exit.
	 */
	if (cmd.Ssflag || cmd.Scflag || cmd.makefileflag) {
		checkfiles(cmd.infile, cmd.outfile);
	}
	else
		checkfiles(cmd.infile, NULL);

	if (cmd.cflag) {
		c_output(cmd.infile, "-DRPC_XDR", DONT_EXTEND, cmd.outfile);
	} else if (cmd.hflag) {
		h_output(cmd.infile, "-DRPC_HDR", DONT_EXTEND, cmd.outfile);
	} else if (cmd.lflag) {
		l_output(cmd.infile, "-DRPC_CLNT", DONT_EXTEND, cmd.outfile);
	} else if (cmd.sflag || cmd.mflag || (cmd.nflag)) {
		s_output(argc, argv, cmd.infile, "-DRPC_SVC", DONT_EXTEND,
			cmd.outfile, cmd.mflag, cmd.nflag);
	} else if (cmd.tflag) {
		t_output(cmd.infile, "-DRPC_TBL", DONT_EXTEND, cmd.outfile);
	} else if  (cmd.Ssflag) {
		svc_output(cmd.infile, "-DRPC_SERVER", DONT_EXTEND,
			cmd.outfile);
	} else if (cmd.Scflag) {
		clnt_output(cmd.infile, "-DRPC_CLIENT", DONT_EXTEND,
			    cmd.outfile);
	} else if (cmd.makefileflag) {
		mkfile_output(&cmd);
	} else {
		/* the rescans are required, since cpp may effect input */
		c_output(cmd.infile, "-DRPC_XDR", EXTEND, "_xdr.c");
		reinitialize();
		h_output(cmd.infile, "-DRPC_HDR", EXTEND, ".h");
		reinitialize();
		l_output(cmd.infile, "-DRPC_CLNT", EXTEND, "_clnt.c");
		reinitialize();
		if (inetdflag || !tirpcflag)
			s_output(allc, allv, cmd.infile, "-DRPC_SVC", EXTEND,
			"_svc.c", cmd.mflag, cmd.nflag);
		else
			s_output(allnc, allnv, cmd.infile, "-DRPC_SVC",
				EXTEND, "_svc.c", cmd.mflag, cmd.nflag);
		if (tblflag) {
			reinitialize();
		t_output(cmd.infile, "-DRPC_TBL", EXTEND, "_tbl.i");
		}

		if (allfiles) {
			reinitialize();
			svc_output(cmd.infile, "-DRPC_SERVER", EXTEND,
				"_server.c");
			reinitialize();
			clnt_output(cmd.infile, "-DRPC_CLIENT", EXTEND,
				"_client.c");

		}
		if (allfiles || (cmd.makefileflag == 1)){
			reinitialize();
			mkfile_output(&cmd);
		}

	}
	exit(nonfatalerrors);
	/* NOTREACHED */
}


/*
 * add extension to filename
 */
static char *
#ifdef __FreeBSD__
extendfile(path, ext)
	char *path;
#else
extendfile(file, ext)
	char *file;
#endif
	char *ext;
{
	char *res;
	char *p;
#ifdef __FreeBSD__
	char *file;

	if ((file = rindex(path, '/')) == NULL)
		file = path;
	else
		file++;
#endif
	res = alloc(strlen(file) + strlen(ext) + 1);
	if (res == NULL) {
		abort();
	}
	p = strrchr(file, '.');
	if (p == NULL) {
		p = file + strlen(file);
	}
	(void) strcpy(res, file);
	(void) strcpy(res + (p - file), ext);
	return (res);
}

/*
 * Open output file with given extension
 */
static void
open_output(infile, outfile)
	char *infile;
	char *outfile;
{

	if (outfile == NULL) {
		fout = stdout;
		return;
	}

	if (infile != NULL && streq(outfile, infile)) {
	f_print(stderr, "%s: %s already exists.  No output generated.\n",
		cmdname, infile);
		crash();
	}
	fout = fopen(outfile, "w");
	if (fout == NULL) {
		f_print(stderr, "%s: unable to open ", cmdname);
		perror(outfile);
		crash();
	}
	record_open(outfile);

	return;
}

static void
add_warning()
{
	f_print(fout, "/*\n");
	f_print(fout, " * Please do not edit this file.\n");
	f_print(fout, " * It was generated using rpcgen.\n");
	f_print(fout, " */\n\n");
}

/* clear list of arguments */
static void clear_args()
{
	int i;
	for (i = FIXEDARGS; i < ARGLISTLEN; i++)
		arglist[i] = NULL;
	argcount = FIXEDARGS;
}

/* make sure that a CPP exists */
static void find_cpp()
{
	struct stat buf;

	if (stat(CPP, &buf) < 0)  { /* SVR4 or explicit cpp does not exist */
		if (cppDefined) {
			fprintf(stderr,
				"cannot find C preprocessor: %s \n", CPP);
			crash();
		} else {	/* try the other one */
			CPP = SUNOS_CPP;
			if (stat(CPP, &buf) < 0) { /* can't find any cpp */
				fprintf(stderr,
		"cannot find any C preprocessor (cpp)\n");
				crash();
			}
		}
	}
}

/*
 * Open input file with given define for C-preprocessor
 */
static void
open_input(infile, define)
	char *infile;
	char *define;
{
	int pd[2];

	infilename = (infile == NULL) ? "<stdin>" : infile;
	(void) pipe(pd);
	switch (childpid = fork()) {
	case 0:
		find_cpp();
		putarg(0, CPP);
		putarg(1, CPPFLAGS);
		addarg(define);
		if (infile)
			addarg(infile);
		addarg((char *)NULL);
		(void) close(1);
		(void) dup2(pd[1], 1);
		(void) close(pd[0]);
		execv(arglist[0], arglist);
		perror("execv");
		exit(1);
	case -1:
		perror("fork");
		exit(1);
	}
	(void) close(pd[1]);
	fin = fdopen(pd[0], "r");
	if (fin == NULL) {
		f_print(stderr, "%s: ", cmdname);
		perror(infilename);
		crash();
	}
}

/* valid tirpc nettypes */
static char* valid_ti_nettypes[] =
{
	"netpath",
	"visible",
	"circuit_v",
	"datagram_v",
	"circuit_n",
	"datagram_n",
	"udp",
	"tcp",
	"raw",
	NULL
	};

/* valid inetd nettypes */
static char* valid_i_nettypes[] =
{
	"udp",
	"tcp",
	NULL
	};

static int check_nettype(name, list_to_check)
char* name;
char* list_to_check[];
{
	int i;
	for (i = 0; list_to_check[i] != NULL; i++) {
		if (strcmp(name, list_to_check[i]) == 0) {
			return (1);
		}
	}
	f_print(stderr, "illegal nettype :\'%s\'\n", name);
	return (0);
}

static char *
file_name(file, ext)
char *file;
char *ext;
{
	char *temp;
	temp = extendfile(file, ext);

	if (access(temp, F_OK) != -1)
		return (temp);
	else
		return ((char *)" ");

}


static void
c_output(infile, define, extend, outfile)
	char *infile;
	char *define;
	int extend;
	char *outfile;
{
	definition *def;
	char *include;
	char *outfilename;
	long tell;

	c_initialize();
	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
		/* .h file already contains rpc/rpc.h */
	} else
		f_print(fout, "#include <rpc/rpc.h>\n");
	tell = ftell(fout);
	while ( (def = get_definition()) ) {
		emit(def);
	}
	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}


void
c_initialize()
{

	/* add all the starting basic types */
	add_type(1, "int");
	add_type(1, "long");
	add_type(1, "short");
	add_type(1, "bool");
	add_type(1, "u_int");
	add_type(1, "u_long");
	add_type(1, "u_short");

}

char rpcgen_table_dcl[] = "struct rpcgen_table {\n\
	char	*(*proc)(); \n\
	xdrproc_t	xdr_arg; \n\
	unsigned	len_arg; \n\
	xdrproc_t	xdr_res; \n\
	unsigned	len_res; \n\
}; \n";


char *generate_guard(pathname)
	char* pathname;
{
	char* filename, *guard, *tmp;

	filename = strrchr(pathname, '/');  /* find last component */
	filename = ((filename == 0) ? pathname : filename+1);
	guard = strdup(filename);
	/* convert to upper case */
	tmp = guard;
	while (*tmp) {
		if (islower(*tmp))
			*tmp = toupper(*tmp);
		tmp++;
	}
	guard = extendfile(guard, "_H_RPCGEN");
	return (guard);
}

/*
 * Compile into an XDR header file
 */


static void
h_output(infile, define, extend, outfile)
	char *infile;
	char *define;
	int extend;
	char *outfile;
{
	definition *def;
	char *outfilename;
	long tell;
	char *guard;
	list *l;
	xdrfunc *xdrfuncp;
	int i;

	open_input(infile, define);
	outfilename =  extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (outfilename || infile){
		guard = generate_guard(outfilename ? outfilename: infile);
	} else
		guard = "STDIN_";

	f_print(fout, "#ifndef _%s\n#define	_%s\n\n", guard,
		guard);

	f_print(fout, "#include <rpc/rpc.h>\n");

	if (mtflag) {
		f_print(fout, "#include <synch.h>\n");
		f_print(fout, "#include <thread.h>\n");
	};

	/* put the C++ support */
	if (Cflag && !CCflag){
		f_print(fout, "\n#ifdef __cplusplus\n");
		f_print(fout, "extern \"C\" {\n");
		f_print(fout, "#endif\n\n");
	}

	/* put in a typedef for quadprecision. Only with Cflag */

	tell = ftell(fout);

	/* print data definitions */
	while ( (def = get_definition()) ) {
		print_datadef(def);
	}

	/*
	 * print function declarations.
	 *  Do this after data definitions because they might be used as
	 *  arguments for functions
	 */
	for (l = defined; l != NULL; l = l->next) {
		print_funcdef(l->val);
	}
	/* Now  print all xdr func declarations */
	if (xdrfunc_head != NULL){

		f_print(fout,
			"\n/* the xdr functions */\n");

		if (CCflag){
		f_print(fout, "\n#ifdef __cplusplus\n");
		f_print(fout, "extern \"C\" {\n");
		f_print(fout, "#endif\n");
	}

		if (!Cflag){
			xdrfuncp = xdrfunc_head;
			while (xdrfuncp != NULL){
				print_xdr_func_def(xdrfuncp->name,
				xdrfuncp->pointerp, 2);
				xdrfuncp = xdrfuncp->next;
			}
		} else {

			for (i = 1; i < 3; i++){
				if (i == 1)
	f_print(fout, "\n#if defined(__STDC__) || defined(__cplusplus)\n");

				else
					f_print(fout, "\n#else /* K&R C */\n");

				xdrfuncp = xdrfunc_head;
				while (xdrfuncp != NULL){
					print_xdr_func_def(xdrfuncp->name,
	xdrfuncp->pointerp, i);
					xdrfuncp = xdrfuncp->next;
				}
			}
		f_print(fout, "\n#endif /* K&R C */\n");
		}
	}

	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	} else if (tblflag) {
		f_print(fout, rpcgen_table_dcl);
	}

	if (Cflag){
		f_print(fout, "\n#ifdef __cplusplus\n");
		f_print(fout, "}\n");
		f_print(fout, "#endif\n");
	}

	f_print(fout, "\n#endif /* !_%s */\n", guard);
}

/*
 * Compile into an RPC service
 */
static void
s_output(argc, argv, infile, define, extend, outfile, nomain, netflag)
	int argc;
	char *argv[];
	char *infile;
	char *define;
	int extend;
	char *outfile;
	int nomain;
	int netflag;
{
	char *include;
	definition *def;
	int foundprogram = 0;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		f_print(fout, "#include <rpc/rpc.h>\n");

	f_print(fout, "#include <stdio.h>\n");
	f_print(fout, "#include <stdlib.h> /* getenv, exit */\n");
	if (Cflag) {
		f_print (fout,
		"#include <rpc/pmap_clnt.h> /* for pmap_unset */\n");
		f_print (fout, "#include <string.h> /* strcmp */\n");
	}
	if (strcmp(svcclosetime, "-1") == 0)
		indefinitewait = 1;
	else if (strcmp(svcclosetime, "0") == 0)
		exitnow = 1;
	else if (inetdflag || pmflag) {
		f_print(fout, "#include <signal.h>\n");
		timerflag = 1;
	}

	if (!tirpcflag && inetdflag)
		f_print(fout, "#include <sys/ttycom.h> /* TIOCNOTTY */\n");
	if (Cflag && (inetdflag || pmflag)) {
		f_print(fout, "#ifdef __cplusplus\n");
		f_print(fout,
			"#include <sysent.h> /* getdtablesize, open */\n");
		f_print(fout, "#endif /* __cplusplus */\n");
		if (tirpcflag)
			f_print(fout, "#include <unistd.h> /* setsid */\n");
	}
	if (tirpcflag)
		f_print(fout, "#include <sys/types.h>\n");

	f_print(fout, "#include <memory.h>\n");
#ifdef __FreeBSD__
	if (tirpcflag)
#endif
	f_print(fout, "#include <stropts.h>\n");
	if (inetdflag || !tirpcflag) {
		f_print(fout, "#include <sys/socket.h>\n");
		f_print(fout, "#include <netinet/in.h>\n");
	}

	if ((netflag || pmflag) && tirpcflag && !nomain) {
		f_print(fout, "#include <netconfig.h>\n");
	}
	if (tirpcflag)
		f_print(fout, "#include <sys/resource.h> /* rlimit */\n");
	if (logflag || inetdflag || pmflag)
		f_print(fout, "#include <syslog.h>\n");

	/* for ANSI-C */
	if (Cflag)
		f_print(fout,
			"\n#ifndef SIG_PF\n#define	SIG_PF void(*)\
(int)\n#endif\n");

	f_print(fout, "\n#ifdef DEBUG\n#define	RPC_SVC_FG\n#endif\n");
	if (timerflag)
		f_print(fout, "\n#define	_RPCSVC_CLOSEDOWN %s\n",
			svcclosetime);
	while ( (def = get_definition()) ) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_most(infile, netflag, nomain);
	if (!nomain) {
		if (!do_registers(argc, argv)) {
			if (outfilename)
				(void) unlink(outfilename);
			usage();
		}
		write_rest();
	}
}

/*
 * generate client side stubs
 */
static void
l_output(infile, define, extend, outfile)
	char *infile;
	char *define;
	int extend;
	char *outfile;
{
	char *include;
	definition *def;
	int foundprogram = 0;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (Cflag)
		f_print (fout, "#include <memory.h> /* for memset */\n");
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		f_print(fout, "#include <rpc/rpc.h>\n");
	while ( (def = get_definition()) ) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_stubs();
}

/*
 * generate the dispatch table
 */
static void
t_output(infile, define, extend, outfile)
	char *infile;
	char *define;
	int extend;
	char *outfile;
{
	definition *def;
	int foundprogram = 0;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	while ( (def = get_definition()) ) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_tables();
}

/* sample routine for the server template */
static void
svc_output(infile, define, extend, outfile)
	char *infile;
	char *define;
	int extend;
	char *outfile;
{
	definition *def;
	char *include;
	char *outfilename;
	long tell;
	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	checkfiles(infile, outfilename);
	/*
	 * Check if outfile already exists.
	 * if so, print an error message and exit
	 */
	open_output(infile, outfilename);
	add_sample_msg();

	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		f_print(fout, "#include <rpc/rpc.h>\n");

	tell = ftell(fout);
	while ( (def = get_definition()) ) {
		write_sample_svc(def);
	}
	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}

/* sample main routine for client */
static void
clnt_output(infile, define, extend, outfile)
	char *infile;
	char *define;
	int extend;
	char *outfile;
{
	definition *def;
	char *include;
	char *outfilename;
	long tell;
	int has_program = 0;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	checkfiles(infile, outfilename);
	/*
	 * Check if outfile already exists.
	 * if so, print an error message and exit
	 */

	open_output(infile, outfilename);
	add_sample_msg();
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		f_print(fout, "#include <rpc/rpc.h>\n");
	tell = ftell(fout);
	while ( (def = get_definition()) ) {
		has_program += write_sample_clnt(def);
	}

	if (has_program)
		write_sample_clnt_main();

	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}


static void mkfile_output(cmd)
struct commandline *cmd;
{
	char *mkfilename, *clientname, *clntname, *xdrname, *hdrname;
	char *servername, *svcname, *servprogname, *clntprogname;
	char *temp;

	svcname = file_name(cmd->infile, "_svc.c");
	clntname = file_name(cmd->infile, "_clnt.c");
	xdrname = file_name(cmd->infile, "_xdr.c");
	hdrname = file_name(cmd->infile, ".h");


	if (allfiles){
		servername = extendfile(cmd->infile, "_server.c");
		clientname = extendfile(cmd->infile, "_client.c");
	}else{
		servername = " ";
		clientname = " ";
	}
	servprogname = extendfile(cmd->infile, "_server");
	clntprogname = extendfile(cmd->infile, "_client");

	if (allfiles){
		mkfilename = alloc(strlen("makefile.") +
			strlen(cmd->infile) + 1);
		temp = (char *)rindex(cmd->infile, '.');
		strcat(mkfilename, "makefile.");
		(void) strncat(mkfilename, cmd->infile,
			(temp - cmd->infile));
	} else
		mkfilename = cmd->outfile;


	checkfiles(NULL, mkfilename);
	open_output(NULL, mkfilename);

	f_print(fout, "\n# This is a template makefile generated\
		by rpcgen \n");

	f_print(fout, "\n# Parameters \n\n");

	f_print(fout, "CLIENT = %s\nSERVER = %s\n\n",
		clntprogname, servprogname);
	f_print(fout, "SOURCES_CLNT.c = \nSOURCES_CLNT.h = \n");
	f_print(fout, "SOURCES_SVC.c = \nSOURCES_SVC.h = \n");
	f_print(fout, "SOURCES.x = %s\n\n", cmd->infile);
	f_print(fout, "TARGETS_SVC.c = %s %s %s \n",
		svcname, servername, xdrname);
	f_print(fout, "TARGETS_CLNT.c = %s %s %s \n",
		clntname, clientname, xdrname);
	f_print(fout, "TARGETS = %s %s %s %s %s %s\n\n",
		hdrname, xdrname, clntname,
		svcname, clientname, servername);

	f_print(fout, "OBJECTS_CLNT = $(SOURCES_CLNT.c:%%.c=%%.o) \
$(TARGETS_CLNT.c:%%.c=%%.o) ");

	f_print(fout, "\nOBJECTS_SVC = $(SOURCES_SVC.c:%%.c=%%.o) \
$(TARGETS_SVC.c:%%.c=%%.o) ");


	f_print(fout, "\n# Compiler flags \n");
	if (mtflag)
		f_print(fout, "\nCPPFLAGS += -D_REENTRANT\nCFLAGS += -g \nLDLIBS += -lnsl -lthread\n");
	else
#ifdef __FreeBSD__
		f_print(fout, "\nCFLAGS += -g \nLDLIBS +=\n");
#else
		f_print(fout, "\nCFLAGS += -g \nLDLIBS += -lnsl\n");
#endif
	f_print(fout, "RPCGENFLAGS = \n");

	f_print(fout, "\n# Targets \n\n");

	f_print(fout, "all : $(CLIENT) $(SERVER)\n\n");
	f_print(fout, "$(TARGETS) : $(SOURCES.x) \n");
	f_print(fout, "\trpcgen $(RPCGENFLAGS) $(SOURCES.x)\n\n");
	f_print(fout, "$(OBJECTS_CLNT) : $(SOURCES_CLNT.c) $(SOURCES_CLNT.h) \
$(TARGETS_CLNT.c) \n\n");

	f_print(fout, "$(OBJECTS_SVC) : $(SOURCES_SVC.c) $(SOURCES_SVC.h) \
$(TARGETS_SVC.c) \n\n");
	f_print(fout, "$(CLIENT) : $(OBJECTS_CLNT) \n");
#ifdef __FreeBSD__
	f_print(fout, "\t$(CC) -o $(CLIENT) $(OBJECTS_CLNT) \
$(LDLIBS) \n\n");
#else
	f_print(fout, "\t$(LINK.c) -o $(CLIENT) $(OBJECTS_CLNT) \
$(LDLIBS) \n\n");
#endif
	f_print(fout, "$(SERVER) : $(OBJECTS_SVC) \n");
#ifdef __FreeBSD__
	f_print(fout, "\t$(CC) -o $(SERVER) $(OBJECTS_SVC) $(LDLIBS)\n\n ");
	f_print(fout, "clean:\n\t $(RM) -f core $(TARGETS) $(OBJECTS_CLNT) \
$(OBJECTS_SVC) $(CLIENT) $(SERVER)\n\n");
#else
	f_print(fout, "\t$(LINK.c) -o $(SERVER) $(OBJECTS_SVC) $(LDLIBS)\n\n ");
	f_print(fout, "clean:\n\t $(RM) core $(TARGETS) $(OBJECTS_CLNT) \
$(OBJECTS_SVC) $(CLIENT) $(SERVER)\n\n");
#endif
}



/*
 * Perform registrations for service output
 * Return 0 if failed; 1 otherwise.
 */
static int
do_registers(argc, argv)
	int argc;
	char *argv[];
{
	int i;

	if (inetdflag || !tirpcflag) {
		for (i = 1; i < argc; i++) {
			if (streq(argv[i], "-s")) {
				if (!check_nettype(argv[i + 1],
						    valid_i_nettypes))
					return (0);
				write_inetd_register(argv[i + 1]);
				i++;
			}
		}
	} else {
		for (i = 1; i < argc; i++)
			if (streq(argv[i], "-s")) {
				if (!check_nettype(argv[i + 1],
						    valid_ti_nettypes))
					return (0);
				write_nettype_register(argv[i + 1]);
				i++;
			} else if (streq(argv[i], "-n")) {
				write_netid_register(argv[i + 1]);
				i++;
			}
	}
	return (1);
}

/*
 * Add another argument to the arg list
 */
static void
addarg(cp)
	char *cp;
{
	if (argcount >= ARGLISTLEN) {
		f_print(stderr, "rpcgen: too many defines\n");
		crash();
		/*NOTREACHED*/
	}
	arglist[argcount++] = cp;

}

static void
putarg(where, cp)
	char *cp;
	int where;
{
	if (where >= ARGLISTLEN) {
		f_print(stderr, "rpcgen: arglist coding error\n");
		crash();
		/*NOTREACHED*/
	}
	arglist[where] = cp;
}

/*
 * if input file is stdin and an output file is specified then complain
 * if the file already exists. Otherwise the file may get overwritten
 * If input file does not exist, exit with an error
 */

static void
checkfiles(infile, outfile)
char *infile;
char *outfile;
{

	struct stat buf;

	if (infile)		/* infile ! = NULL */
		if (stat(infile, &buf) < 0)
		{
			perror(infile);
			crash();
		};
	if (outfile) {
		if (stat(outfile, &buf) < 0)
			return;	/* file does not exist */
		else {
			f_print(stderr,
	"file '%s' already exists and may be overwritten\n",
				outfile);
			crash();
		}
	}
}

/*
 * Parse command line arguments
 */
static int
parseargs(argc, argv, cmd)
	int argc;
	char *argv[];
	struct commandline *cmd;
{
	int i;
	int j;
	char c, ch;
	char flag[(1 << 8 * sizeof (char))];
	int nflags;

	cmdname = argv[0];
	cmd->infile = cmd->outfile = NULL;
	if (argc < 2) {
		return (0);
	}
	allfiles = 0;
	flag['c'] = 0;
	flag['h'] = 0;
	flag['l'] = 0;
	flag['m'] = 0;
	flag['o'] = 0;
	flag['s'] = 0;
	flag['n'] = 0;
	flag['t'] = 0;
	flag['S'] = 0;
	flag['C'] = 0;
	flag['M'] = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (cmd->infile) {
				f_print(stderr,
	"Cannot specify more than one input file.\n");

				return (0);
			}
			cmd->infile = argv[i];
		} else {
			for (j = 1; argv[i][j] != 0; j++) {
				c = argv[i][j];
				switch (c) {
				case 'a':
					allfiles = 1;
					break;
				case 'c':
				case 'h':
				case 'l':
				case 'm':
				case 't':
					if (flag[(int)c]) {
						return (0);
					}
					flag[(int)c] = 1;
					break;
				case 'S':
					/*
					 * sample flag: Ss or Sc.
					 *  Ss means set flag['S'];
					 *  Sc means set flag['C'];
					 *  Sm means set flag['M'];
					 */
					ch = argv[i][++j]; /* get next char */
					if (ch == 's')
						ch = 'S';
					else if (ch == 'c')
						ch = 'C';
					else if (ch == 'm')
						ch = 'M';
					else
						return (0);

					if (flag[(int)ch]) {
						return (0);
					}
					flag[(int)ch] = 1;
					break;
				case 'C': /* ANSI C syntax */
					Cflag = 1;
					ch = argv[i][j+1]; /* get next char */

					if (ch != 'C')
						break;
					CCflag = 1;
					break;
				case 'b':
					/*
					 *  Turn TIRPC flag off for
					 *  generating backward compatible
					 *  code
					 */
#ifdef __FreeBSD__
					tirpcflag = 1;
#else
					tirpcflag = 0;
#endif
					break;

				case 'I':
					inetdflag = 1;
					break;
				case 'N':
					newstyle = 1;
					break;
				case 'L':
					logflag = 1;
					break;
				case 'K':
					if (++i == argc) {
						return (0);
					}
					svcclosetime = argv[i];
					goto nextarg;
				case 'T':
					tblflag = 1;
					break;
				case 'M':
					mtflag = 1;
					break;
				case 'i' :
					if (++i == argc) {
						return (0);
					}
					inline = atoi(argv[i]);
					goto nextarg;
				case 'n':
				case 'o':
				case 's':
					if (argv[i][j - 1] != '-' ||
					    argv[i][j + 1] != 0) {
						return (0);
					}
					flag[(int)c] = 1;
					if (++i == argc) {
						return (0);
					}
					if (c == 'o') {
						if (cmd->outfile) {
							return (0);
						}
						cmd->outfile = argv[i];
					}
					goto nextarg;
				case 'D':
					if (argv[i][j - 1] != '-') {
						return (0);
					}
					(void) addarg(argv[i]);
					goto nextarg;
				case 'Y':
					if (++i == argc) {
						return (0);
					}
					(void) strcpy(pathbuf, argv[i]);
					(void) strcat(pathbuf, "/cpp");
					CPP = pathbuf;
					cppDefined = 1;
					goto nextarg;



				default:
					return (0);
				}
			}
		nextarg:
			;
		}
	}

	cmd->cflag = flag['c'];
	cmd->hflag = flag['h'];
	cmd->lflag = flag['l'];
	cmd->mflag = flag['m'];
	cmd->nflag = flag['n'];
	cmd->sflag = flag['s'];
	cmd->tflag = flag['t'];
	cmd->Ssflag = flag['S'];
	cmd->Scflag = flag['C'];
	cmd->makefileflag = flag['M'];

	if (tirpcflag) {
		pmflag = inetdflag ? 0 : 1;
		/* pmflag or inetdflag is always TRUE */
		if ((inetdflag && cmd->nflag)) {
			/* netid not allowed with inetdflag */
	f_print(stderr, "Cannot use netid flag with inetd flag.\n");
			return (0);
		}
	} else {		/* 4.1 mode */
		pmflag = 0;	/* set pmflag only in tirpcmode */
#ifndef __FreeBSD__
		inetdflag = 1;	/* inetdflag is TRUE by default */
#endif
		if (cmd->nflag) { /* netid needs TIRPC */
	f_print(stderr, "Cannot use netid flag without TIRPC.\n");
			return (0);
		}
	}

	if (newstyle && (tblflag || cmd->tflag)) {
		f_print(stderr, "Cannot use table flags with newstyle.\n");
		return (0);
	}

	/* check no conflicts with file generation flags */
	nflags = cmd->cflag + cmd->hflag + cmd->lflag + cmd->mflag +
		cmd->sflag + cmd->nflag + cmd->tflag + cmd->Ssflag +
			cmd->Scflag + cmd->makefileflag;

	if (nflags == 0) {
		if (cmd->outfile != NULL || cmd->infile == NULL) {
			return (0);
		}
	} else if (cmd->infile == NULL &&
	    (cmd->Ssflag || cmd->Scflag || cmd->makefileflag)) {
		f_print(stderr,
		  "\"infile\" is required for template generation flags.\n");
		return (0);
	} if (nflags > 1) {
		f_print(stderr,
			"Cannot have more than one file generation flag.\n");
		return (0);
	}
	return (1);
}

static void
usage()
{
	f_print(stderr, "usage:  %s infile\n", cmdname);
	f_print(stderr,
		"\t%s [-abCLNTM] [-Dname[=value]] [-i size]\
[-I [-K seconds]] [-Y path] infile\n",
		cmdname);
	f_print(stderr,
		"\t%s [-c | -h | -l | -m | -t | -Sc | -Ss | -Sm]\
[-o outfile] [infile]\n",
		cmdname);
	f_print(stderr, "\t%s [-s nettype]* [-o outfile] [infile]\n", cmdname);
	f_print(stderr, "\t%s [-n netid]* [-o outfile] [infile]\n", cmdname);
	options_usage();
	exit(1);
}

static void
options_usage()
{
	f_print(stderr, "options:\n");
	f_print(stderr, "-a\t\tgenerate all files, including samples\n");
	f_print(stderr, "-b\t\tbackward compatibility mode (generates code\
for SunOS 4.X)\n");
	f_print(stderr, "-c\t\tgenerate XDR routines\n");
	f_print(stderr, "-C\t\tANSI C mode\n");
	f_print(stderr, "-Dname[=value]\tdefine a symbol (same as #define)\n");
	f_print(stderr, "-h\t\tgenerate header file\n");
	f_print(stderr, "-i size\t\tsize at which to start generating\
inline code\n");
	f_print(stderr, "-I\t\tgenerate code for inetd support in server\
(for SunOS 4.X)\n");
	f_print(stderr, "-K seconds\tserver exits after K seconds of\
inactivity\n");
	f_print(stderr, "-l\t\tgenerate client side stubs\n");
	f_print(stderr, "-L\t\tserver errors will be printed to syslog\n");
	f_print(stderr, "-m\t\tgenerate server side stubs\n");
	f_print(stderr, "-M\t\tgenerate MT-safe code\n");
	f_print(stderr, "-n netid\tgenerate server code that supports\
named netid\n");
	f_print(stderr, "-N\t\tsupports multiple arguments and\
call-by-value\n");
	f_print(stderr, "-o outfile\tname of the output file\n");
	f_print(stderr, "-s nettype\tgenerate server code that supports named\
nettype\n");
	f_print(stderr, "-Sc\t\tgenerate sample client code that uses remote\
procedures\n");
	f_print(stderr, "-Ss\t\tgenerate sample server code that defines\
remote procedures\n");
	f_print(stderr, "-Sm \t\tgenerate makefile template \n");

	f_print(stderr, "-t\t\tgenerate RPC dispatch table\n");
	f_print(stderr, "-T\t\tgenerate code to support RPC dispatch tables\n");
	f_print(stderr, "-Y path\t\tpath where cpp is found\n");
	exit(1);
}

#ifndef __FreeBSD__
char *
rindex(sp, c)
	register char *sp, c;
{
	register char *r;

	r = NULL;
	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return (r);
}
#endif
