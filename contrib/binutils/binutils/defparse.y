/* defparse.y - parser for .def files */

/*   Copyright (C) 1995 Free Software Foundation, Inc.

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


%union {
  char *id;
  int number;
};

%token NAME, LIBRARY, DESCRIPTION, STACKSIZE, HEAPSIZE, CODE, DATA
%token SECTIONS, EXPORTS, IMPORTS, VERSION, BASE, CONSTANT
%token READ WRITE EXECUTE SHARED NONAME
%token <id> ID
%token <number> NUMBER
%type  <number> opt_base opt_ordinal opt_NONAME opt_CONSTANT attr attr_list opt_number
%type  <id> opt_name opt_equal_name 

%%

start: start command
	| command
	;

command: 
		NAME opt_name opt_base { def_name ($2, $3); }
	|	LIBRARY opt_name opt_base { def_library ($2, $3); }
	|	EXPORTS explist 
	|	DESCRIPTION ID { def_description ($2);}
	|	STACKSIZE NUMBER opt_number { def_stacksize ($2, $3);}
	|	HEAPSIZE NUMBER opt_number { def_heapsize ($2, $3);}
	|	CODE attr_list { def_code ($2);}
	|	DATA attr_list  { def_data ($2);}
	|	SECTIONS seclist
	|	IMPORTS implist
	|	VERSION NUMBER { def_version ($2,0);}
	|	VERSION NUMBER '.' NUMBER { def_version ($2,$4);}
	;


explist:
		explist expline
	|	expline
	;

expline:
		ID opt_equal_name opt_ordinal opt_NONAME opt_CONSTANT
			{ def_exports ($1, $2, $3, $4, $5);}
	;
implist:	
		implist impline
	|	impline
	;

impline:
		ID '=' ID '.' ID { def_import ($1,$3,$5);}
	|	ID '.' ID	 { def_import (0, $1,$3);}
	;
seclist:
		seclist secline
	|	secline
	;

secline:
	ID attr_list { def_section ($1,$2);}
	;

attr_list:
	attr_list opt_comma attr
	| attr
	;

opt_comma:
	','
	| 
	;
opt_number: ',' NUMBER { $$=$2;}
	|	   { $$=-1;}
	;
	
attr:
		READ { $$ = 1;}
	|	WRITE { $$ = 2;}	
	|	EXECUTE { $$=4;}
	|	SHARED { $$=8;}
	;

opt_CONSTANT:
		CONSTANT {$$=1;}
	|		 {$$=0;}
	;
opt_NONAME:
		NONAME {$$=1;}
	|		 {$$=0;}
	;

opt_name: ID		{ $$ =$1; }
	|		{ $$=""; }
	;

opt_ordinal: 
	  '@' NUMBER     { $$=$2;}
	|                { $$=-1;}
	;

opt_equal_name:
          '=' ID	{ $$ = $2; }
        | 		{ $$ =  0; }			 
	;

opt_base: BASE	'=' NUMBER	{ $$= $3;}
	|	{ $$=-1;}
	;

	

