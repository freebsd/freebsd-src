/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.potion.c - version 1.0.3 */

#include "hack.h"
extern int float_down();
extern char *nomovemsg;
extern struct monst youmonst;
extern struct monst *makemon();

dodrink() {
	register struct obj *otmp,*objs;
	register struct monst *mtmp;
	register int unkn = 0, nothing = 0;

	otmp = getobj("!", "drink");
	if(!otmp) return(0);
	if(!strcmp(objects[otmp->otyp].oc_descr, "smoky") && !rn2(13)) {
		ghost_from_bottle();
		goto use_it;
	}
	switch(otmp->otyp){
	case POT_RESTORE_STRENGTH:
		unkn++;
		pline("Wow!  This makes you feel great!");
		if(u.ustr < u.ustrmax) {
			u.ustr = u.ustrmax;
			flags.botl = 1;
		}
		break;
	case POT_BOOZE:
		unkn++;
		pline("Ooph!  This tastes like liquid fire!");
		Confusion += d(3,8);
		/* the whiskey makes us feel better */
		if(u.uhp < u.uhpmax) losehp(-1, "bottle of whiskey");
		if(!rn2(4)) {
			pline("You pass out.");
			multi = -rnd(15);
			nomovemsg = "You awake with a headache.";
		}
		break;
	case POT_INVISIBILITY:
		if(Invis || See_invisible)
		  nothing++;
		else {
		  if(!Blind)
		    pline("Gee!  All of a sudden, you can't see yourself.");
		  else
		    pline("You feel rather airy."), unkn++;
		  newsym(u.ux,u.uy);
		}
		Invis += rn1(15,31);
		break;
	case POT_FRUIT_JUICE:
		pline("This tastes like fruit juice.");
		lesshungry(20);
		break;
	case POT_HEALING:
		pline("You begin to feel better.");
		flags.botl = 1;
		u.uhp += rnd(10);
		if(u.uhp > u.uhpmax)
			u.uhp = ++u.uhpmax;
		if(Blind) Blind = 1;	/* see on next move */
		if(Sick) Sick = 0;
		break;
	case POT_PARALYSIS:
		if(Levitation)
			pline("You are motionlessly suspended.");
		else
			pline("Your feet are frozen to the floor!");
		nomul(-(rn1(10,25)));
		break;
	case POT_MONSTER_DETECTION:
		if(!fmon) {
			strange_feeling(otmp, "You feel threatened.");
			return(1);
		} else {
			cls();
			for(mtmp = fmon; mtmp; mtmp = mtmp->nmon)
				if(mtmp->mx > 0)
				at(mtmp->mx,mtmp->my,mtmp->data->mlet);
			prme();
			pline("You sense the presence of monsters.");
			more();
			docrt();
		}
		break;
	case POT_OBJECT_DETECTION:
		if(!fobj) {
			strange_feeling(otmp, "You feel a pull downward.");
			return(1);
		} else {
		    for(objs = fobj; objs; objs = objs->nobj)
			if(objs->ox != u.ux || objs->oy != u.uy)
				goto outobjmap;
		    pline("You sense the presence of objects close nearby.");
		    break;
		outobjmap:
			cls();
			for(objs = fobj; objs; objs = objs->nobj)
				at(objs->ox,objs->oy,objs->olet);
			prme();
			pline("You sense the presence of objects.");
			more();
			docrt();
		}
		break;
	case POT_SICKNESS:
		pline("Yech! This stuff tastes like poison.");
		if(Poison_resistance)
    pline("(But in fact it was biologically contaminated orange juice.)");
		losestr(rn1(4,3));
		losehp(rnd(10), "contaminated potion");
		break;
	case POT_CONFUSION:
		if(!Confusion)
			pline("Huh, What?  Where am I?");
		else
			nothing++;
		Confusion += rn1(7,16);
		break;
	case POT_GAIN_STRENGTH:
		pline("Wow do you feel strong!");
		if(u.ustr >= 118) break;	/* > 118 is impossible */
		if(u.ustr > 17) u.ustr += rnd(118-u.ustr);
		else u.ustr++;
		if(u.ustr > u.ustrmax) u.ustrmax = u.ustr;
		flags.botl = 1;
		break;
	case POT_SPEED:
		if(Wounded_legs) {
			heal_legs();
			unkn++;
			break;
		}
		if(!(Fast & ~INTRINSIC))
			pline("You are suddenly moving much faster.");
		else
			pline("Your legs get new energy."), unkn++;
		Fast += rn1(10,100);
		break;
	case POT_BLINDNESS:
		if(!Blind)
			pline("A cloud of darkness falls upon you.");
		else
			nothing++;
		Blind += rn1(100,250);
		seeoff(0);
		break;
	case POT_GAIN_LEVEL: 
		pluslvl();
		break;
	case POT_EXTRA_HEALING:
		pline("You feel much better.");
		flags.botl = 1;
		u.uhp += d(2,20)+1;
		if(u.uhp > u.uhpmax)
			u.uhp = (u.uhpmax += 2);
		if(Blind) Blind = 1;
		if(Sick) Sick = 0;
		break;
	case POT_LEVITATION:
		if(!Levitation)
			float_up();
		else
			nothing++;
		Levitation += rnd(100);
		u.uprops[PROP(RIN_LEVITATION)].p_tofn = float_down;
		break;
	default:
		impossible("What a funny potion! (%u)", otmp->otyp);
		return(0);
	}
	if(nothing) {
	    unkn++;
	    pline("You have a peculiar feeling for a moment, then it passes.");
	}
	if(otmp->dknown && !objects[otmp->otyp].oc_name_known) {
		if(!unkn) {
			objects[otmp->otyp].oc_name_known = 1;
			more_experienced(0,10);
		} else if(!objects[otmp->otyp].oc_uname)
			docall(otmp);
	}
use_it:
	useup(otmp);
	return(1);
}

pluslvl()
{
	register num;

	pline("You feel more experienced.");
	num = rnd(10);
	u.uhpmax += num;
	u.uhp += num;
	if(u.ulevel < 14) {
		extern long newuexp();

		u.uexp = newuexp()+1;
		pline("Welcome to experience level %u.", ++u.ulevel);
	}
	flags.botl = 1;
}

strange_feeling(obj,txt)
register struct obj *obj;
register char *txt;
{
	if(flags.beginner)
	    pline("You have a strange feeling for a moment, then it passes.");
	else
	    pline(txt);
	if(!objects[obj->otyp].oc_name_known && !objects[obj->otyp].oc_uname)
		docall(obj);
	useup(obj);
}

char *bottlenames[] = {
	"bottle", "phial", "flagon", "carafe", "flask", "jar", "vial"
};

potionhit(mon, obj)
register struct monst *mon;
register struct obj *obj;
{
	extern char *xname();
	register char *botlnam = bottlenames[rn2(SIZE(bottlenames))];
	boolean uclose, isyou = (mon == &youmonst);

	if(isyou) {
		uclose = TRUE;
		pline("The %s crashes on your head and breaks into shivers.",
			botlnam);
		losehp(rnd(2), "thrown potion");
	} else {
		uclose = (dist(mon->mx,mon->my) < 3);
		/* perhaps 'E' and 'a' have no head? */
		pline("The %s crashes on %s's head and breaks into shivers.",
			botlnam, monnam(mon));
		if(rn2(5) && mon->mhp > 1)
			mon->mhp--;
	}
	pline("The %s evaporates.", xname(obj));

	if(!isyou && !rn2(3)) switch(obj->otyp) {

	case POT_RESTORE_STRENGTH:
	case POT_GAIN_STRENGTH:
	case POT_HEALING:
	case POT_EXTRA_HEALING:
		if(mon->mhp < mon->mhpmax) {
			mon->mhp = mon->mhpmax;
			pline("%s looks sound and hale again!", Monnam(mon));
		}
		break;
	case POT_SICKNESS:
		if(mon->mhpmax > 3)
			mon->mhpmax /= 2;
		if(mon->mhp > 2)
			mon->mhp /= 2;
		break;
	case POT_CONFUSION:
	case POT_BOOZE:
		mon->mconf = 1;
		break;
	case POT_INVISIBILITY:
		unpmon(mon);
		mon->minvis = 1;
		pmon(mon);
		break;
	case POT_PARALYSIS:
		mon->mfroz = 1;
		break;
	case POT_SPEED:
		mon->mspeed = MFAST;
		break;
	case POT_BLINDNESS:
		mon->mblinded |= 64 + rn2(64);
		break;
/*	
	case POT_GAIN_LEVEL:
	case POT_LEVITATION:
	case POT_FRUIT_JUICE:
	case POT_MONSTER_DETECTION:
	case POT_OBJECT_DETECTION:
		break;
*/
	}
	if(uclose && rn2(5))
		potionbreathe(obj);
	obfree(obj, Null(obj));
}

potionbreathe(obj)
register struct obj *obj;
{
	switch(obj->otyp) {
	case POT_RESTORE_STRENGTH:
	case POT_GAIN_STRENGTH:
		if(u.ustr < u.ustrmax) u.ustr++, flags.botl = 1;
		break;
	case POT_HEALING:
	case POT_EXTRA_HEALING:
		if(u.uhp < u.uhpmax) u.uhp++, flags.botl = 1;
		break;
	case POT_SICKNESS:
		if(u.uhp <= 5) u.uhp = 1; else u.uhp -= 5;
		flags.botl = 1;
		break;
	case POT_CONFUSION:
	case POT_BOOZE:
		if(!Confusion)
			pline("You feel somewhat dizzy.");
		Confusion += rnd(5);
		break;
	case POT_INVISIBILITY:
		pline("For an instant you couldn't see your right hand.");
		break;
	case POT_PARALYSIS:
		pline("Something seems to be holding you.");
		nomul(-rnd(5));
		break;
	case POT_SPEED:
		Fast += rnd(5);
		pline("Your knees seem more flexible now.");
		break;
	case POT_BLINDNESS:
		if(!Blind) pline("It suddenly gets dark.");
		Blind += rnd(5);
		seeoff(0);
		break;
/*	
	case POT_GAIN_LEVEL:
	case POT_LEVITATION:
	case POT_FRUIT_JUICE:
	case POT_MONSTER_DETECTION:
	case POT_OBJECT_DETECTION:
		break;
*/
	}
	/* note: no obfree() */
}

/*
 * -- rudimentary -- to do this correctly requires much more work
 * -- all sharp weapons get one or more qualities derived from the potions
 * -- texts on scrolls may be (partially) wiped out; do they become blank?
 * --   or does their effect change, like under Confusion?
 * -- all objects may be made invisible by POT_INVISIBILITY
 * -- If the flask is small, can one dip a large object? Does it magically
 * --   become a jug? Etc.
 */
dodip(){
	register struct obj *potion, *obj;

	if(!(obj = getobj("#", "dip")))
		return(0);
	if(!(potion = getobj("!", "dip into")))
		return(0);
	pline("Interesting...");
	if(obj->otyp == ARROW || obj->otyp == DART ||
	   obj->otyp == CROSSBOW_BOLT) {
		if(potion->otyp == POT_SICKNESS) {
			useup(potion);
			if(obj->spe < 7) obj->spe++;	/* %% */
		}
	}
	return(1);
}

ghost_from_bottle(){
	extern struct permonst pm_ghost;
	register struct monst *mtmp;

	if(!(mtmp = makemon(PM_GHOST,u.ux,u.uy))){
		pline("This bottle turns out to be empty.");
		return;
	}
	mnexto(mtmp);
	pline("As you open the bottle, an enormous ghost emerges!");
	pline("You are frightened to death, and unable to move.");
	nomul(-3);
}
