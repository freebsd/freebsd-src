/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.options.c - version 1.0.3 */
/* $FreeBSD: src/games/hack/hack.options.c,v 1.5 1999/11/16 02:57:08 billf Exp $ */

#include <stdlib.h>
#include "config.h"
#include "hack.h"
extern char *eos();

initoptions()
{
	char *opts;

	flags.time = flags.nonews = flags.notombstone = flags.end_own =
	flags.standout = flags.nonull = FALSE;
	flags.no_rest_on_space = TRUE;
	flags.invlet_constant = TRUE;
	flags.end_top = 5;
	flags.end_around = 4;
	flags.female = FALSE;			/* players are usually male */

	if(opts = getenv("HACKOPTIONS"))
		parseoptions(opts,TRUE);
}

parseoptions(opts, from_env)
char *opts;
boolean from_env;
{
	char *op,*op2;
	unsigned num;
	boolean negated;

	if(op = index(opts, ',')) {
		*op++ = 0;
		parseoptions(op, from_env);
	}
	if(op = index(opts, ' ')) {
		op2 = op;
		while(*op++)
			if(*op != ' ') *op2++ = *op;
	}
	if(!*opts) return;
	negated = FALSE;
	while((*opts == '!') || !strncmp(opts, "no", 2)) {
		if(*opts == '!') opts++; else opts += 2;
		negated = !negated;
	}

	if(!strncmp(opts,"standout",8)) {
		flags.standout = !negated;
		return;
	}

	if(!strncmp(opts,"null",3)) {
		flags.nonull = negated;
		return;
	}

	if(!strncmp(opts,"tombstone",4)) {
		flags.notombstone = negated;
		return;
	}

	if(!strncmp(opts,"news",4)) {
		flags.nonews = negated;
		return;
	}

	if(!strncmp(opts,"time",4)) {
		flags.time = !negated;
		flags.botl = 1;
		return;
	}

	if(!strncmp(opts,"restonspace",4)) {
		flags.no_rest_on_space = negated;
		return;
	}

	if(!strncmp(opts,"fixinv",4)) {
		if(from_env)
			flags.invlet_constant = !negated;
		else
			pline("The fixinvlet option must be in HACKOPTIONS.");
		return;
	}

	if(!strncmp(opts,"male",4)) {
		flags.female = negated;
		return;
	}
	if(!strncmp(opts,"female",6)) {
		flags.female = !negated;
		return;
	}

	/* name:string */
	if(!strncmp(opts,"name",4)) {
		extern char plname[PL_NSIZ];
		if(!from_env) {
		  pline("The playername can be set only from HACKOPTIONS.");
		  return;
		}
		op = index(opts,':');
		if(!op) goto bad;
		(void) strncpy(plname, op+1, sizeof(plname)-1);
		return;
	}

	/* endgame:5t[op] 5a[round] o[wn] */
	if(!strncmp(opts,"endgame",3)) {
		op = index(opts,':');
		if(!op) goto bad;
		op++;
		while(*op) {
			num = 1;
			if(digit(*op)) {
				num = atoi(op);
				while(digit(*op)) op++;
			} else
			if(*op == '!') {
				negated = !negated;
				op++;
			}
			switch(*op) {
			case 't':
				flags.end_top = num;
				break;
			case 'a':
				flags.end_around = num;
				break;
			case 'o':
				flags.end_own = !negated;
				break;
			default:
				goto bad;
			}
			while(letter(*++op)) ;
			if(*op == '/') op++;
		}
		return;
	}
bad:
	if(!from_env) {
		if(!strncmp(opts, "help", 4)) {
			pline("%s%s%s",
"To set options use `HACKOPTIONS=\"<options>\"' in your environment, or ",
"give the command 'o' followed by the line `<options>' while playing. ",
"Here <options> is a list of <option>s separated by commas." );
			pline("%s%s%s",
"Simple (boolean) options are rest_on_space, news, time, ",
"null, tombstone, (fe)male. ",
"These can be negated by prefixing them with '!' or \"no\"." );
			pline("%s",
"A string option is name, as in HACKOPTIONS=\"name:Merlin-W\"." );
			pline("%s%s%s",
"A compound option is endgame; it is followed by a description of what ",
"parts of the scorelist you want to see. You might for example say: ",
"`endgame:own scores/5 top scores/4 around my score'." );
			return;
		}
		pline("Bad option: %s.", opts);
		pline("Type `o help<cr>' for help.");
		return;
	}
	puts("Bad syntax in HACKOPTIONS.");
	puts("Use for example:");
	puts(
"HACKOPTIONS=\"!restonspace,notombstone,endgame:own/5 topscorers/4 around me\""
	);
	getret();
}

doset()
{
	char buf[BUFSZ];

	pline("What options do you want to set? ");
	getlin(buf);
	if(!buf[0] || buf[0] == '\033') {
	    (void) strcpy(buf,"HACKOPTIONS=");
	    (void) strcat(buf, flags.female ? "female," : "male,");
	    if(flags.standout) (void) strcat(buf,"standout,");
	    if(flags.nonull) (void) strcat(buf,"nonull,");
	    if(flags.nonews) (void) strcat(buf,"nonews,");
	    if(flags.time) (void) strcat(buf,"time,");
	    if(flags.notombstone) (void) strcat(buf,"notombstone,");
	    if(flags.no_rest_on_space)
		(void) strcat(buf,"!rest_on_space,");
	    if(flags.end_top != 5 || flags.end_around != 4 || flags.end_own){
		(void) sprintf(eos(buf), "endgame: %u topscores/%u around me",
			flags.end_top, flags.end_around);
		if(flags.end_own) (void) strcat(buf, "/own scores");
	    } else {
		char *eop = eos(buf);
		if(*--eop == ',') *eop = 0;
	    }
	    pline(buf);
	} else
	    parseoptions(buf, FALSE);

	return(0);
}
