/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.monst.c - version 1.0.2 */

#include "hack.h"
#include "def.eshk.h"
extern char plname[PL_NSIZ];

struct permonst mons[CMNUM+2] = {
	{ "bat",		'B',1,22,8,1,4,0 },
	{ "gnome",		'G',1,6,5,1,6,0 },
	{ "hobgoblin",		'H',1,9,5,1,8,0 },
	{ "jackal",		'J',0,12,7,1,2,0 },
	{ "kobold",		'K',1,6,7,1,4,0 },
	{ "leprechaun",		'L',5,15,8,1,2,0 },
	{ "giant rat",		'r',0,12,7,1,3,0 },
	{ "acid blob",		'a',2,3,8,0,0,0 },
	{ "floating eye",	'E',2,1,9,0,0,0 },
	{ "homunculus",		'h',2,6,6,1,3,0 },
	{ "imp",		'i',2,6,2,1,4,0 },
	{ "orc",		'O',2,9,6,1,8,0 },
	{ "yellow light",	'y',3,15,0,0,0,0 },
	{ "zombie",		'Z',2,6,8,1,8,0 },
	{ "giant ant",		'A',3,18,3,1,6,0 },
	{ "fog cloud",		'f',3,1,0,1,6,0 },
	{ "nymph",		'N',6,12,9,1,2,0 },
	{ "piercer",		'p',3,1,3,2,6,0 },
	{ "quasit",		'Q',3,15,3,1,4,0 },
	{ "quivering blob",	'q',3,1,8,1,8,0 },
	{ "violet fungi",	'v',3,1,7,1,4,0 },
	{ "giant beetle",	'b',4,6,4,3,4,0 },
	{ "centaur",		'C',4,18,4,1,6,0 },
	{ "cockatrice",		'c',4,6,6,1,3,0 },
	{ "gelatinous cube",	'g',4,6,8,2,4,0 },
	{ "jaguar",		'j',4,15,6,1,8,0 },
	{ "killer bee",		'k',4,14,4,2,4,0 },
	{ "snake",		'S',4,15,3,1,6,0 },
	{ "freezing sphere",	'F',2,13,4,0,0,0 },
	{ "owlbear",		'o',5,12,5,2,6,0 },
	{ "rust monster",	'R',10,18,3,0,0,0 },
	{ "scorpion",		's',5,15,3,1,4,0 },
	{ "tengu",		't',5,13,5,1,7,0 },
	{ "wraith",		'W',5,12,5,1,6,0 },
#ifdef NOWORM
	{ "wumpus",		'w',8,3,2,3,6,0 },
#else
	{ "long worm",		'w',8,3,5,1,4,0 },
#endif NOWORM
	{ "large dog",		'd',6,15,4,2,4,0 },
	{ "leocrotta",		'l',6,18,4,3,6,0 },
	{ "mimic",		'M',7,3,7,3,4,0 },
	{ "troll",		'T',7,12,4,2,7,0 },
	{ "unicorn",		'u',8,24,5,1,10,0 },
	{ "yeti",		'Y',5,15,6,1,6,0 },
	{ "stalker",		'I',8,12,3,4,4,0 },
	{ "umber hulk",		'U',9,6,2,2,10,0 },
	{ "vampire",		'V',8,12,1,1,6,0 },
	{ "xorn",		'X',8,9,-2,4,6,0 },
	{ "xan",		'x',7,18,-2,2,4,0 },
	{ "zruty",		'z',9,8,3,3,6,0 },
	{ "chameleon",		':',6,5,6,4,2,0 },
	{ "dragon",		'D',10,9,-1,3,8,0 },
	{ "ettin",		'e',10,12,3,2,8,0 },
	{ "lurker above",	'\'',10,3,3,0,0,0 },
	{ "nurse",		'n',11,6,0,1,3,0 },
	{ "trapper",		',',12,3,3,0,0,0 },
	{ "purple worm",	'P',15,9,6,2,8,0 },
	{ "demon",		'&',10,12,-4,1,4,0 },
	{ "minotaur",		'm',15,15,6,4,10,0 },
	{ "shopkeeper", 	'@', 12, 18, 0, 4, 8, sizeof(struct eshk) }
};

struct permonst pm_ghost = { "ghost", ' ', 10, 3, -5, 1, 1, sizeof(plname) };
struct permonst pm_wizard = {
	"wizard of Yendor", '1', 15, 12, -2, 1, 12, 0
};
#ifdef MAIL
struct permonst pm_mail_daemon = { "mail daemon", '2', 100, 1, 10, 0, 0, 0 };
#endif MAIL
struct permonst pm_eel = { "giant eel", ';', 15, 6, -3, 3, 6, 0 };
