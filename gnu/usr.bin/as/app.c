/* This is the Assembler Pre-Processor
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* App, the assembler pre-processor.  This pre-processor strips out excess
   spaces, turns single-quoted characters into a decimal constant, and turns
   # <number> <filename> <garbage> into a .line <number>;.file <filename> pair.
   This needs better error-handling.
 */
#include <stdio.h>
#ifdef USG
#define bzero(s,n) memset(s,0,n)
#endif
#if !defined(__STDC__) && !defined(const)
#define const /* Nothing */
#endif

static char	lex [256];
static const char	symbol_chars[] = 
	"$._ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

extern const char comment_chars[];
extern const char line_comment_chars[];

#define LEX_IS_SYMBOL_COMPONENT		(1)
#define LEX_IS_WHITESPACE		(2)
#define LEX_IS_LINE_SEPERATOR		(4)
#define LEX_IS_COMMENT_START		(8)	/* JF added these two */
#define LEX_IS_LINE_COMMENT_START	(16)
#define IS_SYMBOL_COMPONENT(c)		(lex [c] & LEX_IS_SYMBOL_COMPONENT)
#define IS_WHITESPACE(c)		(lex [c] & LEX_IS_WHITESPACE)
#define IS_LINE_SEPERATOR(c)		(lex [c] & LEX_IS_LINE_SEPERATOR)
#define IS_COMMENT(c)			(lex [c] & LEX_IS_COMMENT_START)
#define IS_LINE_COMMENT(c)		(lex [c] & LEX_IS_LINE_COMMENT_START)

void
do_scrub_begin()
{
	const char *p;

	bzero (lex, sizeof(lex));		/* Trust NOBODY! */
	lex [' ']		|= LEX_IS_WHITESPACE;
	lex ['\t']		|= LEX_IS_WHITESPACE;
	for (p =symbol_chars;*p;++p)
		lex [*p] |= LEX_IS_SYMBOL_COMPONENT;
	lex ['\n']		|= LEX_IS_LINE_SEPERATOR;
#ifdef DONTDEF
	lex [':']		|= LEX_IS_LINE_SEPERATOR;
#endif
	lex [';']		|= LEX_IS_LINE_SEPERATOR;
	for (p=comment_chars;*p;p++)
		lex[*p] |= LEX_IS_COMMENT_START;
	for (p=line_comment_chars;*p;p++)
		lex[*p] |= LEX_IS_LINE_COMMENT_START;
}

FILE *scrub_file;

int
scrub_from_file()
{
	return getc(scrub_file);
}

void
scrub_to_file(ch)
int ch;
{
	ungetc(ch,scrub_file);
}

char *scrub_string;
char *scrub_last_string;

int
scrub_from_string()
{
	return scrub_string == scrub_last_string ? EOF : *scrub_string++;
}

void
scrub_to_string(ch)
int ch;
{
	*--scrub_string=ch;
}

int
do_scrub_next_char(get,unget)
int (*get)();
void (*unget)();
/* FILE *fp; */
{
	/* State 0: beginning of normal line
		1: After first whitespace on normal line (flush more white)
		2: After first non-white on normal line (keep 1white)
		3: after second white on normal line (flush white)
		4: after putting out a .line, put out digits
		5: parsing a string, then go to old-state
		6: putting out \ escape in a "d string.
		7: After putting out a .file, put out string.
		8: After putting out a .file string, flush until newline.
		-1: output string in out_string and go to the state in old_state
		-2: flush text until a '*' '/' is seen, then go to state old_state
	*/

	static state;
	static old_state;
	static char *out_string;
	static char out_buf[20];
	static add_newlines;
	int ch;

	if(state==-1) {
		ch= *out_string++;
		if(*out_string==0) {
			state=old_state;
			old_state=3;
		}
		return ch;
	}
	if(state==-2) {
		for(;;) {
			do ch=(*get)();
			while(ch!=EOF && ch!='\n' && ch!='*');
			if(ch=='\n' || ch==EOF)
				return ch;
			 ch=(*get)();
			 if(ch==EOF || ch=='/')
			 	break;
			(*unget)(ch);
		}
		state=old_state;
		return ' ';
	}
	if(state==4) {
		ch=(*get)();
		if(ch==EOF || (ch>='0' && ch<='9'))
			return ch;
		else {
			while(ch!=EOF && IS_WHITESPACE(ch))
				ch=(*get)();
			if(ch=='"') {
				(*unget)(ch);
				out_string="; .file ";
				old_state=7;
				state= -1;
				return *out_string++;
			} else {
				while(ch!=EOF && ch!='\n')
					ch=(*get)();
				return ch;
			}
		}
	}
	if(state==5) {
		ch=(*get)();
		if(ch=='"') {
			state=old_state;
			return '"';
		} else if(ch=='\\') {
			state=6;
			return ch;
		} else if(ch==EOF) {
			as_warn("End of file in string: inserted '\"'");
 			state=old_state;
			(*unget)('\n');
			return '"';
		} else {
			return ch;
		}
	}
	if(state==6) {
		state=5;
		ch=(*get)();
		switch(ch) {
			/* This is neet.  Turn "string
			   more string" into "string\n  more string"
			 */
		case '\n':
			(*unget)('n');
			add_newlines++;
			return '\\';

		case '"':
		case '\\':
		case 'b':
		case 'f':
		case 'n':
		case 'r':
		case 't':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			break;
		default:
			as_warn("Unknown escape '\\%c' in string: Ignored",ch);
			break;

		case EOF:
			as_warn("End of file in string: '\"' inserted");
			return '"';
		}
		return ch;
	}

	if(state==7) {
		ch=(*get)();
		state=5;
		old_state=8;
		return ch;
	}

	if(state==8) {
		do ch= (*get)();
		while(ch!='\n');
		state=0;
		return ch;
	}

 flushchar:
	ch=(*get)();
	switch(ch) {
	case ' ':
	case '\t':
		do ch=(*get)();
		while(ch!=EOF && IS_WHITESPACE(ch));
		if(ch==EOF)
			return ch;
		if(IS_COMMENT(ch) || (state==0 && IS_LINE_COMMENT(ch)) || ch=='/' || IS_LINE_SEPERATOR(ch)) {
			(*unget)(ch);
			goto flushchar;
		}
		(*unget)(ch);
		if(state==0 || state==2) {
			state++;
			return ' ';
		} else goto flushchar;

	case '/':
		ch=(*get)();
		if(ch=='*') {
			for(;;) {
				do {
					ch=(*get)();
					if(ch=='\n')
						add_newlines++;
				} while(ch!=EOF && ch!='*');
				ch=(*get)();
				if(ch==EOF || ch=='/')
					break;
				(*unget)(ch);
			}
			if(ch==EOF)
				as_warn("End of file in '/' '*' string: */ inserted");

			(*unget)(' ');
			goto flushchar;
		} else {
			if(IS_COMMENT('/') || (state==0 && IS_LINE_COMMENT('/'))) {
				(*unget)(ch);
				ch='/';
				goto deal_misc;
			}
			if(ch!=EOF)
				(*unget)(ch);
			return '/';
		}
		break;

	case '"':
		old_state=state;
		state=5;
		return '"';
		break;

	case '\'':
		ch=(*get)();
		if(ch==EOF) {
			as_warn("End-of-file after a ': \000 inserted");
			ch=0;
		}
		sprintf(out_buf,"(%d)",ch&0xff);
		old_state=state;
		state= -1;
		out_string=out_buf;
		return *out_string++;

	case ':':
		if(state!=3)
			state=0;
		return ch;

	case '\n':
		if(add_newlines) {
			--add_newlines;
			(*unget)(ch);
		}
	case ';':
		state=0;
		return ch;

	default:
	deal_misc:
		if(state==0 && IS_LINE_COMMENT(ch)) {
			do ch=(*get)();
			while(ch!=EOF && IS_WHITESPACE(ch));
			if(ch==EOF) {
				as_warn("EOF in comment:  Newline inserted");
				return '\n';
			}
			if(ch<'0' || ch>'9') {
				while(ch!=EOF && ch!='\n')
					ch=(*get)();
				if(ch==EOF)
					as_warn("EOF in Comment: Newline inserted");
				state=0;
				return '\n';
			}
			(*unget)(ch);
			old_state=4;
			state= -1;
			out_string=".line ";
			return *out_string++;

		} else if(IS_COMMENT(ch)) {
			do ch=(*get)();
			while(ch!=EOF && ch!='\n');
			if(ch==EOF)
				as_warn("EOF in comment:  Newline inserted");
			state=0;
			return '\n';

		} else if(state==0) {
			state=2;
			return ch;
		} else if(state==1) {
			state=2;
			return ch;
		} else {
			return ch;

		}
	case EOF:
		if(state==0)
			return ch;
		as_warn("End-of-File not at end of a line");
	}
	return -1;
}

#ifdef TEST

char comment_chars[] = "|";
char line_comment_chars[] = "#";

main()
{
	int	ch;

	app_begin();
	while((ch=do_scrub_next_char(stdin))!=EOF)
		putc(ch,stdout);
}

as_warn(str)
char *str;
{
	fputs(str,stderr);
	putc('\n',stderr);
}
#endif
