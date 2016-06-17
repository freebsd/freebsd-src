/*
 * tkparse.c
 *
 * Eric Youngdale was the original author of xconfig.
 * Michael Elizabeth Chastain (mec@shout.net) is the current maintainer.
 *
 * Parse a config.in file and translate it to a wish script.
 * This task has three parts:
 *
 *   tkparse.c	tokenize the input
 *   tkcond.c   transform 'if ...' statements
 *   tkgen.c    generate output
 *
 * Change History
 *
 * 7 January 1999, Michael Elizabeth Chastain, <mec@shout.net>
 * - Teach dep_tristate about a few literals, such as:
 *     dep_tristate 'foo' CONFIG_FOO m
 *   Also have it print an error message and exit on some parse failures.
 *
 * 14 January 1999, Michael Elizabeth Chastain, <mec@shout.net>
 * - Don't fclose stdin.  Thanks to Tony Hoyle for nailing this one.
 *
 * 14 January 1999, Michael Elizabeth Chastain, <mec@shout.net>
 * - Steam-clean this file.  I tested this by generating kconfig.tk for
 *   every architecture and comparing it character-for-character against
 *   the output of the old tkparse.
 *
 * 23 January 1999, Michael Elizabeth Chastain, <mec@shout.net>
 * - Remove bug-compatible code.
 *
 * 07 July 1999, Andrzej M. Krzysztofowicz, <ankry@mif.pg.gda.pl>
 * - Submenus implemented,
 * - plenty of option updating/displaying fixes,
 * - dep_bool, define_hex, define_int, define_string, define_tristate and
 *   undef implemented,
 * - dep_tristate fixed to support multiple dependencies,
 * - handling of variables with an empty value implemented,
 * - value checking for int and hex fields,
 * - more checking during condition parsing; choice variables are treated as
 *   all others now,
 *
 * TO DO:
 * - xconfig is at the end of its life cycle.  Contact <mec@shout.net> if
 *   you are interested in working on the replacement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tkparse.h"

static struct kconfig * config_list = NULL;
static struct kconfig * config_last = NULL;
static const char * current_file = "<unknown file>";
static int lineno = 0;

static void do_source( const char * );

#undef strcmp
int my_strcmp( const char * s1, const char * s2 ) { return strcmp( s1, s2 ); }
#define strcmp my_strcmp

/*
 * Report a syntax error.
 */
static void syntax_error( const char * msg )
{
    fprintf( stderr, "%s: %d: %s\n", current_file, lineno, msg );
    exit( 1 );
}



/*
 * Find index of a specific variable in the symbol table.
 * Create a new entry if it does not exist yet.
 */
struct variable *vartable;
int max_varnum = 0;
static int vartable_size = 0;

int get_varnum( char * name )
{
    int i;
    
    for ( i = 1; i <= max_varnum; i++ )
	if ( strcmp( vartable[i].name, name ) == 0 )
	    return i;
    while (max_varnum+1 >= vartable_size) {
	vartable = realloc(vartable, (vartable_size += 1000)*sizeof(*vartable));
	if (!vartable) {
	    fprintf(stderr, "tkparse realloc vartable failed\n");
	    exit(1);
	}
    }
    vartable[++max_varnum].name = malloc( strlen( name )+1 );
    strcpy( vartable[max_varnum].name, name );
    return max_varnum;
}



/*
 * Get a string.
 */
static const char * get_string( const char * pnt, char ** label )
{
    const char * word;

    word = pnt;
    for ( ; ; )
    {
	if ( *pnt == '\0' || *pnt == ' ' || *pnt == '\t' )
	    break;
	pnt++;
    }

    *label = malloc( pnt - word + 1 );
    memcpy( *label, word, pnt - word );
    (*label)[pnt - word] = '\0';

    if ( *pnt != '\0' )
	pnt++;
    return pnt;
}



/*
 * Get a quoted string.
 * Insert a '\' before any characters that need quoting.
 */
static const char * get_qstring( const char * pnt, char ** label )
{
    char quote_char;
    char newlabel [2048];
    char * pnt1;

    /* advance to the open quote */
    for ( ; ; )
    {
	if ( *pnt == '\0' )
	    return pnt;
	quote_char = *pnt++;
	if ( quote_char == '"' || quote_char == '\'' )
	    break;
    }

    /* copy into an intermediate buffer */
    pnt1 = newlabel;
    for ( ; ; )
    {
	if ( *pnt == '\0' )
	    syntax_error( "unterminated quoted string" );
	if ( *pnt == quote_char && pnt[-1] != '\\' )
	    break;

	/* copy the character, quoting if needed */
	if ( *pnt == '"' || *pnt == '\'' || *pnt == '[' || *pnt == ']' )
	    *pnt1++ = '\\';
	*pnt1++ = *pnt++;
    }

    /* copy the label into a permanent location */
    *pnt1++ = '\0';
    *label = (char *) malloc( pnt1 - newlabel );
    memcpy( *label, newlabel, pnt1 - newlabel );

    /* skip over last quote and next whitespace */
    pnt++;
    while ( *pnt == ' ' || *pnt == '\t' )
	pnt++;
    return pnt;
}



/*
 * Get a quoted or unquoted string. It is recognized by the first 
 * non-white character. '"' and '"' are not allowed inside the string.
 */
static const char * get_qnqstring( const char * pnt, char ** label )
{
    char quote_char;

    while ( *pnt == ' ' || *pnt == '\t' )
	pnt++;

    if ( *pnt == '\0' )
	return pnt;
    quote_char = *pnt;
    if ( quote_char == '"' || quote_char == '\'' )
	return get_qstring( pnt, label );
    else
	return get_string( pnt, label );
}



/*
 * Tokenize an 'if' statement condition.
 */
static struct condition * tokenize_if( const char * pnt )
{
    struct condition * list;
    struct condition * last;
    struct condition * prev;

    /* eat the open bracket */
    while ( *pnt == ' ' || *pnt == '\t' )
	pnt++;
    if ( *pnt != '[' )
	syntax_error( "bad 'if' condition" );
    pnt++;

    list = last = NULL;
    for ( ; ; )
    {
	struct condition * cond;

	/* advance to the next token */
	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;
	if ( *pnt == '\0' )
	    syntax_error( "unterminated 'if' condition" );
	if ( *pnt == ']' )
	    return list;

	/* allocate a new token */
	cond = malloc( sizeof(*cond) );
	memset( cond, 0, sizeof(*cond) );
	if ( last == NULL )
	    { list = last = cond; prev = NULL; }
	else
	    { prev = last; last->next = cond; last = cond; }

	/* determine the token value */
	if ( *pnt == '-' && pnt[1] == 'a' )
	{
	    if ( ! prev || ( prev->op != op_variable && prev->op != op_constant ) )
		syntax_error( "incorrect argument" );
	    cond->op = op_and;  pnt += 2; continue;
	}

	if ( *pnt == '-' && pnt[1] == 'o' )
	{
	    if ( ! prev || ( prev->op != op_variable && prev->op != op_constant ) )
		syntax_error( "incorrect argument" );
	    cond->op = op_or;   pnt += 2; continue;
	}

	if ( *pnt == '!' && pnt[1] == '=' )
	{
	    if ( ! prev || ( prev->op != op_variable && prev->op != op_constant ) )
		syntax_error( "incorrect argument" );
	    cond->op = op_neq;  pnt += 2; continue;
	}

	if ( *pnt == '=' )
	{
	    if ( ! prev || ( prev->op != op_variable && prev->op != op_constant ) )
		syntax_error( "incorrect argument" );
	    cond->op = op_eq;   pnt += 1; continue;
	}

	if ( *pnt == '!' )
	{
	    if ( prev && ( prev->op != op_and && prev->op != op_or
		      && prev->op != op_bang ) )
		syntax_error( "incorrect argument" );
	    cond->op = op_bang; pnt += 1; continue;
	}

	if ( *pnt == '"' )
	{
	    const char * word;

	    if ( prev && ( prev->op == op_variable || prev->op == op_constant ) )
		syntax_error( "incorrect argument" );
	    /* advance to the word */
	    pnt++;
	    if ( *pnt == '$' )
		{ cond->op = op_variable; pnt++; }
	    else
		{ cond->op = op_constant; }

	    /* find the end of the word */
	    word = pnt;
	    for ( ; ; )
	    {
		if ( *pnt == '\0' )
		    syntax_error( "unterminated double quote" );
		if ( *pnt == '"' )
		    break;
		pnt++;
	    }

	    /* store a copy of this word */
	    {
		char * str = malloc( pnt - word + 1 );
		memcpy( str, word, pnt - word );
		str [pnt - word] = '\0';
		if ( cond->op == op_variable )
		{
		    cond->nameindex = get_varnum( str );
		    free( str );
		}
		else /* op_constant */
		{
		    cond->str = str;
		}
	    }

	    pnt++;
	    continue;
	}

	/* unknown token */
	syntax_error( "bad if condition" );
    }
}



/*
 * Tokenize a choice list.  Choices appear as pairs of strings;
 * note that I am parsing *inside* the double quotes.  Ugh.
 */
static const char * tokenize_choices( struct kconfig * cfg_choose,
    const char * pnt )
{
    int default_checked = 0;
    for ( ; ; )
    {
	struct kconfig * cfg;
	char * buffer = malloc( 64 );

	/* skip whitespace */
	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;
	if ( *pnt == '\0' )
	    return pnt;

	/* allocate a new kconfig line */
	cfg = malloc( sizeof(*cfg) );
	memset( cfg, 0, sizeof(*cfg) );
	if ( config_last == NULL )
	    { config_last = config_list = cfg; }
	else
	    { config_last->next = cfg; config_last = cfg; }

	/* fill out the line */
	cfg->token      = token_choice_item;
	cfg->cfg_parent = cfg_choose;
	pnt = get_string( pnt, &cfg->label );
	if ( ! default_checked &&
	     ! strncmp( cfg->label, cfg_choose->value, strlen( cfg_choose->value ) ) )
	{
	    default_checked = 1;
	    free( cfg_choose->value );
	    cfg_choose->value = cfg->label;
	}
	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;
	pnt = get_string( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
    }
    if ( ! default_checked )
	syntax_error( "bad 'choice' default value" );
    return pnt;
}



/*
 * Tokenize one line.
 */
static void tokenize_line( const char * pnt )
{
    static struct kconfig * last_menuoption = NULL;
    enum e_token token;
    struct kconfig * cfg;
    struct dependency ** dep_ptr;
    char * buffer = malloc( 64 );

    /* skip white space */
    while ( *pnt == ' ' || *pnt == '\t' )
	pnt++;

    /*
     * categorize the next token
     */

#define match_token(t, s) \
    if (strncmp(pnt, s, strlen(s)) == 0) { token = t; pnt += strlen(s); break; }

    token = token_UNKNOWN;
    switch ( *pnt )
    {
    default:
	break;

    case '#':
    case '\0':
	return;

    case 'b':
	match_token( token_bool, "bool" );
	break;

    case 'c':
	match_token( token_choice_header, "choice"  );
	match_token( token_comment, "comment" );
	break;

    case 'd':
	match_token( token_define_bool, "define_bool" );
	match_token( token_define_hex, "define_hex" );
	match_token( token_define_int, "define_int" );
	match_token( token_define_string, "define_string" );
	match_token( token_define_tristate, "define_tristate" );
	match_token( token_dep_bool, "dep_bool" );
	match_token( token_dep_mbool, "dep_mbool" );
	match_token( token_dep_tristate, "dep_tristate" );
	break;

    case 'e':
	match_token( token_else, "else" );
	match_token( token_endmenu, "endmenu" );
	break;

    case 'f':
	match_token( token_fi, "fi" );
	break;

    case 'h':
	match_token( token_hex, "hex" );
	break;

    case 'i':
	match_token( token_if, "if" );
	match_token( token_int, "int" );
	break;

    case 'm':
	match_token( token_mainmenu_name, "mainmenu_name" );
	match_token( token_mainmenu_option, "mainmenu_option" );
	break;

    case 's':
	match_token( token_source, "source" );
	match_token( token_string, "string" );
	break;

    case 't':
	match_token( token_then, "then" );
	match_token( token_tristate, "tristate" );
	break;

    case 'u':
	match_token( token_unset, "unset" );
	break;
    }

#undef match_token

    if ( token == token_source )
    {
	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;
	do_source( pnt );
	return;
    }

    if ( token == token_then )
    {
	if ( config_last != NULL && config_last->token == token_if )
	    return;
	syntax_error( "bogus 'then'" );
    }

#if 0
    if ( token == token_unset )
    {
	fprintf( stderr, "Ignoring 'unset' command\n" );
	return;
    }
#endif

    if ( token == token_UNKNOWN )
	syntax_error( "unknown command" );

    /*
     * Allocate an item.
     */
    cfg = malloc( sizeof(*cfg) );
    memset( cfg, 0, sizeof(*cfg) );
    if ( config_last == NULL )
	{ config_last = config_list = cfg; }
    else
	{ config_last->next = cfg; config_last = cfg; }

    /*
     * Tokenize the arguments.
     */
    while ( *pnt == ' ' || *pnt == '\t' )
	pnt++;

    cfg->token = token;
    switch ( token )
    {
    default:
	syntax_error( "unknown token" );

    case token_bool:
    case token_tristate:
	pnt = get_qstring ( pnt, &cfg->label );
	pnt = get_string  ( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	break;

    case token_choice_header:
	{
	    static int choose_number = 0;
	    char * choice_list;

	    pnt = get_qstring ( pnt, &cfg->label  );
	    pnt = get_qstring ( pnt, &choice_list );
	    pnt = get_string  ( pnt, &cfg->value  );
	    cfg->nameindex = -(choose_number++);
	    tokenize_choices( cfg, choice_list );
	    free( choice_list );
	}
	break;

    case token_comment:
	pnt = get_qstring(pnt, &cfg->label);
	if ( last_menuoption != NULL )
	{
	    pnt = get_qstring(pnt, &cfg->label);
	    if (cfg->label == NULL)
		syntax_error( "missing comment text" );
	    last_menuoption->label = cfg->label;
	    last_menuoption = NULL;
	}
	break;

    case token_define_bool:
    case token_define_tristate:
	pnt = get_string( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;
	if ( ( pnt[0] == 'Y'  || pnt[0] == 'M' || pnt[0] == 'N'
	||     pnt[0] == 'y'  || pnt[0] == 'm' || pnt[0] == 'n' )
	&&   ( pnt[1] == '\0' || pnt[1] == ' ' || pnt[1] == '\t' ) )
	{
	    if      ( *pnt == 'n' || *pnt == 'N' ) cfg->value = strdup( "CONSTANT_N" );
	    else if ( *pnt == 'y' || *pnt == 'Y' ) cfg->value = strdup( "CONSTANT_Y" );
	    else if ( *pnt == 'm' || *pnt == 'M' ) cfg->value = strdup( "CONSTANT_M" );
	}
	else if ( *pnt == '$' )
	{
	    pnt++;
	    pnt = get_string( pnt, &cfg->value );
	}
	else
	{
	    syntax_error( "unknown define_bool value" );
	}
	get_varnum( cfg->value );
	break;

    case token_define_hex:
    case token_define_int:
	pnt = get_string( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	pnt = get_string( pnt, &cfg->value );
	break;

    case token_define_string:
	pnt = get_string( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	pnt = get_qnqstring( pnt, &cfg->value );
	if (cfg->value == NULL)
	    syntax_error( "missing value" );
	break;

    case token_dep_bool:
    case token_dep_mbool:
    case token_dep_tristate:
	pnt = get_qstring ( pnt, &cfg->label );
	pnt = get_string  ( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );

	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;

	dep_ptr = &(cfg->depend);

	do {
	    *dep_ptr = (struct dependency *) malloc( sizeof( struct dependency ) );
	    (*dep_ptr)->next = NULL;

	    if ( ( pnt[0] == 'Y'  || pnt[0] == 'M' || pnt[0] == 'N'
	    ||     pnt[0] == 'y'  || pnt[0] == 'm' || pnt[0] == 'n' )
	    &&   ( pnt[1] == '\0' || pnt[1] == ' ' || pnt[1] == '\t' ) )
	    {
		/* dep_tristate 'foo' CONFIG_FOO m */
		if      ( pnt[0] == 'Y' || pnt[0] == 'y' )
		    (*dep_ptr)->name = strdup( "CONSTANT_Y" );
		else if ( pnt[0] == 'N' || pnt[0] == 'n' )
		    (*dep_ptr)->name = strdup( "CONSTANT_N" );
		else
		    (*dep_ptr)->name = strdup( "CONSTANT_M" );
		pnt++;
		get_varnum( (*dep_ptr)->name );
	    }
	    else if ( *pnt == '$' )
	    {
		pnt++;
		pnt = get_string( pnt, &(*dep_ptr)->name );
		get_varnum( (*dep_ptr)->name );
	    }
	    else
	    {
		syntax_error( "can't handle dep_bool/dep_mbool/dep_tristate condition" );
	    }
	    dep_ptr = &(*dep_ptr)->next;
	    while ( *pnt == ' ' || *pnt == '\t' )
		pnt++;
	} while ( *pnt );

	/*
	 * Create a conditional for this object's dependencies.
	 */
	{
	    char fake_if [1024];
	    struct dependency * dep;
	    struct condition ** cond_ptr;
	    int first = 1;

	    cond_ptr = &(cfg->cond);
	    for ( dep = cfg->depend; dep; dep = dep->next )
	    {
		if ( token == token_dep_tristate
		&& ! strcmp( dep->name, "CONSTANT_M" ) )
		{
		    continue;
		}
		if ( first )
		{
		    first = 0;
		}
		else
		{
		    *cond_ptr = malloc( sizeof(struct condition) );
		    memset( *cond_ptr, 0, sizeof(struct condition) );
		    (*cond_ptr)->op = op_and;
		    cond_ptr = &(*cond_ptr)->next;
		}
		*cond_ptr = malloc( sizeof(struct condition) );
		memset( *cond_ptr, 0, sizeof(struct condition) );
		(*cond_ptr)->op = op_lparen;
		if ( token == token_dep_bool )
		    sprintf( fake_if, "[ \"$%s\" = \"y\" -o \"$%s\" = \"\" ]; then",
			dep->name, dep->name );
		else
		    sprintf( fake_if, "[ \"$%s\" = \"y\" -o \"$%s\" = \"m\" -o \"$%s\" = \"\" ]; then",
			dep->name, dep->name, dep->name );
		(*cond_ptr)->next = tokenize_if( fake_if );
		while ( *cond_ptr )
		    cond_ptr = &(*cond_ptr)->next;
		*cond_ptr = malloc( sizeof(struct condition) );
		memset( *cond_ptr, 0, sizeof(struct condition) );
		(*cond_ptr)->op = op_rparen;
		cond_ptr = &(*cond_ptr)->next;
	    }
	}
	break;

    case token_else:
    case token_endmenu:
    case token_fi:
	break;

    case token_hex:
    case token_int:
	pnt = get_qstring ( pnt, &cfg->label );
	pnt = get_string  ( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	pnt = get_string  ( pnt, &cfg->value );
	break;

    case token_string:
	pnt = get_qstring ( pnt, &cfg->label );
	pnt = get_string  ( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	pnt = get_qnqstring  ( pnt, &cfg->value );
	if (cfg->value == NULL)
	    syntax_error( "missing initial value" );
	break;

    case token_if:
	cfg->cond = tokenize_if( pnt );
	break;

    case token_mainmenu_name:
	pnt = get_qstring( pnt, &cfg->label );
	break;

    case token_mainmenu_option:
	if ( strncmp( pnt, "next_comment", 12 ) == 0 )
	    last_menuoption = cfg;
	else
	    pnt = get_qstring( pnt, &cfg->label );
	break;

    case token_unset:
	pnt = get_string( pnt, &buffer );
	cfg->nameindex = get_varnum( buffer );
	while ( *pnt == ' ' || *pnt == '\t' )
	    pnt++;
	while (*pnt)
	{
	    cfg->next = (struct kconfig *) malloc( sizeof(struct kconfig) );
	    memset( cfg->next, 0, sizeof(struct kconfig) );
	    cfg = cfg->next;
	    cfg->token = token_unset;
	    pnt = get_string( pnt, &buffer );
	    cfg->nameindex = get_varnum( buffer );
	    while ( *pnt == ' ' || *pnt == '\t' )
		pnt++;
	}
	break;
    }
    return;
}



/*
 * Implement the "source" command.
 */
static void do_source( const char * filename )
{
    char buffer [2048];
    FILE * infile;
    const char * old_file;
    int old_lineno;
    int offset;

    /* open the file */
    if ( strcmp( filename, "-" ) == 0 )
	infile = stdin;
    else
	infile = fopen( filename, "r" );

    /* if that failed, try ../filename */
    if ( infile == NULL )
    {
	sprintf( buffer, "../%s", filename );
	infile = fopen( buffer, "r" );
    }

    if ( infile == NULL )
    {
	sprintf( buffer, "unable to open %s", filename );
	syntax_error( buffer );
    }

    /* push the new file name and line number */
    old_file     = current_file;
    old_lineno   = lineno;
    current_file = filename;
    lineno       = 0;

    /* read and process lines */
    for ( offset = 0; ; )
    {
	char * pnt;

	/* read a line */
	fgets( buffer + offset, sizeof(buffer) - offset, infile );
	if ( feof( infile ) )
	    break;
	lineno++;

	/* strip the trailing return character */
	pnt = buffer + strlen(buffer) - 1;
	if ( *pnt == '\n' )
	    *pnt-- = '\0';

	/* eat \ NL pairs */
	if ( *pnt == '\\' )
	{
	    offset = pnt - buffer;
	    continue;
	}

	/* tokenize this line */
	tokenize_line( buffer );
	offset = 0;
    }

    /* that's all, folks */
    if ( infile != stdin )
	fclose( infile );
    current_file = old_file;
    lineno       = old_lineno;
    return;
}



/*
 * Main program.
 */
int main( int argc, const char * argv [] )
{
    do_source        ( "-"         );
    fix_conditionals ( config_list );
    dump_tk_script   ( config_list );
    free(vartable);
    return 0;
}
