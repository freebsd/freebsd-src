/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.do.c - version 1.0.3 */

/* Contains code for 'd', 'D' (drop), '>', '<' (up, down) and 't' (throw) */

#include "hack.h"

extern struct obj *splitobj(), *addinv();
extern boolean hmon();
extern boolean level_exists[];
extern struct monst youmonst;
extern char *Doname();
extern char *nomovemsg;

static int drop();

dodrop() {
	return(drop(getobj("0$#", "drop")));
}

static int
drop(obj) register struct obj *obj; {
	if(!obj) return(0);
	if(obj->olet == '$') {		/* pseudo object */
		register long amount = OGOLD(obj);

		if(amount == 0)
			pline("You didn't drop any gold pieces.");
		else {
			mkgold(amount, u.ux, u.uy);
			pline("You dropped %ld gold piece%s.",
				amount, plur(amount));
			if(Invisible) newsym(u.ux, u.uy);
		}
		free((char *) obj);
		return(1);
	}
	if(obj->owornmask & (W_ARMOR | W_RING)){
		pline("You cannot drop something you are wearing.");
		return(0);
	}
	if(obj == uwep) {
		if(uwep->cursed) {
			pline("Your weapon is welded to your hand!");
			return(0);
		}
		setuwep((struct obj *) 0);
	}
	pline("You dropped %s.", doname(obj));
	dropx(obj);
	return(1);
}

/* Called in several places - should not produce texts */
dropx(obj)
register struct obj *obj;
{
	freeinv(obj);
	dropy(obj);
}

dropy(obj)
register struct obj *obj;
{
	if(obj->otyp == CRYSKNIFE)
		obj->otyp = WORM_TOOTH;
	obj->ox = u.ux;
	obj->oy = u.uy;
	obj->nobj = fobj;
	fobj = obj;
	if(Invisible) newsym(u.ux,u.uy);
	subfrombill(obj);
	stackobj(obj);
}

/* drop several things */
doddrop() {
	return(ggetobj("drop", drop, 0));
}

dodown()
{
	if(u.ux != xdnstair || u.uy != ydnstair) {
		pline("You can't go down here.");
		return(0);
	}
	if(u.ustuck) {
		pline("You are being held, and cannot go down.");
		return(1);
	}
	if(Levitation) {
		pline("You're floating high above the stairs.");
		return(0);
	}

	goto_level(dlevel+1, TRUE);
	return(1);
}

doup()
{
	if(u.ux != xupstair || u.uy != yupstair) {
		pline("You can't go up here.");
		return(0);
	}
	if(u.ustuck) {
		pline("You are being held, and cannot go up.");
		return(1);
	}
	if(!Levitation && inv_weight() + 5 > 0) {
		pline("Your load is too heavy to climb the stairs.");
		return(1);
	}

	goto_level(dlevel-1, TRUE);
	return(1);
}

goto_level(newlevel, at_stairs)
register int newlevel;
register boolean at_stairs;
{
	register fd;
	register boolean up = (newlevel < dlevel);

	if(newlevel <= 0) done("escaped");    /* in fact < 0 is impossible */
	if(newlevel > MAXLEVEL) newlevel = MAXLEVEL;	/* strange ... */
	if(newlevel == dlevel) return;	      /* this can happen */

	glo(dlevel);
	fd = creat(lock, FMASK);
	if(fd < 0) {
		/*
		 * This is not quite impossible: e.g., we may have
		 * exceeded our quota. If that is the case then we
		 * cannot leave this level, and cannot save either.
		 * Another possibility is that the directory was not
		 * writable.
		 */
		pline("A mysterious force prevents you from going %s.",
			up ? "up" : "down");
		return;
	}

	if(Punished) unplacebc();
	u.utrap = 0;				/* needed in level_tele */
	u.ustuck = 0;				/* idem */
	keepdogs();
	seeoff(1);
	if(u.uswallow)				/* idem */
		u.uswldtim = u.uswallow = 0;
	flags.nscrinh = 1;
	u.ux = FAR;				/* hack */
	(void) inshop();			/* probably was a trapdoor */

	savelev(fd,dlevel);
	(void) close(fd);

	dlevel = newlevel;
	if(maxdlevel < dlevel)
		maxdlevel = dlevel;
	glo(dlevel);

	if(!level_exists[dlevel])
		mklev();
	else {
		extern int hackpid;

		if((fd = open(lock,0)) < 0) {
			pline("Cannot open %s .", lock);
			pline("Probably someone removed it.");
			done("tricked");
		}
		getlev(fd, hackpid, dlevel);
		(void) close(fd);
	}

	if(at_stairs) {
	    if(up) {
		u.ux = xdnstair;
		u.uy = ydnstair;
		if(!u.ux) {		/* entering a maze from below? */
		    u.ux = xupstair;	/* this will confuse the player! */
		    u.uy = yupstair;
		}
		if(Punished && !Levitation){
			pline("With great effort you climb the stairs.");
			placebc(1);
		}
	    } else {
		u.ux = xupstair;
		u.uy = yupstair;
		if(inv_weight() + 5 > 0 || Punished){
			pline("You fall down the stairs.");	/* %% */
			losehp(rnd(3), "fall");
			if(Punished) {
			    if(uwep != uball && rn2(3)){
				pline("... and are hit by the iron ball.");
				losehp(rnd(20), "iron ball");
			    }
			    placebc(1);
			}
			selftouch("Falling, you");
		}
	    }
	    { register struct monst *mtmp = m_at(u.ux, u.uy);
	      if(mtmp)
		mnexto(mtmp);
	    }
	} else {	/* trapdoor or level_tele */
	    do {
		u.ux = rnd(COLNO-1);
		u.uy = rn2(ROWNO);
	    } while(levl[u.ux][u.uy].typ != ROOM ||
			m_at(u.ux,u.uy));
	    if(Punished){
		if(uwep != uball && !up /* %% */ && rn2(5)){
			pline("The iron ball falls on your head.");
			losehp(rnd(25), "iron ball");
		}
		placebc(1);
	    }
	    selftouch("Falling, you");
	}
	(void) inshop();
	initrack();

	losedogs();
	{ register struct monst *mtmp;
	  if(mtmp = m_at(u.ux, u.uy)) mnexto(mtmp);	/* riv05!a3 */
	}
	flags.nscrinh = 0;
	setsee();
	seeobjs();	/* make old cadavers disappear - riv05!a3 */
	docrt();
	pickup(1);
	read_engr_at(u.ux,u.uy);
}

donull() {
	return(1);	/* Do nothing, but let other things happen */
}

dopray() {
	nomovemsg = "You finished your prayer.";
	nomul(-3);
	return(1);
}

struct monst *bhit(), *boomhit();
dothrow()
{
	register struct obj *obj;
	register struct monst *mon;
	register tmp;

	obj = getobj("#)", "throw");   /* it is also possible to throw food */
				       /* (or jewels, or iron balls ... ) */
	if(!obj || !getdir(1))	       /* ask "in what direction?" */
		return(0);
	if(obj->owornmask & (W_ARMOR | W_RING)){
		pline("You can't throw something you are wearing.");
		return(0);
	}

	u_wipe_engr(2);

	if(obj == uwep){
		if(obj->cursed){
			pline("Your weapon is welded to your hand.");
			return(1);
		}
		if(obj->quan > 1)
			setuwep(splitobj(obj, 1));
		else
			setuwep((struct obj *) 0);
	}
	else if(obj->quan > 1)
		(void) splitobj(obj, 1);
	freeinv(obj);
	if(u.uswallow) {
		mon = u.ustuck;
		bhitpos.x = mon->mx;
		bhitpos.y = mon->my;
	} else if(u.dz) {
	  if(u.dz < 0) {
	    pline("%s hits the ceiling, then falls back on top of your head.",
		Doname(obj));		/* note: obj->quan == 1 */
	    if(obj->olet == POTION_SYM)
		potionhit(&youmonst, obj);
	    else {
		if(uarmh) pline("Fortunately, you are wearing a helmet!");
		losehp(uarmh ? 1 : rnd((int)(obj->owt)), "falling object");
		dropy(obj);
	    }
	  } else {
	    pline("%s hits the floor.", Doname(obj));
	    if(obj->otyp == EXPENSIVE_CAMERA) {
		pline("It is shattered in a thousand pieces!");
		obfree(obj, Null(obj));
	    } else if(obj->otyp == EGG) {
		pline("\"Splash!\"");
		obfree(obj, Null(obj));
	    } else if(obj->olet == POTION_SYM) {
		pline("The flask breaks, and you smell a peculiar odor ...");
		potionbreathe(obj);
		obfree(obj, Null(obj));
	    } else {
		dropy(obj);
	    }
	  }
	  return(1);
	} else if(obj->otyp == BOOMERANG) {
		mon = boomhit(u.dx, u.dy);
		if(mon == &youmonst) {		/* the thing was caught */
			(void) addinv(obj);
			return(1);
		}
	} else {
		if(obj->otyp == PICK_AXE && shkcatch(obj))
		    return(1);

		mon = bhit(u.dx, u.dy, (obj->otyp == ICE_BOX) ? 1 :
			(!Punished || obj != uball) ? 8 : !u.ustuck ? 5 : 1,
			obj->olet,
			(int (*)()) 0, (int (*)()) 0, obj);
	}
	if(mon) {
		/* awake monster if sleeping */
		wakeup(mon);

		if(obj->olet == WEAPON_SYM) {
			tmp = -1+u.ulevel+mon->data->ac+abon();
			if(obj->otyp < ROCK) {
				if(!uwep ||
				    uwep->otyp != obj->otyp+(BOW-ARROW))
					tmp -= 4;
				else {
					tmp += uwep->spe;
				}
			} else
			if(obj->otyp == BOOMERANG) tmp += 4;
			tmp += obj->spe;
			if(u.uswallow || tmp >= rnd(20)) {
				if(hmon(mon,obj,1) == TRUE){
				  /* mon still alive */
#ifndef NOWORM
				  cutworm(mon,bhitpos.x,bhitpos.y,obj->otyp);
#endif NOWORM
				} else mon = 0;
				/* weapons thrown disappear sometimes */
				if(obj->otyp < BOOMERANG && rn2(3)) {
					/* check bill; free */
					obfree(obj, (struct obj *) 0);
					return(1);
				}
			} else miss(objects[obj->otyp].oc_name, mon);
		} else if(obj->otyp == HEAVY_IRON_BALL) {
			tmp = -1+u.ulevel+mon->data->ac+abon();
			if(!Punished || obj != uball) tmp += 2;
			if(u.utrap) tmp -= 2;
			if(u.uswallow || tmp >= rnd(20)) {
				if(hmon(mon,obj,1) == FALSE)
					mon = 0;	/* he died */
			} else miss("iron ball", mon);
		} else if(obj->olet == POTION_SYM && u.ulevel > rn2(15)) {
			potionhit(mon, obj);
			return(1);
		} else {
			if(cansee(bhitpos.x,bhitpos.y))
				pline("You miss %s.",monnam(mon));
			else pline("You miss it.");
			if(obj->olet == FOOD_SYM && mon->data->mlet == 'd')
				if(tamedog(mon,obj)) return(1);
			if(obj->olet == GEM_SYM && mon->data->mlet == 'u' &&
				!mon->mtame){
			 if(obj->dknown && objects[obj->otyp].oc_name_known){
			  if(objects[obj->otyp].g_val > 0){
			    u.uluck += 5;
			    goto valuable;
			  } else {
			    pline("%s is not interested in your junk.",
				Monnam(mon));
			  }
			 } else { /* value unknown to @ */
			    u.uluck++;
			valuable:
			    if(u.uluck > LUCKMAX)	/* dan@ut-ngp */
				u.uluck = LUCKMAX;
			    pline("%s graciously accepts your gift.",
				Monnam(mon));
			    mpickobj(mon, obj);
			    rloc(mon);
			    return(1);
			 }
			}
		}
	}
		/* the code following might become part of dropy() */
	if(obj->otyp == CRYSKNIFE)
		obj->otyp = WORM_TOOTH;
	obj->ox = bhitpos.x;
	obj->oy = bhitpos.y;
	obj->nobj = fobj;
	fobj = obj;
	/* prevent him from throwing articles to the exit and escaping */
	/* subfrombill(obj); */
	stackobj(obj);
	if(Punished && obj == uball &&
		(bhitpos.x != u.ux || bhitpos.y != u.uy)){
		freeobj(uchain);
		unpobj(uchain);
		if(u.utrap){
			if(u.utraptype == TT_PIT)
				pline("The ball pulls you out of the pit!");
			else {
			    register long side =
				rn2(3) ? LEFT_SIDE : RIGHT_SIDE;
			    pline("The ball pulls you out of the bear trap.");
			    pline("Your %s leg is severely damaged.",
				(side == LEFT_SIDE) ? "left" : "right");
			    set_wounded_legs(side, 500+rn2(1000));
			    losehp(2, "thrown ball");
			}
			u.utrap = 0;
		}
		unsee();
		uchain->nobj = fobj;
		fobj = uchain;
		u.ux = uchain->ox = bhitpos.x - u.dx;
		u.uy = uchain->oy = bhitpos.y - u.dy;
		setsee();
		(void) inshop();
	}
	if(cansee(bhitpos.x, bhitpos.y)) prl(bhitpos.x,bhitpos.y);
	return(1);
}

/* split obj so that it gets size num */
/* remainder is put in the object structure delivered by this call */
struct obj *
splitobj(obj, num) register struct obj *obj; register int num; {
register struct obj *otmp;
	otmp = newobj(0);
	*otmp = *obj;		/* copies whole structure */
	otmp->o_id = flags.ident++;
	otmp->onamelth = 0;
	obj->quan = num;
	obj->owt = weight(obj);
	otmp->quan -= num;
	otmp->owt = weight(otmp);	/* -= obj->owt ? */
	obj->nobj = otmp;
	if(obj->unpaid) splitbill(obj,otmp);
	return(otmp);
}

more_experienced(exp,rexp)
register int exp, rexp;
{
	extern char pl_character[];

	u.uexp += exp;
	u.urexp += 4*exp + rexp;
	if(exp) flags.botl = 1;
	if(u.urexp >= ((pl_character[0] == 'W') ? 1000 : 2000))
		flags.beginner = 0;
}

set_wounded_legs(side, timex)
register long side;
register int timex;
{
	if(!Wounded_legs || (Wounded_legs & TIMEOUT))
		Wounded_legs |= side + timex;
	else
		Wounded_legs |= side;
}

heal_legs()
{
	if(Wounded_legs) {
		if((Wounded_legs & BOTH_SIDES) == BOTH_SIDES)
			pline("Your legs feel somewhat better.");
		else
			pline("Your leg feels somewhat better.");
		Wounded_legs = 0;
	}
}
