/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.objnam.c - version 1.0.2 */
/* $FreeBSD$ */

#include	"hack.h"
#define Sprintf (void) sprintf
#define Strcat  (void) strcat
#define	Strcpy	(void) strcpy
#define	PREFIX	15
extern char *eos();
extern int bases[];

char *
strprepend(s,pref) char *s, *pref; {
int i = strlen(pref);
	if(i > PREFIX) {
		pline("WARNING: prefix too short.");
		return(s);
	}
	s -= i;
	(void) strncpy(s, pref, i);	/* do not copy trailing 0 */
	return(s);
}

char *
sitoa(a) int a; {
static char buf[13];
	Sprintf(buf, (a < 0) ? "%d" : "+%d", a);
	return(buf);
}

char *
typename(otyp)
int otyp;
{
static char buf[BUFSZ];
struct objclass *ocl = &objects[otyp];
char *an = ocl->oc_name;
char *dn = ocl->oc_descr;
char *un = ocl->oc_uname;
int nn = ocl->oc_name_known;
	switch(ocl->oc_olet) {
	case POTION_SYM:
		Strcpy(buf, "potion");
		break;
	case SCROLL_SYM:
		Strcpy(buf, "scroll");
		break;
	case WAND_SYM:
		Strcpy(buf, "wand");
		break;
	case RING_SYM:
		Strcpy(buf, "ring");
		break;
	default:
		if(nn) {
			Strcpy(buf, an);
			if(otyp >= TURQUOISE && otyp <= JADE)
				Strcat(buf, " stone");
			if(un)
				Sprintf(eos(buf), " called %s", un);
			if(dn)
				Sprintf(eos(buf), " (%s)", dn);
		} else {
			Strcpy(buf, dn ? dn : an);
			if(ocl->oc_olet == GEM_SYM)
				Strcat(buf, " gem");
			if(un)
				Sprintf(eos(buf), " called %s", un);
		}
		return(buf);
	}
	/* here for ring/scroll/potion/wand */
	if(nn)
		Sprintf(eos(buf), " of %s", an);
	if(un)
		Sprintf(eos(buf), " called %s", un);
	if(dn)
		Sprintf(eos(buf), " (%s)", dn);
	return(buf);
}

char *
xname(obj)
struct obj *obj;
{
static char bufr[BUFSZ];
char *buf = &(bufr[PREFIX]);	/* leave room for "17 -3 " */
int nn = objects[obj->otyp].oc_name_known;
char *an = objects[obj->otyp].oc_name;
char *dn = objects[obj->otyp].oc_descr;
char *un = objects[obj->otyp].oc_uname;
int pl = (obj->quan != 1);
	if(!obj->dknown && !Blind) obj->dknown = 1; /* %% doesnt belong here */
	switch(obj->olet) {
	case AMULET_SYM:
		Strcpy(buf, (obj->spe < 0 && obj->known)
			? "cheap plastic imitation of the " : "");
		Strcat(buf,"Amulet of Yendor");
		break;
	case TOOL_SYM:
		if(!nn) {
			Strcpy(buf, dn);
			break;
		}
		Strcpy(buf,an);
		break;
	case FOOD_SYM:
		if(obj->otyp == DEAD_HOMUNCULUS && pl) {
			pl = 0;
			Strcpy(buf, "dead homunculi");
			break;
		}
		/* fungis ? */
		/* fall into next case */
	case WEAPON_SYM:
		if(obj->otyp == WORM_TOOTH && pl) {
			pl = 0;
			Strcpy(buf, "worm teeth");
			break;
		}
		if(obj->otyp == CRYSKNIFE && pl) {
			pl = 0;
			Strcpy(buf, "crysknives");
			break;
		}
		/* fall into next case */
	case ARMOR_SYM:
	case CHAIN_SYM:
	case ROCK_SYM:
		Strcpy(buf,an);
		break;
	case BALL_SYM:
		Sprintf(buf, "%sheavy iron ball",
		  (obj->owt > objects[obj->otyp].oc_weight) ? "very " : "");
		break;
	case POTION_SYM:
		if(nn || un || !obj->dknown) {
			Strcpy(buf, "potion");
			if(pl) {
				pl = 0;
				Strcat(buf, "s");
			}
			if(!obj->dknown) break;
			if(un) {
				Strcat(buf, " called ");
				Strcat(buf, un);
			} else {
				Strcat(buf, " of ");
				Strcat(buf, an);
			}
		} else {
			Strcpy(buf, dn);
			Strcat(buf, " potion");
		}
		break;
	case SCROLL_SYM:
		Strcpy(buf, "scroll");
		if(pl) {
			pl = 0;
			Strcat(buf, "s");
		}
		if(!obj->dknown) break;
		if(nn) {
			Strcat(buf, " of ");
			Strcat(buf, an);
		} else if(un) {
			Strcat(buf, " called ");
			Strcat(buf, un);
		} else {
			Strcat(buf, " labeled ");
			Strcat(buf, dn);
		}
		break;
	case WAND_SYM:
		if(!obj->dknown)
			Sprintf(buf, "wand");
		else if(nn)
			Sprintf(buf, "wand of %s", an);
		else if(un)
			Sprintf(buf, "wand called %s", un);
		else
			Sprintf(buf, "%s wand", dn);
		break;
	case RING_SYM:
		if(!obj->dknown)
			Sprintf(buf, "ring");
		else if(nn)
			Sprintf(buf, "ring of %s", an);
		else if(un)
			Sprintf(buf, "ring called %s", un);
		else
			Sprintf(buf, "%s ring", dn);
		break;
	case GEM_SYM:
		if(!obj->dknown) {
			Strcpy(buf, "gem");
			break;
		}
		if(!nn) {
			Sprintf(buf, "%s gem", dn);
			break;
		}
		Strcpy(buf, an);
		if(obj->otyp >= TURQUOISE && obj->otyp <= JADE)
			Strcat(buf, " stone");
		break;
	default:
		Sprintf(buf,"glorkum %c (0%o) %u %d",
			obj->olet,obj->olet,obj->otyp,obj->spe);
	}
	if(pl) {
		char *p;

		for(p = buf; *p; p++) {
			if(!strncmp(" of ", p, 4)) {
				/* pieces of, cloves of, lumps of */
				int c1, c2 = 's';

				do {
					c1 = c2; c2 = *p; *p++ = c1;
				} while(c1);
				goto nopl;
			}
		}
		p = eos(buf)-1;
		if(*p == 's' || *p == 'z' || *p == 'x' ||
		    (*p == 'h' && p[-1] == 's'))
			Strcat(buf, "es");	/* boxes */
		else if(*p == 'y' && !index(vowels, p[-1]))
			Strcpy(p, "ies");	/* rubies, zruties */
		else
			Strcat(buf, "s");
	}
nopl:
	if(obj->onamelth) {
		Strcat(buf, " named ");
		Strcat(buf, ONAME(obj));
	}
	return(buf);
}

char *
doname(obj)
struct obj *obj;
{
char prefix[PREFIX];
char *bp = xname(obj);
	if(obj->quan != 1)
		Sprintf(prefix, "%u ", obj->quan);
	else
		Strcpy(prefix, "a ");
	switch(obj->olet) {
	case AMULET_SYM:
		if(strncmp(bp, "cheap ", 6))
			Strcpy(prefix, "the ");
		break;
	case ARMOR_SYM:
		if(obj->owornmask & W_ARMOR)
			Strcat(bp, " (being worn)");
		/* fall into next case */
	case WEAPON_SYM:
		if(obj->known) {
			Strcat(prefix, sitoa(obj->spe));
			Strcat(prefix, " ");
		}
		break;
	case WAND_SYM:
		if(obj->known)
			Sprintf(eos(bp), " (%d)", obj->spe);
		break;
	case RING_SYM:
		if(obj->owornmask & W_RINGR) Strcat(bp, " (on right hand)");
		if(obj->owornmask & W_RINGL) Strcat(bp, " (on left hand)");
		if(obj->known && (objects[obj->otyp].bits & SPEC)) {
			Strcat(prefix, sitoa(obj->spe));
			Strcat(prefix, " ");
		}
		break;
	}
	if(obj->owornmask & W_WEP)
		Strcat(bp, " (weapon in hand)");
	if(obj->unpaid)
		Strcat(bp, " (unpaid)");
	if(!strcmp(prefix, "a ") && index(vowels, *bp))
		Strcpy(prefix, "an ");
	bp = strprepend(bp, prefix);
	return(bp);
}

/* used only in hack.fight.c (thitu) */
setan(str,buf)
char *str,*buf;
{
	if(index(vowels,*str))
		Sprintf(buf, "an %s", str);
	else
		Sprintf(buf, "a %s", str);
}

char *
aobjnam(otmp,verb) struct obj *otmp; char *verb; {
char *bp = xname(otmp);
char prefix[PREFIX];
	if(otmp->quan != 1) {
		Sprintf(prefix, "%u ", otmp->quan);
		bp = strprepend(bp, prefix);
	}

	if(verb) {
		/* verb is given in plural (i.e., without trailing s) */
		Strcat(bp, " ");
		if(otmp->quan != 1)
			Strcat(bp, verb);
		else if(!strcmp(verb, "are"))
			Strcat(bp, "is");
		else {
			Strcat(bp, verb);
			Strcat(bp, "s");
		}
	}
	return(bp);
}

char *
Doname(obj)
struct obj *obj;
{
	char *s = doname(obj);

	if('a' <= *s && *s <= 'z') *s -= ('a' - 'A');
	return(s);
}

char *wrp[] = { "wand", "ring", "potion", "scroll", "gem" };
char wrpsym[] = { WAND_SYM, RING_SYM, POTION_SYM, SCROLL_SYM, GEM_SYM };

struct obj *
readobjnam(bp) char *bp; {
char *p;
int i;
int cnt, spe, spesgn, typ, heavy;
char let;
char *un, *dn, *an;
/* int the = 0; char *oname = 0; */
	cnt = spe = spesgn = typ = heavy = 0;
	let = 0;
	an = dn = un = 0;
	for(p = bp; *p; p++)
		if('A' <= *p && *p <= 'Z') *p += 'a'-'A';
	if(!strncmp(bp, "the ", 4)){
/*		the = 1; */
		bp += 4;
	} else if(!strncmp(bp, "an ", 3)){
		cnt = 1;
		bp += 3;
	} else if(!strncmp(bp, "a ", 2)){
		cnt = 1;
		bp += 2;
	}
	if(!cnt && digit(*bp)){
		cnt = atoi(bp);
		while(digit(*bp)) bp++;
		while(*bp == ' ') bp++;
	}
	if(!cnt) cnt = 1;		/* %% what with "gems" etc. ? */

	if(*bp == '+' || *bp == '-'){
		spesgn = (*bp++ == '+') ? 1 : -1;
		spe = atoi(bp);
		while(digit(*bp)) bp++;
		while(*bp == ' ') bp++;
	} else {
		p = rindex(bp, '(');
		if(p) {
			if(p > bp && p[-1] == ' ') p[-1] = 0;
			else *p = 0;
			p++;
			spe = atoi(p);
			while(digit(*p)) p++;
			if(strcmp(p, ")")) spe = 0;
			else spesgn = 1;
		}
	}
	/* now we have the actual name, as delivered by xname, say
		green potions called whisky
		scrolls labeled "QWERTY"
		egg
		dead zruties
		fortune cookies
		very heavy iron ball named hoei
		wand of wishing
		elven cloak
	*/
	for(p = bp; *p; p++) if(!strncmp(p, " named ", 7)) {
		*p = 0;
/*		oname = p+7; */
	}
	for(p = bp; *p; p++) if(!strncmp(p, " called ", 8)) {
		*p = 0;
		un = p+8;
	}
	for(p = bp; *p; p++) if(!strncmp(p, " labeled ", 9)) {
		*p = 0;
		dn = p+9;
	}

	/* first change to singular if necessary */
	if(cnt != 1) {
		/* find "cloves of garlic", "worthless pieces of blue glass" */
		for(p = bp; *p; p++) if(!strncmp(p, "s of ", 5)){
			while(*p = p[1]) p++;
			goto sing;
		}
		/* remove -s or -es (boxes) or -ies (rubies, zruties) */
		p = eos(bp);
		if(p[-1] == 's') {
			if(p[-2] == 'e') {
				if(p[-3] == 'i') {
					if(!strcmp(p-7, "cookies"))
						goto mins;
					Strcpy(p-3, "y");
					goto sing;
				}

				/* note: cloves / knives from clove / knife */
				if(!strcmp(p-6, "knives")) {
					Strcpy(p-3, "fe");
					goto sing;
				}

				/* note: nurses, axes but boxes */
				if(!strcmp(p-5, "boxes")) {
					p[-2] = 0;
					goto sing;
				}
			}
		mins:
			p[-1] = 0;
		} else {
			if(!strcmp(p-9, "homunculi")) {
				Strcpy(p-1, "us"); /* !! makes string longer */
				goto sing;
			}
			if(!strcmp(p-5, "teeth")) {
				Strcpy(p-5, "tooth");
				goto sing;
			}
			/* here we cannot find the plural suffix */
		}
	}
sing:
	if(!strcmp(bp, "amulet of yendor")) {
		typ = AMULET_OF_YENDOR;
		goto typfnd;
	}
	p = eos(bp);
	if(!strcmp(p-5, " mail")){	/* Note: ring mail is not a ring ! */
		let = ARMOR_SYM;
		an = bp;
		goto srch;
	}
	for(i = 0; i < sizeof(wrpsym); i++) {
		int j = strlen(wrp[i]);
		if(!strncmp(bp, wrp[i], j)){
			let = wrpsym[i];
			bp += j;
			if(!strncmp(bp, " of ", 4)) an = bp+4;
			/* else if(*bp) ?? */
			goto srch;
		}
		if(!strcmp(p-j, wrp[i])){
			let = wrpsym[i];
			p -= j;
			*p = 0;
			if(p[-1] == ' ') p[-1] = 0;
			dn = bp;
			goto srch;
		}
	}
	if(!strcmp(p-6, " stone")){
		p[-6] = 0;
		let = GEM_SYM;
		an = bp;
		goto srch;
	}
	if(!strcmp(bp, "very heavy iron ball")){
		heavy = 1;
		typ = HEAVY_IRON_BALL;
		goto typfnd;
	}
	an = bp;
srch:
	if(!an && !dn && !un)
		goto any;
	i = 1;
	if(let) i = bases[letindex(let)];
	while(i <= NROFOBJECTS && (!let || objects[i].oc_olet == let)){
		char *zn = objects[i].oc_name;

		if(!zn) goto nxti;
		if(an && strcmp(an, zn))
			goto nxti;
		if(dn && (!(zn = objects[i].oc_descr) || strcmp(dn, zn)))
			goto nxti;
		if(un && (!(zn = objects[i].oc_uname) || strcmp(un, zn)))
			goto nxti;
		typ = i;
		goto typfnd;
	nxti:
		i++;
	}
any:
	if(!let) let = wrpsym[rn2(sizeof(wrpsym))];
	typ = probtype(let);
typfnd:
	{ struct obj *otmp;
	  extern struct obj *mksobj();
	let = objects[typ].oc_olet;
	otmp = mksobj(typ);
	if(heavy)
		otmp->owt += 15;
	if(cnt > 0 && index("%?!*)", let) &&
		(cnt < 4 || (let == WEAPON_SYM && typ <= ROCK && cnt < 20)))
		otmp->quan = cnt;

	if(spe > 3 && spe > otmp->spe)
		spe = 0;
	else if(let == WAND_SYM)
		spe = otmp->spe;
	if(spe == 3 && u.uluck < 0)
		spesgn = -1;
	if(let != WAND_SYM && spesgn == -1)
		spe = -spe;
	if(let == BALL_SYM)
		spe = 0;
	else if(let == AMULET_SYM)
		spe = -1;
	else if(typ == WAN_WISHING && rn2(10))
		spe = (rn2(10) ? -1 : 0);
	otmp->spe = spe;

	if(spesgn == -1)
		otmp->cursed = 1;

	return(otmp);
    }
}
