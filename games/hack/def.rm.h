/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* def.rm.h - version 1.0.2 */

/* Level location types */
#define	HWALL 1
#define	VWALL 2
#define	SDOOR 3
#define	SCORR 4
#define	LDOOR 5
#define	POOL	6	/* not yet fully implemented */
			/* this should in fact be a bit like lit */
#define	DOOR 7
#define	CORR 8
#define	ROOM 9
#define	STAIRS 10

/*
 * Avoid using the level types in inequalities:
 *  these types are subject to change.
 * Instead, use one of the macros below.
 */
#define	IS_WALL(typ)	((typ) <= VWALL)
#define IS_ROCK(typ)	((typ) < POOL)		/* absolutely nonaccessible */
#define	ACCESSIBLE(typ)	((typ) >= DOOR)			/* good position */
#define	IS_ROOM(typ)		((typ) >= ROOM)		/* ROOM or STAIRS */
#define	ZAP_POS(typ)		((typ) > DOOR)

/*
 * A few of the associated symbols are not hardwired.
 */
#ifdef QUEST
#define	CORR_SYM	':'
#else
#define	CORR_SYM	'#'
#endif QUEST
#define	POOL_SYM	'}'

#define	ERRCHAR	'{'

/*
 * The structure describing a coordinate position.
 * Before adding fields, remember that this will significantly affect
 * the size of temporary files and save files.
 */
struct rm {
	char scrsym;
	unsigned typ:5;
	unsigned new:1;
	unsigned seen:1;
	unsigned lit:1;
};
extern struct rm levl[COLNO][ROWNO];
