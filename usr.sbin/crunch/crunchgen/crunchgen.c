/*
 * Copyright (c) 1994 University of Maryland
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of U.M. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  U.M. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * U.M. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL U.M.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: James da Silva, Systems Design and Analysis Group
 *			   Computer Science Department
 *			   University of Maryland at College Park
 */
/*
 * ========================================================================
 * crunchgen.c
 *
 * Generates a Makefile and main C file for a crunched executable,
 * from specs given in a .conf file.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#define CRUNCH_VERSION	"0.2"

#define MAXLINELEN	16384
#define MAXFIELDS 	 2048


/* internal representation of conf file: */

/* simple lists of strings suffice for most parms */

typedef struct strlst {
    struct strlst *next;
    char *str;
} strlst_t;

/* progs have structure, each field can be set with "special" or calculated */

typedef struct prog {
    struct prog *next;
    char *name, *ident;
    char *srcdir, *objdir;
    strlst_t *objs, *objpaths;
    strlst_t *links;
    int goterror;
} prog_t;


/* global state */

strlst_t *srcdirs = NULL;
strlst_t *libs    = NULL;
prog_t   *progs   = NULL;

char line[MAXLINELEN];

char confname[MAXPATHLEN], infilename[MAXPATHLEN];
char outmkname[MAXPATHLEN], outcfname[MAXPATHLEN], execfname[MAXPATHLEN];
char tempfname[MAXPATHLEN], cachename[MAXPATHLEN], curfilename[MAXPATHLEN];
int linenum = -1;
int goterror = 0;

char *pname = "crunchgen";

int verbose, readcache;	/* options */
int reading_cache;

int list_mode;

/* general library routines */

void status(char *str);
void out_of_memory(void);
void add_string(strlst_t **listp, char *str);
int is_dir(char *pathname);
int is_nonempty_file(char *pathname);

/* helper routines for main() */

void usage(void);
void parse_conf_file(void);
void gen_outputs(void);


int main(int argc, char **argv)
{
    char *p;
    int optc;
    extern int optind;
    extern char *optarg;

    verbose = 1;
    readcache = 1;
    *outmkname = *outcfname = *execfname = '\0';

    if(argc > 0) pname = argv[0];

    while((optc = getopt(argc, argv, "lm:c:e:fq")) != -1) {
	switch(optc) {
	case 'f':	readcache = 0; break;
	case 'q':	verbose = 0; break;

	case 'm':	strcpy(outmkname, optarg); break;
	case 'c':	strcpy(outcfname, optarg); break;
	case 'e':	strcpy(execfname, optarg); break;
	case 'l':	list_mode++; verbose = 0; break;

	case '?':
	default:	usage();
	}
    }

    argc -= optind;
    argv += optind;

    if(argc != 1) usage();

    /*
     * generate filenames
     */

    strcpy(infilename, argv[0]);

    /* confname = `basename infilename .conf` */

    if((p=strrchr(infilename, '/')) != NULL) strcpy(confname, p+1);
    else strcpy(confname, infilename);
    if((p=strrchr(confname, '.')) != NULL && !strcmp(p, ".conf")) *p = '\0';

    if(!*outmkname) sprintf(outmkname, "%s.mk", confname);
    if(!*outcfname) sprintf(outcfname, "%s.c", confname);
    if(!*execfname) sprintf(execfname, "%s", confname);

    sprintf(cachename, "%s.cache", confname);
    sprintf(tempfname, ".tmp_%sXXXXXX", confname);
    if(mktemp(tempfname) == NULL) {
	perror(tempfname);
	exit(1);
    }

    parse_conf_file();
    if (list_mode)
	exit(goterror);

    gen_outputs();

    exit(goterror);
}


void usage(void)
{
    fprintf(stderr,
	"%s [-fq] [-m <makefile>] [-c <c file>] [-e <exec file>] <conffile>\n",
	    pname);
    exit(1);
}


/*
 * ========================================================================
 * parse_conf_file subsystem
 *
 */

/* helper routines for parse_conf_file */

void parse_one_file(char *filename);
void parse_line(char *line, int *fc, char **fv, int nf);
void add_srcdirs(int argc, char **argv);
void add_progs(int argc, char **argv);
void add_link(int argc, char **argv);
void add_libs(int argc, char **argv);
void add_special(int argc, char **argv);

prog_t *find_prog(char *str);
void add_prog(char *progname);


void parse_conf_file(void)
{
    if(!is_nonempty_file(infilename)) {
	fprintf(stderr, "%s: fatal: input file \"%s\" not found.\n",
		pname, infilename);
	exit(1);
    }
    parse_one_file(infilename);
    if(readcache && is_nonempty_file(cachename)) {
	reading_cache = 1;
	parse_one_file(cachename);
    }
}


void parse_one_file(char *filename)
{
    char *fieldv[MAXFIELDS];
    int fieldc;
    void (*f)(int c, char **v);
    FILE *cf;

    sprintf(line, "reading %s", filename);
    status(line);
    strcpy(curfilename, filename);

    if((cf = fopen(curfilename, "r")) == NULL) {
	perror(curfilename);
	goterror = 1;
	return;
    }

    linenum = 0;
    while(fgets(line, MAXLINELEN, cf) != NULL) {
	linenum++;
	parse_line(line, &fieldc, fieldv, MAXFIELDS);
	if(fieldc < 1) continue;
	if(!strcmp(fieldv[0], "srcdirs"))	f = add_srcdirs;
	else if(!strcmp(fieldv[0], "progs"))    f = add_progs;
	else if(!strcmp(fieldv[0], "ln"))	f = add_link;
	else if(!strcmp(fieldv[0], "libs"))	f = add_libs;
	else if(!strcmp(fieldv[0], "special"))	f = add_special;
	else {
	    fprintf(stderr, "%s:%d: skipping unknown command `%s'.\n",
		    curfilename, linenum, fieldv[0]);
	    goterror = 1;
	    continue;
	}
	if(fieldc < 2) {
	    fprintf(stderr,
		    "%s:%d: %s command needs at least 1 argument, skipping.\n",
		    curfilename, linenum, fieldv[0]);
	    goterror = 1;
	    continue;
	}
	f(fieldc, fieldv);
    }

    if(ferror(cf)) {
	perror(curfilename);
	goterror = 1;
    }
    fclose(cf);
}


void parse_line(char *line, int *fc, char **fv, int nf)
{
    char *p;

    p = line;
    *fc = 0;
    while(1) {
	while(isspace(*p)) p++;
	if(*p == '\0' || *p == '#') break;

	if(*fc < nf) fv[(*fc)++] = p;
	while(*p && !isspace(*p) && *p != '#') p++;
	if(*p == '\0' || *p == '#') break;
	*p++ = '\0';
    }
    if(*p) *p = '\0';		/* needed for '#' case */
}


void add_srcdirs(int argc, char **argv)
{
    int i;

    for(i=1;i<argc;i++) {
	if(is_dir(argv[i]))
	    add_string(&srcdirs, argv[i]);
	else {
	    fprintf(stderr, "%s:%d: `%s' is not a directory, skipping it.\n",
		    curfilename, linenum, argv[i]);
	    goterror = 1;
	}
    }
}


void add_progs(int argc, char **argv)
{
    int i;

    for(i=1;i<argc;i++)
	add_prog(argv[i]);
}


void add_prog(char *progname)
{
    prog_t *p1, *p2;

    /* add to end, but be smart about dups */

    for(p1 = NULL, p2 = progs; p2 != NULL; p1 = p2, p2 = p2->next)
	if(!strcmp(p2->name, progname)) return;

    p2 = malloc(sizeof(prog_t));
    if(p2) p2->name = strdup(progname);
    if(!p2 || !p2->name)
	out_of_memory();

    p2->next = NULL;
    if(p1 == NULL) progs = p2;
    else p1->next = p2;

    p2->ident = p2->srcdir = p2->objdir = NULL;
    p2->links = p2->objs = NULL;
    p2->goterror = 0;
    if (list_mode)
        printf("%s\n",progname);
}


void add_link(int argc, char **argv)
{
    int i;
    prog_t *p = find_prog(argv[1]);

    if(p == NULL) {
	fprintf(stderr,
		"%s:%d: no prog %s previously declared, skipping link.\n",
		curfilename, linenum, argv[1]);
	goterror = 1;
	return;
    }
    for(i=2;i<argc;i++) {
	if (list_mode)
		printf("%s\n",argv[i]);
	add_string(&p->links, argv[i]);
    }
}


void add_libs(int argc, char **argv)
{
    int i;

    for(i=1;i<argc;i++)
	add_string(&libs, argv[i]);
}


void add_special(int argc, char **argv)
{
    int i;
    prog_t *p = find_prog(argv[1]);

    if(p == NULL) {
	if(reading_cache) return;
	fprintf(stderr,
		"%s:%d: no prog %s previously declared, skipping special.\n",
		curfilename, linenum, argv[1]);
	goterror = 1;
	return;
    }

    if(!strcmp(argv[2], "ident")) {
	if(argc != 4) goto argcount;
	if((p->ident = strdup(argv[3])) == NULL)
	    out_of_memory();
    }
    else if(!strcmp(argv[2], "srcdir")) {
	if(argc != 4) goto argcount;
	if((p->srcdir = strdup(argv[3])) == NULL)
	    out_of_memory();
    }
    else if(!strcmp(argv[2], "objdir")) {
	if(argc != 4) goto argcount;
	if((p->objdir = strdup(argv[3])) == NULL)
	    out_of_memory();
    }
    else if(!strcmp(argv[2], "objs")) {
	p->objs = NULL;
	for(i=3;i<argc;i++)
	    add_string(&p->objs, argv[i]);
    }
    else if(!strcmp(argv[2], "objpaths")) {
	p->objpaths = NULL;
	for(i=3;i<argc;i++)
	    add_string(&p->objpaths, argv[i]);
    }
    else {
	fprintf(stderr, "%s:%d: bad parameter name `%s', skipping line.\n",
		curfilename, linenum, argv[2]);
	goterror = 1;
    }
    return;


 argcount:
    fprintf(stderr,
	    "%s:%d: too %s arguments, expected \"special %s %s <string>\".\n",
	    curfilename, linenum, argc < 4? "few" : "many", argv[1], argv[2]);
    goterror = 1;
}


prog_t *find_prog(char *str)
{
    prog_t *p;

    for(p = progs; p != NULL; p = p->next)
	if(!strcmp(p->name, str)) return p;

    return NULL;
}


/*
 * ========================================================================
 * gen_outputs subsystem
 *
 */

/* helper subroutines */

void remove_error_progs(void);
void fillin_program(prog_t *p);
void gen_specials_cache(void);
void gen_output_makefile(void);
void gen_output_cfile(void);

void fillin_program_objs(prog_t *p, char *path);
void top_makefile_rules(FILE *outmk);
void prog_makefile_rules(FILE *outmk, prog_t *p);
void output_strlst(FILE *outf, strlst_t *lst);
char *genident(char *str);
char *dir_search(char *progname);


void gen_outputs(void)
{
    prog_t *p;

    for(p = progs; p != NULL; p = p->next)
	fillin_program(p);

    remove_error_progs();
    gen_specials_cache();
    gen_output_cfile();
    gen_output_makefile();
    status("");
    fprintf(stderr,
	    "Run \"make -f %s objs exe\" to build crunched binary.\n",
	    outmkname);
}


void fillin_program(prog_t *p)
{
    char path[MAXPATHLEN];
    char *srcparent;
    strlst_t *s;

    sprintf(line, "filling in parms for %s", p->name);
    status(line);

    if(!p->ident)
	p->ident = genident(p->name);
    if(!p->srcdir) {
	srcparent = dir_search(p->name);
	if(srcparent)
	    sprintf(path, "%s/%s", srcparent, p->name);
	if(is_dir(path))
	    p->srcdir = strdup(path);
    }
    if(!p->objdir && p->srcdir) {
	FILE *f;

	sprintf(path, "cd %s && echo -n /usr/obj/`pwd`", p->srcdir);
        p->objdir = p->srcdir;
	f = popen(path,"r");
	if (f) {
	    fgets(path,sizeof path, f);
	    if (!pclose(f)) {
		if(is_dir(path))
		    p->objdir = strdup(path);
	    }
	}


    }

    if(p->srcdir) sprintf(path, "%s/Makefile", p->srcdir);
    if(!p->objs && p->srcdir && is_nonempty_file(path))
	fillin_program_objs(p, path);

    if(!p->objpaths && p->objdir && p->objs)
	for(s = p->objs; s != NULL; s = s->next) {
	    sprintf(line, "%s/%s", p->objdir, s->str);
	    add_string(&p->objpaths, line);
	}

    if(!p->srcdir && verbose)
	fprintf(stderr, "%s: %s: warning: could not find source directory.\n",
		infilename, p->name);
    if(!p->objs && verbose)
	fprintf(stderr, "%s: %s: warning: could not find any .o files.\n",
		infilename, p->name);

    if(!p->objpaths) {
	fprintf(stderr,
		"%s: %s: error: no objpaths specified or calculated.\n",
		infilename, p->name);
	p->goterror = goterror = 1;
    }
}

void fillin_program_objs(prog_t *p, char *path)
{
    char *obj, *cp;
    int rc;
    FILE *f;

    /* discover the objs from the srcdir Makefile */

    if((f = fopen(tempfname, "w")) == NULL) {
	perror(tempfname);
	goterror = 1;
	return;
    }

    fprintf(f, ".include \"%s\"\n", path);
    fprintf(f, ".if defined(PROG) && !defined(OBJS)\n");
    fprintf(f, "OBJS=${PROG}.o\n");
    fprintf(f, ".endif\n");
    fprintf(f, "crunchgen_objs:\n\t@echo 'OBJS= '${OBJS}\n");
    fclose(f);

    sprintf(line, "make -f %s crunchgen_objs 2>&1", tempfname);
    if((f = popen(line, "r")) == NULL) {
	perror("submake pipe");
	goterror = 1;
	return;
    }

    while(fgets(line, MAXLINELEN, f)) {
	if(strncmp(line, "OBJS= ", 6)) {
	    fprintf(stderr, "make error: %s", line);
	    goterror = 1;
	    continue;
	}
	cp = line + 6;
	while(isspace(*cp)) cp++;
	while(*cp) {
	    obj = cp;
	    while(*cp && !isspace(*cp)) cp++;
	    if(*cp) *cp++ = '\0';
	    add_string(&p->objs, obj);
	    while(isspace(*cp)) cp++;
	}
    }
    if((rc=pclose(f)) != 0) {
	fprintf(stderr, "make error: make returned %d\n", rc);
	goterror = 1;
    }
    unlink(tempfname);
}

void remove_error_progs(void)
{
    prog_t *p1, *p2;

    p1 = NULL; p2 = progs;
    while(p2 != NULL) {
	if(!p2->goterror)
	    p1 = p2, p2 = p2->next;
	else {
	    /* delete it from linked list */
	    fprintf(stderr, "%s: %s: ignoring program because of errors.\n",
		    infilename, p2->name);
	    if(p1) p1->next = p2->next;
	    else progs = p2->next;
	    p2 = p2->next;
	}
    }
}

void gen_specials_cache(void)
{
    FILE *cachef;
    prog_t *p;

    sprintf(line, "generating %s", cachename);
    status(line);

    if((cachef = fopen(cachename, "w")) == NULL) {
	perror(cachename);
	goterror = 1;
	return;
    }

    fprintf(cachef, "# %s - parm cache generated from %s by crunchgen %s\n\n",
	    cachename, infilename, CRUNCH_VERSION);

    for(p = progs; p != NULL; p = p->next) {
	fprintf(cachef, "\n");
	if(p->srcdir)
	    fprintf(cachef, "special %s srcdir %s\n", p->name, p->srcdir);
	if(p->objdir)
	    fprintf(cachef, "special %s objdir %s\n", p->name, p->objdir);
	if(p->objs) {
	    fprintf(cachef, "special %s objs", p->name);
	    output_strlst(cachef, p->objs);
	}
	fprintf(cachef, "special %s objpaths", p->name);
	output_strlst(cachef, p->objpaths);
    }
    fclose(cachef);
}


void gen_output_makefile(void)
{
    prog_t *p;
    FILE *outmk;

    sprintf(line, "generating %s", outmkname);
    status(line);

    if((outmk = fopen(outmkname, "w")) == NULL) {
	perror(outmkname);
	goterror = 1;
	return;
    }

    fprintf(outmk, "# %s - generated from %s by crunchgen %s\n\n",
	    outmkname, infilename, CRUNCH_VERSION);

    top_makefile_rules(outmk);

    for(p = progs; p != NULL; p = p->next)
	prog_makefile_rules(outmk, p);

    fprintf(outmk, "\n# ========\n");
    fclose(outmk);
}


void gen_output_cfile(void)
{
    extern char *crunched_skel[];
    char **cp;
    FILE *outcf;
    prog_t *p;
    strlst_t *s;

    sprintf(line, "generating %s", outcfname);
    status(line);

    if((outcf = fopen(outcfname, "w")) == NULL) {
	perror(outcfname);
	goterror = 1;
	return;
    }

    fprintf(outcf,
	  "/* %s - generated from %s by crunchgen %s */\n",
	    outcfname, infilename, CRUNCH_VERSION);

    fprintf(outcf, "#define EXECNAME \"%s\"\n", execfname);
    for(cp = crunched_skel; *cp != NULL; cp++)
	fprintf(outcf, "%s\n", *cp);

    for(p = progs; p != NULL; p = p->next)
	fprintf(outcf, "extern int _crunched_%s_stub();\n", p->ident);

    fprintf(outcf, "\nstruct stub entry_points[] = {\n");
    for(p = progs; p != NULL; p = p->next) {
	fprintf(outcf, "\t{ \"%s\", _crunched_%s_stub },\n",
		p->name, p->ident);
	for(s = p->links; s != NULL; s = s->next)
	    fprintf(outcf, "\t{ \"%s\", _crunched_%s_stub },\n",
		    s->str, p->ident);
    }

    fprintf(outcf, "\t{ EXECNAME, crunched_main },\n");
    fprintf(outcf, "\t{ NULL, NULL }\n};\n");
    fclose(outcf);
}


char *genident(char *str)
{
    char *n,*s,*d;

    /*
     * generates a Makefile/C identifier from a program name, mapping '-' to
     * '_' and ignoring all other non-identifier characters.  This leads to
     * programs named "foo.bar" and "foobar" to map to the same identifier.
     */

    if((n = strdup(str)) == NULL)
	return NULL;
    for(d = s = n; *s != '\0'; s++) {
	if(*s == '-') *d++ = '_';
	else if(*s == '_' || isalnum(*s)) *d++ = *s;
    }
    *d = '\0';
    return n;
}


char *dir_search(char *progname)
{
    char path[MAXPATHLEN];
    strlst_t *dir;

    for(dir=srcdirs; dir != NULL; dir=dir->next) {
	sprintf(path, "%s/%s", dir->str, progname);
	if(is_dir(path)) return dir->str;
    }
    return NULL;
}


void top_makefile_rules(FILE *outmk)
{
    prog_t *p;

    fprintf(outmk, "LIBS=");
    output_strlst(outmk, libs);

    fprintf(outmk, "CRUNCHED_OBJS=");
    for(p = progs; p != NULL; p = p->next)
	fprintf(outmk, " %s.lo", p->name);
    fprintf(outmk, "\n");

    fprintf(outmk, "SUBMAKE_TARGETS=");
    for(p = progs; p != NULL; p = p->next)
	fprintf(outmk, " %s_make", p->ident);
    fprintf(outmk, "\n\n");

    fprintf(outmk, "%s: %s.o $(CRUNCHED_OBJS)\n",
	    execfname, execfname);
    fprintf(outmk, "\t$(CC) -static -o %s %s.o $(CRUNCHED_OBJS) $(LIBS)\n",
	    execfname, execfname);
    fprintf(outmk, "\tstrip %s\n", execfname);
    fprintf(outmk, "all: objs exe\nobjs: $(SUBMAKE_TARGETS)\n");
    fprintf(outmk, "exe: %s\n", execfname);
    fprintf(outmk, "clean:\n\trm -f %s *.lo *.o *_stub.c\n",
	    execfname);
}


void prog_makefile_rules(FILE *outmk, prog_t *p)
{
    fprintf(outmk, "\n# -------- %s\n\n", p->name);

    if(p->srcdir && p->objs) {
	fprintf(outmk, "%s_SRCDIR=%s\n", p->ident, p->srcdir);
	fprintf(outmk, "%s_OBJS=", p->ident);
	output_strlst(outmk, p->objs);
	fprintf(outmk, "%s_make:\n", p->ident);
	fprintf(outmk, "\t(cd $(%s_SRCDIR) && make depend && make $(%s_OBJS))\n\n",
		p->ident, p->ident);
    }
    else
	fprintf(outmk, "%s_make:\n\t@echo \"** cannot make objs for %s\"\n\n",
		p->ident, p->name);

    fprintf(outmk,   "%s_OBJPATHS=", p->ident);
    output_strlst(outmk, p->objpaths);

    fprintf(outmk, "%s_stub.c:\n", p->name);
    fprintf(outmk, "\techo \""
	           "int _crunched_%s_stub(int argc, char **argv, char **envp)"
	           "{return main(argc,argv,envp);}\" >%s_stub.c\n",
	    p->ident, p->name);
    fprintf(outmk, "%s.lo: %s_stub.o $(%s_OBJPATHS)\n",
	    p->name, p->name, p->ident);
    fprintf(outmk, "\tld -dc -r -o %s.lo %s_stub.o $(%s_OBJPATHS)\n",
	    p->name, p->name, p->ident);
    fprintf(outmk, "\tcrunchide -k __crunched_%s_stub %s.lo\n",
	    p->ident, p->name);
}

void output_strlst(FILE *outf, strlst_t *lst)
{
    for(; lst != NULL; lst = lst->next)
	fprintf(outf, " %s", lst->str);
    fprintf(outf, "\n");
}


/*
 * ========================================================================
 * general library routines
 *
 */

void status(char *str)
{
    static int lastlen = 0;
    int len, spaces;

    if(!verbose) return;

    len = strlen(str);
    spaces = lastlen - len;
    if(spaces < 1) spaces = 1;

    fprintf(stderr, " [%s]%*.*s\r", str, spaces, spaces, " ");
    fflush(stderr);
    lastlen = len;
}


void out_of_memory(void)
{
    fprintf(stderr, "%s: %d: out of memory, stopping.\n", infilename, linenum);
    exit(1);
}


void add_string(strlst_t **listp, char *str)
{
    strlst_t *p1, *p2;

    /* add to end, but be smart about dups */

    for(p1 = NULL, p2 = *listp; p2 != NULL; p1 = p2, p2 = p2->next)
	if(!strcmp(p2->str, str)) return;

    p2 = malloc(sizeof(strlst_t));
    if(p2) p2->str = strdup(str);
    if(!p2 || !p2->str)
	out_of_memory();

    p2->next = NULL;
    if(p1 == NULL) *listp = p2;
    else p1->next = p2;
}


int is_dir(char *pathname)
{
    struct stat buf;

    if(stat(pathname, &buf) == -1)
	return 0;
    return S_ISDIR(buf.st_mode);
}

int is_nonempty_file(char *pathname)
{
    struct stat buf;

    if(stat(pathname, &buf) == -1)
	return 0;

    return S_ISREG(buf.st_mode) && buf.st_size > 0;
}
