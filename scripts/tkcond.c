/*
 * tkcond.c
 *
 * Eric Youngdale was the original author of xconfig.
 * Michael Elizabeth Chastain (mec@shout.net) is the current maintainer.
 *
 * This file takes the tokenized statement list and transforms 'if ...'
 * statements.  For each simple statement, I find all of the 'if' statements
 * that enclose it, and attach the aggregate conditionals of those 'if'
 * statements to the cond list of the simple statement.
 *
 * 14 January 1999, Michael Elizabeth Chastain, <mec@shout.net>
 * - Steam-clean this file.  I tested this by generating kconfig.tk for
 *   every architecture and comparing it character-for-character against
 *   the output of the old tkparse.
 *
 * 07 July 1999, Andrzej M. Krzysztofowicz <ankry@mif.pg.gda.pl>
 * - kvariables removed; all variables are stored in a single table now
 * - some elimination of options non-valid for current architecture
 *   implemented.
 * - negation (!) eliminated from conditions
 *
 * TO DO:
 * - xconfig is at the end of its life cycle.  Contact <mec@shout.net> if
 *   you are interested in working on the replacement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tkparse.h"



/*
 * Mark variables which are defined anywhere.
 */
static void mark_variables( struct kconfig * scfg )
{
    struct kconfig * cfg;
    int i;

    for ( i = 1; i <= max_varnum; i++ )
	vartable[i].defined = 0;
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	if ( cfg->token == token_bool
	||   cfg->token == token_choice_item
	||   cfg->token == token_define_bool
	||   cfg->token == token_define_hex
	||   cfg->token == token_define_int
	||   cfg->token == token_define_string
	||   cfg->token == token_define_tristate
	||   cfg->token == token_dep_bool
	||   cfg->token == token_dep_mbool
	||   cfg->token == token_dep_tristate
	||   cfg->token == token_hex
	||   cfg->token == token_int
	||   cfg->token == token_string
	||   cfg->token == token_tristate
	||   cfg->token == token_unset )
	{
	    if ( cfg->nameindex > 0 )	/* paranoid */
	    {
		vartable[cfg->nameindex].defined = 1;
	    }
	}
    }
}



static void free_cond( struct condition *cond )
{
    struct condition *tmp, *tmp1;
    for ( tmp = cond; tmp; tmp = tmp1 )
    {
	tmp1 = tmp->next;
	free( (void*)tmp );
    }
}



/*
 * Remove the bang operator from a condition to avoid priority problems.
 * "!" has different priorities as "test" command argument and in 
 * a tk script.
 */
static struct condition * remove_bang( struct condition * condition )
{
    struct condition * conda, * condb, * prev = NULL;

    for ( conda = condition; conda; conda = conda->next )
    {
	if ( conda->op == op_bang && conda->next &&
	   ( condb = conda->next->next ) )
	{
	    if ( condb->op == op_eq || condb->op == op_neq )
	    {
		condb->op = (condb->op == op_eq) ? op_neq : op_eq;
		conda->op = op_nuked;
		if ( prev )
		{
		    prev->next = conda->next;
		}
		else
		{
		    condition = conda->next;
		}
		conda->next = NULL;
		free_cond( conda );
		conda = condb;
	    }
	}
	prev = conda;
    }
    return condition;
}



/*
 * Make a new condition chain by joining the current condition stack with
 * the "&&" operator for glue.
 */
static struct condition * join_condition_stack( struct condition * conditions [],
    int depth )
{
    struct condition * cond_list;
    struct condition * cond_last;
    int i, is_first = 1;

    cond_list = cond_last = NULL;

    for ( i = 0; i < depth; i++ )
    {
	if ( conditions[i]->op == op_false )
	{
	    struct condition * cnew;

	    /* It is always false condition */
	    cnew = malloc( sizeof(*cnew) );
	    memset( cnew, 0, sizeof(*cnew) );
	    cnew->op = op_false;
	    cond_list = cond_last = cnew;
	    goto join_done;
	}
    }
    for ( i = 0; i < depth; i++ )
    {
	struct condition * cond;
	struct condition * cnew;
	int add_paren;

	/* omit always true conditions */
	if ( conditions[i]->op == op_true )
	    continue;

	/* if i have another condition, add an '&&' operator */
	if ( !is_first )
	{
	    cnew = malloc( sizeof(*cnew) );
	    memset( cnew, 0, sizeof(*cnew) );
	    cnew->op = op_and;
	    cond_last->next = cnew;
	    cond_last = cnew;
	}

	if ( conditions[i]->op != op_lparen )
	{
	    /* add a '(' */
	    add_paren = 1;
	    cnew = malloc( sizeof(*cnew) );
	    memset( cnew, 0, sizeof(*cnew) );
	    cnew->op = op_lparen;
	    if ( cond_last == NULL )
		{ cond_list = cond_last = cnew; }
	    else
		{ cond_last->next = cnew; cond_last = cnew; }
	}
	else
	{
	    add_paren = 0;
	}

	/* duplicate the chain */
	for ( cond = conditions [i]; cond != NULL; cond = cond->next )
	{
	    cnew            = malloc( sizeof(*cnew) );
	    cnew->next      = NULL;
	    cnew->op        = cond->op;
	    cnew->str       = cond->str ? strdup( cond->str ) : NULL;
	    cnew->nameindex = cond->nameindex;
	    if ( cond_last == NULL )
		{ cond_list = cond_last = cnew; }
	    else
		{ cond_last->next = cnew; cond_last = cnew; }
	}

	if ( add_paren )
	{
	    /* add a ')' */
	    cnew = malloc( sizeof(*cnew) );
	    memset( cnew, 0, sizeof(*cnew) );
	    cnew->op = op_rparen;
	    cond_last->next = cnew;
	    cond_last = cnew;
	}
	is_first = 0;
    }

    /*
     * Remove duplicate conditions.
     */
    {
	struct condition *cond1, *cond1b, *cond1c, *cond1d, *cond1e, *cond1f;

	for ( cond1 = cond_list; cond1 != NULL; cond1 = cond1->next )
	{
	    if ( cond1->op == op_lparen )
	    {
		cond1b = cond1 ->next; if ( cond1b == NULL ) break;
		cond1c = cond1b->next; if ( cond1c == NULL ) break;
		cond1d = cond1c->next; if ( cond1d == NULL ) break;
		cond1e = cond1d->next; if ( cond1e == NULL ) break;
		cond1f = cond1e->next; if ( cond1f == NULL ) break;

		if ( cond1b->op == op_variable
		&& ( cond1c->op == op_eq || cond1c->op == op_neq )
		&&   cond1d->op == op_constant 
		&&   cond1e->op == op_rparen )
		{
		    struct condition *cond2, *cond2b, *cond2c, *cond2d, *cond2e, *cond2f;

		    for ( cond2 = cond1f->next; cond2 != NULL; cond2 = cond2->next )
		    {
			if ( cond2->op == op_lparen )
			{
			    cond2b = cond2 ->next; if ( cond2b == NULL ) break;
			    cond2c = cond2b->next; if ( cond2c == NULL ) break;
			    cond2d = cond2c->next; if ( cond2d == NULL ) break;
			    cond2e = cond2d->next; if ( cond2e == NULL ) break;
			    cond2f = cond2e->next;

			    /* look for match */
			    if ( cond2b->op == op_variable
			    &&   cond2b->nameindex == cond1b->nameindex
			    &&   cond2c->op == cond1c->op
			    &&   cond2d->op == op_constant
			    &&   strcmp( cond2d->str, cond1d->str ) == 0
			    &&   cond2e->op == op_rparen )
			    {
				/* one of these must be followed by && */
				if ( cond1f->op == op_and
				|| ( cond2f != NULL && cond2f->op == op_and ) )
				{
				    /* nuke the first duplicate */
				    cond1 ->op = op_nuked;
				    cond1b->op = op_nuked;
				    cond1c->op = op_nuked;
				    cond1d->op = op_nuked;
				    cond1e->op = op_nuked;
				    if ( cond1f->op == op_and )
					cond1f->op = op_nuked;
				    else
					cond2f->op = op_nuked;
				}
			    }
			}
		    }
		}
	    }
	}
    }

join_done:
    return cond_list;
}



static char * current_arch = NULL;

/*
 * Eliminating conditions with ARCH = <not current>.
 */
static struct condition *eliminate_other_arch( struct condition *list )
{
    struct condition *cond1a = list, *cond1b = NULL, *cond1c = NULL, *cond1d = NULL;
    if ( current_arch == NULL )
	current_arch = getenv( "ARCH" );
    if ( current_arch == NULL )
    {
	fprintf( stderr, "error: ARCH undefined\n" );
	exit( 1 );
    }
    if ( cond1a->op == op_variable
    && ! strcmp( vartable[cond1a->nameindex].name, "ARCH" ) )
    {
	cond1b = cond1a->next; if ( cond1b == NULL ) goto done;
	cond1c = cond1b->next; if ( cond1c == NULL ) goto done;
	cond1d = cond1c->next;
	if ( cond1c->op == op_constant && cond1d == NULL )
	{
	    if ( (cond1b->op == op_eq && strcmp( cond1c->str, current_arch ))
	    ||   (cond1b->op == op_neq && ! strcmp( cond1c->str, current_arch )) )
	    {
		/* This is for another architecture */ 
		cond1a->op = op_false;
		cond1a->next = NULL;
		free_cond( cond1b );
		return cond1a;
	    }
	    else if ( (cond1b->op == op_neq && strcmp( cond1c->str, current_arch ))
		 ||   (cond1b->op == op_eq && ! strcmp( cond1c->str, current_arch )) )
	    {
		/* This is for current architecture */
		cond1a->op = op_true;
		cond1a->next = NULL;
		free_cond( cond1b );
		return cond1a;
	    }
	}
	else if ( cond1c->op == op_constant && cond1d->op == op_or )
	{
	    if ( (cond1b->op == op_eq && strcmp( cond1c->str, current_arch ))
	    ||   (cond1b->op == op_neq && ! strcmp( cond1c->str, current_arch )) )
	    {
		/* This is for another architecture */ 
		cond1b = cond1d->next;
		cond1d->next = NULL;
		free_cond( cond1a );
		return eliminate_other_arch( cond1b );
	    }
	    else if ( (cond1b->op == op_neq && strcmp( cond1c->str, current_arch ))
		 || (cond1b->op == op_eq && ! strcmp( cond1c->str, current_arch )) )
	    {
		/* This is for current architecture */
		cond1a->op = op_true;
		cond1a->next = NULL;
		free_cond( cond1b );
		return cond1a;
	    }
	}
	else if ( cond1c->op == op_constant && cond1d->op == op_and )
	{
	    if ( (cond1b->op == op_eq && strcmp( cond1c->str, current_arch ))
	    ||   (cond1b->op == op_neq && ! strcmp( cond1c->str, current_arch )) )
	    {
		/* This is for another architecture */
		int l_par = 0;
		
		for ( cond1c = cond1d->next; cond1c; cond1c = cond1c->next )
		{
		    if ( cond1c->op == op_lparen )
			l_par++;
		    else if ( cond1c->op == op_rparen )
			l_par--;
		    else if ( cond1c->op == op_or && l_par == 0 )
		    /* Expression too complex - don't touch */
			return cond1a;
		    else if ( l_par < 0 )
		    {
			fprintf( stderr, "incorrect condition: programming error ?\n" );
			exit( 1 );
		    }
		}
		cond1a->op = op_false;
		cond1a->next = NULL;
		free_cond( cond1b );
		return cond1a;
	    }
	    else if ( (cond1b->op == op_neq && strcmp( cond1c->str, current_arch ))
		 || (cond1b->op == op_eq && ! strcmp( cond1c->str, current_arch )) )
	    {
		/* This is for current architecture */
		cond1b = cond1d->next;
		cond1d->next = NULL;
		free_cond( cond1a );
		return eliminate_other_arch( cond1b );
	    }
	}
    }
    if ( cond1a->op == op_variable && ! vartable[cond1a->nameindex].defined )
    {
	cond1b = cond1a->next; if ( cond1b == NULL ) goto done;
	cond1c = cond1b->next; if ( cond1c == NULL ) goto done;
	cond1d = cond1c->next;

	if ( cond1c->op == op_constant
	&& ( cond1d == NULL || cond1d->op == op_and ) ) /*???*/
	{
	    if ( cond1b->op == op_eq && strcmp( cond1c->str, "" ) )
	    {
		cond1a->op = op_false;
		cond1a->next = NULL;
		free_cond( cond1b );
		return cond1a;
	    }
	}
	else if ( cond1c->op == op_constant && cond1d->op == op_or )
	{
	    if ( cond1b->op == op_eq && strcmp( cond1c->str, "" ) )
	    {
		cond1b = cond1d->next;
		cond1d->next = NULL;
		free_cond( cond1a );
		return eliminate_other_arch( cond1b );
	    }
	}
    }
done:
    return list;
}



/*
 * This is the main transformation function.
 */
void fix_conditionals( struct kconfig * scfg )
{
    struct kconfig * cfg;

    /*
     * Transform op_variable to op_kvariable.
     */
    mark_variables( scfg );

    /*
     * Walk the statement list, maintaining a stack of current conditions.
     *   token_if      push its condition onto the stack.
     *   token_else    invert the condition on the top of the stack.
     *   token_endif   pop the stack.
     *
     * For a simple statement, create a condition chain by joining together
     * all of the conditions on the stack.
     */
    {
	struct condition * cond_stack [32];
	int depth = 0;
	struct kconfig * prev = NULL;

	for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
	{
	    int good = 1;
	    switch ( cfg->token )
	    {
	    default:
		break;

	    case token_if:
		cond_stack [depth++] =
		    remove_bang( eliminate_other_arch( cfg->cond ) );
		cfg->cond = NULL;
		break;

	    case token_else:
		{
		    /*
		     * Invert the condition chain.
		     *
		     * Be careful to transfrom op_or to op_and1, not op_and.
		     * I will need this later in the code that removes
		     * duplicate conditions.
		     */
		    struct condition * cond;

		    for ( cond  = cond_stack [depth-1];
			  cond != NULL;
			  cond  = cond->next )
		    {
			switch( cond->op )
			{
			default:     break;
			case op_and: cond->op = op_or;   break;
			case op_or:  cond->op = op_and1; break;
			case op_neq: cond->op = op_eq;   break;
			case op_eq:  cond->op = op_neq;  break;
			case op_true: cond->op = op_false;break;
			case op_false:cond->op = op_true; break;
			}
		    }
		}
		break;

	    case token_fi:
		--depth;
		break;

	    case token_bool:
	    case token_choice_item:
	    case token_choice_header:
	    case token_comment:
	    case token_define_bool:
	    case token_define_hex:
	    case token_define_int:
	    case token_define_string:
	    case token_define_tristate:
	    case token_endmenu:
	    case token_hex:
	    case token_int:
	    case token_mainmenu_option:
	    case token_string:
	    case token_tristate:
	    case token_unset:
		cfg->cond = join_condition_stack( cond_stack, depth );
		if ( cfg->cond && cfg->cond->op == op_false )
		{
		    good = 0;
		    if ( prev )
			prev->next = cfg->next;
		    else
			scfg = cfg->next;
		}
		break;

	    case token_dep_bool:
	    case token_dep_mbool:
	    case token_dep_tristate:
		/*
		 * Same as the other simple statements, plus an additional
		 * condition for the dependency.
		 */
		if ( cfg->cond )
		{
		    cond_stack [depth] = eliminate_other_arch( cfg->cond );
		    cfg->cond = join_condition_stack( cond_stack, depth+1 );
		}
		else
		{
		    cfg->cond = join_condition_stack( cond_stack, depth );
		}
		if ( cfg->cond && cfg->cond->op == op_false )
		{
		    good = 0;
		    if ( prev )
			prev->next = cfg->next;
		    else
			scfg = cfg->next;
		}
		break;
	    }
	    if ( good )
		prev = cfg;
	}
    }
}



#if 0
void dump_condition( struct condition *list )
{
    struct condition *tmp;
    for ( tmp = list; tmp; tmp = tmp->next )
    {
	switch (tmp->op)
	{
	default:
	    break;
	case op_variable:
	    printf( " %s", vartable[tmp->nameindex].name );
	    break;
	case op_constant: 
	    printf( " %s", tmp->str );
	    break;
	case op_eq:
	    printf( " =" );
	    break;
	case op_bang:
	    printf( " !" );
	    break;
	case op_neq:
	    printf( " !=" );
	    break;
	case op_and:
	case op_and1:
	    printf( " -a" );
	    break;
	case op_or:
	    printf( " -o" );
	    break;
	case op_true:
	    printf( " TRUE" );
	    break;
	case op_false:
	    printf( " FALSE" );
	    break;
	case op_lparen:
	    printf( " (" );
	    break;
	case op_rparen:
	    printf( " )" );
	    break;
	}
    }
    printf( "\n" );
}
#endif
