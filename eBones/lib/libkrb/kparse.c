/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Purpose:
 * This module was developed to parse the "~/.klogin" files for
 * Kerberos-authenticated rlogin/rcp/rsh services.  However, it is
 * general purpose and can be used to parse any such parameter file.
 *
 * The parameter file should consist of one or more entries, with each
 * entry on a separate line and consisting of zero or more
 * "keyword=value" combinations.  The keyword is case insensitive, but
 * the value is not.  Any string may be enclosed in quotes, and
 * c-style "\" literals are supported.  A comma may be used to
 * separate the k/v combinations, and multiple commas are ignored.
 * Whitespace (blank or tab) may be used freely and is ignored.
 *
 * Full error processing is available.  When PS_BAD_KEYWORD or
 * PS_SYNTAX is returned from fGetParameterSet(), the string ErrorMsg
 * contains a meaningful error message.
 *
 * Keywords and their default values are programmed by an external
 * table.
 *
 * Routines:
 * fGetParameterSet()      parse one line of the parameter file
 * fGetKeywordValue()      parse one "keyword=value" combo
 * fGetToken()             parse one token
 *
 *
 *	from: kparse.c,v 4.5 89/01/21 17:20:39 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <kparse.h>

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define MAXKEY          80
#define MAXVALUE        80

int fUngetChar(int ch, FILE *fp);
int fGetChar(FILE *fp);
int fGetLiteral(FILE *fp);

int LineNbr=1;		/* current line nbr in parameter file */
char ErrorMsg[80];	/* meaningful only when KV_SYNTAX, PS_SYNTAX,
                         * or PS_BAD_KEYWORD is returned by
                         * fGetKeywordValue or fGetParameterSet */

int
fGetParameterSet( fp,parm,parmcount )
    FILE *fp;
    parmtable parm[];
    int parmcount;
{
    int rc,i;
    char keyword[MAXKEY];
    char value[MAXVALUE];

    while (TRUE) {
        rc=fGetKeywordValue(fp,keyword,MAXKEY,value,MAXVALUE);

        switch (rc) {

        case KV_EOF:
            return(PS_EOF);

        case KV_EOL:
            return(PS_OKAY);

        case KV_SYNTAX:
            return(PS_SYNTAX);

        case KV_OKAY:
            /*
             * got a reasonable keyword/value pair.  Search the
             * parameter table to see if we recognize the keyword; if
             * not, return an error.  If we DO recognize it, make sure
             * it has not already been given.  If not already given,
             * save the value.
             */
            for (i=0; i<parmcount; i++) {
                if (strcmp(strutol(keyword),parm[i].keyword)==0) {
                    if (parm[i].value) {
                        sprintf(ErrorMsg,"duplicate keyword \"%s\" found",
                                keyword);
                        return(PS_BAD_KEYWORD);
                    }
                    parm[i].value = strsave( value );
                    break;
                }
            }
            if (i >= parmcount) {
                sprintf(ErrorMsg, "unrecognized keyword \"%s\" found",
			keyword);
                return(PS_BAD_KEYWORD);
            }
            break;

        default:
            sprintf(ErrorMsg,
		    "panic: bad return (%d) from fGetToken()",rc);
            break;
        }
    }
}

/*
 * Routine: ParmCompare
 *
 * Purpose:
 * ParmCompare checks a specified value for a particular keyword.
 * fails if keyword not found or keyword found but the value was
 * different. Like strcmp, ParmCompare returns 0 for a match found, -1
 * otherwise
 */
int
ParmCompare( parm, parmcount, keyword, value )
    parmtable parm[];
    int parmcount;
    char *keyword;
    char *value;
{
    int i;

    for (i=0; i<parmcount; i++) {
        if (strcmp(parm[i].keyword,keyword)==0) {
            if (parm[i].value) {
                return(strcmp(parm[i].value,value));
            } else {
                return(strcmp(parm[i].defvalue,value));
            }
        }
    }
    return(-1);
}

void
FreeParameterSet(parm,parmcount)
    parmtable parm[];
    int parmcount;
{
    int i;

    for (i=0; i<parmcount; i++) {
        if (parm[i].value) {
            free(parm[i].value);
            parm[i].value = (char *)NULL;
        }
    }
}

int
fGetKeywordValue( fp, keyword, klen, value, vlen )
    FILE *fp;
    char *keyword;
    int klen;
    char *value;
    int vlen;
{
    int rc;
    int gotit;

    *keyword = *value = '\0';   /* preset strings to NULL */

    /*
     * Looking for a keyword.
     *          return an exception for EOF or BAD_QSTRING
     *          ignore leading WHITEspace
     *          ignore any number of leading commas
     *          newline means we have all the parms for this
     *          	statement; give an indication that there is
     *          	nothing more on this line.
     *          stop looking if we find QSTRING, STRING, or NUMBER
     *          return syntax error for any other PUNKtuation
     */
    gotit = FALSE;
    do {
        rc = fGetToken(fp,keyword,klen);

        switch (rc) {

        case GTOK_WHITE:
            break;

        case GTOK_EOF:
            return(KV_EOF);

        case GTOK_BAD_QSTRING:
            sprintf(ErrorMsg,"unterminated string \"%s found",keyword);
            return(KV_SYNTAX);

        case GTOK_PUNK:
            if (strcmp("\n",keyword)==0) {
                return(KV_EOL);
            } else if (strcmp(",",keyword)!=0) {
                sprintf(ErrorMsg,"expecting rvalue, found \'%s\'",keyword);
            }
            break;

        case GTOK_STRING:
        case GTOK_QSTRING:
        case GTOK_NUMBER:
            gotit = TRUE;
            break;

        default:
            sprintf(ErrorMsg,"panic: bad return (%d) from fGetToken()",rc);
            return(KV_SYNTAX);
        }

    } while (!gotit);

    /*
     * now we expect an equal sign.
     *          skip any whitespace
     *          stop looking if we find an equal sign
     *          anything else causes a syntax error
     */
    gotit = FALSE;
    do {
        rc = fGetToken(fp,value,vlen);

        switch (rc) {

        case GTOK_WHITE:
            break;

        case GTOK_BAD_QSTRING:
            sprintf(ErrorMsg,
		    "expecting \'=\', found unterminated string \"%s",
                    value);
            return(KV_SYNTAX);

        case GTOK_PUNK:
            if (strcmp("=",value)==0) {
                gotit = TRUE;
            } else {
                if (strcmp("\n",value)==0) {
                    sprintf(ErrorMsg,"expecting \"=\", found newline");
                    fUngetChar('\n',fp);
                } else {
                    sprintf(ErrorMsg,
			    "expecting rvalue, found \'%s\'",keyword);
                }
                return(KV_SYNTAX);
            }
            break;

        case GTOK_STRING:
        case GTOK_QSTRING:
        case GTOK_NUMBER:
            sprintf(ErrorMsg,"expecting \'=\', found \"%s\"",value);
            return(KV_SYNTAX);

        case GTOK_EOF:
            sprintf(ErrorMsg,"expecting \'=\', found EOF");
            return(KV_SYNTAX);

        default:
            sprintf(ErrorMsg,
		    "panic: bad return (%d) from fGetToken()",rc);
            return(KV_SYNTAX);
        }

    } while ( !gotit );

    /*
     * got the keyword and equal sign, now get a value.
     *          ignore any whitespace
     *          any punctuation is a syntax error
     */
    gotit = FALSE;
    do {
        rc = fGetToken(fp,value,vlen);

        switch (rc) {

        case GTOK_WHITE:
            break;

        case GTOK_EOF:
            sprintf(ErrorMsg,"expecting rvalue, found EOF");
            return(KV_SYNTAX);

        case GTOK_BAD_QSTRING:
            sprintf(ErrorMsg,"unterminated quoted string \"%s",value);
            return(KV_SYNTAX);

        case GTOK_PUNK:
            if (strcmp("\n",value)==0) {
                sprintf(ErrorMsg,"expecting rvalue, found newline");
                fUngetChar('\n',fp);
            } else {
                sprintf(ErrorMsg,
			"expecting rvalue, found \'%s\'",value);
            }
            return(KV_SYNTAX);
            break;

        case GTOK_STRING:
        case GTOK_QSTRING:
        case GTOK_NUMBER:
            gotit = TRUE;
            return(KV_OKAY);

        default:
            sprintf(ErrorMsg,
		    "panic: bad return (%d) from fGetToken()",rc);
            return(KV_SYNTAX);
        }

    } while ( !gotit );
    /*NOTREACHED*/
    return(0); /*just to shut up -Wall MRVM*/
}

/*
 * Routine Name: fGetToken
 *
 * Function: read the next token from the specified file.
 * A token is defined as a group of characters
 * terminated by a white space char (SPACE, CR,
 * LF, FF, TAB). The token returned is stripped of
 * both leading and trailing white space, and is
 * terminated by a NULL terminator.  An alternate
 * definition of a token is a string enclosed in
 * single or double quotes.
 *
 * Explicit Parameters:
 * fp              pointer to the input FILE
 * dest    pointer to destination buffer
 * maxlen  length of the destination buffer. The buffer
 * length INCLUDES the NULL terminator.
 *
 * Implicit Parameters: stderr  where the "token too long" message goes
 *
 * External Procedures: fgetc
 *
 * Side Effects:                None
 *
 * Return Value:                A token classification value, as
 *				defined in kparse.h. Note that the
 *				classification for end of file is
 *				always zero.
 */
int
fGetToken(fp, dest, maxlen)
    FILE *fp;
    char *dest;
    int  maxlen;
{
    int ch='\0';
    int len=0;
    char *p = dest;
    int digits;

    ch=fGetChar(fp);

    /*
     * check for a quoted string.  If found, take all characters
     * that fit until a closing quote is found.  Note that this
     * algorithm will not behave well for a string which is too long.
     */
    if (ISQUOTE(ch)) {
        int done = FALSE;
        do {
            ch = fGetChar(fp);
            done = ((maxlen<++len)||ISLINEFEED(ch)||(ch==EOF)
		    ||ISQUOTE(ch));
            if (ch=='\\')
                ch = fGetLiteral(fp);
            if (!done)
                *p++ = ch;
            else if ((ch!=EOF) && !ISQUOTE(ch))
                fUngetChar(ch,fp);
        } while (!done);
        *p = '\0';
        if (ISLINEFEED(ch)) return(GTOK_BAD_QSTRING);
        return(GTOK_QSTRING);
    }

    /*
     * Not a quoted string.  If its a token character (rules are
     * defined via the ISTOKENCHAR macro, in kparse.h) take it and all
     * token chars following it until we run out of space.
     */
    digits=TRUE;
    if (ISTOKENCHAR(ch)) {
        while ( (ISTOKENCHAR(ch)) && len<maxlen-1 ) {
            if (!isdigit(ch)) digits=FALSE;
            *p++ = ch;
            len++;
            ch = fGetChar(fp);
        };
        *p = '\0';

        if (ch!=EOF) {
            fUngetChar(ch,fp);
        }
        if (digits) {
            return(GTOK_NUMBER);
        } else {
            return(GTOK_STRING);
        }
    }

    /*
     * Neither a quoted string nor a token character.  Return a string
     * with just that one character in it.
     */
    if (ch==EOF) {
        return(GTOK_EOF);
    }
    if (!ISWHITESPACE(ch)) {
        *p++ = ch;
        *p='\0';
    } else {
        *p++ = ' ';		/* white space is always the
				 * blank character */
        *p='\0';
        /*
         * The character is a white space. Flush all additional white
         * space.
         */
        while (ISWHITESPACE(ch) && ((ch=fGetChar(fp)) != EOF))
            ;
        if (ch!=EOF) {
            fUngetChar(ch,fp);
        }
        return(GTOK_WHITE);
    }
    return(GTOK_PUNK);
}

/*
 * fGetLiteral is called after we find a '\' in the input stream.  A
 * string of numbers following the backslash are converted to the
 * appropriate value; hex (0xn), octal (0n), and decimal (otherwise)
 * are all supported.  If the char after the \ is not a number, we
 * special case certain values (\n, \f, \r, \b) or return a literal
 * otherwise (useful for \", for example).
 */
int
fGetLiteral(fp)
    FILE *fp;
{
    int ch;
    int n=0;
    int base;

    ch = fGetChar(fp);

    if (!isdigit(ch)) {
        switch (ch) {
        case 'n':       return('\n');
        case 'f':       return('\f');
        case 'r':       return('\r');
        case 'b':       return('\b');
        default:        return(ch);
        }
    }

    /*
     * got a number.  might be decimal (no prefix), octal (prefix 0),
     * or hexadecimal (prefix 0x).  Set the base appropriately.
     */
    if (ch!='0') {
        base=10;                /* its a decimal number */
    } else {
        /*
         * found a zero, its either hex or octal
         */
        ch = fGetChar(fp);
        if ((ch!='x') && (ch!='X')) {
            base=010;
        } else {
            ch = fGetChar(fp);
            base=0x10;
        }
    }

    switch (base) {

    case 010:                   /* octal */
        while (ISOCTAL(ch)) {
            n = (n*base) + ch - '0';
            ch = fGetChar(fp);
        }
        break;

    case 10:                    /* decimal */
        while (isdigit(ch)) {
            n = (n*base) + ch - '0';
            ch = fGetChar(fp);
        }
        break;
    case 0x10:                  /* hexadecimal */
        while (isxdigit(ch)) {
            if (isdigit(ch)) {
                n = (n*base) + ch - '0';
            } else {
                n = (n*base) + toupper(ch) - 'A' + 0xA ;
            }
            ch = fGetChar(fp);
        }
        break;
    default:
        fprintf(stderr,"fGetLiteral() died real bad. Fix gettoken.c.");
        exit(1);
        break;
    }
    fUngetChar(ch,fp);
    return(n);
}

/*
 * exactly the same as ungetc(3) except that the line number of the
 * input file is maintained.
 */
int
fUngetChar(ch,fp)
    int ch;
    FILE *fp;
{
    if (ch=='\n') LineNbr--;
    return(ungetc(ch,fp));
}


/*
 * exactly the same as fgetc(3) except that the line number of the
 * input file is maintained.
 */
int
fGetChar(fp)
    FILE *fp;
{
    int ch = fgetc(fp);
    if (ch=='\n') LineNbr++;
    return(ch);
}


/*
 * Routine Name: strsave
 *
 * Function: return a pointer to a saved copy of the
 * input string. the copy will be allocated
 * as large as necessary.
 *
 * Explicit Parameters: pointer to string to save
 *
 * Implicit Parameters: None
 *
 * External Procedures: malloc,strcpy,strlen
 *
 * Side Effects: None
 *
 * Return Value: pointer to copied string
 *
 */
char *
strsave(p)
    char *p;
{
    return(strcpy(malloc(strlen(p)+1),p));
}


/*
 * strutol changes all characters in a string to lower case, in place.
 * the pointer to the beginning of the string is returned.
 */

char *
strutol( start )
    char *start;
{
    char *q;
    for (q=start; *q; q++)
        if (isupper(*q))
	    *q=tolower(*q);
    return(start);
}

#ifdef GTOK_TEST	     /* mainline test routine for fGetToken() */

#define MAXTOKEN 100

char *pgm = "gettoken";

main(argc,argv)
    int argc;
    char **argv;
{
    char *p;
    int type;
    FILE *fp;

    if (--argc) {
        fp = fopen(*++argv,"ra");
        if (fp == (FILE *)NULL) {
            fprintf(stderr,"can\'t open \"%s\"\n",*argv);
        }
    } else
        fp = stdin;

    p = malloc(MAXTOKEN);
    while (type = fGetToken(fp,p,MAXTOKEN)) {
        switch(type) {
        case GTOK_BAD_QSTRING:
	    printf("BAD QSTRING!\t");
	    break;
        case GTOK_EOF:
	    printf("EOF!\t");
	    break;
        case GTOK_QSTRING:
	    printf("QSTRING\t");
	    break;
        case GTOK_STRING:
	    printf("STRING\t");
	    break;
        case GTOK_NUMBER:
	    printf("NUMBER\t");
	    break;
        case GTOK_PUNK:
	    printf("PUNK\t");
	    break;
        case GTOK_WHITE:
	    printf("WHITE\t");
	    break;
        default:
	    printf("HUH?\t");
	    break;
        }
        if (*p=='\n')
            printf("\\n\n");
	else
            printf("%s\n",p);
    }
    exit(0);
}
#endif

#ifdef KVTEST

main(argc,argv)
    int argc;
    char **argv;
{
    int rc,ch;
    FILE *fp;
    char key[MAXKEY],valu[MAXVALUE];
    char *filename;

    if (argc != 2) {
        fprintf(stderr,"usage: test <filename>\n");
        exit(1);
    }

    if (!(fp=fopen(*++argv,"r"))) {
        fprintf(stderr,"can\'t open input file \"%s\"\n",filename);
        exit(1);
    }
    filename = *argv;

    while ((rc=fGetKeywordValue(fp,key,MAXKEY,valu,MAXVALUE))!=KV_EOF){

        switch (rc) {

        case KV_EOL:
            printf("%s, line %d: nada mas.\n",filename,LineNbr-1);
            break;

        case KV_SYNTAX:
            printf("%s, line %d: syntax error: %s\n",
                   filename,LineNbr,ErrorMsg);
            while ( ((ch=fGetChar(fp))!=EOF) && (ch!='\n') );
            break;

        case KV_OKAY:
            printf("%s, line %d: okay, %s=\"%s\"\n",
                   filename,LineNbr,key,valu);
            break;

        default:
            printf("panic: bad return (%d) from fGetKeywordValue\n",rc);
            break;
        }
    }
    printf("EOF");
    fclose(fp);
    exit(0);
}
#endif

#ifdef PSTEST

parmtable kparm[] = {
    /*  keyword, default, found value */
    { "user",       "",    (char *)NULL },
    { "realm",   "Athena", (char *)NULL },
    { "instance",   "",    (char *)NULL }
};

main(argc,argv)
    int argc;
    char **argv;
{
    int rc,i,ch;
    FILE *fp;
    char *filename;

    if (argc != 2) {
        fprintf(stderr,"usage: test <filename>\n");
        exit(1);
    }

    if (!(fp=fopen(*++argv,"r"))) {
        fprintf(stderr,"can\'t open input file \"%s\"\n",filename);
        exit(1);
    }
    filename = *argv;

    while ((rc=fGetParameterSet(fp,kparm,PARMCOUNT(kparm))) != PS_EOF) {

        switch (rc) {

        case PS_BAD_KEYWORD:
            printf("%s, line %d: %s\n",filename,LineNbr,ErrorMsg);
            while ( ((ch=fGetChar(fp))!=EOF) && (ch!='\n') );
            break;

        case PS_SYNTAX:
            printf("%s, line %d: syntax error: %s\n",
                   filename,LineNbr,ErrorMsg);
            while ( ((ch=fGetChar(fp))!=EOF) && (ch!='\n') );
            break;

        case PS_OKAY:
            printf("%s, line %d: valid parameter set found:\n",
                   filename,LineNbr-1);
            for (i=0; i<PARMCOUNT(kparm); i++) {
                printf("\t%s = \"%s\"\n",kparm[i].keyword,
                       (kparm[i].value ? kparm[i].value
			: kparm[i].defvalue));
            }
            break;

        default:
            printf("panic: bad return (%d) from fGetParameterSet\n",rc);
            break;
        }
        FreeParameterSet(kparm,PARMCOUNT(kparm));
    }
    printf("EOF");
    fclose(fp);
    exit(0);
}
#endif
