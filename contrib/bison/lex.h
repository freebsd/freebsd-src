/* Token type definitions for bison's input reader,
   Copyright (C) 1984, 1989, 1992 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


#define ENDFILE		0
#define IDENTIFIER		1
#define COMMA		2
#define COLON		3
#define SEMICOLON	4
#define BAR   			5
#define LEFT_CURLY      	6
#define TWO_PERCENTS	7
#define PERCENT_LEFT_CURLY	8
#define TOKEN		9
#define NTERM		10
#define GUARD		11
#define TYPE  		12
#define UNION		13
#define START		14
#define LEFT  		15
#define RIGHT		16
#define NONASSOC		17
#define PREC  		18
#define SEMANTIC_PARSER 19
#define PURE_PARSER    	20
#define TYPENAME  	21
#define NUMBER		22
#define EXPECT		23
#define THONG		24
#define NOOP		25
#define SETOPT		26
#define ILLEGAL		27

#define	MAXTOKEN	1024
