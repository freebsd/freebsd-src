/* misc - miscellaneous flex routines */

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

/* $Header: /home/ncvs/src/usr.bin/lex/misc.c,v 1.1.1.2 1996/06/19 20:26:19 nate Exp $ */

#include "flexdef.h"


void action_define( defname, value )
char *defname;
int value;
	{
	char buf[MAXLINE];

	if ( (int) strlen( defname ) > MAXLINE / 2 )
		{
		format_pinpoint_message( _( "name \"%s\" ridiculously long" ), 
			defname );
		return;
		}

	sprintf( buf, "#define %s %d\n", defname, value );
	add_action( buf );
	}


void add_action( new_text )
char *new_text;
	{
	int len = strlen( new_text );

	while ( len + action_index >= action_size - 10 /* slop */ )
		{
		int new_size = action_size * 2;

		if ( new_size <= 0 )
			/* Increase just a little, to try to avoid overflow
			 * on 16-bit machines.
			 */
			action_size += action_size / 8;
		else
			action_size = new_size;

		action_array =
			reallocate_character_array( action_array, action_size );
		}

	strcpy( &action_array[action_index], new_text );

	action_index += len;
	}


/* allocate_array - allocate memory for an integer array of the given size */

void *allocate_array( size, element_size )
int size;
size_t element_size;
	{
	register void *mem;
	size_t num_bytes = element_size * size;

	mem = flex_alloc( num_bytes );
	if ( ! mem )
		flexfatal(
			_( "memory allocation failed in allocate_array()" ) );

	return mem;
	}


/* all_lower - true if a string is all lower-case */

int all_lower( str )
register char *str;
	{
	while ( *str )
		{
		if ( ! isascii( (Char) *str ) || ! islower( *str ) )
			return 0;
		++str;
		}

	return 1;
	}


/* all_upper - true if a string is all upper-case */

int all_upper( str )
register char *str;
	{
	while ( *str )
		{
		if ( ! isascii( (Char) *str ) || ! isupper( *str ) )
			return 0;
		++str;
		}

	return 1;
	}


/* bubble - bubble sort an integer array in increasing order
 *
 * synopsis
 *   int v[n], n;
 *   void bubble( v, n );
 *
 * description
 *   sorts the first n elements of array v and replaces them in
 *   increasing order.
 *
 * passed
 *   v - the array to be sorted
 *   n - the number of elements of 'v' to be sorted
 */

void bubble( v, n )
int v[], n;
	{
	register int i, j, k;

	for ( i = n; i > 1; --i )
		for ( j = 1; j < i; ++j )
			if ( v[j] > v[j + 1] )	/* compare */
				{
				k = v[j];	/* exchange */
				v[j] = v[j + 1];
				v[j + 1] = k;
				}
	}


/* check_char - checks a character to make sure it's within the range
 *		we're expecting.  If not, generates fatal error message
 *		and exits.
 */

void check_char( c )
int c;
	{
	if ( c >= CSIZE )
		lerrsf( _( "bad character '%s' detected in check_char()" ),
			readable_form( c ) );

	if ( c >= csize )
		lerrsf(
		_( "scanner requires -8 flag to use the character %s" ),
			readable_form( c ) );
	}



/* clower - replace upper-case letter to lower-case */

Char clower( c )
register int c;
	{
	return (Char) ((isascii( c ) && isupper( c )) ? tolower( c ) : c);
	}


/* copy_string - returns a dynamically allocated copy of a string */

char *copy_string( str )
register const char *str;
	{
	register const char *c1;
	register char *c2;
	char *copy;
	unsigned int size;

	/* find length */
	for ( c1 = str; *c1; ++c1 )
		;

	size = (c1 - str + 1) * sizeof( char );
	copy = (char *) flex_alloc( size );

	if ( copy == NULL )
		flexfatal( _( "dynamic memory failure in copy_string()" ) );

	for ( c2 = copy; (*c2++ = *str++) != 0; )
		;

	return copy;
	}


/* copy_unsigned_string -
 *    returns a dynamically allocated copy of a (potentially) unsigned string
 */

Char *copy_unsigned_string( str )
register Char *str;
	{
	register Char *c;
	Char *copy;

	/* find length */
	for ( c = str; *c; ++c )
		;

	copy = allocate_Character_array( c - str + 1 );

	for ( c = copy; (*c++ = *str++) != 0; )
		;

	return copy;
	}


/* cshell - shell sort a character array in increasing order
 *
 * synopsis
 *
 *   Char v[n];
 *   int n, special_case_0;
 *   cshell( v, n, special_case_0 );
 *
 * description
 *   Does a shell sort of the first n elements of array v.
 *   If special_case_0 is true, then any element equal to 0
 *   is instead assumed to have infinite weight.
 *
 * passed
 *   v - array to be sorted
 *   n - number of elements of v to be sorted
 */

void cshell( v, n, special_case_0 )
Char v[];
int n, special_case_0;
	{
	int gap, i, j, jg;
	Char k;

	for ( gap = n / 2; gap > 0; gap = gap / 2 )
		for ( i = gap; i < n; ++i )
			for ( j = i - gap; j >= 0; j = j - gap )
				{
				jg = j + gap;

				if ( special_case_0 )
					{
					if ( v[jg] == 0 )
						break;

					else if ( v[j] != 0 && v[j] <= v[jg] )
						break;
					}

				else if ( v[j] <= v[jg] )
					break;

				k = v[j];
				v[j] = v[jg];
				v[jg] = k;
				}
	}


/* dataend - finish up a block of data declarations */

void dataend()
	{
	if ( datapos > 0 )
		dataflush();

	/* add terminator for initialization; { for vi */
	outn( "    } ;\n" );

	dataline = 0;
	datapos = 0;
	}


/* dataflush - flush generated data statements */

void dataflush()
	{
	outc( '\n' );

	if ( ++dataline >= NUMDATALINES )
		{
		/* Put out a blank line so that the table is grouped into
		 * large blocks that enable the user to find elements easily.
		 */
		outc( '\n' );
		dataline = 0;
		}

	/* Reset the number of characters written on the current line. */
	datapos = 0;
	}


/* flexerror - report an error message and terminate */

void flexerror( msg )
const char msg[];
	{
	fprintf( stderr, "%s: %s\n", program_name, msg );
	flexend( 1 );
	}


/* flexfatal - report a fatal error message and terminate */

void flexfatal( msg )
const char msg[];
	{
	fprintf( stderr, _( "%s: fatal internal error, %s\n" ),
		program_name, msg );
	exit( 1 );
	}


/* htoi - convert a hexadecimal digit string to an integer value */

int htoi( str )
Char str[];
	{
	unsigned int result;

	(void) sscanf( (char *) str, "%x", &result );

	return result;
	}


/* lerrif - report an error message formatted with one integer argument */

void lerrif( msg, arg )
const char msg[];
int arg;
	{
	char errmsg[MAXLINE];
	(void) sprintf( errmsg, msg, arg );
	flexerror( errmsg );
	}


/* lerrsf - report an error message formatted with one string argument */

void lerrsf( msg, arg )
const char msg[], arg[];
	{
	char errmsg[MAXLINE];

	(void) sprintf( errmsg, msg, arg );
	flexerror( errmsg );
	}


/* line_directive_out - spit out a "#line" statement */

void line_directive_out( output_file, do_infile )
FILE *output_file;
int do_infile;
	{
	char directive[MAXLINE], filename[MAXLINE];
	char *s1, *s2, *s3;
	static char line_fmt[] = "#line %d \"%s\"\n";

	if ( ! gen_line_dirs )
		return;

	if ( (do_infile && ! infilename) || (! do_infile && ! outfilename) )
		/* don't know the filename to use, skip */
		return;

	s1 = do_infile ? infilename : outfilename;
	s2 = filename;
	s3 = &filename[sizeof( filename ) - 2];

	while ( s2 < s3 && *s1 )
		{
		if ( *s1 == '\\' )
			/* Escape the '\' */
			*s2++ = '\\';

		*s2++ = *s1++;
		}

	*s2 = '\0';

	if ( do_infile )
		sprintf( directive, line_fmt, linenum, filename );
	else
		{
		if ( output_file == stdout )
			/* Account for the line directive itself. */
			++out_linenum;

		sprintf( directive, line_fmt, out_linenum, filename );
		}

	/* If output_file is nil then we should put the directive in
	 * the accumulated actions.
	 */
	if ( output_file )
		{
		fputs( directive, output_file );
		}
	else
		add_action( directive );
	}


/* mark_defs1 - mark the current position in the action array as
 *               representing where the user's section 1 definitions end
 *		 and the prolog begins
 */
void mark_defs1()
	{
	defs1_offset = 0;
	action_array[action_index++] = '\0';
	action_offset = prolog_offset = action_index;
	action_array[action_index] = '\0';
	}


/* mark_prolog - mark the current position in the action array as
 *               representing the end of the action prolog
 */
void mark_prolog()
	{
	action_array[action_index++] = '\0';
	action_offset = action_index;
	action_array[action_index] = '\0';
	}


/* mk2data - generate a data statement for a two-dimensional array
 *
 * Generates a data statement initializing the current 2-D array to "value".
 */
void mk2data( value )
int value;
	{
	if ( datapos >= NUMDATAITEMS )
		{
		outc( ',' );
		dataflush();
		}

	if ( datapos == 0 )
		/* Indent. */
		out( "    " );

	else
		outc( ',' );

	++datapos;

	out_dec( "%5d", value );
	}


/* mkdata - generate a data statement
 *
 * Generates a data statement initializing the current array element to
 * "value".
 */
void mkdata( value )
int value;
	{
	if ( datapos >= NUMDATAITEMS )
		{
		outc( ',' );
		dataflush();
		}

	if ( datapos == 0 )
		/* Indent. */
		out( "    " );
	else
		outc( ',' );

	++datapos;

	out_dec( "%5d", value );
	}


/* myctoi - return the integer represented by a string of digits */

int myctoi( array )
char array[];
	{
	int val = 0;

	(void) sscanf( array, "%d", &val );

	return val;
	}


/* myesc - return character corresponding to escape sequence */

Char myesc( array )
Char array[];
	{
	Char c, esc_char;

	switch ( array[1] )
		{
		case 'b': return '\b';
		case 'f': return '\f';
		case 'n': return '\n';
		case 'r': return '\r';
		case 't': return '\t';

#if __STDC__
		case 'a': return '\a';
		case 'v': return '\v';
#else
		case 'a': return '\007';
		case 'v': return '\013';
#endif

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			{ /* \<octal> */
			int sptr = 1;

			while ( isascii( array[sptr] ) &&
				isdigit( array[sptr] ) )
				/* Don't increment inside loop control
				 * because if isdigit() is a macro it might
				 * expand into multiple increments ...
				 */
				++sptr;

			c = array[sptr];
			array[sptr] = '\0';

			esc_char = otoi( array + 1 );

			array[sptr] = c;

			return esc_char;
			}

		case 'x':
			{ /* \x<hex> */
			int sptr = 2;

			while ( isascii( array[sptr] ) &&
				isxdigit( (char) array[sptr] ) )
				/* Don't increment inside loop control
				 * because if isdigit() is a macro it might
				 * expand into multiple increments ...
				 */
				++sptr;

			c = array[sptr];
			array[sptr] = '\0';

			esc_char = htoi( array + 2 );

			array[sptr] = c;

			return esc_char;
			}

		default:
			return array[1];
		}
	}


/* otoi - convert an octal digit string to an integer value */

int otoi( str )
Char str[];
	{
	unsigned int result;

	(void) sscanf( (char *) str, "%o", &result );
	return result;
	}


/* out - various flavors of outputing a (possibly formatted) string for the
 *	 generated scanner, keeping track of the line count.
 */

void out( str )
const char str[];
	{
	fputs( str, stdout );
	out_line_count( str );
	}

void out_dec( fmt, n )
const char fmt[];
int n;
	{
	printf( fmt, n );
	out_line_count( fmt );
	}

void out_dec2( fmt, n1, n2 )
const char fmt[];
int n1, n2;
	{
	printf( fmt, n1, n2 );
	out_line_count( fmt );
	}

void out_hex( fmt, x )
const char fmt[];
unsigned int x;
	{
	printf( fmt, x );
	out_line_count( fmt );
	}

void out_line_count( str )
const char str[];
	{
	register int i;

	for ( i = 0; str[i]; ++i )
		if ( str[i] == '\n' )
			++out_linenum;
	}

void out_str( fmt, str )
const char fmt[], str[];
	{
	printf( fmt, str );
	out_line_count( fmt );
	out_line_count( str );
	}

void out_str3( fmt, s1, s2, s3 )
const char fmt[], s1[], s2[], s3[];
	{
	printf( fmt, s1, s2, s3 );
	out_line_count( fmt );
	out_line_count( s1 );
	out_line_count( s2 );
	out_line_count( s3 );
	}

void out_str_dec( fmt, str, n )
const char fmt[], str[];
int n;
	{
	printf( fmt, str, n );
	out_line_count( fmt );
	out_line_count( str );
	}

void outc( c )
int c;
	{
	putc( c, stdout );

	if ( c == '\n' )
		++out_linenum;
	}

void outn( str )
const char str[];
	{
	puts( str );
	out_line_count( str );
	++out_linenum;
	}


/* readable_form - return the the human-readable form of a character
 *
 * The returned string is in static storage.
 */

char *readable_form( c )
register int c;
	{
	static char rform[10];

	if ( (c >= 0 && c < 32) || c >= 127 )
		{
		switch ( c )
			{
			case '\b': return "\\b";
			case '\f': return "\\f";
			case '\n': return "\\n";
			case '\r': return "\\r";
			case '\t': return "\\t";

#if __STDC__
			case '\a': return "\\a";
			case '\v': return "\\v";
#endif

			default:
				(void) sprintf( rform, "\\%.3o",
						(unsigned int) c );
				return rform;
			}
		}

	else if ( c == ' ' )
		return "' '";

	else
		{
		rform[0] = c;
		rform[1] = '\0';

		return rform;
		}
	}


/* reallocate_array - increase the size of a dynamic array */

void *reallocate_array( array, size, element_size )
void *array;
int size;
size_t element_size;
	{
	register void *new_array;
	size_t num_bytes = element_size * size;

	new_array = flex_realloc( array, num_bytes );
	if ( ! new_array )
		flexfatal( _( "attempt to increase array size failed" ) );

	return new_array;
	}


/* skelout - write out one section of the skeleton file
 *
 * Description
 *    Copies skelfile or skel array to stdout until a line beginning with
 *    "%%" or EOF is found.
 */
void skelout()
	{
	char buf_storage[MAXLINE];
	char *buf = buf_storage;
	int do_copy = 1;

	/* Loop pulling lines either from the skelfile, if we're using
	 * one, or from the skel[] array.
	 */
	while ( skelfile ?
		(fgets( buf, MAXLINE, skelfile ) != NULL) :
		((buf = (char *) skel[skel_ind++]) != 0) )
		{ /* copy from skel array */
		if ( buf[0] == '%' )
			{ /* control line */
			switch ( buf[1] )
				{
				case '%':
					return;

				case '+':
					do_copy = C_plus_plus;
					break;

				case '-':
					do_copy = ! C_plus_plus;
					break;

				case '*':
					do_copy = 1;
					break;

				default:
					flexfatal(
					_( "bad line in skeleton file" ) );
				}
			}

		else if ( do_copy )
			{
			if ( skelfile )
				/* Skeleton file reads include final
				 * newline, skel[] array does not.
				 */
				out( buf );
			else
				outn( buf );
			}
		}
	}


/* transition_struct_out - output a yy_trans_info structure
 *
 * outputs the yy_trans_info structure with the two elements, element_v and
 * element_n.  Formats the output with spaces and carriage returns.
 */

void transition_struct_out( element_v, element_n )
int element_v, element_n;
	{
	out_dec2( " {%4d,%4d },", element_v, element_n );

	datapos += TRANS_STRUCT_PRINT_LENGTH;

	if ( datapos >= 79 - TRANS_STRUCT_PRINT_LENGTH )
		{
		outc( '\n' );

		if ( ++dataline % 10 == 0 )
			outc( '\n' );

		datapos = 0;
		}
	}


/* The following is only needed when building flex's parser using certain
 * broken versions of bison.
 */
void *yy_flex_xmalloc( size )
int size;
	{
	void *result = flex_alloc( (size_t) size );

	if ( ! result  )
		flexfatal(
			_( "memory allocation failed in yy_flex_xmalloc()" ) );

	return result;
	}


/* zero_out - set a region of memory to 0
 *
 * Sets region_ptr[0] through region_ptr[size_in_bytes - 1] to zero.
 */

void zero_out( region_ptr, size_in_bytes )
char *region_ptr;
size_t size_in_bytes;
	{
	register char *rp, *rp_end;

	rp = region_ptr;
	rp_end = region_ptr + size_in_bytes;

	while ( rp < rp_end )
		*rp++ = 0;
	}
