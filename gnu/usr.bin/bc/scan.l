%{
/* scan.l: the (f)lex description file for the scanner. */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include "bcdefs.h"
#include "y.tab.h"
#include "global.h"
#include "proto.h"

/* Using flex, we can ask for a smaller input buffer.  With lex, this
   does nothing! */

#ifdef SMALL_BUF
#undef YY_READ_BUF_SIZE
#define YY_READ_BUF_SIZE 512
#endif

/* We want to define our own yywrap. */
#undef yywrap
_PROTOTYPE(int yywrap, (void));

/* MINIX returns from read with < 0 if SIGINT is  encountered.
   In flex, we can redefine YY_INPUT to the following.  In lex, this
   does nothing! */
#include <errno.h>
#undef  YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	while ( (result = read( fileno(yyin), (char *) buf, max_size )) < 0 ) \
	    if (errno != EINTR) \
		YY_FATAL_ERROR( "read() in flex scanner failed" );

%}
DIGIT [0-9A-F]
LETTER [a-z]
%%
define return(Define);
break  return(Break);
quit   return(Quit);
length return(Length);
return return(Return);
for    return(For);
if     return(If);
while  return(While);
sqrt   return(Sqrt);
scale  return(Scale);
ibase  return(Ibase);
obase  return(Obase);
auto   return(Auto);
else   return(Else);
read   return(Read);
halt   return(Halt);
last   return(Last);
warranty return(Warranty);
continue return(Continue);
print  return(Print);
limits return(Limits);
"." {
#ifdef DOT_IS_LAST
       return(Last);
#else
       yyerror ("illegal character: %s",yytext);
#endif
    }
"+"|"-"|";"|"("|")"|"{"|"}"|"["|"]"|","|"^" { yylval.c_value = yytext[0]; 
					      return((int)yytext[0]); }
&& { return(AND); }
\|\| { return(OR); }
"!" { return(NOT); }
"*"|"/"|"%" { yylval.c_value = yytext[0]; return(MUL_OP); }
"="|\+=|-=|\*=|\/=|%=|\^=  { yylval.c_value = yytext[0]; return(ASSIGN_OP); }
=\+|=-|=\*|=\/|=%|=\^  { 
#ifdef OLD_EQ_OP
			 char warn_save;
			 warn_save = warn_not_std;
			 warn_not_std = TRUE;
			 warn ("Old fashioned =<op>");
			 warn_not_std = warn_save;
			 yylval.c_value = yytext[1];
#else
			 yylval.c_value = '=';
			 yyless (1);
#endif
			 return(ASSIGN_OP);
		       }
==|\<=|\>=|\!=|"<"|">" { yylval.s_value = strcopyof(yytext); return(REL_OP); }
\+\+|-- { yylval.c_value = yytext[0]; return(INCR_DECR); }
"\n" { line_no++; return(NEWLINE); }
\\\n {  line_no++;  /* ignore a "quoted" newline */ }
[ \t]+  { /* ignore spaces and tabs */ }
"/*"  {
	int c;

	for (;;)
	  {
	    while ( ((c=input()) != '*') && (c != EOF)) 
	      /* eat it */
	      if (c == '\n') line_no++;
	    if (c == '*')
 	      {
		while ( (c=input()) == '*') /* eat it*/;
		if (c == '/') break; /* at end of comment */
		if (c == '\n') line_no++;
	      }
	    if (c == EOF)
	      {
		fprintf (stderr,"EOF encountered in a comment.\n");
		break;
	      }
	  }
      }
[a-z][a-z0-9_]* { yylval.s_value = strcopyof(yytext); return(NAME); }
\"[^\"]*\"  {
 	      unsigned char *look;
	      int count = 0;
	      yylval.s_value = strcopyof(yytext);
	      for (look = yytext; *look != 0; look++)
		{
		  if (*look == '\n') line_no++;
		  if (*look == '"')  count++;
		}
	      if (count != 2) yyerror ("NUL character in string.");
	      return(STRING);
	    }
{DIGIT}({DIGIT}|\\\n)*("."({DIGIT}|\\\n)*)?|"."(\\\n)*{DIGIT}({DIGIT}|\\\n)* {
	      unsigned char *src, *dst;
	      int len;
	      /* remove a trailing decimal point. */
	      len = strlen(yytext);
	      if (yytext[len-1] == '.')
	        yytext[len-1] = 0;
	      /* remove leading zeros. */
	      src = yytext;
	      dst = yytext;
	      while (*src == '0') src++;
	      if (*src == 0) src--;
	      /* Copy strings removing the newlines. */
	      while (*src != 0)
		{
	          if (*src == '\\')
		    {
		      src++; src++;
		      line_no++;
		    }
		  else
		    *dst++ = *src++;
	        }
	      *dst = 0;
	      yylval.s_value = strcopyof(yytext); 
	      return(NUMBER);
	    }
.       {
	  if (yytext[0] < ' ')
	    yyerror ("illegal character: ^%c",yytext[0] + '@');
	  else
	    if (yytext[0] > '~')
	      yyerror ("illegal character: \\%3d", (int) yytext[0]);
	    else
	      yyerror ("illegal character: %s",yytext);
	}
%%



/* This is the way to get multiple files input into lex. */

int
yywrap()
{
  if (!open_new_file ()) return (1);	/* EOF on standard in. */
  return (0);				/* We have more input. */
}
