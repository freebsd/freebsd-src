/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Include file for kparse routines.
 *
 *	from: kparse.h,v 4.5 89/01/11 12:05:53 steiner Exp $
 *	$FreeBSD$
 */

#ifndef KPARSE_DEFS
#define KPARSE_DEFS

/*
 * values returned by fGetParameterSet()
 */

#define PS_BAD_KEYWORD	  -2	/* unknown or duplicate keyword */
#define PS_SYNTAX	  -1	/* syntax error */
#define PS_OKAY		   0	/* got a complete parameter set */
#define PS_EOF		   1	/* nothing more in the file */

/*
 * values returned by fGetKeywordValue()
 */

#define KV_SYNTAX	 -2	/* syntax error */
#define KV_EOF		 -1	/* nothing more in the file */
#define KV_OKAY		  0	/* got a keyword/value pair */
#define KV_EOL		  1	/* nothing more on this line */

/*
 * values returned by fGetToken()
 */

#define GTOK_BAD_QSTRING -1	/* newline found in quoted string */
#define GTOK_EOF	  0	/* end of file encountered */
#define GTOK_QSTRING	  1	/* quoted string */
#define GTOK_STRING	  2	/* unquoted string */
#define GTOK_NUMBER	  3	/* one or more digits */
#define GTOK_PUNK	  4	/* punks are punctuation, newline,
				 * etc. */
#define GTOK_WHITE	  5	/* one or more whitespace chars */

/*
 * extended character classification macros
 */

#define ISOCTAL(CH) 	( (CH>='0')  && (CH<='7') )
#define ISQUOTE(CH) 	( (CH=='\"') || (CH=='\'') || (CH=='`') )
#define ISWHITESPACE(C) ( (C==' ')   || (C=='\t') )
#define ISLINEFEED(C) 	( (C=='\n')  || (C=='\r')  || (C=='\f') )

/*
 * tokens consist of any printable charcacter except comma, equal, or
 * whitespace
 */

#define ISTOKENCHAR(C) ((C>040) && (C<0177) && (C != ',') && (C != '='))

/*
 * the parameter table defines the keywords that will be recognized by
 * fGetParameterSet, and their default values if not specified.
 */

typedef struct {
    char *keyword;
    char *defvalue;
    char *value;
}       parmtable;

#define PARMCOUNT(P) (sizeof(P)/sizeof(P[0]))

extern int LineNbr;		/* current line # in parameter file */

extern char ErrorMsg[];		/*
				 * meaningful only when KV_SYNTAX,
 				 * PS_SYNTAX,  or PS_BAD_KEYWORD is
				 * returned by fGetKeywordValue or
				 * fGetParameterSet
				 */

extern char *strsave(char *p);	/* defined in this module */
extern char *strutol(char *p);		/* defined in this module */

int fGetParameterSet(FILE *fp, parmtable parm[], int parmcount);
int fGetKeywordValue(FILE *fp, char *keyword, int klen, char *value, int vlen);
int fGetToken(FILE *fp, char *dest, int maxlen);

#endif /* KPARSE_DEFS */
