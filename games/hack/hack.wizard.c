/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.wizard.c - version 1.0.3 */

/* wizard code - inspired by rogue code from Merlyn Leroy (digi-g!brian) */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "hack.h"
extern struct permonst pm_wizard;
extern struct monst *makemon();

#define	WIZSHOT	    6	/* one chance in WIZSHOT that wizard will try magic */
#define	BOLT_LIM    8	/* from this distance D and 1 will try to hit you */

char wizapp[] = "@DNPTUVXcemntx";

/* If he has found the Amulet, make the wizard appear after some time */
amulet(){
	struct obj *otmp;
	struct monst *mtmp;

	if(!flags.made_amulet || !flags.no_of_wizards)
		return;
	/* find wizard, and wake him if necessary */
	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if(mtmp->data->mlet == '1' && mtmp->msleep && !rn2(40))
		for(otmp = invent; otmp; otmp = otmp->nobj)
		    if(otmp->olet == AMULET_SYM && !otmp->spe) {
			mtmp->msleep = 0;
			if(dist(mtmp->mx,mtmp->my) > 2)
			    pline(
    "You get the creepy feeling that somebody noticed your taking the Amulet."
			    );
			return;
		    }
}

wiz_hit(mtmp)
struct monst *mtmp;
{
	/* if we have stolen or found the amulet, we disappear */
	if(mtmp->minvent && mtmp->minvent->olet == AMULET_SYM &&
	    mtmp->minvent->spe == 0) {
		/* vanish -- very primitive */
		fall_down(mtmp);
		return(1);
	}

	/* if it is lying around someplace, we teleport to it */
	if(!carrying(AMULET_OF_YENDOR)) {
	    struct obj *otmp;

	    for(otmp = fobj; otmp; otmp = otmp->nobj)
		if(otmp->olet == AMULET_SYM && !otmp->spe) {
		    if((u.ux != otmp->ox || u.uy != otmp->oy) &&
		       !m_at(otmp->ox, otmp->oy)) {

			/* teleport to it and pick it up */
			mtmp->mx = otmp->ox;
			mtmp->my = otmp->oy;
			freeobj(otmp);
			mpickobj(mtmp, otmp);
			pmon(mtmp);
			return(0);
		    }
		    goto hithim;
		}
	    return(0);				/* we don't know where it is */
	}
hithim:
	if(rn2(2)) {				/* hit - perhaps steal */

	    /* if hit 1/20 chance of stealing amulet & vanish
		- amulet is on level 26 again. */
	    if(hitu(mtmp, d(mtmp->data->damn,mtmp->data->damd))
		&& !rn2(20) && stealamulet(mtmp))
		;
	}
	else
	    inrange(mtmp);			/* try magic */
	return(0);
}

inrange(mtmp)
struct monst *mtmp;
{
	schar tx,ty;

	/* do nothing if cancelled (but make '1' say something) */
	if(mtmp->data->mlet != '1' && mtmp->mcan)
		return;

	/* spit fire only when both in a room or both in a corridor */
	if(inroom(u.ux,u.uy) != inroom(mtmp->mx,mtmp->my)) return;
	tx = u.ux - mtmp->mx;
	ty = u.uy - mtmp->my;
	if((!tx && abs(ty) < BOLT_LIM) || (!ty && abs(tx) < BOLT_LIM)
	    || (abs(tx) == abs(ty) && abs(tx) < BOLT_LIM)){
	    switch(mtmp->data->mlet) {
	    case 'D':
		/* spit fire in the direction of @ (not nec. hitting) */
		buzz(-1,mtmp->mx,mtmp->my,sgn(tx),sgn(ty));
		break;
	    case '1':
		if(rn2(WIZSHOT)) break;
		/* if you zapped wizard with wand of cancellation,
		he has to shake off the effects before he can throw
		spells successfully.  1/2 the time they fail anyway */
		if(mtmp->mcan || rn2(2)) {
		    if(canseemon(mtmp))
			pline("%s makes a gesture, then curses.",
			    Monnam(mtmp));
		    else
			pline("You hear mumbled cursing.");
		    if(!rn2(3)) {
			mtmp->mspeed = 0;
			mtmp->minvis = 0;
		    }
		    if(!rn2(3))
			mtmp->mcan = 0;
		} else {
		    if(canseemon(mtmp)){
			if(!rn2(6) && !Invis) {
			    pline("%s hypnotizes you.", Monnam(mtmp));
			    nomul(rn2(3) + 3);
			    break;
			} else
			    pline("%s chants an incantation.",
				Monnam(mtmp));
		    } else
			    pline("You hear a mumbled incantation.");
		    switch(rn2(Invis ? 5 : 6)) {
		    case 0:
			/* create a nasty monster from a deep level */
			/* (for the moment, 'nasty' is not implemented) */
			(void) makemon((struct permonst *)0, u.ux, u.uy);
			break;
		    case 1:
			pline("\"Destroy the thief, my pets!\"");
			aggravate();	/* aggravate all the monsters */
			/* FALLTHROUGH */
		    case 2:
			if (flags.no_of_wizards == 1 && rnd(5) == 0)
			    /* if only 1 wizard, clone himself */
			    clonewiz(mtmp);
			break;
		    case 3:
			if(mtmp->mspeed == MSLOW)
				mtmp->mspeed = 0;
			else
				mtmp->mspeed = MFAST;
			break;
		    case 4:
			mtmp->minvis = 1;
			break;
		    case 5:
			/* Only if not Invisible */
			pline("You hear a clap of thunder!");
			/* shoot a bolt of fire or cold, or a sleep ray */
			buzz(-rnd(3),mtmp->mx,mtmp->my,sgn(tx),sgn(ty));
			break;
		    }
		}
	    }
	    if(u.uhp < 1) done_in_by(mtmp);
	}
}

aggravate()
{
	struct monst *mtmp;

	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
		mtmp->msleep = 0;
		if(mtmp->mfroz && !rn2(5))
			mtmp->mfroz = 0;
	}
}

clonewiz(mtmp)
struct monst *mtmp;
{
	struct monst *mtmp2;

	if(mtmp2 = makemon(PM_WIZARD, mtmp->mx, mtmp->my)) {
		flags.no_of_wizards = 2;
		unpmon(mtmp2);
		mtmp2->mappearance = wizapp[rn2(sizeof(wizapp)-1)];
		pmon(mtmp);
	}
}
