/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.c - version 1.0.3 */
/* $FreeBSD$ */

#include "hack.h"
#include <stdio.h>

extern char news0();
extern char *nomovemsg;
extern char *exclam();
extern struct obj *addinv();
extern boolean hmon();

/* called on movement:
	1. when throwing ball+chain far away
	2. when teleporting
	3. when walking out of a lit room
 */
unsee() {
	int x,y;
	struct rm *lev;

/*
	if(u.udispl){
		u.udispl = 0;
		newsym(u.udisx, u.udisy);
	}
*/
#ifndef QUEST
	if(seehx){
		seehx = 0;
	} else
#endif QUEST
	for(x = u.ux-1; x < u.ux+2; x++)
	  for(y = u.uy-1; y < u.uy+2; y++) {
		if(!isok(x, y)) continue;
		lev = &levl[x][y];
		if(!lev->lit && lev->scrsym == '.') {
			lev->scrsym =' ';
			lev->new = 1;
			on_scr(x,y);
		}
	}
}

/* called:
	in hack.eat.c: seeoff(0) - blind after eating rotten food
	in hack.mon.c: seeoff(0) - blinded by a yellow light
	in hack.mon.c: seeoff(1) - swallowed
	in hack.do.c:  seeoff(0) - blind after drinking potion
	in hack.do.c:  seeoff(1) - go up or down the stairs
	in hack.trap.c:seeoff(1) - fall through trapdoor
 */
seeoff(mode)	/* 1 to redo @, 0 to leave them */
{	/* 1 means misc movement, 0 means blindness */
	int x,y;
	struct rm *lev;

	if(u.udispl && mode){
		u.udispl = 0;
		levl[u.udisx][u.udisy].scrsym = news0(u.udisx,u.udisy);
	}
#ifndef QUEST
	if(seehx) {
		seehx = 0;
	} else
#endif QUEST
	if(!mode) {
		for(x = u.ux-1; x < u.ux+2; x++)
			for(y = u.uy-1; y < u.uy+2; y++) {
				if(!isok(x, y)) continue;
				lev = &levl[x][y];
				if(!lev->lit && lev->scrsym == '.')
					lev->seen = 0;
			}
	}
}

domove()
{
	xchar oldx,oldy;
	struct monst *mtmp;
	struct rm *tmpr,*ust;
	struct trap *trap;
	struct obj *otmp;

	u_wipe_engr(rnd(5));

	if(inv_weight() > 0){
		pline("You collapse under your load.");
		nomul(0);
		return;
	}
	if(u.uswallow) {
		u.dx = u.dy = 0;
		u.ux = u.ustuck->mx;
		u.uy = u.ustuck->my;
	} else {
		if(Confusion) {
			do {
				confdir();
			} while(!isok(u.ux+u.dx, u.uy+u.dy) ||
			    IS_ROCK(levl[u.ux+u.dx][u.uy+u.dy].typ));
		}
		if(!isok(u.ux+u.dx, u.uy+u.dy)){
			nomul(0);
			return;
		}
	}

	ust = &levl[u.ux][u.uy];
	oldx = u.ux;
	oldy = u.uy;
	if(!u.uswallow && (trap = t_at(u.ux+u.dx, u.uy+u.dy)) && trap->tseen)
		nomul(0);
	if(u.ustuck && !u.uswallow && (u.ux+u.dx != u.ustuck->mx ||
		u.uy+u.dy != u.ustuck->my)) {
		if(dist(u.ustuck->mx, u.ustuck->my) > 2){
			/* perhaps it fled (or was teleported or ... ) */
			u.ustuck = 0;
		} else {
			if(Blind) pline("You cannot escape from it!");
			else pline("You cannot escape from %s!",
				monnam(u.ustuck));
			nomul(0);
			return;
		}
	}
	if(u.uswallow || (mtmp = m_at(u.ux+u.dx,u.uy+u.dy))) {
	/* attack monster */

		nomul(0);
		gethungry();
		if(multi < 0) return;	/* we just fainted */

		/* try to attack; note that it might evade */
		if(attack(u.uswallow ? u.ustuck : mtmp))
			return;
	}
	/* not attacking an animal, so we try to move */
	if(u.utrap) {
		if(u.utraptype == TT_PIT) {
			pline("You are still in a pit.");
			u.utrap--;
		} else {
			pline("You are caught in a beartrap.");
			if((u.dx && u.dy) || !rn2(5)) u.utrap--;
		}
		return;
	}
	tmpr = &levl[u.ux+u.dx][u.uy+u.dy];
	if(IS_ROCK(tmpr->typ) ||
	   (u.dx && u.dy && (tmpr->typ == DOOR || ust->typ == DOOR))){
		flags.move = 0;
		nomul(0);
		return;
	}
	while(otmp = sobj_at(ENORMOUS_ROCK, u.ux+u.dx, u.uy+u.dy)) {
		xchar rx = u.ux+2*u.dx, ry = u.uy+2*u.dy;
		struct trap *ttmp;
		nomul(0);
		if(isok(rx,ry) && !IS_ROCK(levl[rx][ry].typ) &&
		    (levl[rx][ry].typ != DOOR || !(u.dx && u.dy)) &&
		    !sobj_at(ENORMOUS_ROCK, rx, ry)) {
			if(m_at(rx,ry)) {
			    pline("You hear a monster behind the rock.");
			    pline("Perhaps that's why you cannot move it.");
			    goto cannot_push;
			}
			if(ttmp = t_at(rx,ry))
			    switch(ttmp->ttyp) {
			    case PIT:
				pline("You push the rock into a pit!");
				deltrap(ttmp);
				delobj(otmp);
				pline("It completely fills the pit!");
				continue;
			    case TELEP_TRAP:
				pline("You push the rock and suddenly it disappears!");
				delobj(otmp);
				continue;
			    }
			if(levl[rx][ry].typ == POOL) {
				levl[rx][ry].typ = ROOM;
				mnewsym(rx,ry);
				prl(rx,ry);
				pline("You push the rock into the water.");
				pline("Now you can cross the water!");
				delobj(otmp);
				continue;
			}
			otmp->ox = rx;
			otmp->oy = ry;
			/* pobj(otmp); */
			if(cansee(rx,ry)) atl(rx,ry,otmp->olet);
			if(Invisible) newsym(u.ux+u.dx, u.uy+u.dy);

			{ static long lastmovetime;
			/* note: this var contains garbage initially and
			   after a restore */
			if(moves > lastmovetime+2 || moves < lastmovetime)
			pline("With great effort you move the enormous rock.");
			lastmovetime = moves;
			}
		} else {
		    pline("You try to move the enormous rock, but in vain.");
	    cannot_push:
		    if((!invent || inv_weight()+90 <= 0) &&
			(!u.dx || !u.dy || (IS_ROCK(levl[u.ux][u.uy+u.dy].typ)
					&& IS_ROCK(levl[u.ux+u.dx][u.uy].typ)))){
			pline("However, you can squeeze yourself into a small opening.");
			break;
		    } else
			return;
		}
	    }
	if(u.dx && u.dy && IS_ROCK(levl[u.ux][u.uy+u.dy].typ) &&
		IS_ROCK(levl[u.ux+u.dx][u.uy].typ) &&
		invent && inv_weight()+40 > 0) {
		pline("You are carrying too much to get through.");
		nomul(0);
		return;
	}
	if(Punished &&
	   DIST(u.ux+u.dx, u.uy+u.dy, uchain->ox, uchain->oy) > 2){
		if(carried(uball)) {
			movobj(uchain, u.ux, u.uy);
			goto nodrag;
		}

		if(DIST(u.ux+u.dx, u.uy+u.dy, uball->ox, uball->oy) < 3){
			/* leave ball, move chain under/over ball */
			movobj(uchain, uball->ox, uball->oy);
			goto nodrag;
		}

		if(inv_weight() + (int) uball->owt/2 > 0) {
			pline("You cannot %sdrag the heavy iron ball.",
			invent ? "carry all that and also " : "");
			nomul(0);
			return;
		}

		movobj(uball, uchain->ox, uchain->oy);
		unpobj(uball);		/* BAH %% */
		uchain->ox = u.ux;
		uchain->oy = u.uy;
		nomul(-2);
		nomovemsg = "";
	nodrag:	;
	}
	u.ux += u.dx;
	u.uy += u.dy;
	if(flags.run) {
		if(tmpr->typ == DOOR ||
		(xupstair == u.ux && yupstair == u.uy) ||
		(xdnstair == u.ux && ydnstair == u.uy))
			nomul(0);
	}

	if(tmpr->typ == POOL && !Levitation)
		drown();	/* not necessarily fatal */

/*
	if(u.udispl) {
		u.udispl = 0;
		newsym(oldx,oldy);
	}
*/
	if(!Blind) {
#ifdef QUEST
		setsee();
#else
		if(ust->lit) {
			if(tmpr->lit) {
				if(tmpr->typ == DOOR)
					prl1(u.ux+u.dx,u.uy+u.dy);
				else if(ust->typ == DOOR)
					nose1(oldx-u.dx,oldy-u.dy);
			} else {
				unsee();
				prl1(u.ux+u.dx,u.uy+u.dy);
			}
		} else {
			if(tmpr->lit) setsee();
			else {
				prl1(u.ux+u.dx,u.uy+u.dy);
				if(tmpr->typ == DOOR) {
					if(u.dy) {
						prl(u.ux-1,u.uy);
						prl(u.ux+1,u.uy);
					} else {
						prl(u.ux,u.uy-1);
						prl(u.ux,u.uy+1);
					}
				}
			}
			nose1(oldx-u.dx,oldy-u.dy);
		}
#endif QUEST
	} else {
		pru();
	}
	if(!flags.nopick) pickup(1);
	if(trap) dotrap(trap);		/* fall into pit, arrow trap, etc. */
	(void) inshop();
	if(!Blind) read_engr_at(u.ux,u.uy);
}

movobj(obj, ox, oy)
struct obj *obj;
int ox, oy;
{
	/* Some dirty programming to get display right */
	freeobj(obj);
	unpobj(obj);
	obj->nobj = fobj;
	fobj = obj;
	obj->ox = ox;
	obj->oy = oy;
}

dopickup(){
	if(!g_at(u.ux,u.uy) && !o_at(u.ux,u.uy)) {
		pline("There is nothing here to pick up.");
		return(0);
	}
	if(Levitation) {
		pline("You cannot reach the floor.");
		return(1);
	}
	pickup(0);
	return(1);
}

pickup(all)
{
	struct gold *gold;
	struct obj *obj, *obj2;
	int wt;

	if(Levitation) return;
	while(gold = g_at(u.ux,u.uy)) {
		pline("%ld gold piece%s.", gold->amount, plur(gold->amount));
		u.ugold += gold->amount;
		flags.botl = 1;
		freegold(gold);
		if(flags.run) nomul(0);
		if(Invisible) newsym(u.ux,u.uy);
	}

	/* check for more than one object */
	if(!all) {
		int ct = 0;

		for(obj = fobj; obj; obj = obj->nobj)
			if(obj->ox == u.ux && obj->oy == u.uy)
				if(!Punished || obj != uchain)
					ct++;
		if(ct < 2)
			all++;
		else
			pline("There are several objects here.");
	}

	for(obj = fobj; obj; obj = obj2) {
	    obj2 = obj->nobj;	/* perhaps obj will be picked up */
	    if(obj->ox == u.ux && obj->oy == u.uy) {
		if(flags.run) nomul(0);

		/* do not pick up uchain */
		if(Punished && obj == uchain)
			continue;

		if(!all) {
			char c;

			pline("Pick up %s ? [ynaq]", doname(obj));
			while(!index("ynaq ", (c = readchar())))
				bell();
			if(c == 'q') return;
			if(c == 'n') continue;
			if(c == 'a') all = 1;
		}

		if(obj->otyp == DEAD_COCKATRICE && !uarmg){
		    pline("Touching the dead cockatrice is a fatal mistake.");
		    pline("You turn to stone.");
		    killer = "cockatrice cadaver";
		    done("died");
		}

		if(obj->otyp == SCR_SCARE_MONSTER){
		  if(!obj->spe) obj->spe = 1;
		  else {
		    /* Note: perhaps the 1st pickup failed: you cannot
			carry anymore, and so we never dropped it -
			let's assume that treading on it twice also
			destroys the scroll */
		    pline("The scroll turns to dust as you pick it up.");
		    delobj(obj);
		    continue;
		  }
		}

		wt = inv_weight() + obj->owt;
		if(wt > 0) {
			if(obj->quan > 1) {
				/* see how many we can lift */
				extern struct obj *splitobj();
				int savequan = obj->quan;
				int iw = inv_weight();
				int qq;
				for(qq = 1; qq < savequan; qq++){
					obj->quan = qq;
					if(iw + weight(obj) > 0)
						break;
				}
				obj->quan = savequan;
				qq--;
				/* we can carry qq of them */
				if(!qq) goto too_heavy;
			pline("You can only carry %s of the %s lying here.",
					(qq == 1) ? "one" : "some",
					doname(obj));
				(void) splitobj(obj, qq);
				/* note: obj2 is set already, so we'll never
				 * encounter the other half; if it should be
				 * otherwise then write
				 *	obj2 = splitobj(obj,qq);
				 */
				goto lift_some;
			}
		too_heavy:
			pline("There %s %s here, but %s.",
				(obj->quan == 1) ? "is" : "are",
				doname(obj),
				!invent ? "it is too heavy for you to lift"
					: "you cannot carry anymore");
			break;
		}
	lift_some:
		if(inv_cnt() >= 52) {
		    pline("Your knapsack cannot accomodate anymore items.");
		    break;
		}
		if(wt > -5) pline("You have a little trouble lifting");
		freeobj(obj);
		if(Invisible) newsym(u.ux,u.uy);
		addtobill(obj);       /* sets obj->unpaid if necessary */
		{ int pickquan = obj->quan;
		  int mergquan;
		if(!Blind) obj->dknown = 1;	/* this is done by prinv(),
				 but addinv() needs it already for merging */
		obj = addinv(obj);    /* might merge it with other objects */
		  mergquan = obj->quan;
		  obj->quan = pickquan;	/* to fool prinv() */
		prinv(obj);
		  obj->quan = mergquan;
		}
	    }
	}
}

/* stop running if we see something interesting */
/* turn around a corner if that is the only way we can proceed */
/* do not turn left or right twice */
lookaround(){
int x,y,i,x0,y0,m0,i0 = 9;
int corrct = 0, noturn = 0;
struct monst *mtmp;
#ifdef lint
	/* suppress "used before set" message */
	x0 = y0 = 0;
#endif lint
	if(Blind || flags.run == 0) return;
	if(flags.run == 1 && levl[u.ux][u.uy].typ == ROOM) return;
#ifdef QUEST
	if(u.ux0 == u.ux+u.dx && u.uy0 == u.uy+u.dy) goto stop;
#endif QUEST
	for(x = u.ux-1; x <= u.ux+1; x++) for(y = u.uy-1; y <= u.uy+1; y++){
		if(x == u.ux && y == u.uy) continue;
		if(!levl[x][y].typ) continue;
		if((mtmp = m_at(x,y)) && !mtmp->mimic &&
		    (!mtmp->minvis || See_invisible)){
			if(!mtmp->mtame || (x == u.ux+u.dx && y == u.uy+u.dy))
				goto stop;
		} else mtmp = 0; /* invisible M cannot influence us */
		if(x == u.ux-u.dx && y == u.uy-u.dy) continue;
		switch(levl[x][y].scrsym){
		case '|':
		case '-':
		case '.':
		case ' ':
			break;
		case '+':
			if(x != u.ux && y != u.uy) break;
			if(flags.run != 1) goto stop;
			/* fall into next case */
		case CORR_SYM:
		corr:
			if(flags.run == 1 || flags.run == 3) {
				i = DIST(x,y,u.ux+u.dx,u.uy+u.dy);
				if(i > 2) break;
				if(corrct == 1 && DIST(x,y,x0,y0) != 1)
					noturn = 1;
				if(i < i0) {
					i0 = i;
					x0 = x;
					y0 = y;
					m0 = mtmp ? 1 : 0;
				}
			}
			corrct++;
			break;
		case '^':
			if(flags.run == 1) goto corr;	/* if you must */
			if(x == u.ux+u.dx && y == u.uy+u.dy) goto stop;
			break;
		default:	/* e.g. objects or trap or stairs */
			if(flags.run == 1) goto corr;
			if(mtmp) break;		/* d */
		stop:
			nomul(0);
			return;
		}
	}
#ifdef QUEST
	if(corrct > 0 && (flags.run == 4 || flags.run == 5)) goto stop;
#endif QUEST
	if(corrct > 1 && flags.run == 2) goto stop;
	if((flags.run == 1 || flags.run == 3) && !noturn && !m0 && i0 &&
		(corrct == 1 || (corrct == 2 && i0 == 1))) {
		/* make sure that we do not turn too far */
		if(i0 == 2) {
		    if(u.dx == y0-u.uy && u.dy == u.ux-x0)
			i = 2;		/* straight turn right */
		    else
			i = -2;		/* straight turn left */
		} else if(u.dx && u.dy) {
		    if((u.dx == u.dy && y0 == u.uy) ||
			(u.dx != u.dy && y0 != u.uy))
			i = -1;		/* half turn left */
		    else
			i = 1;		/* half turn right */
		} else {
		    if((x0-u.ux == y0-u.uy && !u.dy) ||
			(x0-u.ux != y0-u.uy && u.dy))
			i = 1;		/* half turn right */
		    else
			i = -1;		/* half turn left */
		}
		i += u.last_str_turn;
		if(i <= 2 && i >= -2) {
			u.last_str_turn = i;
			u.dx = x0-u.ux, u.dy = y0-u.uy;
		}
	}
}

/* something like lookaround, but we are not running */
/* react only to monsters that might hit us */
monster_nearby() {
int x,y;
struct monst *mtmp;
	if(!Blind)
	for(x = u.ux-1; x <= u.ux+1; x++) for(y = u.uy-1; y <= u.uy+1; y++){
		if(x == u.ux && y == u.uy) continue;
		if((mtmp = m_at(x,y)) && !mtmp->mimic && !mtmp->mtame &&
			!mtmp->mpeaceful && !index("Ea", mtmp->data->mlet) &&
			!mtmp->mfroz && !mtmp->msleep &&  /* aplvax!jcn */
			(!mtmp->minvis || See_invisible))
			return(1);
	}
	return(0);
}

#ifdef QUEST
cansee(x,y) xchar x,y; {
int dx,dy,adx,ady,sdx,sdy,dmax,d;
	if(Blind) return(0);
	if(!isok(x,y)) return(0);
	d = dist(x,y);
	if(d < 3) return(1);
	if(d > u.uhorizon*u.uhorizon) return(0);
	if(!levl[x][y].lit)
		return(0);
	dx = x - u.ux;	adx = abs(dx);	sdx = sgn(dx);
	dy = y - u.uy;  ady = abs(dy);	sdy = sgn(dy);
	if(dx == 0 || dy == 0 || adx == ady){
		dmax = (dx == 0) ? ady : adx;
		for(d = 1; d <= dmax; d++)
			if(!rroom(sdx*d,sdy*d))
				return(0);
		return(1);
	} else if(ady > adx){
		for(d = 1; d <= ady; d++){
			if(!rroom(sdx*( (d*adx)/ady ), sdy*d) ||
			   !rroom(sdx*( (d*adx-1)/ady+1 ), sdy*d))
				return(0);
		}
		return(1);
	} else {
		for(d = 1; d <= adx; d++){
			if(!rroom(sdx*d, sdy*( (d*ady)/adx )) ||
			   !rroom(sdx*d, sdy*( (d*ady-1)/adx+1 )))
				return(0);
		}
		return(1);
	}
}

rroom(x,y) int x,y; {
	return(IS_ROOM(levl[u.ux+x][u.uy+y].typ));
}

#else

cansee(x,y) xchar x,y; {
	if(Blind || u.uswallow) return(0);
	if(dist(x,y) < 3) return(1);
	if(levl[x][y].lit && seelx <= x && x <= seehx && seely <= y &&
		y <= seehy) return(1);
	return(0);
}
#endif QUEST

sgn(a) int a; {
	return((a > 0) ? 1 : (a == 0) ? 0 : -1);
}

#ifdef QUEST
setsee()
{
	x,y;

	if(Blind) {
		pru();
		return;
	}
	for(y = u.uy-u.uhorizon; y <= u.uy+u.uhorizon; y++)
		for(x = u.ux-u.uhorizon; x <= u.ux+u.uhorizon; x++) {
			if(cansee(x,y))
				prl(x,y);
	}
}

#else

setsee()
{
	int x,y;

	if(Blind) {
		pru();
		return;
	}
	if(!levl[u.ux][u.uy].lit) {
		seelx = u.ux-1;
		seehx = u.ux+1;
		seely = u.uy-1;
		seehy = u.uy+1;
	} else {
		for(seelx = u.ux; levl[seelx-1][u.uy].lit; seelx--);
		for(seehx = u.ux; levl[seehx+1][u.uy].lit; seehx++);
		for(seely = u.uy; levl[u.ux][seely-1].lit; seely--);
		for(seehy = u.uy; levl[u.ux][seehy+1].lit; seehy++);
	}
	for(y = seely; y <= seehy; y++)
		for(x = seelx; x <= seehx; x++) {
			prl(x,y);
	}
	if(!levl[u.ux][u.uy].lit) seehx = 0; /* seems necessary elsewhere */
	else {
	    if(seely == u.uy) for(x = u.ux-1; x <= u.ux+1; x++) prl(x,seely-1);
	    if(seehy == u.uy) for(x = u.ux-1; x <= u.ux+1; x++) prl(x,seehy+1);
	    if(seelx == u.ux) for(y = u.uy-1; y <= u.uy+1; y++) prl(seelx-1,y);
	    if(seehx == u.ux) for(y = u.uy-1; y <= u.uy+1; y++) prl(seehx+1,y);
	}
}
#endif QUEST

nomul(nval)
int nval;
{
	if(multi < 0) return;
	multi = nval;
	flags.mv = flags.run = 0;
}

abon()
{
	if(u.ustr == 3) return(-3);
	else if(u.ustr < 6) return(-2);
	else if(u.ustr < 8) return(-1);
	else if(u.ustr < 17) return(0);
	else if(u.ustr < 69) return(1);	/* up to 18/50 */
	else if(u.ustr < 118) return(2);
	else return(3);
}

dbon()
{
	if(u.ustr < 6) return(-1);
	else if(u.ustr < 16) return(0);
	else if(u.ustr < 18) return(1);
	else if(u.ustr == 18) return(2);	/* up to 18 */
	else if(u.ustr < 94) return(3);		/* up to 18/75 */
	else if(u.ustr < 109) return(4);	/* up to 18/90 */
	else if(u.ustr < 118) return(5);	/* up to 18/99 */
	else return(6);
}

losestr(num)	/* may kill you; cause may be poison or monster like 'A' */
int num;
{
	u.ustr -= num;
	while(u.ustr < 3) {
		u.ustr++;
		u.uhp -= 6;
		u.uhpmax -= 6;
	}
	flags.botl = 1;
}

losehp(n,knam)
int n;
char *knam;
{
	u.uhp -= n;
	if(u.uhp > u.uhpmax)
		u.uhpmax = u.uhp;	/* perhaps n was negative */
	flags.botl = 1;
	if(u.uhp < 1) {
		killer = knam;	/* the thing that killed you */
		done("died");
	}
}

losehp_m(n,mtmp)
int n;
struct monst *mtmp;
{
	u.uhp -= n;
	flags.botl = 1;
	if(u.uhp < 1)
		done_in_by(mtmp);
}

losexp()	/* hit by V or W */
{
	int num;
	extern long newuexp();

	if(u.ulevel > 1)
		pline("Goodbye level %u.", u.ulevel--);
	else
		u.uhp = -1;
	num = rnd(10);
	u.uhp -= num;
	u.uhpmax -= num;
	u.uexp = newuexp();
	flags.botl = 1;
}

inv_weight(){
struct obj *otmp = invent;
int wt = (u.ugold + 500)/1000;
int carrcap;
	if(Levitation)			/* pugh@cornell */
		carrcap = MAX_CARR_CAP;
	else {
		carrcap = 5*(((u.ustr > 18) ? 20 : u.ustr) + u.ulevel);
		if(carrcap > MAX_CARR_CAP) carrcap = MAX_CARR_CAP;
		if(Wounded_legs & LEFT_SIDE) carrcap -= 10;
		if(Wounded_legs & RIGHT_SIDE) carrcap -= 10;
	}
	while(otmp){
		wt += otmp->owt;
		otmp = otmp->nobj;
	}
	return(wt - carrcap);
}

inv_cnt(){
struct obj *otmp = invent;
int ct = 0;
	while(otmp){
		ct++;
		otmp = otmp->nobj;
	}
	return(ct);
}

long
newuexp()
{
	return(10*(1L << (u.ulevel-1)));
}
