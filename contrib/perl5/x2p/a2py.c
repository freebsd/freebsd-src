/* $RCSfile: a2py.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:14 $
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	a2py.c,v $
 */

#if defined(OS2) || defined(WIN32)
#if defined(WIN32)
#include <io.h>
#endif
#include "../patchlevel.h"
#endif
#include "util.h"

char *filename;
char *myname;

int checkers = 0;

int oper0(int type);
int oper1(int type, int arg1);
int oper2(int type, int arg1, int arg2);
int oper3(int type, int arg1, int arg2, int arg3);
int oper4(int type, int arg1, int arg2, int arg3, int arg4);
int oper5(int type, int arg1, int arg2, int arg3, int arg4, int arg5);
STR *walk(int useval, int level, register int node, int *numericptr, int minprec);

#if defined(OS2) || defined(WIN32)
static void usage(void);

static void
usage()
{
    printf("\nThis is the AWK to PERL translator, version 5.0, patchlevel %d\n", PATCHLEVEL);
    printf("\nUsage: %s [-D<number>] [-F<char>] [-n<fieldlist>] [-<number>] filename\n", myname);
    printf("\n  -D<number>      sets debugging flags."
           "\n  -F<character>   the awk script to translate is always invoked with"
           "\n                  this -F switch."
           "\n  -n<fieldlist>   specifies the names of the input fields if input does"
           "\n                  not have to be split into an array."
           "\n  -<number>       causes a2p to assume that input will always have that"
           "\n                  many fields.\n");
    exit(1);
}
#endif

int
main(register int argc, register char **argv, register char **env)
{
    register STR *str;
    int i;
    STR *tmpstr;

    myname = argv[0];
    linestr = str_new(80);
    str = str_new(0);		/* first used for -I flags */
    for (argc--,argv++; argc; argc--,argv++) {
	if (argv[0][0] != '-' || !argv[0][1])
	    break;
      reswitch:
	switch (argv[0][1]) {
#ifdef DEBUGGING
	case 'D':
	    debug = atoi(argv[0]+2);
#if YYDEBUG
	    yydebug = (debug & 1);
#endif
	    break;
#endif
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    maxfld = atoi(argv[0]+1);
	    absmaxfld = TRUE;
	    break;
	case 'F':
	    fswitch = argv[0][2];
	    break;
	case 'n':
	    namelist = savestr(argv[0]+2);
	    break;
	case 'o':
	    old_awk = TRUE;
	    break;
	case '-':
	    argc--,argv++;
	    goto switch_end;
	case 0:
	    break;
	default:
#if defined(OS2) || defined(WIN32)
	    fprintf(stderr, "Unrecognized switch: %s\n",argv[0]);
            usage();
#else
	    fatal("Unrecognized switch: %s\n",argv[0]);
#endif
	}
    }
  switch_end:

    /* open script */

    if (argv[0] == Nullch) {
#if defined(OS2) || defined(WIN32)
	if ( isatty(fileno(stdin)) )
	    usage();
#endif
        argv[0] = "-";
    }
    filename = savestr(argv[0]);

    filename = savestr(argv[0]);
    if (strEQ(filename,"-"))
	argv[0] = "";
    if (!*argv[0])
	rsfp = stdin;
    else
	rsfp = fopen(argv[0],"r");
    if (rsfp == Nullfp)
	fatal("Awk script \"%s\" doesn't seem to exist.\n",filename);

    /* init tokener */

    bufptr = str_get(linestr);
    symtab = hnew();
    curarghash = hnew();

    /* now parse the report spec */

    if (yyparse())
	fatal("Translation aborted due to syntax errors.\n");

#ifdef DEBUGGING
    if (debug & 2) {
	int type, len;

	for (i=1; i<mop;) {
	    type = ops[i].ival;
	    len = type >> 8;
	    type &= 255;
	    printf("%d\t%d\t%d\t%-10s",i++,type,len,opname[type]);
	    if (type == OSTRING)
		printf("\t\"%s\"\n",ops[i].cval),i++;
	    else {
		while (len--) {
		    printf("\t%d",ops[i].ival),i++;
		}
		putchar('\n');
	    }
	}
    }
    if (debug & 8)
	dump(root);
#endif

    /* first pass to look for numeric variables */

    prewalk(0,0,root,&i);

    /* second pass to produce new program */

    tmpstr = walk(0,0,root,&i,P_MIN);
    str = str_make(STARTPERL);
    str_cat(str, "\neval 'exec ");
    str_cat(str, BIN);
    str_cat(str, "/perl -S $0 ${1+\"$@\"}'\n\
    if $running_under_some_shell;\n\
			# this emulates #! processing on NIH machines.\n\
			# (remove #! line above if indigestible)\n\n");
    str_cat(str,
      "eval '$'.$1.'$2;' while $ARGV[0] =~ /^([A-Za-z_0-9]+=)(.*)/ && shift;\n");
    str_cat(str,
      "			# process any FOO=bar switches\n\n");
    if (do_opens && opens) {
	str_scat(str,opens);
	str_free(opens);
	str_cat(str,"\n");
    }
    str_scat(str,tmpstr);
    str_free(tmpstr);
#ifdef DEBUGGING
    if (!(debug & 16))
#endif
    fixup(str);
    putlines(str);
    if (checkers) {
	fprintf(stderr,
	  "Please check my work on the %d line%s I've marked with \"#???\".\n",
		checkers, checkers == 1 ? "" : "s" );
	fprintf(stderr,
	  "The operation I've selected may be wrong for the operand types.\n");
    }
    exit(0);
}

#define RETURN(retval) return (bufptr = s,retval)
#define XTERM(retval) return (expectterm = TRUE,bufptr = s,retval)
#define XOP(retval) return (expectterm = FALSE,bufptr = s,retval)
#define ID(x) return (yylval=string(x,0),expectterm = FALSE,bufptr = s,idtype)

int idtype;

int
yylex(void)
{
    register char *s = bufptr;
    register char *d;
    register int tmp;

  retry:
#if YYDEBUG
    if (yydebug)
	if (strchr(s,'\n'))
	    fprintf(stderr,"Tokener at %s",s);
	else
	    fprintf(stderr,"Tokener at %s\n",s);
#endif
    switch (*s) {
    default:
	fprintf(stderr,
	    "Unrecognized character %c in file %s line %d--ignoring.\n",
	     *s++,filename,line);
	goto retry;
    case '\\':
	s++;
	if (*s && *s != '\n') {
	    yyerror("Ignoring spurious backslash");
	    goto retry;
	}
	/*FALLSTHROUGH*/
    case 0:
	s = str_get(linestr);
	*s = '\0';
	if (!rsfp)
	    RETURN(0);
	line++;
	if ((s = str_gets(linestr, rsfp)) == Nullch) {
	    if (rsfp != stdin)
		fclose(rsfp);
	    rsfp = Nullfp;
	    s = str_get(linestr);
	    RETURN(0);
	}
	goto retry;
    case ' ': case '\t':
	s++;
	goto retry;
    case '\n':
	*s = '\0';
	XTERM(NEWLINE);
    case '#':
	yylval = string(s,0);
	*s = '\0';
	XTERM(COMMENT);
    case ';':
	tmp = *s++;
	if (*s == '\n') {
	    s++;
	    XTERM(SEMINEW);
	}
	XTERM(tmp);
    case '(':
	tmp = *s++;
	XTERM(tmp);
    case '{':
    case '[':
    case ')':
    case ']':
    case '?':
    case ':':
	tmp = *s++;
	XOP(tmp);
#ifdef EBCDIC
    case 7:
#else
    case 127:
#endif
	s++;
	XTERM('}');
    case '}':
	for (d = s + 1; isspace(*d); d++) ;
	if (!*d)
	    s = d - 1;
	*s = 127;
	XTERM(';');
    case ',':
	tmp = *s++;
	XTERM(tmp);
    case '~':
	s++;
	yylval = string("~",1);
	XTERM(MATCHOP);
    case '+':
    case '-':
	if (s[1] == *s) {
	    s++;
	    if (*s++ == '+')
		XTERM(INCR);
	    else
		XTERM(DECR);
	}
	/* FALL THROUGH */
    case '*':
    case '%':
    case '^':
	tmp = *s++;
	if (*s == '=') {
	    if (tmp == '^')
		yylval = string("**=",3);
	    else
		yylval = string(s-1,2);
	    s++;
	    XTERM(ASGNOP);
	}
	XTERM(tmp);
    case '&':
	s++;
	tmp = *s++;
	if (tmp == '&')
	    XTERM(ANDAND);
	s--;
	XTERM('&');
    case '|':
	s++;
	tmp = *s++;
	if (tmp == '|')
	    XTERM(OROR);
	s--;
	while (*s == ' ' || *s == '\t')
	    s++;
	if (strnEQ(s,"getline",7))
	    XTERM('p');
	else
	    XTERM('|');
    case '=':
	s++;
	tmp = *s++;
	if (tmp == '=') {
	    yylval = string("==",2);
	    XTERM(RELOP);
	}
	s--;
	yylval = string("=",1);
	XTERM(ASGNOP);
    case '!':
	s++;
	tmp = *s++;
	if (tmp == '=') {
	    yylval = string("!=",2);
	    XTERM(RELOP);
	}
	if (tmp == '~') {
	    yylval = string("!~",2);
	    XTERM(MATCHOP);
	}
	s--;
	XTERM(NOT);
    case '<':
	s++;
	tmp = *s++;
	if (tmp == '=') {
	    yylval = string("<=",2);
	    XTERM(RELOP);
	}
	s--;
	XTERM('<');
    case '>':
	s++;
	tmp = *s++;
	if (tmp == '>') {
	    yylval = string(">>",2);
	    XTERM(GRGR);
	}
	if (tmp == '=') {
	    yylval = string(">=",2);
	    XTERM(RELOP);
	}
	s--;
	XTERM('>');

#define SNARFWORD \
	d = tokenbuf; \
	while (isalpha(*s) || isdigit(*s) || *s == '_') \
	    *d++ = *s++; \
	*d = '\0'; \
	d = tokenbuf; \
	if (*s == '(') \
	    idtype = USERFUN; \
	else \
	    idtype = VAR;

    case '$':
	s++;
	if (*s == '0') {
	    s++;
	    do_chop = TRUE;
	    need_entire = TRUE;
	    idtype = VAR;
	    ID("0");
	}
	do_split = TRUE;
	if (isdigit(*s)) {
	    for (d = s; isdigit(*s); s++) ;
	    yylval = string(d,s-d);
	    tmp = atoi(d);
	    if (tmp > maxfld)
		maxfld = tmp;
	    XOP(FIELD);
	}
	split_to_array = set_array_base = TRUE;
	XOP(VFIELD);

    case '/':			/* may either be division or pattern */
	if (expectterm) {
	    s = scanpat(s);
	    XTERM(REGEX);
	}
	tmp = *s++;
	if (*s == '=') {
	    yylval = string("/=",2);
	    s++;
	    XTERM(ASGNOP);
	}
	XTERM(tmp);

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '.':
	s = scannum(s);
	XOP(NUMBER);
    case '"':
	s++;
	s = cpy2(tokenbuf,s,s[-1]);
	if (!*s)
	    fatal("String not terminated:\n%s",str_get(linestr));
	s++;
	yylval = string(tokenbuf,0);
	XOP(STRING);

    case 'a': case 'A':
	SNARFWORD;
	if (strEQ(d,"ARGC"))
	    set_array_base = TRUE;
	if (strEQ(d,"ARGV")) {
	    yylval=numary(string("ARGV",0));
	    XOP(VAR);
	}
	if (strEQ(d,"atan2")) {
	    yylval = OATAN2;
	    XTERM(FUNN);
	}
	ID(d);
    case 'b': case 'B':
	SNARFWORD;
	if (strEQ(d,"break"))
	    XTERM(BREAK);
	if (strEQ(d,"BEGIN"))
	    XTERM(BEGIN);
	ID(d);
    case 'c': case 'C':
	SNARFWORD;
	if (strEQ(d,"continue"))
	    XTERM(CONTINUE);
	if (strEQ(d,"cos")) {
	    yylval = OCOS;
	    XTERM(FUN1);
	}
	if (strEQ(d,"close")) {
	    do_fancy_opens = 1;
	    yylval = OCLOSE;
	    XTERM(FUN1);
	}
	if (strEQ(d,"chdir"))
	    *d = toupper(*d);
	else if (strEQ(d,"crypt"))
	    *d = toupper(*d);
	else if (strEQ(d,"chop"))
	    *d = toupper(*d);
	else if (strEQ(d,"chmod"))
	    *d = toupper(*d);
	else if (strEQ(d,"chown"))
	    *d = toupper(*d);
	ID(d);
    case 'd': case 'D':
	SNARFWORD;
	if (strEQ(d,"do"))
	    XTERM(DO);
	if (strEQ(d,"delete"))
	    XTERM(DELETE);
	if (strEQ(d,"die"))
	    *d = toupper(*d);
	ID(d);
    case 'e': case 'E':
	SNARFWORD;
	if (strEQ(d,"END"))
	    XTERM(END);
	if (strEQ(d,"else"))
	    XTERM(ELSE);
	if (strEQ(d,"exit")) {
	    saw_line_op = TRUE;
	    XTERM(EXIT);
	}
	if (strEQ(d,"exp")) {
	    yylval = OEXP;
	    XTERM(FUN1);
	}
	if (strEQ(d,"elsif"))
	    *d = toupper(*d);
	else if (strEQ(d,"eq"))
	    *d = toupper(*d);
	else if (strEQ(d,"eval"))
	    *d = toupper(*d);
	else if (strEQ(d,"eof"))
	    *d = toupper(*d);
	else if (strEQ(d,"each"))
	    *d = toupper(*d);
	else if (strEQ(d,"exec"))
	    *d = toupper(*d);
	ID(d);
    case 'f': case 'F':
	SNARFWORD;
	if (strEQ(d,"FS")) {
	    saw_FS++;
	    if (saw_FS == 1 && in_begin) {
		for (d = s; *d && isspace(*d); d++) ;
		if (*d == '=') {
		    for (d++; *d && isspace(*d); d++) ;
		    if (*d == '"' && d[2] == '"')
			const_FS = d[1];
		}
	    }
	    ID(tokenbuf);
	}
	if (strEQ(d,"for"))
	    XTERM(FOR);
	else if (strEQ(d,"function"))
	    XTERM(FUNCTION);
	if (strEQ(d,"FILENAME"))
	    d = "ARGV";
	if (strEQ(d,"foreach"))
	    *d = toupper(*d);
	else if (strEQ(d,"format"))
	    *d = toupper(*d);
	else if (strEQ(d,"fork"))
	    *d = toupper(*d);
	else if (strEQ(d,"fh"))
	    *d = toupper(*d);
	ID(d);
    case 'g': case 'G':
	SNARFWORD;
	if (strEQ(d,"getline"))
	    XTERM(GETLINE);
	if (strEQ(d,"gsub"))
	    XTERM(GSUB);
	if (strEQ(d,"ge"))
	    *d = toupper(*d);
	else if (strEQ(d,"gt"))
	    *d = toupper(*d);
	else if (strEQ(d,"goto"))
	    *d = toupper(*d);
	else if (strEQ(d,"gmtime"))
	    *d = toupper(*d);
	ID(d);
    case 'h': case 'H':
	SNARFWORD;
	if (strEQ(d,"hex"))
	    *d = toupper(*d);
	ID(d);
    case 'i': case 'I':
	SNARFWORD;
	if (strEQ(d,"if"))
	    XTERM(IF);
	if (strEQ(d,"in"))
	    XTERM(IN);
	if (strEQ(d,"index")) {
	    set_array_base = TRUE;
	    XTERM(INDEX);
	}
	if (strEQ(d,"int")) {
	    yylval = OINT;
	    XTERM(FUN1);
	}
	ID(d);
    case 'j': case 'J':
	SNARFWORD;
	if (strEQ(d,"join"))
	    *d = toupper(*d);
	ID(d);
    case 'k': case 'K':
	SNARFWORD;
	if (strEQ(d,"keys"))
	    *d = toupper(*d);
	else if (strEQ(d,"kill"))
	    *d = toupper(*d);
	ID(d);
    case 'l': case 'L':
	SNARFWORD;
	if (strEQ(d,"length")) {
	    yylval = OLENGTH;
	    XTERM(FUN1);
	}
	if (strEQ(d,"log")) {
	    yylval = OLOG;
	    XTERM(FUN1);
	}
	if (strEQ(d,"last"))
	    *d = toupper(*d);
	else if (strEQ(d,"local"))
	    *d = toupper(*d);
	else if (strEQ(d,"lt"))
	    *d = toupper(*d);
	else if (strEQ(d,"le"))
	    *d = toupper(*d);
	else if (strEQ(d,"locatime"))
	    *d = toupper(*d);
	else if (strEQ(d,"link"))
	    *d = toupper(*d);
	ID(d);
    case 'm': case 'M':
	SNARFWORD;
	if (strEQ(d,"match")) {
	    set_array_base = TRUE;
	    XTERM(MATCH);
	}
	if (strEQ(d,"m"))
	    *d = toupper(*d);
	ID(d);
    case 'n': case 'N':
	SNARFWORD;
	if (strEQ(d,"NF"))
	    do_chop = do_split = split_to_array = set_array_base = TRUE;
	if (strEQ(d,"next")) {
	    saw_line_op = TRUE;
	    XTERM(NEXT);
	}
	if (strEQ(d,"ne"))
	    *d = toupper(*d);
	ID(d);
    case 'o': case 'O':
	SNARFWORD;
	if (strEQ(d,"ORS")) {
	    saw_ORS = TRUE;
	    d = "\\";
	}
	if (strEQ(d,"OFS")) {
	    saw_OFS = TRUE;
	    d = ",";
	}
	if (strEQ(d,"OFMT")) {
	    d = "#";
	}
	if (strEQ(d,"open"))
	    *d = toupper(*d);
	else if (strEQ(d,"ord"))
	    *d = toupper(*d);
	else if (strEQ(d,"oct"))
	    *d = toupper(*d);
	ID(d);
    case 'p': case 'P':
	SNARFWORD;
	if (strEQ(d,"print")) {
	    XTERM(PRINT);
	}
	if (strEQ(d,"printf")) {
	    XTERM(PRINTF);
	}
	if (strEQ(d,"push"))
	    *d = toupper(*d);
	else if (strEQ(d,"pop"))
	    *d = toupper(*d);
	ID(d);
    case 'q': case 'Q':
	SNARFWORD;
	ID(d);
    case 'r': case 'R':
	SNARFWORD;
	if (strEQ(d,"RS")) {
	    d = "/";
	    saw_RS = TRUE;
	}
	if (strEQ(d,"rand")) {
	    yylval = ORAND;
	    XTERM(FUN1);
	}
	if (strEQ(d,"return"))
	    XTERM(RET);
	if (strEQ(d,"reset"))
	    *d = toupper(*d);
	else if (strEQ(d,"redo"))
	    *d = toupper(*d);
	else if (strEQ(d,"rename"))
	    *d = toupper(*d);
	ID(d);
    case 's': case 'S':
	SNARFWORD;
	if (strEQ(d,"split")) {
	    set_array_base = TRUE;
	    XOP(SPLIT);
	}
	if (strEQ(d,"substr")) {
	    set_array_base = TRUE;
	    XTERM(SUBSTR);
	}
	if (strEQ(d,"sub"))
	    XTERM(SUB);
	if (strEQ(d,"sprintf"))
	    XTERM(SPRINTF);
	if (strEQ(d,"sqrt")) {
	    yylval = OSQRT;
	    XTERM(FUN1);
	}
	if (strEQ(d,"SUBSEP")) {
	    d = ";";
	}
	if (strEQ(d,"sin")) {
	    yylval = OSIN;
	    XTERM(FUN1);
	}
	if (strEQ(d,"srand")) {
	    yylval = OSRAND;
	    XTERM(FUN1);
	}
	if (strEQ(d,"system")) {
	    yylval = OSYSTEM;
	    XTERM(FUN1);
	}
	if (strEQ(d,"s"))
	    *d = toupper(*d);
	else if (strEQ(d,"shift"))
	    *d = toupper(*d);
	else if (strEQ(d,"select"))
	    *d = toupper(*d);
	else if (strEQ(d,"seek"))
	    *d = toupper(*d);
	else if (strEQ(d,"stat"))
	    *d = toupper(*d);
	else if (strEQ(d,"study"))
	    *d = toupper(*d);
	else if (strEQ(d,"sleep"))
	    *d = toupper(*d);
	else if (strEQ(d,"symlink"))
	    *d = toupper(*d);
	else if (strEQ(d,"sort"))
	    *d = toupper(*d);
	ID(d);
    case 't': case 'T':
	SNARFWORD;
	if (strEQ(d,"tr"))
	    *d = toupper(*d);
	else if (strEQ(d,"tell"))
	    *d = toupper(*d);
	else if (strEQ(d,"time"))
	    *d = toupper(*d);
	else if (strEQ(d,"times"))
	    *d = toupper(*d);
	ID(d);
    case 'u': case 'U':
	SNARFWORD;
	if (strEQ(d,"until"))
	    *d = toupper(*d);
	else if (strEQ(d,"unless"))
	    *d = toupper(*d);
	else if (strEQ(d,"umask"))
	    *d = toupper(*d);
	else if (strEQ(d,"unshift"))
	    *d = toupper(*d);
	else if (strEQ(d,"unlink"))
	    *d = toupper(*d);
	else if (strEQ(d,"utime"))
	    *d = toupper(*d);
	ID(d);
    case 'v': case 'V':
	SNARFWORD;
	if (strEQ(d,"values"))
	    *d = toupper(*d);
	ID(d);
    case 'w': case 'W':
	SNARFWORD;
	if (strEQ(d,"while"))
	    XTERM(WHILE);
	if (strEQ(d,"write"))
	    *d = toupper(*d);
	else if (strEQ(d,"wait"))
	    *d = toupper(*d);
	ID(d);
    case 'x': case 'X':
	SNARFWORD;
	if (strEQ(d,"x"))
	    *d = toupper(*d);
	ID(d);
    case 'y': case 'Y':
	SNARFWORD;
	if (strEQ(d,"y"))
	    *d = toupper(*d);
	ID(d);
    case 'z': case 'Z':
	SNARFWORD;
	ID(d);
    }
}

char *
scanpat(register char *s)
{
    register char *d;

    switch (*s++) {
    case '/':
	break;
    default:
	fatal("Search pattern not found:\n%s",str_get(linestr));
    }

    d = tokenbuf;
    for (; *s; s++,d++) {
	if (*s == '\\') {
	    if (s[1] == '/')
		*d++ = *s++;
	    else if (s[1] == '\\')
		*d++ = *s++;
	    else if (s[1] == '[')
		*d++ = *s++;
	}
	else if (*s == '[') {
	    *d++ = *s++;
	    do {
		if (*s == '\\' && s[1])
		    *d++ = *s++;
		if (*s == '/' || (*s == '-' && s[1] == ']'))
		    *d++ = '\\';
		*d++ = *s++;
	    } while (*s && *s != ']');
	}
	else if (*s == '/')
	    break;
	*d = *s;
    }
    *d = '\0';

    if (!*s)
	fatal("Search pattern not terminated:\n%s",str_get(linestr));
    s++;
    yylval = string(tokenbuf,0);
    return s;
}

void
yyerror(char *s)
{
    fprintf(stderr,"%s in file %s at line %d\n",
      s,filename,line);
}

char *
scannum(register char *s)
{
    register char *d;

    switch (*s) {
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '0' : case '.':
	d = tokenbuf;
	while (isdigit(*s)) {
	    *d++ = *s++;
	}
	if (*s == '.') {
	    if (isdigit(s[1])) {
		*d++ = *s++;
		while (isdigit(*s)) {
		    *d++ = *s++;
		}
	    }
	    else
		s++;
	}
	if (strchr("eE",*s) && strchr("+-0123456789",s[1])) {
	    *d++ = *s++;
	    if (*s == '+' || *s == '-')
		*d++ = *s++;
	    while (isdigit(*s))
		*d++ = *s++;
	}
	*d = '\0';
	yylval = string(tokenbuf,0);
	break;
    }
    return s;
}

int
string(char *ptr, int len)
{
    int retval = mop;

    ops[mop++].ival = OSTRING + (1<<8);
    if (!len)
	len = strlen(ptr);
    ops[mop].cval = (char *) safemalloc(len+1);
    strncpy(ops[mop].cval,ptr,len);
    ops[mop++].cval[len] = '\0';
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int
oper0(int type)
{
    int retval = mop;

    if (type > 255)
	fatal("type > 255 (%d)\n",type);
    ops[mop++].ival = type;
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int
oper1(int type, int arg1)
{
    int retval = mop;

    if (type > 255)
	fatal("type > 255 (%d)\n",type);
    ops[mop++].ival = type + (1<<8);
    ops[mop++].ival = arg1;
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int
oper2(int type, int arg1, int arg2)
{
    int retval = mop;

    if (type > 255)
	fatal("type > 255 (%d)\n",type);
    ops[mop++].ival = type + (2<<8);
    ops[mop++].ival = arg1;
    ops[mop++].ival = arg2;
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int
oper3(int type, int arg1, int arg2, int arg3)
{
    int retval = mop;

    if (type > 255)
	fatal("type > 255 (%d)\n",type);
    ops[mop++].ival = type + (3<<8);
    ops[mop++].ival = arg1;
    ops[mop++].ival = arg2;
    ops[mop++].ival = arg3;
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int
oper4(int type, int arg1, int arg2, int arg3, int arg4)
{
    int retval = mop;

    if (type > 255)
	fatal("type > 255 (%d)\n",type);
    ops[mop++].ival = type + (4<<8);
    ops[mop++].ival = arg1;
    ops[mop++].ival = arg2;
    ops[mop++].ival = arg3;
    ops[mop++].ival = arg4;
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int
oper5(int type, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    int retval = mop;

    if (type > 255)
	fatal("type > 255 (%d)\n",type);
    ops[mop++].ival = type + (5<<8);
    ops[mop++].ival = arg1;
    ops[mop++].ival = arg2;
    ops[mop++].ival = arg3;
    ops[mop++].ival = arg4;
    ops[mop++].ival = arg5;
    if (mop >= OPSMAX)
	fatal("Recompile a2p with larger OPSMAX\n");
    return retval;
}

int depth = 0;

void
dump(int branch)
{
    register int type;
    register int len;
    register int i;

    type = ops[branch].ival;
    len = type >> 8;
    type &= 255;
    for (i=depth; i; i--)
	printf(" ");
    if (type == OSTRING) {
	printf("%-5d\"%s\"\n",branch,ops[branch+1].cval);
    }
    else {
	printf("(%-5d%s %d\n",branch,opname[type],len);
	depth++;
	for (i=1; i<=len; i++)
	    dump(ops[branch+i].ival);
	depth--;
	for (i=depth; i; i--)
	    printf(" ");
	printf(")\n");
    }
}

int
bl(int arg, int maybe)
{
    if (!arg)
	return 0;
    else if ((ops[arg].ival & 255) != OBLOCK)
	return oper2(OBLOCK,arg,maybe);
    else if ((ops[arg].ival >> 8) < 2)
	return oper2(OBLOCK,ops[arg+1].ival,maybe);
    else
	return arg;
}

void
fixup(STR *str)
{
    register char *s;
    register char *t;

    for (s = str->str_ptr; *s; s++) {
	if (*s == ';' && s[1] == ' ' && s[2] == '\n') {
	    strcpy(s+1,s+2);
	    s++;
	}
	else if (*s == '\n') {
	    for (t = s+1; isspace(*t & 127); t++) ;
	    t--;
	    while (isspace(*t & 127) && *t != '\n') t--;
	    if (*t == '\n' && t-s > 1) {
		if (s[-1] == '{')
		    s--;
		strcpy(s+1,t);
	    }
	    s++;
	}
    }
}

void
putlines(STR *str)
{
    register char *d, *s, *t, *e;
    register int pos, newpos;

    d = tokenbuf;
    pos = 0;
    for (s = str->str_ptr; *s; s++) {
	*d++ = *s;
	pos++;
	if (*s == '\n') {
	    *d = '\0';
	    d = tokenbuf;
	    pos = 0;
	    putone();
	}
	else if (*s == '\t')
	    pos += 7;
	if (pos > 78) {		/* split a long line? */
	    *d-- = '\0';
	    newpos = 0;
	    for (t = tokenbuf; isspace(*t & 127); t++) {
		if (*t == '\t')
		    newpos += 8;
		else
		    newpos += 1;
	    }
	    e = d;
	    while (d > tokenbuf && (*d != ' ' || d[-1] != ';'))
		d--;
	    if (d < t+10) {
		d = e;
		while (d > tokenbuf &&
		  (*d != ' ' || d[-1] != '|' || d[-2] != '|') )
		    d--;
	    }
	    if (d < t+10) {
		d = e;
		while (d > tokenbuf &&
		  (*d != ' ' || d[-1] != '&' || d[-2] != '&') )
		    d--;
	    }
	    if (d < t+10) {
		d = e;
		while (d > tokenbuf && (*d != ' ' || d[-1] != ','))
		    d--;
	    }
	    if (d < t+10) {
		d = e;
		while (d > tokenbuf && *d != ' ')
		    d--;
	    }
	    if (d > t+3) {
                char save[2048];
                strcpy(save, d);
		*d = '\n';
                d[1] = '\0';
		putone();
		putchar('\n');
		if (d[-1] != ';' && !(newpos % 4)) {
		    *t++ = ' ';
		    *t++ = ' ';
		    newpos += 2;
		}
		strcpy(t,save+1);
		newpos += strlen(t);
		d = t + strlen(t);
		pos = newpos;
	    }
	    else
		d = e + 1;
	}
    }
}

void
putone(void)
{
    register char *t;

    for (t = tokenbuf; *t; t++) {
	*t &= 127;
	if (*t == 127) {
	    *t = ' ';
	    strcpy(t+strlen(t)-1, "\t#???\n");
	    checkers++;
	}
    }
    t = tokenbuf;
    if (*t == '#') {
	if (strnEQ(t,"#!/bin/awk",10) || strnEQ(t,"#! /bin/awk",11))
	    return;
	if (strnEQ(t,"#!/usr/bin/awk",14) || strnEQ(t,"#! /usr/bin/awk",15))
	    return;
    }
    fputs(tokenbuf,stdout);
}

int
numary(int arg)
{
    STR *key;
    int dummy;

    key = walk(0,0,arg,&dummy,P_MIN);
    str_cat(key,"[]");
    hstore(symtab,key->str_ptr,str_make("1"));
    str_free(key);
    set_array_base = TRUE;
    return arg;
}

int
rememberargs(int arg)
{
    int type;
    STR *str;

    if (!arg)
	return arg;
    type = ops[arg].ival & 255;
    if (type == OCOMMA) {
	rememberargs(ops[arg+1].ival);
	rememberargs(ops[arg+3].ival);
    }
    else if (type == OVAR) {
	str = str_new(0);
	hstore(curarghash,ops[ops[arg+1].ival+1].cval,str);
    }
    else
	fatal("panic: unknown argument type %d, line %d\n",type,line);
    return arg;
}

int
aryrefarg(int arg)
{
    int type = ops[arg].ival & 255;
    STR *str;

    if (type != OSTRING)
	fatal("panic: aryrefarg %d, line %d\n",type,line);
    str = hfetch(curarghash,ops[arg+1].cval);
    if (str)
	str_set(str,"*");
    return arg;
}

int
fixfargs(int name, int arg, int prevargs)
{
    int type;
    STR *str;
    int numargs;

    if (!arg)
	return prevargs;
    type = ops[arg].ival & 255;
    if (type == OCOMMA) {
	numargs = fixfargs(name,ops[arg+1].ival,prevargs);
	numargs = fixfargs(name,ops[arg+3].ival,numargs);
    }
    else if (type == OVAR) {
	str = hfetch(curarghash,ops[ops[arg+1].ival+1].cval);
	if (strEQ(str_get(str),"*")) {
	    char tmpbuf[128];

	    str_set(str,"");		/* in case another routine has this */
	    ops[arg].ival &= ~255;
	    ops[arg].ival |= OSTAR;
	    sprintf(tmpbuf,"%s:%d",ops[name+1].cval,prevargs);
	    fprintf(stderr,"Adding %s\n",tmpbuf);
	    str = str_new(0);
	    str_set(str,"*");
	    hstore(curarghash,tmpbuf,str);
	}
	numargs = prevargs + 1;
    }
    else
	fatal("panic: unknown argument type %d, arg %d, line %d\n",
	  type,prevargs+1,line);
    return numargs;
}

int
fixrargs(char *name, int arg, int prevargs)
{
    int type;
    STR *str;
    int numargs;

    if (!arg)
	return prevargs;
    type = ops[arg].ival & 255;
    if (type == OCOMMA) {
	numargs = fixrargs(name,ops[arg+1].ival,prevargs);
	numargs = fixrargs(name,ops[arg+3].ival,numargs);
    }
    else {
	char *tmpbuf = (char *) safemalloc(strlen(name) + (sizeof(prevargs) * 3) + 5);
	sprintf(tmpbuf,"%s:%d",name,prevargs);
	str = hfetch(curarghash,tmpbuf);
	safefree(tmpbuf);
	if (str && strEQ(str->str_ptr,"*")) {
	    if (type == OVAR || type == OSTAR) {
		ops[arg].ival &= ~255;
		ops[arg].ival |= OSTAR;
	    }
	    else
		fatal("Can't pass expression by reference as arg %d of %s\n",
		    prevargs+1, name);
	}
	numargs = prevargs + 1;
    }
    return numargs;
}
