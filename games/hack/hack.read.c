/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.read.c - version 1.0.3 */
/* $FreeBSD$ */

#include "hack.h"

extern struct monst *makemon();
extern struct obj *mkobj_at();
int identify();

doread() {
	struct obj *scroll;
	boolean confused = (Confusion != 0);
	boolean known = FALSE;
	extern struct obj *some_armor();

	scroll = getobj("?", "read");
	if(!scroll) return(0);
	if(!scroll->dknown && Blind) {
	    pline("Being blind, you cannot read the formula on the scroll.");
	    return(0);
	}
	if(Blind)
	  pline("As you pronounce the formula on it, the scroll disappears.");
	else
	  pline("As you read the scroll, it disappears.");
	if(confused)
	  pline("Being confused, you mispronounce the magic words ... ");

	switch(scroll->otyp) {
#ifdef MAIL
	case SCR_MAIL:
		readmail(/* scroll */);
		break;
#endif MAIL
	case SCR_ENCHANT_ARMOR:
	    {	struct obj *otmp = some_armor();
		if(!otmp) {
			strange_feeling(scroll,"Your skin glows then fades.");
			return(1);
		}
		if(confused) {
			pline("Your %s glows silver for a moment.",
				objects[otmp->otyp].oc_name);
			otmp->rustfree = 1;
			break;
		}
		if(otmp->spe > 3 && rn2(otmp->spe)) {
	pline("Your %s glows violently green for a while, then evaporates.",
			objects[otmp->otyp].oc_name);
			useup(otmp);
			break;
		}
		pline("Your %s glows green for a moment.",
			objects[otmp->otyp].oc_name);
		otmp->cursed = 0;
		otmp->spe++;
		break;
	    }
	case SCR_DESTROY_ARMOR:
		if(confused) {
			struct obj *otmp = some_armor();
			if(!otmp) {
				strange_feeling(scroll,"Your bones itch.");
				return(1);
			}
			pline("Your %s glows purple for a moment.",
				objects[otmp->otyp].oc_name);
			otmp->rustfree = 0;
			break;
		}
		if(uarm) {
		    pline("Your armor turns to dust and falls to the floor!");
		    useup(uarm);
		} else if(uarmh) {
		    pline("Your helmet turns to dust and is blown away!");
		    useup(uarmh);
		} else if(uarmg) {
			pline("Your gloves vanish!");
			useup(uarmg);
			selftouch("You");
		} else {
			strange_feeling(scroll,"Your skin itches.");
			return(1);
		}
		break;
	case SCR_CONFUSE_MONSTER:
		if(confused) {
			pline("Your hands begin to glow purple.");
			Confusion += rnd(100);
		} else {
			pline("Your hands begin to glow blue.");
			u.umconf = 1;
		}
		break;
	case SCR_SCARE_MONSTER:
	    {	int ct = 0;
		struct monst *mtmp;

		for(mtmp = fmon; mtmp; mtmp = mtmp->nmon)
			if(cansee(mtmp->mx,mtmp->my)) {
				if(confused)
					mtmp->mflee = mtmp->mfroz =
					mtmp->msleep = 0;
				else
					mtmp->mflee = 1;
				ct++;
			}
		if(!ct) {
		    if(confused)
			pline("You hear sad wailing in the distance.");
		    else
			pline("You hear maniacal laughter in the distance.");
		}
		break;
	    }
	case SCR_BLANK_PAPER:
		if(confused)
		    pline("You see strange patterns on this scroll.");
		else
		    pline("This scroll seems to be blank.");
		break;
	case SCR_REMOVE_CURSE:
	    {	struct obj *obj;
		if(confused)
		  pline("You feel like you need some help.");
		else
		  pline("You feel like someone is helping you.");
		for(obj = invent; obj ; obj = obj->nobj)
			if(obj->owornmask)
				obj->cursed = confused;
		if(Punished && !confused) {
			Punished = 0;
			freeobj(uchain);
			unpobj(uchain);
			free((char *) uchain);
			uball->spe = 0;
			uball->owornmask &= ~W_BALL;
			uchain = uball = (struct obj *) 0;
		}
		break;
	    }
	case SCR_CREATE_MONSTER:
	    {	int cnt = 1;

		if(!rn2(73)) cnt += rnd(4);
		if(confused) cnt += 12;
		while(cnt--)
		    (void) makemon(confused ? PM_ACID_BLOB :
			(struct permonst *) 0, u.ux, u.uy);
		break;
	    }
	case SCR_ENCHANT_WEAPON:
		if(uwep && confused) {
			pline("Your %s glows silver for a moment.",
				objects[uwep->otyp].oc_name);
			uwep->rustfree = 1;
		} else
			if(!chwepon(scroll, 1))		/* tests for !uwep */
				return(1);
		break;
	case SCR_DAMAGE_WEAPON:
		if(uwep && confused) {
			pline("Your %s glows purple for a moment.",
				objects[uwep->otyp].oc_name);
			uwep->rustfree = 0;
		} else
			if(!chwepon(scroll, -1))	/* tests for !uwep */
				return(1);
		break;
	case SCR_TAMING:
	    {	int i,j;
		int bd = confused ? 5 : 1;
		struct monst *mtmp;

		for(i = -bd; i <= bd; i++) for(j = -bd; j <= bd; j++)
		if(mtmp = m_at(u.ux+i, u.uy+j))
			(void) tamedog(mtmp, (struct obj *) 0);
		break;
	    }
	case SCR_GENOCIDE:
	    {	extern char genocided[], fut_geno[];
		char buf[BUFSZ];
		struct monst *mtmp, *mtmp2;

		pline("You have found a scroll of genocide!");
		known = TRUE;
		if(confused)
			*buf = u.usym;
		else do {
	    pline("What monster do you want to genocide (Type the letter)? ");
			getlin(buf);
		} while(strlen(buf) != 1 || !monstersym(*buf));
		if(!index(fut_geno, *buf))
			charcat(fut_geno, *buf);
		if(!index(genocided, *buf))
			charcat(genocided, *buf);
		else {
			pline("Such monsters do not exist in this world.");
			break;
		}
		for(mtmp = fmon; mtmp; mtmp = mtmp2){
			mtmp2 = mtmp->nmon;
			if(mtmp->data->mlet == *buf)
				mondead(mtmp);
		}
		pline("Wiped out all %c's.", *buf);
		if(*buf == u.usym) {
			killer = "scroll of genocide";
			u.uhp = -1;
		}
		break;
		}
	case SCR_LIGHT:
		if(!Blind) known = TRUE;
		litroom(!confused);
		break;
	case SCR_TELEPORTATION:
		if(confused)
			level_tele();
		else {
#ifdef QUEST
			int oux = u.ux, ouy = u.uy;
			tele();
			if(dist(oux, ouy) > 100) known = TRUE;
#else QUEST
			int uroom = inroom(u.ux, u.uy);
			tele();
			if(uroom != inroom(u.ux, u.uy)) known = TRUE;
#endif QUEST
		}
		break;
	case SCR_GOLD_DETECTION:
	    /* Unfortunately this code has become slightly less elegant,
	       now that gold and traps no longer are of the same type. */
	    if(confused) {
		struct trap *ttmp;

		if(!ftrap) {
			strange_feeling(scroll, "Your toes stop itching.");
			return(1);
		} else {
			for(ttmp = ftrap; ttmp; ttmp = ttmp->ntrap)
				if(ttmp->tx != u.ux || ttmp->ty != u.uy)
					goto outtrapmap;
			/* only under me - no separate display required */
			pline("Your toes itch!");
			break;
		outtrapmap:
			cls();
			for(ttmp = ftrap; ttmp; ttmp = ttmp->ntrap)
				at(ttmp->tx, ttmp->ty, '$');
			prme();
			pline("You feel very greedy!");
		}
	    } else {
		struct gold *gtmp;

		if(!fgold) {
			strange_feeling(scroll, "You feel materially poor.");
			return(1);
		} else {
			known = TRUE;
			for(gtmp = fgold; gtmp; gtmp = gtmp->ngold)
				if(gtmp->gx != u.ux || gtmp->gy != u.uy)
					goto outgoldmap;
			/* only under me - no separate display required */
			pline("You notice some gold between your feet.");
			break;
		outgoldmap:
			cls();
			for(gtmp = fgold; gtmp; gtmp = gtmp->ngold)
				at(gtmp->gx, gtmp->gy, '$');
			prme();
			pline("You feel very greedy, and sense gold!");
		}
	    }
		/* common sequel */
		more();
		docrt();
		break;
	case SCR_FOOD_DETECTION:
	    {	int ct = 0, ctu = 0;
		struct obj *obj;
		char foodsym = confused ? POTION_SYM : FOOD_SYM;

		for(obj = fobj; obj; obj = obj->nobj)
			if(obj->olet == FOOD_SYM) {
				if(obj->ox == u.ux && obj->oy == u.uy) ctu++;
				else ct++;
			}
		if(!ct && !ctu) {
			strange_feeling(scroll,"Your nose twitches.");
			return(1);
		} else if(!ct) {
			known = TRUE;
			pline("You smell %s close nearby.",
				confused ? "something" : "food");

		} else {
			known = TRUE;
			cls();
			for(obj = fobj; obj; obj = obj->nobj)
			    if(obj->olet == foodsym)
				at(obj->ox, obj->oy, FOOD_SYM);
			prme();
			pline("Your nose tingles and you smell %s!",
				confused ? "something" : "food");
			more();
			docrt();
		}
		break;
	    }
	case SCR_IDENTIFY:
		/* known = TRUE; */
		if(confused)
			pline("You identify this as an identify scroll.");
		else
			pline("This is an identify scroll.");
		useup(scroll);
		objects[SCR_IDENTIFY].oc_name_known = 1;
		if(!confused)
		    while(
			!ggetobj("identify", identify, rn2(5) ? 1 : rn2(5))
			&& invent
		    );
		return(1);
	case SCR_MAGIC_MAPPING:
	    {	struct rm *lev;
		int num, zx, zy;

		known = TRUE;
		pline("On this scroll %s a map!",
			confused ? "was" : "is");
		for(zy = 0; zy < ROWNO; zy++)
			for(zx = 0; zx < COLNO; zx++) {
				if(confused && rn2(7)) continue;
				lev = &(levl[zx][zy]);
				if((num = lev->typ) == 0)
					continue;
				if(num == SCORR) {
					lev->typ = CORR;
					lev->scrsym = CORR_SYM;
				} else
				if(num == SDOOR) {
					lev->typ = DOOR;
					lev->scrsym = '+';
					/* do sth in doors ? */
				} else if(lev->seen) continue;
#ifndef QUEST
				if(num != ROOM)
#endif QUEST
				{
				  lev->seen = lev->new = 1;
				  if(lev->scrsym == ' ' || !lev->scrsym)
				    newsym(zx,zy);
				  else
				    on_scr(zx,zy);
				}
			}
		break;
	    }
	case SCR_AMNESIA:
	    {	int zx, zy;

		known = TRUE;
		for(zx = 0; zx < COLNO; zx++) for(zy = 0; zy < ROWNO; zy++)
		    if(!confused || rn2(7))
			if(!cansee(zx,zy))
			    levl[zx][zy].seen = 0;
		docrt();
		pline("Thinking of Maud you forget everything else.");
		break;
	    }
	case SCR_FIRE:
	    {	int num;
		struct monst *mtmp;

		known = TRUE;
		if(confused) {
		    pline("The scroll catches fire and you burn your hands.");
		    losehp(1, "scroll of fire");
		} else {
		    pline("The scroll erupts in a tower of flame!");
		    if(Fire_resistance)
			pline("You are uninjured.");
		    else {
			num = rnd(6);
			u.uhpmax -= num;
			losehp(num, "scroll of fire");
		    }
		}
		num = (2*num + 1)/3;
		for(mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
		    if(dist(mtmp->mx,mtmp->my) < 3) {
			mtmp->mhp -= num;
			if(index("FY", mtmp->data->mlet))
			    mtmp->mhp -= 3*num;	/* this might well kill 'F's */
			if(mtmp->mhp < 1) {
			    killed(mtmp);
			    break;		/* primitive */
			}
		    }
		}
		break;
	    }
	case SCR_PUNISHMENT:
		known = TRUE;
		if(confused) {
			pline("You feel guilty.");
			break;
		}
		pline("You are being punished for your misbehaviour!");
		if(Punished){
			pline("Your iron ball gets heavier.");
			uball->owt += 15;
			break;
		}
		Punished = INTRINSIC;
		setworn(mkobj_at(CHAIN_SYM, u.ux, u.uy), W_CHAIN);
		setworn(mkobj_at(BALL_SYM, u.ux, u.uy), W_BALL);
		uball->spe = 1;		/* special ball (see save) */
		break;
	default:
		impossible("What weird language is this written in? (%u)",
			scroll->otyp);
	}
	if(!objects[scroll->otyp].oc_name_known) {
		if(known && !confused) {
			objects[scroll->otyp].oc_name_known = 1;
			more_experienced(0,10);
		} else if(!objects[scroll->otyp].oc_uname)
			docall(scroll);
	}
	useup(scroll);
	return(1);
}

identify(otmp)		/* also called by newmail() */
struct obj *otmp;
{
	objects[otmp->otyp].oc_name_known = 1;
	otmp->known = otmp->dknown = 1;
	prinv(otmp);
	return(1);
}

litroom(on)
boolean on;
{
	int num,zx,zy;

	/* first produce the text (provided he is not blind) */
	if(Blind) goto do_it;
	if(!on) {
		if(u.uswallow || !xdnstair || levl[u.ux][u.uy].typ == CORR ||
		    !levl[u.ux][u.uy].lit) {
			pline("It seems even darker in here than before.");
			return;
		} else
			pline("It suddenly becomes dark in here.");
	} else {
		if(u.uswallow){
			pline("%s's stomach is lit.", Monnam(u.ustuck));
			return;
		}
		if(!xdnstair){
			pline("Nothing Happens.");
			return;
		}
#ifdef QUEST
		pline("The cave lights up around you, then fades.");
		return;
#else QUEST
		if(levl[u.ux][u.uy].typ == CORR) {
		    pline("The corridor lights up around you, then fades.");
		    return;
		} else if(levl[u.ux][u.uy].lit) {
		    pline("The light here seems better now.");
		    return;
		} else
		    pline("The room is lit.");
#endif QUEST
	}

do_it:
#ifdef QUEST
	return;
#else QUEST
	if(levl[u.ux][u.uy].lit == on)
		return;
	if(levl[u.ux][u.uy].typ == DOOR) {
		if(IS_ROOM(levl[u.ux][u.uy+1].typ)) zy = u.uy+1;
		else if(IS_ROOM(levl[u.ux][u.uy-1].typ)) zy = u.uy-1;
		else zy = u.uy;
		if(IS_ROOM(levl[u.ux+1][u.uy].typ)) zx = u.ux+1;
		else if(IS_ROOM(levl[u.ux-1][u.uy].typ)) zx = u.ux-1;
		else zx = u.ux;
	} else {
		zx = u.ux;
		zy = u.uy;
	}
	for(seelx = u.ux; (num = levl[seelx-1][zy].typ) != CORR && num != 0;
		seelx--);
	for(seehx = u.ux; (num = levl[seehx+1][zy].typ) != CORR && num != 0;
		seehx++);
	for(seely = u.uy; (num = levl[zx][seely-1].typ) != CORR && num != 0;
		seely--);
	for(seehy = u.uy; (num = levl[zx][seehy+1].typ) != CORR && num != 0;
		seehy++);
	for(zy = seely; zy <= seehy; zy++)
		for(zx = seelx; zx <= seehx; zx++) {
			levl[zx][zy].lit = on;
			if(!Blind && dist(zx,zy) > 2) {
				if(on) prl(zx,zy); else nosee(zx,zy);
			}
		}
	if(!on) seehx = 0;
#endif	QUEST
}

/* Test whether we may genocide all monsters with symbol  ch  */
monstersym(ch)				/* arnold@ucsfcgl */
char ch;
{
	struct permonst *mp;
	extern struct permonst pm_eel;

	/*
	 * can't genocide certain monsters
	 */
	if (index("12 &:", ch))
		return FALSE;

	if (ch == pm_eel.mlet)
		return TRUE;
	for (mp = mons; mp < &mons[CMNUM+2]; mp++)
		if (mp->mlet == ch)
			return TRUE;
	return FALSE;
}
