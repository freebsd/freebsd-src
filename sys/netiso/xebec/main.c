/*
 *	from: main.c,v 2.4 88/09/19 12:55:13 nhall Exp
 *	$Id: main.c,v 1.2 1993/10/16 21:33:13 rgrimes Exp $
 */

/*
 * TODO:
 * rewrite the command line stuff altogether - it's kludged beyond
 * belief (as is the rest of the code...)
 *
 * DISCLAIMER DISCLAIMER DISCLAIMER
 * This code is such a kludge that I don't want to put my name on it.
 * It was a ridiculously fast hack and needs rewriting.
 * However it does work...
 */

#include <stdio.h>
#include <strings.h>
#include "malloc.h"
#include "debug.h"
#include "main.h"

int	debug[128];

int lineno = 1;

FILE *statefile, *actfile, *eventfile_h, *statevalfile;
FILE *infile, *astringfile;
char *Transfilename;
char *astringfile_name = DEBUGFILE;
char *actfile_name = ACTFILE;
char *statefile_name = STATEFILE;
char *statevalfile_name = STATEVALFILE;
char *eventfile_h_name = EVENTFILE_H;
int print_trans = 0;
int print_protoerrs = 0;
int pgoption = 0;
char kerneldirname[50] = "\0";

char protocol[50];

char *synonyms[] = {
	"EVENT",
	"PCB",
	0
};

usage(a)
char *a;
{
	fprintf(stderr, 
	"usage: %s <transition file> {-D<debug options>} <other options>\n",
		a);
	fprintf(stderr, "\t<other options> is any combination of:\n");
	fprintf(stderr, "\t\t-A<action file name>\n");
	fprintf(stderr, "\t\t-E<event file name>\n");
	fprintf(stderr, "\t\t-S<state file name>\n");
	fprintf(stderr, "\t\t-I<initial values file name>\n");
	fprintf(stderr, "\t\t-X<debugging file name>\n");
	fprintf(stderr, "\t\t-K<directory name>\n");
	fprintf(stderr, 
	"\tThese names do NOT include the suffices (.c, .h)\n");
	fprintf(stderr, 
	"\t\t-D<options> to turn on debug options for xebec itself\n");
	fprintf(stderr, "\t-<nn> for levels of debugging output\n");
	fprintf(stderr, "\t\t<nn> ranges from 1 to 3, 1 is default(everything)\n");
	fprintf(stderr, "\t\t-T to print transitions\n");
	fprintf(stderr, "\t\t-e to print list of combinations of\n");
	fprintf(stderr, "\t\t\t [event,old_state] that produce protocol errors\n");
	fprintf(stderr, "\t\t-g include profiling code in driver\n");
	Exit(-1);
}

openfiles(proto)
register char *proto;
{
	register char *junk;
	register int lenp = strlen(proto);

	IFDEBUG(b)
		fprintf(OUT, "openfiles %s\n",proto);
	ENDDEBUG

#define HEADER Header
#define SOURCE Source
#define DOIT(X)\
	/* GAG */\
	junk = Malloc( 2 + lenp + strlen(X/**/_name) );\
	(void) sprintf(junk, "%s_", proto);\
	X/**/_name = strcat(junk, X/**/_name);\
	X = fopen(X/**/_name, "w");\
	if((X)==(FILE *)0)\
	{ fprintf(stderr,"Open failed: %s\n", "X"); Exit(-1); }\
	fprintf(X, "/* %cHeader%c */\n",'$', '$' );\
	fprintf(X, "/* %cSource%c */\n",'$', '$' );

	DOIT(eventfile_h);

	IFDEBUG(X)
#ifdef DEBUG
		DOIT(astringfile);
#endif DEBUG
		fprintf(astringfile, 
				"#ifndef _NFILE\n#include <stdio.h>\n#endif _NFILE\n" );
	ENDDEBUG

	DOIT(statevalfile);
	DOIT(statefile);
	DOIT(actfile);
	fprintf(actfile,
		"#ifndef lint\nstatic char *rcsid = \"$Header/**/$\";\n#endif lint\n");

	if(pgoption)
		putdriver(actfile, 15);
	else 
		putdriver(actfile, 14);

	FakeFilename(actfile, Transfilename, lineno);
	putdriver(actfile, 1);
	FakeFilename(actfile, Transfilename, lineno);
	putdriver(actfile, 12);
	fprintf(actfile, "#include \"%s%s\"\n", kerneldirname, statevalfile_name);
	FakeFilename(actfile, Transfilename, lineno);
	putdriver(actfile, 2);

	initsets(eventfile_h, statefile);
}

includecode(file, f)
FILE *file;
register char *f;
{
	register int count=1;
	static char o='{';
	static char c='}';
	register char *g;

	IFDEBUG(a)
		fprintf(stdout, "including: %s, f=0x%x", f,f);
	ENDDEBUG
	g = ++f;
	while(count>0) {
		if(*g == o) count++;
		if(*g == c) count--;
		g++;
	}
	*(--g) = '\0';
	IFDEBUG(a)
		fprintf(stdout, "derived: %s", f);
	ENDDEBUG
	fprintf(file, "%s", f);
	FakeFilename(file, Transfilename, lineno);
}

putincludes()
{
	FakeFilename(actfile, Transfilename, lineno);
	fprintf(actfile, "\n#include \"%s%s\"\n", kerneldirname, eventfile_h_name);
	IFDEBUG(X)
		if( !debug['K'] )
			fprintf(actfile, "\n#include \"%s\"\n", astringfile_name);
			/* not in kernel mode */
	ENDDEBUG
	FakeFilename(actfile, Transfilename, lineno);
}

main(argc, argv)
int argc;
char *argv[];
{
	register int i = 2;
	extern char *strcpy();
	int start, finish;
	extern int FirstEventAttribute;
	extern int Nevents, Nstates;

	start = time(0);
	if(argc < 2) {
		usage(argv[0]);
	}
	IFDEBUG(a)
		fprintf(stdout, "infile = %s\n",argv[1]);
	ENDDEBUG
	Transfilename = argv[1];
	infile = fopen(argv[1], "r");

	if(argc > 2) while(i < argc) {
		register int j=0;
		char c;
		char *name;

		if(argv[i][j] == '-') j++;
		switch(c = argv[i][j]) {

		/* GROT */
		case 'A':
			name = &argv[i][++j];
			actfile_name = Malloc( strlen(name)+4);
			actfile_name =  (char *)strcpy(actfile_name,name);
#ifdef LINT
			name =
#endif LINT
			strcat(actfile_name, ".c");
			fprintf(stdout, "debugging file is %s\n",actfile_name);
			break;
		case 'K':
			debug[c]=1;
			fprintf(OUT, "option %c file %s\n",c, &argv[i][j+1]);
			(void) strcpy(kerneldirname,&argv[i][++j]);
			break;
		case 'X':
			debug[c]=1;
			name = &argv[i][++j];
			astringfile_name = Malloc( strlen(name)+4);
			astringfile_name =  (char *)strcpy(astringfile_name,name);
#ifdef LINT
			name =
#endif LINT
			strcat(astringfile_name, ".c");
			fprintf(OUT, "option %c, astringfile name %s\n",c, name);
			break;
		case 'E':
			name = &argv[i][++j];
			eventfile_h_name = Malloc( strlen(name)+4);
			eventfile_h_name =  (char *)strcpy(eventfile_h_name,name);
#ifdef LINT
			name =
#endif LINT
			strcat(eventfile_h_name, ".h");
			fprintf(stdout, "event files is %s\n",eventfile_h_name);
			break;
		case 'I':
			name = &argv[i][++j];
			statevalfile_name = Malloc( strlen(name)+4 );
			statevalfile_name =  (char *)strcpy(statevalfile_name,name);
#ifdef LINT
			name =
#endif LINT
			strcat(statevalfile_name, ".init");
			fprintf(stdout, "state table initial values file is %s\n",statevalfile_name);
			break;
		case 'S':
			name = &argv[i][++j];
			statefile_name = Malloc( strlen(name)+4);
			statefile_name =  (char *)strcpy(statefile_name,name);
#ifdef LINT
			name =
#endif LINT
			strcat(statefile_name, ".h");
			fprintf(stdout, "state file is %s\n",statefile_name);
			break;
		/* END GROT */
		case '1':
		case '2':
		case '3':
			debug['X']= (int)argv[i][j] - (int) '0';
			fprintf(OUT, "value of debug['X'] is 0x%x,%d\n", debug['X'],
				debug['X']);
			break;
		case 'D':
			while( c = argv[i][++j] ) {
				if(c ==  'X') {
					fprintf(OUT, "debugging on");
					if(debug['X']) fprintf(OUT,
						" - overrides any -%d flags used\n", debug['X']);
				}
				debug[c]=1;
				fprintf(OUT, "debug %c\n",c);
			}
			break;
		case 'g':
			pgoption = 1;
			fprintf(stdout, "Profiling\n");
			break;
		case 'e':
			print_protoerrs = 1;
			fprintf(stdout, "Protocol error table:\n");
			break;

		case 'T':
			print_trans = 1;
			fprintf(stdout, "Transitions:\n");
			break;
		default:
			usage(argv[0]);
			break;
		}
		i++;
	}
	if(kerneldirname[0]) {
		char *c;
#ifdef notdef
		if(debug['X']) {
			fprintf(OUT, "Option K overrides option X\n");
			debug['X'] = 0;
		}
#endif notdef
		if(strlen(kerneldirname)<1) {
			fprintf(OUT, "K option: dir name too short!\n");
			exit(-1);
		}
		/* add ../name/ */
		c = (char *) Malloc(strlen(kerneldirname)+6) ;
		if(c <= (char *)0) {
			fprintf(OUT, "Cannot allocate %d bytes for kerneldirname\n",
				strlen(kerneldirname + 6) );
			fprintf(OUT, "kerneldirname is %s\n", kerneldirname  );
			exit(-1);
		}
		*c = '.';
		*(c+1) = '.';
		*(c+2) = '/';
		(void) strcat(c, kerneldirname);
		(void) strcat(c, "/\0");
		strcpy(kerneldirname, c);
	}

	init_alloc();

	(void) llparse();

	/* {{ */
	if( !FirstEventAttribute )
		fprintf(eventfile_h, "\t}ev_union;\n");
	fprintf(eventfile_h, "};/* end struct event */\n");
	fprintf(eventfile_h, "\n#define %s_NEVENTS 0x%x\n", protocol, Nevents);
	fprintf(eventfile_h,
		"\n#define ATTR(X)ev_union.%s/**/X/**/\n",EV_PREFIX);
	(void) fclose(eventfile_h);

	/* {{ */ fprintf(actfile, "\t}\nreturn 0;\n}\n"); /* end switch; end action() */
	dump_predtable(actfile);

	putdriver(actfile, 3);
	IFDEBUG(X)
		if(!debug['K'])
			putdriver(actfile, 4);
	ENDDEBUG
	putdriver(actfile, 6);
	IFDEBUG(X)
		/*
		putdriver(actfile, 10);
		*/
		if(debug['K']) { 
			putdriver(actfile, 11);
		} else {
			switch(debug['X']) {
			case 1:
			default:
				putdriver(actfile, 7);
				break;
			case 2:
				putdriver(actfile, 13);
				break;
			case 3:
				break;
			}
		}
	ENDDEBUG
	putdriver(actfile, 8);
	(void) fclose(actfile);
	IFDEBUG(X) 
		/* { */ 
		fprintf(astringfile, "};\n");
		(void) fclose(astringfile);
	ENDDEBUG

	(void) fclose(statevalfile);

	fprintf(statefile, "\n#define %s_NSTATES 0x%x\n", protocol, Nstates);
	(void) fclose(statefile);

	finish = time(0);
	fprintf(stdout, "%d seconds\n", finish - start);
	if( print_protoerrs ) 
		printprotoerrs();
}

int transno = 0;

Exit(n)
{
	fprintf(stderr, "Error at line %d\n",lineno);
	if(transno) fprintf(stderr, "Transition number %d\n",transno);
	(void) fflush(stdout);
	(void) fflush(statefile);
	(void) fflush(eventfile_h);
	(void) fflush(actfile);
	exit(n);
}

syntax() 
{
	static char *synt[] = {
		"*PROTOCOL <string>\n",
		"*PCB <string> <optional: SYNONYM synonymstring>\n",
		"<optional: *INCLUDE {\n<C source>\n} >\n",
		"*STATES <string>\n",
		"*EVENTS <string>\n",
		"*TRANSITIONS <string>\n",
	};
}
	
FakeFilename(outfile, name, l)
FILE *outfile;
char *name;
int l;
{
	/*
	doesn't work
	fprintf(outfile, "\n\n\n\n# line %d \"%s\"\n", l, name);
	*/
}
