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

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /a/cvs/386BSD/src/usr.bin/lex/misc.c,v 1.2 1993/06/29 03:27:16 nate Exp $ (LBL)";
#endif

#include <ctype.h>
#include "flexdef.h"


/* ANSI C does not guarantee that isascii() is defined */
#ifndef isascii
#define isascii(c) ((c) <= 0177)
#endif



/* declare functions that have forward references */

void dataflush PROTO(());
int otoi PROTO((Char []));


/* action_out - write the actions from the temporary file to lex.yy.c
 *
 * synopsis
 *     action_out();
 *
 *     Copies the action file up to %% (or end-of-file) to lex.yy.c
 */

void action_out()

    {
    char buf[MAXLINE];

    while ( fgets( buf, MAXLINE, temp_action_file ) != NULL )
	if ( buf[0] == '%' && buf[1] == '%' )
	    break;
	else
	    fputs( buf, stdout );
    }


/* allocate_array - allocate memory for an integer array of the given size */

void *allocate_array( size, element_size )
int size, element_size;

    {
    register void *mem;

    /* on 16-bit int machines (e.g., 80286) we might be trying to
     * allocate more than a signed int can hold, and that won't
     * work.  Cheap test:
     */
    if ( element_size * size <= 0 )
        flexfatal( "request for < 1 byte in allocate_array()" );

    mem = (void *) malloc( (unsigned) (element_size * size) );

    if ( mem == NULL )
	flexfatal( "memory allocation failed in allocate_array()" );

    return ( mem );
    }


/* all_lower - true if a string is all lower-case
 *
 * synopsis:
 *    Char *str;
 *    int all_lower();
 *    true/false = all_lower( str );
 */

int all_lower( str )
register Char *str;

    {
    while ( *str )
	{
	if ( ! isascii( *str ) || ! islower( *str ) )
	    return ( 0 );
	++str;
	}

    return ( 1 );
    }


/* all_upper - true if a string is all upper-case
 *
 * synopsis:
 *    Char *str;
 *    int all_upper();
 *    true/false = all_upper( str );
 */

int all_upper( str )
register Char *str;

    {
    while ( *str )
	{
	if ( ! isascii( *str ) || ! isupper( (char) *str ) )
	    return ( 0 );
	++str;
	}

    return ( 1 );
    }


/* bubble - bubble sort an integer array in increasing order
 *
 * synopsis
 *   int v[n], n;
 *   bubble( v, n );
 *
 * description
 *   sorts the first n elements of array v and replaces them in
 *   increasing order.
 *
 * passed
 *   v - the array to be sorted
 *   n - the number of elements of 'v' to be sorted */

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


/* clower - replace upper-case letter to lower-case
 *
 * synopsis:
 *    Char clower();
 *    int c;
 *    c = clower( c );
 */

Char clower( c )
register int c;

    {
    return ( (isascii( c ) && isupper( c )) ? tolower( c ) : c );
    }


/* copy_string - returns a dynamically allocated copy of a string
 *
 * synopsis
 *    char *str, *copy, *copy_string();
 *    copy = copy_string( str );
 */

char *copy_string( str )
register char *str;

    {
    register char *c;
    char *copy;

    /* find length */
    for ( c = str; *c; ++c )
	;

    copy = malloc( (unsigned) ((c - str + 1) * sizeof( char )) );

    if ( copy == NULL )
	flexfatal( "dynamic memory failure in copy_string()" );

    for ( c = copy; (*c++ = *str++); )
	;

    return ( copy );
    }


/* copy_unsigned_string -
 *    returns a dynamically allocated copy of a (potentially) unsigned string
 *
 * synopsis
 *    Char *str, *copy, *copy_unsigned_string();
 *    copy = copy_unsigned_string( str );
 */

Char *copy_unsigned_string( str )
register Char *str;

    {
    register Char *c;
    Char *copy;

    /* find length */
    for ( c = str; *c; ++c )
	;

    copy = (Char *) malloc( (unsigned) ((c - str + 1) * sizeof( Char )) );

    if ( copy == NULL )
	flexfatal( "dynamic memory failure in copy_unsigned_string()" );

    for ( c = copy; (*c++ = *str++); )
	;

    return ( copy );
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
 *   does a shell sort of the first n elements of array v.
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


/* dataend - finish up a block of data declarations
 *
 * synopsis
 *    dataend();
 */

void dataend()

    {
    if ( datapos > 0 )
	dataflush();

    /* add terminator for initialization */
    puts( "    } ;\n" );

    dataline = 0;
    datapos = 0;
    }



/* dataflush - flush generated data statements
 *
 * synopsis
 *    dataflush();
 */

void dataflush()

    {
    putchar( '\n' );

    if ( ++dataline >= NUMDATALINES )
	{
	/* put out a blank line so that the table is grouped into
	 * large blocks that enable the user to find elements easily
	 */
	putchar( '\n' );
	dataline = 0;
	}

    /* reset the number of characters written on the current line */
    datapos = 0;
    }


/* flexerror - report an error message and terminate
 *
 * synopsis
 *    char msg[];
 *    flexerror( msg );
 */

void flexerror( msg )
char msg[];

    {
    fprintf( stderr, "%s: %s\n", program_name, msg );

    flexend( 1 );
    }


/* flexfatal - report a fatal error message and terminate
 *
 * synopsis
 *    char msg[];
 *    flexfatal( msg );
 */

void flexfatal( msg )
char msg[];

    {
    fprintf( stderr, "%s: fatal internal error, %s\n", program_name, msg );
    exit( 1 );
    }


/* flex_gettime - return current time
 *
 * synopsis
 *    char *flex_gettime(), *time_str;
 *    time_str = flex_gettime();
 *
 * note
 *    the routine name has the "flex_" prefix because of name clashes
 *    with Turbo-C
 */

/* include sys/types.h to use time_t and make lint happy */

#ifndef MS_DOS
#ifndef VMS
#include <sys/types.h>
#else
#include <types.h>
#endif
#endif

#ifdef MS_DOS
#include <time.h>
typedef long time_t;
#endif

char *flex_gettime()

    {
    time_t t, time();
    char *result, *ctime(), *copy_string();

    t = time( (long *) 0 );

    result = copy_string( ctime( &t ) );

    /* get rid of trailing newline */
    result[24] = '\0';

    return ( result );
    }


/* lerrif - report an error message formatted with one integer argument
 *
 * synopsis
 *    char msg[];
 *    int arg;
 *    lerrif( msg, arg );
 */

void lerrif( msg, arg )
char msg[];
int arg;

    {
    char errmsg[MAXLINE];
    (void) sprintf( errmsg, msg, arg );
    flexerror( errmsg );
    }


/* lerrsf - report an error message formatted with one string argument
 *
 * synopsis
 *    char msg[], arg[];
 *    lerrsf( msg, arg );
 */

void lerrsf( msg, arg )
char msg[], arg[];

    {
    char errmsg[MAXLINE];

    (void) sprintf( errmsg, msg, arg );
    flexerror( errmsg );
    }


/* htoi - convert a hexadecimal digit string to an integer value
 *
 * synopsis:
 *    int val, htoi();
 *    Char str[];
 *    val = htoi( str );
 */

int htoi( str )
Char str[];

    {
    int result;

    (void) sscanf( (char *) str, "%x", &result );

    return ( result );
    }


/* is_hex_digit - returns true if a character is a valid hex digit, false
 *		  otherwise
 *
 * synopsis:
 *    int true_or_false, is_hex_digit();
 *    int ch;
 *    val = is_hex_digit( ch );
 */

int is_hex_digit( ch )
int ch;

    {
    if ( isdigit( ch ) )
	return ( 1 );

    switch ( clower( ch ) )
	{
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	    return ( 1 );

	default:
	    return ( 0 );
	}
    }


/* line_directive_out - spit out a "# line" statement */

void line_directive_out( output_file_name )
FILE *output_file_name;

    {
    if ( infilename && gen_line_dirs )
        fprintf( output_file_name, "# line %d \"%s\"\n", linenum, infilename );
    }


/* mk2data - generate a data statement for a two-dimensional array
 *
 * synopsis
 *    int value;
 *    mk2data( value );
 *
 *  generates a data statement initializing the current 2-D array to "value"
 */
void mk2data( value )
int value;

    {
    if ( datapos >= NUMDATAITEMS )
	{
	putchar( ',' );
	dataflush();
	}

    if ( datapos == 0 )
	/* indent */
	fputs( "    ", stdout );

    else
	putchar( ',' );

    ++datapos;

    printf( "%5d", value );
    }


/* mkdata - generate a data statement
 *
 * synopsis
 *    int value;
 *    mkdata( value );
 *
 *  generates a data statement initializing the current array element to
 *  "value"
 */
void mkdata( value )
int value;

    {
    if ( datapos >= NUMDATAITEMS )
	{
	putchar( ',' );
	dataflush();
	}

    if ( datapos == 0 )
	/* indent */
	fputs( "    ", stdout );

    else
	putchar( ',' );

    ++datapos;

    printf( "%5d", value );
    }


/* myctoi - return the integer represented by a string of digits
 *
 * synopsis
 *    Char array[];
 *    int val, myctoi();
 *    val = myctoi( array );
 *
 */

int myctoi( array )
Char array[];

    {
    int val = 0;

    (void) sscanf( (char *) array, "%d", &val );

    return ( val );
    }


/* myesc - return character corresponding to escape sequence
 *
 * synopsis
 *    Char array[], c, myesc();
 *    c = myesc( array );
 *
 */

Char myesc( array )
Char array[];

    {
    Char c, esc_char;
    register int sptr;

    switch ( array[1] )
	{
#ifdef __STDC__
	case 'a': return ( '\a' );
#endif
	case 'b': return ( '\b' );
	case 'f': return ( '\f' );
	case 'n': return ( '\n' );
	case 'r': return ( '\r' );
	case 't': return ( '\t' );
	case 'v': return ( '\v' );

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    { /* \<octal> */
	    sptr = 1;

	    while ( isascii( array[sptr] ) && isdigit( array[sptr] ) )
		/* don't increment inside loop control because if
		 * isdigit() is a macro it might expand into multiple
		 * increments ...
		 */
		++sptr;

	    c = array[sptr];
	    array[sptr] = '\0';

	    esc_char = otoi( array + 1 );

	    array[sptr] = c;

	    return ( esc_char );
	    }

	case 'x':
	    { /* \x<hex> */
	    int sptr = 2;

	    while ( isascii( array[sptr] ) && is_hex_digit( array[sptr] ) )
		/* don't increment inside loop control because if
		 * isdigit() is a macro it might expand into multiple
		 * increments ...
		 */
		++sptr;

	    c = array[sptr];
	    array[sptr] = '\0';

	    esc_char = htoi( array + 2 );

	    array[sptr] = c;

	    return ( esc_char );
	    }

	default:
	    return ( array[1] );
	}
    }


/* otoi - convert an octal digit string to an integer value
 *
 * synopsis:
 *    int val, otoi();
 *    Char str[];
 *    val = otoi( str );
 */

int otoi( str )
Char str[];

    {
    int result;

    (void) sscanf( (char *) str, "%o", &result );

    return ( result );
    }


/* readable_form - return the the human-readable form of a character
 *
 * synopsis:
 *    int c;
 *    char *readable_form();
 *    <string> = readable_form( c );
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
	    case '\n': return ( "\\n" );
	    case '\t': return ( "\\t" );
	    case '\f': return ( "\\f" );
	    case '\r': return ( "\\r" );
	    case '\b': return ( "\\b" );

	    default:
		(void) sprintf( rform, "\\%.3o", c );
		return ( rform );
	    }
	}

    else if ( c == ' ' )
	return ( "' '" );

    else
	{
	rform[0] = c;
	rform[1] = '\0';

	return ( rform );
	}
    }


/* reallocate_array - increase the size of a dynamic array */

void *reallocate_array( array, size, element_size )
void *array;
int size, element_size;

    {
    register void *new_array;

    /* same worry as in allocate_array(): */
    if ( size * element_size <= 0 )
        flexfatal( "attempt to increase array size by less than 1 byte" );

    new_array =
	(void *) realloc( (char *)array, (unsigned) (size * element_size ));

    if ( new_array == NULL )
	flexfatal( "attempt to increase array size failed" );

    return ( new_array );
    }


/* skelout - write out one section of the skeleton file
 *
 * synopsis
 *    skelout();
 *
 * DESCRIPTION
 *    Copies from skelfile to stdout until a line beginning with "%%" or
 *    EOF is found.
 */
void skelout()

    {
    char buf[MAXLINE];

    while ( fgets( buf, MAXLINE, skelfile ) != NULL )
	if ( buf[0] == '%' && buf[1] == '%' )
	    break;
	else
	    fputs( buf, stdout );
    }


/* transition_struct_out - output a yy_trans_info structure
 *
 * synopsis
 *     int element_v, element_n;
 *     transition_struct_out( element_v, element_n );
 *
 * outputs the yy_trans_info structure with the two elements, element_v and
 * element_n.  Formats the output with spaces and carriage returns.
 */

void transition_struct_out( element_v, element_n )
int element_v, element_n;

    {
    printf( "%7d, %5d,", element_v, element_n );

    datapos += TRANS_STRUCT_PRINT_LENGTH;

    if ( datapos >= 75 )
	{
	putchar( '\n' );

	if ( ++dataline % 10 == 0 )
	    putchar( '\n' );

	datapos = 0;
	}
    }
