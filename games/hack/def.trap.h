/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* def.trap.h - version 1.0.2 */

struct trap {
	struct trap *ntrap;
	xchar tx,ty;
	unsigned ttyp:5;
	unsigned tseen:1;
	unsigned once:1;
};

extern struct trap *ftrap;
struct trap *t_at();
#define newtrap()	(struct trap *) alloc(sizeof(struct trap))

/* various kinds of traps */
#define BEAR_TRAP	0
#define	ARROW_TRAP	1
#define	DART_TRAP	2
#define TRAPDOOR	3
#define	TELEP_TRAP	4
#define PIT 		5
#define SLP_GAS_TRAP	6
#define	PIERC		7
#define	MIMIC		8	/* used only in mklev.c */
#define TRAPNUM 	9	/* if not less than 32, change sizeof(ttyp) */
				/* see also mtrapseen (bit map) */
