/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)externs.h	8.1 (Berkeley) 5/31/93
 *
 *	$FreeBSD$
 */

#include <errno.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BITS (8)

#define OUTSIDE		(position > 68 && position < 246 && position != 218)
#define rnd(x)          (random() % (x))
#define max(a,b)	((a) < (b) ? (b) : (a))
#define testbit(array, index)	(array[index/BITS] & (1 << (index % BITS)))
#ifndef setbit
#define setbit(array, index)	(array[index/BITS] |= (1 << (index % BITS)))
#endif
#define clearbit(array, index)	(array[index/BITS] &= ~(1 << (index % BITS)))

	/* well known rooms */
#define FINAL	275
#define GARDEN	197
#define POOLS	126
#define DOCK	93

	/* word types */
#define VERB	0
#define OBJECT  1
#define NOUNS	2
#define PREPS	3
#define ADJS	4
#define CONJ	5

	/* words numbers */
#define KNIFE		0
#define SWORD		1
#define LAND		2
#define WOODSMAN 	3
#define TWO_HANDED	4
#define CLEAVER		5
#define BROAD		6
#define MAIL		7
#define HELM		8
#define SHIELD		9
#define MAID		10
#define BODY		10
#define VIPER		11
#define LAMPON		12
#define SHOES		13
#define CYLON		14
#define PAJAMAS		15
#define ROBE		16
#define AMULET		17
#define MEDALION	18
#define TALISMAN	19
#define DEADWOOD	20
#define MALLET		21
#define LASER		22
#define BATHGOD		23
#define NORMGOD		24
#define GRENADE		25
#define CHAIN		26
#define ROPE		27
#define LEVIS		28
#define MACE		29
#define SHOVEL		30
#define HALBERD		31
#define	COMPASS		32
#define	CRASH		33
#define ELF		34
#define FOOT		35
#define COINS		36
#define MATCHES		37
#define MAN		38
#define PAPAYAS		39
#define PINEAPPLE	40
#define KIWI		41
#define COCONUTS	42
#define MANGO		43
#define RING		44
#define POTION		45
#define BRACELET	46
#define GIRL		47
#define GIRLTALK	48
#define DARK		49
#define TIMER		50
#define CHAR		53
#define BOMB		54
#define DEADGOD		55
#define DEADTIME	56
#define DEADNATIVE	57
#define NATIVE		58
#define HORSE		59
#define CAR		60
#define POT		61
#define BAR		62
#define	BLOCK		63
#define NUMOFOBJECTS	64
	/* non-objects below */
#define UP	1000
#define DOWN	1001
#define AHEAD	1002
#define BACK	1003
#define RIGHT	1004
#define LEFT	1005
#define TAKE	1006
#define USE	1007
#define LOOK	1008
#define QUIT	1009
#define NORTH	1010
#define SOUTH	1011
#define EAST	1012
#define WEST	1013
#define SU      1014
#define DROP	1015
#define TAKEOFF	1016
#define DRAW	1017
#define PUTON	1018
#define WEARIT	1019
#define PUT	1020
#define INVEN	1021
#define EVERYTHING 1022
#define AND	1023
#define KILL	1024
#define RAVAGE	1025
#define UNDRESS	1026
#define THROW	1027
#define LAUNCH	1028
#define LANDIT	1029
#define LIGHT	1030
#define FOLLOW	1031
#define KISS	1032
#define LOVE	1033
#define GIVE	1034
#define SMITE	1035
#define SHOOT	1036
#define ON	1037
#define	OFF	1038
#define TIME	1039
#define SLEEP	1040
#define DIG	1041
#define EAT	1042
#define SWIM	1043
#define DRINK	1044
#define DOOR	1045
#define SAVE	1046
#define RIDE	1047
#define DRIVE	1048
#define SCORE	1049
#define BURY	1050
#define JUMP	1051
#define KICK	1052

	/* injuries */
#define ARM	6		/* broken arm */
#define RIBS	7		/* broken ribs */
#define SPINE	9		/* broken back */
#define SKULL	11		/* fractured skull */
#define INCISE	10		/* deep incisions */
#define NECK	12		/* broken NECK */
#define NUMOFINJURIES 13

	/* notes */
#define	CANTLAUNCH	0
#define LAUNCHED	1
#define CANTSEE		2
#define CANTMOVE	3
#define JINXED		4
#define DUG		5
#define NUMOFNOTES	6

	/* fundamental constants */
#define NUMOFROOMS	275
#define NUMOFWORDS	((NUMOFOBJECTS + BITS - 1) / BITS)
#define LINELENGTH	81

#define TODAY		0
#define TONIGHT		1
#define CYCLE		100

	/* initial variable values */
#define TANKFULL	250
#define TORPEDOES	10
#define MAXWEIGHT	60
#define MAXCUMBER	10

struct room {
	const char *name;
	int link[8];
#define north	link[0]
#define south	link[1]
#define east	link[2]
#define west	link[3]
#define up	link[4]
#define access	link[5]
#define down	link[6]
#define flyhere	link[7]
	const char *desc;
	unsigned int objects[NUMOFWORDS];
};
extern struct room dayfile[];
extern struct room nightfile[];
struct room *location;

	/* object characteristics */
extern const char *const objdes[NUMOFOBJECTS];
extern const char *const objsht[NUMOFOBJECTS];
extern const char *const ouch[NUMOFINJURIES];
extern const int objwt[NUMOFOBJECTS];
extern const int objcumber[NUMOFOBJECTS];

	/* current input line */
#define NWORD	20			/* words per line */
char words[NWORD][15];
int wordvalue[NWORD];
int wordtype[NWORD];
int wordcount, wordnumber;


int	 battlestar_move __P((int, int));
void	 bury __P((void));
int	 card __P((const char *, int));
void	 chime __P((void));
void	 crash __P((void));
int	 cypher __P((void));
void	 die __P((int));
void	 dig __P((void));
int	 draw __P((void));
void	 drink __P((void));
int	 drive __P((void));
int	 drop __P((const char *name));
int	 eat __P((void));
int	 fight __P((int, int));
int	 follow __P((void));
char	*getcom __P((char *, int, const char *, const char *));
char	*getword __P((char *, char *, int));
int	 give __P((void));
void	 initialize __P((int));
int	 jump __P((void));
void	 kiss __P((void));
int	 land __P((void));
int	 launch __P((void));
void	 light __P((void));
void	 live __P((void));
void	 love __P((void));
void	 murder __P((void));
void	 news __P((void));
void	 newway  __P((int));
void	 open_score_file __P((void));
void	 parse __P((void));
void	 printobjs __P((void));
int	 put __P((void));
int	 puton __P((void));
void	 ravage __P((void));
const char *rate __P((void));
void	 restore __P((void));
int	 ride __P((void));
void	 save __P((void));
int	 shoot __P((void));
int	 take __P((unsigned int from[]));
int	 takeoff(void);
int	 throw __P((const char *name));
const char *truedirec __P((int, unsigned int));
int	 ucard __P((const unsigned int *));
int	 use __P((void));
int	 visual __P((void));
int	 wearit __P((void));
void	 whichway __P((struct room));
void	 wordinit __P((void));
void	 writedes __P((void));
int	 zzz __P((void));

	/* state of the game */
extern int gclock; 
int gtime;
int position;
int direction;
int left, right, ahead, back;
int fuel, torps;
int carrying, encumber;
int rythmn;
extern int followfight;
int ate;
int snooze;
int meetgirl;
extern int followgod;
int godready;
extern int bs_win;
int wintime;
int wiz;
int tempwiz;
int matchlight;
extern int matchcount;
int loved;
int pleasure, power, ego;
extern int WEIGHT;
extern int CUMBER;
int notes[NUMOFNOTES];
unsigned int inven[NUMOFWORDS];
u_int wear[NUMOFWORDS];
char beenthere[NUMOFROOMS+1];
char injuries[NUMOFINJURIES];

char uname[MAXLOGNAME];

struct wlist {
	const char *string;
	int value, article;
	struct wlist *next;
};
#define HASHSIZE	256
#define HASHMUL		81
#define HASHMASK	(HASHSIZE - 1)
struct wlist *hashtab[HASHSIZE];
extern struct wlist wlist[];

struct objs {
	short room;
	short obj;
};
extern const struct objs dayobjs[];
extern const struct objs nightobjs[];

extern gid_t	egid;
