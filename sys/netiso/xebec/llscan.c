/* $Header: llscan.c,v 2.2 88/09/19 12:55:06 nhall Exp $ */
/* $Source: /var/home/tadl/src/argo/xebec/RCS/llscan.c,v $ */
/*
 * ************************* NOTICE *******************************
 * This code is in the public domain.  It cannot be copyrighted.
 * This scanner was originally written by Keith Thompson for the 
 * University of Wisconsin Crystal project.
 * It was subsequently modified significantly by Nancy Hall at the 
 * University of Wisconsin for the ARGO project.
 * ****************************************************************
 */
#include "xebec.h"
#include "llparse.h"

#include "main.h"
#include <stdio.h>
#include "procs.h"
#include "debug.h"

#define EOFILE	0x01
#define UNUSED	0x02
#define IGNORE	0x04
#define OPCHAR	0x8
#define DIGITS	0x10
#define	LETTER	0x20

int chtype[128] = {
/*	null,	soh ^a,	stx ^b	etx ^c	eot ^d	enq ^e	ack ^f	bel ^g	*/
	EOFILE,	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,
/*	bs ^h	ht ^i	lf ^j	vt ^k	ff ^l	cr ^m	so ^n	si ^o	*/
	UNUSED,	IGNORE,	IGNORE,	UNUSED,	IGNORE,	IGNORE,	UNUSED,	UNUSED,
/*	dle ^p	dc1 ^q	dc2 ^r	dc3 ^s	dc4 ^t	nak ^u	syn ^v	etb ^w	*/
	UNUSED,	UNUSED,	UNUSED,	UNUSED,	EOFILE,	UNUSED,	UNUSED,	UNUSED,
/*	can ^x	em ^y	sub ^z	esc ^]	fs ^\ 	gs ^}	rs ^`	us ^/	*/
	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,	UNUSED,

/*			!		"		#		$		%		&		'		*/
	IGNORE,	UNUSED,	OPCHAR,	UNUSED,	OPCHAR,	UNUSED,	OPCHAR,	OPCHAR,
/*	(		)		*		+		,		-		.		/		*/
	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,
/*	0		1		2		3		4		5		6		7		*/
	DIGITS,	DIGITS,	DIGITS,	DIGITS,	DIGITS,	DIGITS,	DIGITS,	DIGITS,
/*	8		9		:		;		<		=		>		?		*/
	DIGITS,	DIGITS,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,	OPCHAR,

/*	@		A		B		C		D		E		F		G		*/
	UNUSED,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
/*	H		I		J		K		L		M		N		O		*/
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
/*	P		Q		R		S		T		U		V		W		*/
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
/* 	X		Y		Z		[		\		]		^		_		*/
	LETTER,	LETTER,	LETTER,	OPCHAR,	UNUSED,	OPCHAR,	OPCHAR,	LETTER,

/*	`		a		b		c		d		e		f		g		*/
	UNUSED,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
/*	h		i		j		k		l		m		n		o		*/
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
/*	p		q		r		s		t		u		v		w		*/
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
/*	x		y		z		{		|		}		~		del		*/
	LETTER,	LETTER,	LETTER,	OPCHAR,	UNUSED,	OPCHAR,	UNUSED,	UNUSED
};


extern FILE *astringfile; 
static char *buffptr;
static char buffer[2][LINELEN];
static int currentbuf = 1;

#define addbuf(x) *buffptr++ = x

static int ch = ' ';

skip()
{
	while((chtype[ch] == IGNORE) ) {
		ch = getch();
	}
}

llaccept(t)
LLtoken *t;
{
	switch(t->llstate) {
	case NORMAL:
		break;
	case INSERT:
		fprintf(stderr,"Insert %s\n", llstrings[t->llterm]);
		break;
	case DELETE:
		fprintf(stderr,"Delete %s\n", llstrings[t->llterm]);
		break;
	}
}

#define	TVAL	(t->llattrib)


dump_buffer()
{
	register int i;
	for(i=0; i<20; i++)
	(void) fputc(buffer[currentbuf][i], stderr);
	(void) fputc('\n', stderr);
	(void) fflush(stderr);
}

int iskey(c, buf)
char *c;
char **buf;
{
	register int i;
	static struct { char *key_word; int term_type; } keys[] = {
			{ "SAME", T_SAME },
			{ "DEFAULT", T_DEFAULT },
			{ "NULLACTION", T_NULLACTION },
			{ "STRUCT", T_STRUCT },
			{ "SYNONYM", T_SYNONYM },
			{ "TRANSITIONS", T_TRANSITIONS },
			{ "STATES", T_STATES },
			{ "EVENTS", T_EVENTS },
			{ "PCB", T_PCB },
			{ "INCLUDE", T_INCLUDE },
			{ "PROTOCOL", T_PROTOCOL },
			{ 0, 0},
	};

	for (i = 0; keys[i].key_word ; i++) {
		if( !strcmp(c, (*buf = keys[i].key_word) ) ) {
			return ( keys[i].term_type );
		}
	}
	*buf = (char *)0;
	return(0);
}

getstr(o,c) 
	/* c is the string delimiter 
	 * allow the delimiter to be escaped 
	 * the messy part: translate $ID to
	 *   e->ev_union.ID
	 * where ID is an event with a non-zero obj_struc
	 * need we check for the field???
	 */
char o,c;
{
	register int nested = 1;
	register int allow_nesting = (o==c)?-1:1; 

	IFDEBUG(S)
		fprintf(stdout,"getstr: ch=%c, delimiters %c %c\n",
			ch,o, c);
		fprintf(stdout,"getstr: buffptr 0x%x, currentbuf 0x%x\n",
			buffptr, currentbuf);
	ENDDEBUG

	if( ch == c ) nested--;
	while(nested) {
		if(ch == '\0') {
			fprintf(stderr,
			"Eof inside of a string, delims= %c,%c, nesting %d",c,o, nested);
			Exit(-1);
			/* notreached */
		} else if(ch == '$') {
			/* might be an attribute */
			IFDEBUG(S)
				fprintf(stdout,"getstr: atttribute?\n");
			ENDDEBUG

			/* assume it's an event */
			/* addbuf is a macro so this isn't as bad as
			 * it looks 
			 * add "e->ev_union."
			 */
			if( (ch = getch()) == '$' ) {
				addbuf('e'); addbuf('-'); addbuf('>');
				addbuf('e'); addbuf('v'); addbuf('_');
				addbuf('u'); addbuf('n'); addbuf('i');
				addbuf('o'); addbuf('n'); 
				addbuf('.');
				AddCurrentEventName(& buffptr);
			} else {
				char *obufp = buffptr;

				do {
					addbuf(ch);
					ch = getch();
				} while(chtype[ch] & LETTER);
				addbuf('\0');
				if( !strncmp(obufp, synonyms[PCB_SYN],
										strlen(synonyms[PCB_SYN]) )) {
					buffptr = obufp;
					addbuf('p');
				} else if( !strncmp(obufp, synonyms[EVENT_SYN],
										strlen(synonyms[EVENT_SYN]))) {
					buffptr = obufp;
					addbuf('e'); 
				} else {
					fprintf(stderr, "Unknown synonym %s\n", obufp);
					Exit(-1);
				}
				if(ch == '.') {
					addbuf('-'); addbuf('>');
				} else  {
					/* needs to be checked for nesting */
					goto check;
				}
			}
			/* end of attribute handling */
			goto skip;
		} else if(ch == '\\') {
			/* possible escape - this is kludgy beyond belief:
			 * \ is used to escape open and closing delimiters
			 * and '$'
			 * otherwise it's passed through to be compiled by C
			 */
			ch = getch();
			if( (ch != o ) && (ch != c) && (ch != '$') ) {
			/* may need to handle case where \ is last char in file... */
				/* don't treat is as escape; not open or close so
				 * don't have to worry about nesting either 
				 */
				addbuf('\\');
			}
		}
		addbuf(ch);
	skip:
		ch = getch();
	check:
		if( ch == o ) nested += allow_nesting;
		else if( ch == c ) nested--;
		if ( (buffptr - buffer[currentbuf]) > LINELEN) {
			fprintf(stderr, 
			"%s too long.\n", (o=='{')?"Action":"Predicate"); /*}*/
			fprintf(stderr, 
			"buffptr, currentbuf 0x%x, 0x%x\n",buffptr,currentbuf );
			Exit(-1);
		}
		IFDEBUG(S)
			fprintf(stdout,"loop in getstr: ch 0x%x,%c o=%c,c=%c nested=%d\n", 
				ch,ch,o,c,nested);
		ENDDEBUG
	}
	addbuf(ch);
	addbuf('\0');

	IFDEBUG(S)
		fprintf(stdout,"exit getstr: got %s\n", buffer[currentbuf]);
		fprintf(stdout,"exit getstr: buffptr 0x%x, currentbuf 0x%x\n",
			buffptr, currentbuf);
	ENDDEBUG
}

getch()
{
	char c;
	extern FILE *infile;
	extern int lineno;

	c = fgetc(infile) ;
	if (c == '\n') lineno++;
	if ((int)c ==  EOF) c = (char)0;
	if (feof(infile)) c = (char) 0;
	IFDEBUG(e)
		fprintf(stdout, "getch: 0x%x\n", c);
		(void) fputc( c, stdout);
		fflush(stdout);
	ENDDEBUG

	return c;
}

llscan(t)
LLtoken *t;
{
	char c;

	t->llstate = NORMAL;

	++currentbuf;
	currentbuf&=1;
again:
	buffptr =  &buffer[currentbuf][0];

	skip();

	switch(chtype[ch]) {

	case EOFILE:
		t->llterm = T_ENDMARKER;
		break;

	case UNUSED:
		fprintf(stderr, "Illegal character in input - 0x%x ignored.",  ch);
		ch = getch();
		goto again;

	case OPCHAR:

		switch(ch) {

		case '/':
			/* possible comment : elide ; kludge */
			IFDEBUG(S)
				fprintf(stdout, "Comment ch=%c\n", ch);
			ENDDEBUG
			c = getch();
			if (c != '*') {
				fprintf(stderr,"Syntax error : character(0x%x) ignored", ch);
				ch = c;
				goto again;
			} else {
				register int state = 2,  whatchar=0;
				static int dfa[3][3] = {
					/* 		 	done	seen-star  middle */
					/* star */	{ 	0,	1,		1	},
					/* /    */	{	0,	0,		2 	},
					/* other */ {	0,	2,		2	}
				};

				while( state ) {
					if( (c = getch()) == (char)0)
						break;
					whatchar = (c=='*')?0:(c=='/'?1:2);
					IFDEBUG(S)
						fprintf(stdout, 
							"comment: whatchar = %d, c = 0x%x,%c, oldstate=%d",
							whatchar, c,c, state);
					ENDDEBUG
					state = dfa[whatchar][state];
					IFDEBUG(S)
						fprintf(stdout, ", newstate=%d\n", state);
					ENDDEBUG
				}
				if(state) {
					fprintf(stderr,
						"Syntax error: end of file inside a comment");
					Exit(-1);
				} else ch = getch();
			}
			IFDEBUG(S)
				fprintf(stdout, "end of comment at 0x%x,%c\n",ch,ch);
			ENDDEBUG
			goto again;


		case '*':
			t->llterm = T_STAR;
			break;

		case ',':
			t->llterm = T_COMMA;
			break;

		case ';':
			t->llterm = T_SEMI;
			break;

		case '<':
			t->llterm = T_LANGLE;
			break;

		case '=':
			t->llterm = T_EQUAL;
			break;

		case '[':
			t->llterm = T_LBRACK;
			break;

		case ']':
			t->llterm = T_RBRACK;
			break;

#ifdef T_FSTRING
		case '"':
			t->llterm = T_FSTRING;
			addbuf(ch);
			ch = getch();
			getstr('"', '"');
			TVAL.FSTRING.address = stash(buffer[currentbuf]);
			break;
#endif T_FSTRING

		case '(':
			t->llterm = T_PREDICATE;
			getstr(ch, ')' );
			TVAL.PREDICATE.address = buffer[currentbuf];
			break;

		case '{':
			t->llterm = T_ACTION;
			getstr(ch, '}');
			TVAL.ACTION.address = buffer[currentbuf];
			break;

		default:
			fprintf(stderr,"Syntax error : character(0x%x) ignored", ch);
			ch = getch();
			goto again;

		}
		ch = getch();
		break;

	case LETTER:
		do {
			addbuf(ch);
			ch = getch();
		} while(chtype[ch] & (LETTER | DIGITS));

		addbuf('\0');

		t->llterm = iskey(buffer[currentbuf], &TVAL.ID.address);
		if(!t->llterm) {
			t->llterm = T_ID;
			TVAL.ID.address = buffer[currentbuf];
		}
		IFDEBUG(S)
			fprintf(stdout, "llscan: id or keyword 0x%x, %s\n",
			TVAL.ID.address, TVAL.ID.address);
		ENDDEBUG
		break;

	default:
		fprintf(stderr, "Snark in llscan: chtype=0x%x, ch=0x%x\n",
			chtype[ch], ch);
	}
}
