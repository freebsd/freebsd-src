/* yylex - scanner front-end for flex */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Vern Paxson.
 * 
 * The United States Government has rights in this work pursuant
 * to contract no. DE-AC03-76SF00098 between the United States
 * Department of Energy and the University of California.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* $Header: /home/daffy/u0/vern/flex/RCS/yylex.c,v 2.10 93/09/16 20:31:48 vern Exp $ */

#include <ctype.h>
#include "flexdef.h"
#include "parse.h"


/* yylex - scan for a regular expression token */

int yylex()
	{
	int toktype;
	static int beglin = false;

	if ( eofseen )
		toktype = EOF;
	else
		toktype = flexscan();

	if ( toktype == EOF || toktype == 0 )
		{
		eofseen = 1;

		if ( sectnum == 1 )
			{
			synerr( "premature EOF" );
			sectnum = 2;
			toktype = SECTEND;
			}

		else
			toktype = 0;
		}

	if ( trace )
		{
		if ( beglin )
			{
			fprintf( stderr, "%d\t", num_rules + 1 );
			beglin = 0;
			}

		switch ( toktype )
			{
			case '<':
			case '>':
			case '^':
			case '$':
			case '"':
			case '[':
			case ']':
			case '{':
			case '}':
			case '|':
			case '(':
			case ')':
			case '-':
			case '/':
			case '\\':
			case '?':
			case '.':
			case '*':
			case '+':
			case ',':
				(void) putc( toktype, stderr );
				break;

			case '\n':
				(void) putc( '\n', stderr );

				if ( sectnum == 2 )
				beglin = 1;

				break;

			case SCDECL:
				fputs( "%s", stderr );
				break;

			case XSCDECL:
				fputs( "%x", stderr );
				break;

			case WHITESPACE:
				(void) putc( ' ', stderr );
				break;

			case SECTEND:
				fputs( "%%\n", stderr );

				/* We set beglin to be true so we'll start
				 * writing out numbers as we echo rules.
				 * flexscan() has already assigned sectnum.
				 */

				if ( sectnum == 2 )
				beglin = 1;

				break;

			case NAME:
				fprintf( stderr, "'%s'", nmstr );
				break;

			case CHAR:
				switch ( yylval )
					{
					case '<':
					case '>':
					case '^':
					case '$':
					case '"':
					case '[':
					case ']':
					case '{':
					case '}':
					case '|':
					case '(':
					case ')':
					case '-':
					case '/':
					case '\\':
					case '?':
					case '.':
					case '*':
					case '+':
					case ',':
						fprintf( stderr, "\\%c",
							yylval );
						break;

					default:
						if ( ! isascii( yylval ) ||
						     ! isprint( yylval ) )
							fprintf( stderr,
								"\\%.3o",
							(unsigned int) yylval );
						else
							(void) putc( yylval,
								stderr );
					break;
					}

				break;

			case NUMBER:
				fprintf( stderr, "%d", yylval );
				break;

			case PREVCCL:
				fprintf( stderr, "[%d]", yylval );
				break;

			case EOF_OP:
				fprintf( stderr, "<<EOF>>" );
				break;

			case 0:
				fprintf( stderr, "End Marker" );
				break;

			default:
				fprintf( stderr,
					"*Something Weird* - tok: %d val: %d\n",
					toktype, yylval );
				break;
			}
		}

	return toktype;
	}
