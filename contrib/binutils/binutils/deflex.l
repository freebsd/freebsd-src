%{/* deflex.l - Lexer for .def files */

/*   Copyright (C) 1995, 1997, 1998 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* Contributed by Steve Chamberlain
   		  sac@cygnus.com

*/
#define DONTDECLARE_MALLOC
#include "libiberty.h"
#include "defparse.h"
#include "dlltool.h"

int linenumber;

%}
%%
"NAME" 		{ return NAME;}
"LIBRARY"	{ return LIBRARY;}
"DESCRIPTION" 	{ return DESCRIPTION;}
"STACKSIZE"	{ return STACKSIZE;}
"HEAPSIZE" 	{ return HEAPSIZE;}
"CODE" 		{ return CODE;}
"DATA"		{ return DATA;}
"SECTIONS"	{ return SECTIONS;}
"EXPORTS"	{ return EXPORTS;}
"IMPORTS"	{ return IMPORTS;}
"VERSION"	{ return VERSIONK;}
"BASE"		{ return BASE;}
"CONSTANT"	{ return CONSTANT; }
"NONAME"	{ return NONAME; }
"READ"		{ return READ;}
"WRITE"		{ return WRITE;}
"EXECUTE"	{ return EXECUTE;}
"SHARED"	{ return SHARED;}

[0-9][x0-9A-Fa-f]* { yylval.number = strtol (yytext,0,0); 
		return NUMBER; }

[A-Za-z$:\-\_?][A-Za-z0-9/$:\-\_@?]+ { 	
		yylval.id =  xstrdup (yytext);
		return ID;
		}

"\""[^\"]*"\"" {
		yylval.id = xstrdup (yytext+1);
		yylval.id[yyleng-2] = 0;
		return ID;
		}

"\'"[^\']*"\'" {
		yylval.id = xstrdup (yytext+1);
		yylval.id[yyleng-2] = 0;
		return ID;
		}
"*".* 		{ }
";".* 		{ }
" "		{ }
"\t"		{ }
"\n"	 	{ linenumber ++ ;}
"=" 		{ return '=';}
"." 		{ return '.';}
"@"	 	{ return '@';}
","		{ return ',';}
%%
#ifndef yywrap
/* Needed for lex, though not flex. */
int yywrap() { return 1; }
#endif
