/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* def.obj.h - version 1.0.3 */

struct obj {
	struct obj *nobj;
	unsigned o_id;
	unsigned o_cnt_id;		/* id of container object is in */
	xchar ox,oy;
	xchar odx,ody;
	uchar otyp;
	uchar owt;
	uchar quan;		/* use oextra for tmp gold objects */
	schar spe;		/* quality of weapon, armor or ring (+ or -)
				   number of charges for wand ( >= -1 )
				   special for uball and amulet %% BAH */
	char olet;
	char invlet;
	Bitfield(oinvis,1);	/* not yet implemented */
	Bitfield(odispl,1);
	Bitfield(known,1);	/* exact nature known */
	Bitfield(dknown,1);	/* color or text known */
	Bitfield(cursed,1);
	Bitfield(unpaid,1);	/* on some bill */
	Bitfield(rustfree,1);
	Bitfield(onamelth,6);
	long age;		/* creation date */
	long owornmask;
#define	W_ARM	01L
#define	W_ARM2	02L
#define	W_ARMH	04L
#define	W_ARMS	010L
#define	W_ARMG	020L
#define	W_ARMOR		(W_ARM | W_ARM2 | W_ARMH | W_ARMS | W_ARMG)
#define	W_RINGL	010000L	/* make W_RINGL = RING_LEFT (see uprop) */
#define	W_RINGR	020000L
#define	W_RING		(W_RINGL | W_RINGR)
#define	W_WEP	01000L
#define	W_BALL	02000L
#define	W_CHAIN	04000L
	long oextra[1];		/* used for name of ordinary objects - length
				   is flexible; amount for tmp gold objects */
};

extern struct obj *fobj;

#define newobj(xl)	(struct obj *) alloc((unsigned)(xl) + sizeof(struct obj))
#define	ONAME(otmp)	((char *) otmp->oextra)
#define	OGOLD(otmp)	(otmp->oextra[0])
