/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* def.objclass.h - version 1.0.3 */

/* definition of a class of objects */

struct objclass {
	char *oc_name;		/* actual name */
	char *oc_descr;		/* description when name unknown */
	char *oc_uname;		/* called by user */
	Bitfield(oc_name_known,1);
	Bitfield(oc_merge,1);	/* merge otherwise equal objects */
	char oc_olet;
	schar oc_prob;		/* probability for mkobj() */
	schar oc_delay;		/* delay when using such an object */
	uchar oc_weight;
	schar oc_oc1, oc_oc2;
	int oc_oi;
#define	nutrition	oc_oi	/* for foods */
#define	a_ac		oc_oc1	/* for armors - only used in ARM_BONUS */
#define ARM_BONUS(obj)	((10 - objects[obj->otyp].a_ac) + obj->spe)
#define	a_can		oc_oc2	/* for armors */
#define bits		oc_oc1	/* for wands and rings */
				/* wands */
#define		NODIR		1
#define		IMMEDIATE	2
#define		RAY		4
				/* rings */
#define		SPEC		1	/* +n is meaningful */
#define	wldam		oc_oc1	/* for weapons and PICK_AXE */
#define	wsdam		oc_oc2	/* for weapons and PICK_AXE */
#define	g_val		oc_oi	/* for gems: value on exit */
};

extern struct objclass objects[];

/* definitions of all object-symbols */

#define	ILLOBJ_SYM	'\\'
#define	AMULET_SYM	'"'
#define	FOOD_SYM	'%'
#define	WEAPON_SYM	')'
#define	TOOL_SYM	'('
#define	BALL_SYM	'0'
#define	CHAIN_SYM	'_'
#define	ROCK_SYM	'`'
#define	ARMOR_SYM	'['
#define	POTION_SYM	'!'
#define	SCROLL_SYM	'?'
#define	WAND_SYM	'/'
#define	RING_SYM	'='
#define	GEM_SYM		'*'
/* Other places with explicit knowledge of object symbols:
 * ....shk.c:	char shtypes[] = "=/)%?![";
 * mklev.c:	"=/)%?![<>"
 * hack.mkobj.c:	char mkobjstr[] = "))[[!!!!????%%%%/=**";
 * hack.apply.c:   otmp = getobj("0#%", "put in");
 * hack.eat.c:     otmp = getobj("%", "eat");
 * hack.invent.c:          if(index("!%?[)=*(0/\"", sym)){
 * hack.invent.c:    || index("%?!*",otmp->olet))){
 */
