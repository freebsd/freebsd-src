/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.trap.c - version 1.0.3 */
/* $FreeBSD: src/games/hack/hack.trap.c,v 1.5 1999/11/16 10:26:38 marcel Exp $ */

#include	"hack.h"

extern struct monst *makemon();

char vowels[] = "aeiou";

char *traps[] = {
	" bear trap",
	"n arrow trap",
	" dart trap",
	" trapdoor",
	" teleportation trap",
	" pit",
	" sleeping gas trap",
	" piercer",
	" mimic"
};

struct trap *
maketrap(x,y,typ)
int x,y,typ;
{
	struct trap *ttmp;

	ttmp = newtrap();
	ttmp->ttyp = typ;
	ttmp->tseen = 0;
	ttmp->once = 0;
	ttmp->tx = x;
	ttmp->ty = y;
	ttmp->ntrap = ftrap;
	ftrap = ttmp;
	return(ttmp);
}

dotrap(trap) struct trap *trap; {
	int ttype = trap->ttyp;

	nomul(0);
	if(trap->tseen && !rn2(5) && ttype != PIT)
		pline("You escape a%s.", traps[ttype]);
	else {
		trap->tseen = 1;
		switch(ttype) {
		case SLP_GAS_TRAP:
			pline("A cloud of gas puts you to sleep!");
			nomul(-rnd(25));
			break;
		case BEAR_TRAP:
			if(Levitation) {
				pline("You float over a bear trap.");
				break;
			}
			u.utrap = 4 + rn2(4);
			u.utraptype = TT_BEARTRAP;
			pline("A bear trap closes on your foot!");
			break;
		case PIERC:
			deltrap(trap);
			if(makemon(PM_PIERCER,u.ux,u.uy)) {
			  pline("A piercer suddenly drops from the ceiling!");
			  if(uarmh)
				pline("Its blow glances off your helmet.");
			  else
				(void) thitu(3,d(4,6),"falling piercer");
			}
			break;
		case ARROW_TRAP:
			pline("An arrow shoots out at you!");
			if(!thitu(8,rnd(6),"arrow")){
				mksobj_at(ARROW, u.ux, u.uy);
				fobj->quan = 1;
			}
			break;
		case TRAPDOOR:
			if(!xdnstair) {
pline("A trap door in the ceiling opens and a rock falls on your head!");
if(uarmh) pline("Fortunately, you are wearing a helmet!");
			    losehp(uarmh ? 2 : d(2,10),"falling rock");
			    mksobj_at(ROCK, u.ux, u.uy);
			    fobj->quan = 1;
			    stackobj(fobj);
			    if(Invisible) newsym(u.ux, u.uy);
			} else {
			    int newlevel = dlevel + 1;
				while(!rn2(4) && newlevel < 29)
					newlevel++;
				pline("A trap door opens up under you!");
				if(Levitation || u.ustuck) {
 				pline("For some reason you don't fall in.");
					break;
				}

				goto_level(newlevel, FALSE);
			}
			break;
		case DART_TRAP:
			pline("A little dart shoots out at you!");
			if(thitu(7,rnd(3),"little dart")) {
			    if(!rn2(6))
				poisoned("dart","poison dart");
			} else {
				mksobj_at(DART, u.ux, u.uy);
				fobj->quan = 1;
			}
			break;
		case TELEP_TRAP:
			if(trap->once) {
				deltrap(trap);
				newsym(u.ux,u.uy);
				vtele();
			} else {
				newsym(u.ux,u.uy);
				tele();
			}
			break;
		case PIT:
			if(Levitation) {
				pline("A pit opens up under you!");
				pline("You don't fall in!");
				break;
			}
			pline("You fall into a pit!");
			u.utrap = rn1(6,2);
			u.utraptype = TT_PIT;
			losehp(rnd(6),"fall into a pit");
			selftouch("Falling, you");
			break;
		default:
			impossible("You hit a trap of type %u", trap->ttyp);
		}
	}
}

mintrap(mtmp) struct monst *mtmp; {
	struct trap *trap = t_at(mtmp->mx, mtmp->my);
	int wasintrap = mtmp->mtrapped;

	if(!trap) {
		mtmp->mtrapped = 0;	/* perhaps teleported? */
	} else if(wasintrap) {
		if(!rn2(40)) mtmp->mtrapped = 0;
	} else {
	    int tt = trap->ttyp;
	    int in_sight = cansee(mtmp->mx,mtmp->my);
	    extern char mlarge[];

	    if(mtmp->mtrapseen & (1 << tt)) {
		/* he has been in such a trap - perhaps he escapes */
		if(rn2(4)) return(0);
	    }
	    mtmp->mtrapseen |= (1 << tt);
	    switch (tt) {
		case BEAR_TRAP:
			if(index(mlarge, mtmp->data->mlet)) {
				if(in_sight)
				  pline("%s is caught in a bear trap!",
					Monnam(mtmp));
				else
				  if(mtmp->data->mlet == 'o')
			    pline("You hear the roaring of an angry bear!");
				mtmp->mtrapped = 1;
			}
			break;
		case PIT:
			/* there should be a mtmp/data -> floating */
			if(!index("EywBfk'& ", mtmp->data->mlet)) { /* ab */
				mtmp->mtrapped = 1;
				if(in_sight)
				  pline("%s falls in a pit!", Monnam(mtmp));
			}
			break;
		case SLP_GAS_TRAP:
			if(!mtmp->msleep && !mtmp->mfroz) {
				mtmp->msleep = 1;
				if(in_sight)
				  pline("%s suddenly falls asleep!",
					Monnam(mtmp));
			}
			break;
		case TELEP_TRAP:
			rloc(mtmp);
			if(in_sight && !cansee(mtmp->mx,mtmp->my))
				pline("%s suddenly disappears!",
					Monnam(mtmp));
			break;
		case ARROW_TRAP:
			if(in_sight) {
				pline("%s is hit by an arrow!",
					Monnam(mtmp));
			}
			mtmp->mhp -= 3;
			break;
		case DART_TRAP:
			if(in_sight) {
				pline("%s is hit by a dart!",
					Monnam(mtmp));
			}
			mtmp->mhp -= 2;
			/* not mondied here !! */
			break;
		case TRAPDOOR:
			if(!xdnstair) {
				mtmp->mhp -= 10;
				if(in_sight)
pline("A trap door in the ceiling opens and a rock hits %s!", monnam(mtmp));
				break;
			}
			if(mtmp->data->mlet != 'w'){
				fall_down(mtmp);
				if(in_sight)
		pline("Suddenly, %s disappears out of sight.", monnam(mtmp));
				return(2);	/* no longer on this level */
			}
			break;
		case PIERC:
			break;
		default:
			impossible("Some monster encountered a strange trap.");
	    }
	}
	return(mtmp->mtrapped);
}

selftouch(arg) char *arg; {
	if(uwep && uwep->otyp == DEAD_COCKATRICE){
		pline("%s touch the dead cockatrice.", arg);
		pline("You turn to stone.");
		killer = objects[uwep->otyp].oc_name;
		done("died");
	}
}

float_up(){
	if(u.utrap) {
		if(u.utraptype == TT_PIT) {
			u.utrap = 0;
			pline("You float up, out of the pit!");
		} else {
			pline("You float up, only your leg is still stuck.");
		}
	} else
		pline("You start to float in the air!");
}

float_down(){
	struct trap *trap;
	pline("You float gently to the ground.");
	if(trap = t_at(u.ux,u.uy))
		switch(trap->ttyp) {
		case PIERC:
			break;
		case TRAPDOOR:
			if(!xdnstair || u.ustuck) break;
			/* fall into next case */
		default:
			dotrap(trap);
	}
	pickup(1);
}

vtele() {
#include "def.mkroom.h"
	struct mkroom *croom;
	for(croom = &rooms[0]; croom->hx >= 0; croom++)
	    if(croom->rtype == VAULT) {
		int x,y;

		x = rn2(2) ? croom->lx : croom->hx;
		y = rn2(2) ? croom->ly : croom->hy;
		if(teleok(x,y)) {
		    teleds(x,y);
		    return;
		}
	    }
	tele();
}

tele() {
	extern coord getpos();
	coord cc;
	int nux,nuy;

	if(Teleport_control) {
		pline("To what position do you want to be teleported?");
		cc = getpos(1, "the desired position"); /* 1: force valid */
		/* possible extensions: introduce a small error if
		   magic power is low; allow transfer to solid rock */
		if(teleok(cc.x, cc.y)){
			teleds(cc.x, cc.y);
			return;
		}
		pline("Sorry ...");
	}
	do {
		nux = rnd(COLNO-1);
		nuy = rn2(ROWNO);
	} while(!teleok(nux, nuy));
	teleds(nux, nuy);
}

teleds(nux, nuy)
int nux,nuy;
{
	if(Punished) unplacebc();
	unsee();
	u.utrap = 0;
	u.ustuck = 0;
	u.ux = nux;
	u.uy = nuy;
	setsee();
	if(Punished) placebc(1);
	if(u.uswallow){
		u.uswldtim = u.uswallow = 0;
		docrt();
	}
	nomul(0);
	if(levl[nux][nuy].typ == POOL && !Levitation)
		drown();
	(void) inshop();
	pickup(1);
	if(!Blind) read_engr_at(u.ux,u.uy);
}

teleok(x,y) int x,y; {	/* might throw him into a POOL */
	return( isok(x,y) && !IS_ROCK(levl[x][y].typ) && !m_at(x,y) &&
		!sobj_at(ENORMOUS_ROCK,x,y) && !t_at(x,y)
	);
	/* Note: gold is permitted (because of vaults) */
}

dotele() {
	extern char pl_character[];

	if(
#ifdef WIZARD
	   !wizard &&
#endif WIZARD
		      (!Teleportation || u.ulevel < 6 ||
			(pl_character[0] != 'W' && u.ulevel < 10))) {
		pline("You are not able to teleport at will.");
		return(0);
	}
	if(u.uhunger <= 100 || u.ustr < 6) {
		pline("You miss the strength for a teleport spell.");
		return(1);
	}
	tele();
	morehungry(100);
	return(1);
}

placebc(attach) int attach; {
	if(!uchain || !uball){
		impossible("Where are your chain and ball??");
		return;
	}
	uball->ox = uchain->ox = u.ux;
	uball->oy = uchain->oy = u.uy;
	if(attach){
		uchain->nobj = fobj;
		fobj = uchain;
		if(!carried(uball)){
			uball->nobj = fobj;
			fobj = uball;
		}
	}
}

unplacebc(){
	if(!carried(uball)){
		freeobj(uball);
		unpobj(uball);
	}
	freeobj(uchain);
	unpobj(uchain);
}

level_tele() {
int newlevel;
	if(Teleport_control) {
	    char buf[BUFSZ];

	    do {
	      pline("To what level do you want to teleport? [type a number] ");
	      getlin(buf);
	    } while(!digit(buf[0]) && (buf[0] != '-' || !digit(buf[1])));
	    newlevel = atoi(buf);
	} else {
	    newlevel  = 5 + rn2(20);	/* 5 - 24 */
	    if(dlevel == newlevel) {
		if(!xdnstair) newlevel--; else newlevel++;
	    }
	}
	if(newlevel >= 30) {
	    if(newlevel > MAXLEVEL) newlevel = MAXLEVEL;
	    pline("You arrive at the center of the earth ...");
	    pline("Unfortunately it is here that hell is located.");
	    if(Fire_resistance) {
		pline("But the fire doesn't seem to harm you.");
	    } else {
		pline("You burn to a crisp.");
		dlevel = maxdlevel = newlevel;
		killer = "visit to the hell";
		done("burned");
	    }
	}
	if(newlevel < 0) {
	    newlevel = 0;
	    pline("You are now high above the clouds ...");
	    if(Levitation) {
		pline("You float gently down to earth.");
		done("escaped");
	    }
	    pline("Unfortunately, you don't know how to fly.");
	    pline("You fall down a few thousand feet and break your neck.");
	    dlevel = 0;
	    killer = "fall";
	    done("died");
	}

	goto_level(newlevel, FALSE); /* calls done("escaped") if newlevel==0 */
}

drown()
{
	pline("You fall into a pool!");
	pline("You can't swim!");
	if(rn2(3) < u.uluck+2) {
		/* most scrolls become unreadable */
		struct obj *obj;

		for(obj = invent; obj; obj = obj->nobj)
			if(obj->olet == SCROLL_SYM && rn2(12) > u.uluck)
				obj->otyp = SCR_BLANK_PAPER;
		/* we should perhaps merge these scrolls ? */

		pline("You attempt a teleport spell.");	/* utcsri!carroll */
		(void) dotele();
		if(levl[u.ux][u.uy].typ != POOL) return;
	}
	pline("You drown ...");
	killer = "pool of water";
	done("drowned");
}
