/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.apply.c - version 1.0.3 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include	"hack.h"
#include	"def.edog.h"
#include	"def.mkroom.h"
static struct monst *bchit();
extern struct obj *addinv();
extern struct trap *maketrap();
extern int (*occupation)();
extern char *occtxt;
extern char quitchars[];
extern char pl_character[];

static void use_camera(), use_ice_box(), use_whistle(), use_magic_whistle();
static int use_pick_axe();

doapply() {
	struct obj *obj;
	int res = 1;

	obj = getobj("(", "use or apply");
	if(!obj) return(0);

	switch(obj->otyp){
	case EXPENSIVE_CAMERA:
		use_camera(obj); break;
	case ICE_BOX:
		use_ice_box(obj); break;
	case PICK_AXE:
		res = use_pick_axe(obj);
		break;

	case MAGIC_WHISTLE:
		if(pl_character[0] == 'W' || u.ulevel > 9) {
			use_magic_whistle(obj);
			break;
		}
		/* FALLTHROUGH */
	case WHISTLE:
		use_whistle(obj);
		break;

	case CAN_OPENER:
		if(!carrying(TIN)) {
			pline("You have no can to open.");
			goto xit;
		}
		pline("You cannot open a tin without eating its contents.");
		pline("In order to eat, use the 'e' command.");
		if(obj != uwep)
    pline("Opening the tin will be much easier if you wield the can-opener.");
		goto xit;

	default:
		pline("Sorry, I don't know how to use that.");
	xit:
		nomul(0);
		return(0);
	}
	nomul(0);
	return(res);
}

/* ARGSUSED */
static void
use_camera(obj) /* */ struct obj *obj; {
struct monst *mtmp;
	if(!getdir(1)){		/* ask: in what direction? */
		flags.move = multi = 0;
		return;
	}
	if(u.uswallow) {
		pline("You take a picture of %s's stomach.", monnam(u.ustuck));
		return;
	}
	if(u.dz) {
		pline("You take a picture of the %s.",
			(u.dz > 0) ? "floor" : "ceiling");
		return;
	}
	if(mtmp = bchit(u.dx, u.dy, COLNO, '!')) {
		if(mtmp->msleep){
			mtmp->msleep = 0;
			pline("The flash awakens %s.", monnam(mtmp)); /* a3 */
		} else
		if(mtmp->data->mlet != 'y')
		if(mtmp->mcansee || mtmp->mblinded){
			int tmp = dist(mtmp->mx,mtmp->my);
			int tmp2;
			if(cansee(mtmp->mx,mtmp->my))
			  pline("%s is blinded by the flash!", Monnam(mtmp));
			setmangry(mtmp);
			if(tmp < 9 && !mtmp->isshk && rn2(4)) {
				mtmp->mflee = 1;
				if(rn2(4)) mtmp->mfleetim = rnd(100);
			}
			if(tmp < 3) mtmp->mcansee  = mtmp->mblinded = 0;
			else {
				tmp2 = mtmp->mblinded;
				tmp2 += rnd(1 + 50/tmp);
				if(tmp2 > 127) tmp2 = 127;
				mtmp->mblinded = tmp2;
				mtmp->mcansee = 0;
			}
		}
	}
}

static
struct obj *current_ice_box;	/* a local variable of use_ice_box, to be
				used by its local procedures in/ck_ice_box */
static
in_ice_box(obj) struct obj *obj; {
	if(obj == current_ice_box ||
		(Punished && (obj == uball || obj == uchain))){
		pline("You must be kidding.");
		return(0);
	}
	if(obj->owornmask & (W_ARMOR | W_RING)) {
		pline("You cannot refrigerate something you are wearing.");
		return(0);
	}
	if(obj->owt + current_ice_box->owt > 70) {
		pline("It won't fit.");
		return(1);	/* be careful! */
	}
	if(obj == uwep) {
		if(uwep->cursed) {
			pline("Your weapon is welded to your hand!");
			return(0);
		}
		setuwep((struct obj *) 0);
	}
	current_ice_box->owt += obj->owt;
	freeinv(obj);
	obj->o_cnt_id = current_ice_box->o_id;
	obj->nobj = fcobj;
	fcobj = obj;
	obj->age = moves - obj->age;	/* actual age */
	return(1);
}

static
ck_ice_box(obj) struct obj *obj; {
	return(obj->o_cnt_id == current_ice_box->o_id);
}

static
out_ice_box(obj) struct obj *obj; {
struct obj *otmp;
	if(obj == fcobj) fcobj = fcobj->nobj;
	else {
		for(otmp = fcobj; otmp->nobj != obj; otmp = otmp->nobj)
			if(!otmp->nobj) panic("out_ice_box");
		otmp->nobj = obj->nobj;
	}
	current_ice_box->owt -= obj->owt;
	obj->age = moves - obj->age;	/* simulated point of time */
	(void) addinv(obj);
}

static void
use_ice_box(obj) struct obj *obj; {
int cnt = 0;
struct obj *otmp;
	current_ice_box = obj;	/* for use by in/out_ice_box */
	for(otmp = fcobj; otmp; otmp = otmp->nobj)
		if(otmp->o_cnt_id == obj->o_id)
			cnt++;
	if(!cnt) pline("Your ice-box is empty.");
	else {
	    pline("Do you want to take something out of the ice-box? [yn] ");
	    if(readchar() == 'y')
		if(askchain(fcobj, (char *) 0, 0, out_ice_box, ck_ice_box, 0))
		    return;
		pline("That was all. Do you wish to put something in? [yn] ");
		if(readchar() != 'y') return;
	}
	/* call getobj: 0: allow cnt; #: allow all types; %: expect food */
	otmp = getobj("0#%", "put in");
	if(!otmp || !in_ice_box(otmp))
		flags.move = multi = 0;
}

static
struct monst *
bchit(ddx,ddy,range,sym) int ddx,ddy,range; char sym; {
	struct monst *mtmp = (struct monst *) 0;
	int bchx = u.ux, bchy = u.uy;

	if(sym) Tmp_at(-1, sym);	/* open call */
	while(range--) {
		bchx += ddx;
		bchy += ddy;
		if(mtmp = m_at(bchx,bchy))
			break;
		if(!ZAP_POS(levl[bchx][bchy].typ)) {
			bchx -= ddx;
			bchy -= ddy;
			break;
		}
		if(sym) Tmp_at(bchx, bchy);
	}
	if(sym) Tmp_at(-1, -1);
	return(mtmp);
}

/* ARGSUSED */
static void
use_whistle(obj) struct obj *obj; {
struct monst *mtmp = fmon;
	pline("You produce a high whistling sound.");
	while(mtmp) {
		if(dist(mtmp->mx,mtmp->my) < u.ulevel*20) {
			if(mtmp->msleep)
				mtmp->msleep = 0;
			if(mtmp->mtame)
				EDOG(mtmp)->whistletime = moves;
		}
		mtmp = mtmp->nmon;
	}
}

/* ARGSUSED */
static void
use_magic_whistle(obj) struct obj *obj; {
struct monst *mtmp = fmon;
	pline("You produce a strange whistling sound.");
	while(mtmp) {
		if(mtmp->mtame) mnexto(mtmp);
		mtmp = mtmp->nmon;
	}
}

static int dig_effort;	/* effort expended on current pos */
static uchar dig_level;
static coord dig_pos;
static boolean dig_down;

static
dig() {
	struct rm *lev;
	int dpx = dig_pos.x, dpy = dig_pos.y;

	/* perhaps a nymph stole his pick-axe while he was busy digging */
	/* or perhaps he teleported away */
	if(u.uswallow || !uwep || uwep->otyp != PICK_AXE ||
	    dig_level != dlevel ||
	    ((dig_down && (dpx != u.ux || dpy != u.uy)) ||
	     (!dig_down && dist(dpx,dpy) > 2)))
		return(0);

	dig_effort += 10 + abon() + uwep->spe + rn2(5);
	if(dig_down) {
		if(!xdnstair) {
			pline("The floor here seems too hard to dig in.");
			return(0);
		}
		if(dig_effort > 250) {
			dighole();
			return(0);	/* done with digging */
		}
		if(dig_effort > 50) {
			struct trap *ttmp = t_at(dpx,dpy);

			if(!ttmp) {
				ttmp = maketrap(dpx,dpy,PIT);
				ttmp->tseen = 1;
				pline("You have dug a pit.");
				u.utrap = rn1(4,2);
				u.utraptype = TT_PIT;
				return(0);
			}
		}
	} else
	if(dig_effort > 100) {
		char *digtxt;
		struct obj *obj;

		lev = &levl[dpx][dpy];
		if(obj = sobj_at(ENORMOUS_ROCK, dpx, dpy)) {
			fracture_rock(obj);
			digtxt = "The rock falls apart.";
		} else if(!lev->typ || lev->typ == SCORR) {
			lev->typ = CORR;
			digtxt = "You succeeded in cutting away some rock.";
		} else if(lev->typ == HWALL || lev->typ == VWALL
					    || lev->typ == SDOOR) {
			lev->typ = xdnstair ? DOOR : ROOM;
			digtxt = "You just made an opening in the wall.";
		} else
		  digtxt = "Now what exactly was it that you were digging in?";
		mnewsym(dpx, dpy);
		prl(dpx, dpy);
		pline("%s", digtxt);		/* after mnewsym & prl */
		return(0);
	} else {
		if(IS_WALL(levl[dpx][dpy].typ)) {
			int rno = inroom(dpx,dpy);

			if(rno >= 0 && rooms[rno].rtype >= 8) {
			  pline("This wall seems too hard to dig into.");
			  return(0);
			}
		}
		pline("You hit the rock with all your might.");
	}
	return(1);
}

/* When will hole be finished? Very rough indication used by shopkeeper. */
holetime() {
	return( (occupation == dig) ? (250 - dig_effort)/20 : -1);
}

dighole()
{
	struct trap *ttmp = t_at(u.ux, u.uy);

	if(!xdnstair) {
		pline("The floor here seems too hard to dig in.");
	} else {
		if(ttmp)
			ttmp->ttyp = TRAPDOOR;
		else
			ttmp = maketrap(u.ux, u.uy, TRAPDOOR);
		ttmp->tseen = 1;
		pline("You've made a hole in the floor.");
		if(!u.ustuck) {
			if(inshop())
				shopdig(1);
			pline("You fall through ...");
			if(u.utraptype == TT_PIT) {
				u.utrap = 0;
				u.utraptype = 0;
			}
			goto_level(dlevel+1, FALSE);
		}
	}
}

static
use_pick_axe(obj)
struct obj *obj;
{
	char dirsyms[12];
	extern char sdir[];
	char *dsp = dirsyms, *sdp = sdir;
	struct monst *mtmp;
	struct rm *lev;
	int rx, ry, res = 0;

	if(obj != uwep) {
		if(uwep && uwep->cursed) {
			/* Andreas Bormann - ihnp4!decvax!mcvax!unido!ab */
			pline("Since your weapon is welded to your hand,");
			pline("you cannot use that pick-axe.");
			return(0);
		}
		pline("You now wield %s.", doname(obj));
		setuwep(obj);
		res = 1;
	}
	while(*sdp) {
		(void) movecmd(*sdp);	/* sets u.dx and u.dy and u.dz */
		rx = u.ux + u.dx;
		ry = u.uy + u.dy;
		if(u.dz > 0 || (u.dz == 0 && isok(rx, ry) &&
		    (IS_ROCK(levl[rx][ry].typ)
		    || sobj_at(ENORMOUS_ROCK, rx, ry))))
			*dsp++ = *sdp;
		sdp++;
	}
	*dsp = 0;
	pline("In what direction do you want to dig? [%s] ", dirsyms);
	if(!getdir(0))		/* no txt */
		return(res);
	if(u.uswallow && attack(u.ustuck)) /* return(1) */;
	else
	if(u.dz < 0)
		pline("You cannot reach the ceiling.");
	else
	if(u.dz == 0) {
		if(Confusion)
			confdir();
		rx = u.ux + u.dx;
		ry = u.uy + u.dy;
		if((mtmp = m_at(rx, ry)) && attack(mtmp))
			return(1);
		if(!isok(rx, ry)) {
			pline("Clash!");
			return(1);
		}
		lev = &levl[rx][ry];
		if(lev->typ == DOOR)
			pline("Your %s against the door.",
				aobjnam(obj, "clang"));
		else if(!IS_ROCK(lev->typ)
		     && !sobj_at(ENORMOUS_ROCK, rx, ry)) {
			/* ACCESSIBLE or POOL */
			pline("You swing your %s through thin air.",
				aobjnam(obj, (char *) 0));
		} else {
			if(dig_pos.x != rx || dig_pos.y != ry
			    || dig_level != dlevel || dig_down) {
				dig_down = FALSE;
				dig_pos.x = rx;
				dig_pos.y = ry;
				dig_level = dlevel;
				dig_effort = 0;
				pline("You start digging.");
			} else
				pline("You continue digging.");
			occupation = dig;
			occtxt = "digging";
		}
	} else if(Levitation) {
		pline("You cannot reach the floor.");
	} else {
		if(dig_pos.x != u.ux || dig_pos.y != u.uy
		    || dig_level != dlevel || !dig_down) {
			dig_down = TRUE;
			dig_pos.x = u.ux;
			dig_pos.y = u.uy;
			dig_level = dlevel;
			dig_effort = 0;
			pline("You start digging in the floor.");
			if(inshop())
				shopdig(0);
		} else
			pline("You continue digging in the floor.");
		occupation = dig;
		occtxt = "digging";
	}
	return(1);
}
