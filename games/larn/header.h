/*	header.h		Larn is copyrighted 1986 by Noah Morgan. */

#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <time.h>
#include <unistd.h>

#define MAXLEVEL 11
	/*	max # levels in the dungeon			*/
#define MAXVLEVEL 3
	/*	max # of levels in the temple of the luran	*/
#define MAXX 67
#define MAXY 17

#define SCORESIZE 10
	/*	this is the number of people on a scoreboard max */
#define MAXPLEVEL 100
	/*	maximum player level allowed		*/
#define MAXMONST 56
	/*	maximum # monsters in the dungeon	*/
#define SPNUM 38
	/*	maximum number of spells in existance	*/
#define MAXSCROLL 28
	/*	maximum number of scrolls that are possible	*/
#define MAXPOTION 35
	/*	maximum number of potions that are possible	*/
#define TIMELIMIT 30000
	/*	the maximum number of moves before the game is called */
#define TAXRATE 1/20
	/*	the tax rate for the LRS */
#define MAXOBJ 93
	/* the maximum number of objects   n < MAXOBJ */

/*	this is the structure definition of the monster data	*/
struct monst
	{
	char	*name;
	char	level;
	short	armorclass;
	char	damage;
	char	attack;
	char	defense;
	char	genocided;
	char 	intelligence; /* monsters intelligence -- used to choose movement */
	short	gold;
	short	hitpoints;
	unsigned long experience;
	};

/*	this is the structure definition for the items in the dnd store */
struct _itm
	{
	short	price;
	char	**mem;
	char	obj;
	char	arg;
	char	qty;
	};

/*	this is the structure that holds the entire dungeon specifications	*/
struct cel
	{
	short	hitp;	/*	monster's hit points	*/
	char	mitem;	/*	the monster ID			*/
	char	item;	/*	the object's ID			*/
	short	iarg;	/*	the object's argument	*/
	char	know;	/*	have we been here before*/
	};

/* this is the structure for maintaining & moving the spheres of annihilation */
struct sphere
	{
	struct sphere *p;	/* pointer to next structure */
	char x,y,lev;		/* location of the sphere */
	char dir;			/* direction sphere is going in */
	char lifetime;		/* duration of the sphere */
	};

/*	defines for the character attribute array	c[]	*/
#define STRENGTH 0		/* characters physical strength not due to objects */
#define INTELLIGENCE 1
#define WISDOM 2
#define CONSTITUTION 3
#define DEXTERITY 4
#define CHARISMA 5
#define HPMAX 6
#define HP 7
#define GOLD 8
#define EXPERIENCE 9
#define LEVEL 10
#define REGEN 11
#define WCLASS 12
#define AC 13
#define BANKACCOUNT 14
#define SPELLMAX 15
#define SPELLS 16
#define ENERGY 17
#define ECOUNTER 18
#define MOREDEFENSES 19
#define WEAR 20
#define PROTECTIONTIME 21
#define WIELD 22
#define AMULET 23
#define REGENCOUNTER 24
#define MOREDAM 25
#define DEXCOUNT 26
#define STRCOUNT 27
#define BLINDCOUNT 28
#define CAVELEVEL 29
#define CONFUSE 30
#define ALTPRO 31
#define HERO 32
#define CHARMCOUNT 33
#define INVISIBILITY 34
#define CANCELLATION 35
#define HASTESELF 36
#define EYEOFLARN 37
#define AGGRAVATE 38
#define GLOBE 39
#define TELEFLAG 40
#define SLAYING 41
#define NEGATESPIRIT 42
#define SCAREMONST 43
#define AWARENESS 44
#define HOLDMONST 45
#define TIMESTOP 46
#define HASTEMONST 47
#define CUBEofUNDEAD 48
#define GIANTSTR 49
#define FIRERESISTANCE 50
#define BESSMANN 51
#define NOTHEFT 52
#define HARDGAME 53
#define CPUTIME 54
#define BYTESIN 55
#define BYTESOUT 56
#define MOVESMADE 57
#define MONSTKILLED 58
#define SPELLSCAST 59
#define LANCEDEATH 60
#define SPIRITPRO 61
#define UNDEADPRO 62
#define SHIELD 63
#define STEALTH 64
#define ITCHING 65
#define LAUGHING 66
#define DRAINSTRENGTH 67
#define CLUMSINESS 68
#define INFEEBLEMENT 69
#define HALFDAM 70
#define SEEINVISIBLE 71
#define FILLROOM 72
#define RANDOMWALK 73
#define SPHCAST 74	/* nz if an active sphere of annihilation */
#define WTW 75		/* walk through walls */
#define STREXTRA 76	/* character strength due to objects or enchantments */
#define TMP 77	/* misc scratch space */
#define LIFEPROT 78 /* life protection counter */

/*	defines for the objects in the game		*/

#define OALTAR 1
#define OTHRONE 2
#define OORB 3
#define OPIT 4
#define OSTAIRSUP 5
#define OELEVATORUP 6
#define OFOUNTAIN 7
#define OSTATUE 8
#define OTELEPORTER 9
#define OSCHOOL 10
#define OMIRROR 11
#define ODNDSTORE 12
#define OSTAIRSDOWN 13
#define OELEVATORDOWN 14
#define OBANK2 15
#define OBANK 16
#define ODEADFOUNTAIN 17
#define OMAXGOLD 70
#define OGOLDPILE 18
#define OOPENDOOR 19
#define OCLOSEDDOOR 20
#define OWALL 21
#define OTRAPARROW 66
#define OTRAPARROWIV 67

#define OLARNEYE 22

#define OPLATE 23
#define OCHAIN 24
#define OLEATHER 25
#define ORING 60
#define OSTUDLEATHER 61
#define OSPLINT 62
#define OPLATEARMOR 63
#define OSSPLATE 64
#define OSHIELD 68
#define OELVENCHAIN 92

#define OSWORDofSLASHING 26
#define OHAMMER 27
#define OSWORD 28
#define O2SWORD 29
#define OSPEAR 30
#define ODAGGER 31
#define OBATTLEAXE 57
#define OLONGSWORD 58
#define OFLAIL 59
#define OLANCE 65
#define OVORPAL 90
#define OSLAYER 91

#define ORINGOFEXTRA 32
#define OREGENRING 33
#define OPROTRING 34
#define OENERGYRING 35
#define ODEXRING 36
#define OSTRRING 37
#define OCLEVERRING 38
#define ODAMRING 39

#define OBELT 40

#define OSCROLL 41
#define OPOTION 42
#define OBOOK 43
#define OCHEST 44
#define OAMULET 45

#define OORBOFDRAGON 46
#define OSPIRITSCARAB 47
#define OCUBEofUNDEAD 48
#define ONOTHEFT 49

#define ODIAMOND 50
#define ORUBY 51
#define OEMERALD 52
#define OSAPPHIRE 53

#define OENTRANCE 54
#define OVOLDOWN 55
#define OVOLUP 56
#define OHOME 69

#define OKGOLD 71
#define ODGOLD 72
#define OIVDARTRAP 73
#define ODARTRAP 74
#define OTRAPDOOR 75
#define OIVTRAPDOOR 76
#define OTRADEPOST 77
#define OIVTELETRAP 78
#define ODEADTHRONE 79
#define OANNIHILATION 80		/* sphere of annihilation */
#define OTHRONE2 81
#define OLRS 82				/* Larn Revenue Service */
#define OCOOKIE 83
#define OURN 84
#define OBRASSLAMP 85
#define OHANDofFEAR 86		/* hand of fear */
#define OSPHTAILSMAN 87		/* tailsman of the sphere */
#define OWWAND 88			/* wand of wonder */
#define OPSTAFF 89			/* staff of power */
/* used up to 92 */

/*	defines for the monsters as objects		*/

#define BAT 1
#define GNOME 2
#define HOBGOBLIN 3
#define JACKAL 4
#define KOBOLD 5
#define ORC 6
#define SNAKE 7
#define CENTIPEDE 8
#define JACULI 9
#define TROGLODYTE 10
#define ANT 11
#define EYE 12
#define LEPRECHAUN 13
#define NYMPH 14
#define QUASIT 15
#define RUSTMONSTER 16
#define ZOMBIE 17
#define ASSASSINBUG 18
#define BUGBEAR 19
#define HELLHOUND 20
#define ICELIZARD 21
#define CENTAUR 22
#define TROLL 23
#define YETI 24
#define WHITEDRAGON 25
#define ELF 26
#define CUBE 27
#define METAMORPH 28
#define VORTEX 29
#define ZILLER 30
#define VIOLETFUNGI 31
#define WRAITH 32
#define FORVALAKA 33
#define LAMANOBE 34
#define OSEQUIP 35
#define ROTHE 36
#define XORN 37
#define VAMPIRE 38
#define INVISIBLESTALKER 39
#define POLTERGEIST 40
#define DISENCHANTRESS 41
#define SHAMBLINGMOUND 42
#define YELLOWMOLD 43
#define UMBERHULK 44
#define GNOMEKING 45
#define MIMIC 46
#define WATERLORD 47
#define BRONZEDRAGON 48
#define GREENDRAGON 49
#define PURPLEWORM 50
#define XVART 51
#define SPIRITNAGA 52
#define SILVERDRAGON 53
#define PLATINUMDRAGON 54
#define GREENURCHIN 55
#define REDDRAGON 56
#define DEMONLORD 57
#define DEMONPRINCE 64

#define NULL 0
#define BUFBIG	4096			/* size of the output buffer */
#define MAXIBUF	4096			/* size of the input buffer */
#define LOGNAMESIZE 40			/* max size of the players name */
#define PSNAMESIZE 40			/* max size of the process name */

#ifndef NODEFS
extern char VERSION,SUBVERSION;
extern char aborted[],alpha[],beenhere[],boldon,cheat,ckpfile[],ckpflag;
extern char *class[],course[],diagfile[],fortfile[],helpfile[];
extern char *inbuffer,is_alpha[],is_digit[];
extern char item[MAXX][MAXY],iven[],know[MAXX][MAXY],larnlevels[],lastmonst[];
extern char level,*levelname[],logfile[],loginname[],logname[],*lpbuf,*lpend;
extern char *lpnt,moved[MAXX][MAXY],mitem[MAXX][MAXY],monstlevel[];
extern char monstnamelist[],nch[],ndgg[],nlpts[],nomove,nosignal,nowelcome;
extern char nplt[],nsw[],*objectname[];
extern char objnamelist[],optsfile[],*potionname[],playerids[],potprob[];
extern char predostuff,psname[],restorflag,savefilename[],scorefile[],scprob[];
extern char screen[MAXX][MAXY],*scrollname[],sex,*spelcode[],*speldescript[];
extern char spelknow[],*spelname[],*spelmes[],spelweird[MAXMONST+8][SPNUM];
extern char splev[],stealth[MAXX][MAXY],to_lower[],to_upper[],wizard;
extern short diroffx[],diroffy[],hitflag,hit2flag,hit3flag,hitp[MAXX][MAXY];
extern short iarg[MAXX][MAXY],ivenarg[],lasthx,lasthy,lastnum,lastpx,lastpy;
extern short nobeep,oldx,oldy,playerx,playery;
extern int dayplay,enable_scroll,srcount,yrepcount,userid,wisid,lfd,fd;
extern time_t initialtime;
extern long outstanding_taxes,skill[],gtime,c[],cbak[];
extern struct cel *cell;
extern struct monst monster[];
extern struct sphere *spheres;
extern struct _itm itm[];

char *fortune(),*lgetw(),*lgetl();
char *tmcapcnv();
long paytaxes(),lgetc(),lrint();
unsigned long readnum();

	/* macro to create scroll #'s with probability of occurrence */
#define newscroll() (scprob[rund(81)])
	/* macro to return a potion # created with probability of occurrence */
#define newpotion() (potprob[rund(41)])
	/* macro to return the + points on created leather armor */
#define newleather() (nlpts[rund(c[HARDGAME]?13:15)])
	/* macro to return the + points on chain armor */
#define newchain() (nch[rund(10)])
	/* macro to return + points on plate armor */
#define newplate() (nplt[rund(c[HARDGAME]?4:12)])
	/* macro to return + points on new daggers */
#define newdagger() (ndgg[rund(13)])
	/* macro to return + points on new swords */
#define newsword() (nsw[rund(c[HARDGAME]?6:13)])
	/* macro to destroy object at present location */
#define forget() (item[playerx][playery]=know[playerx][playery]=0)
	/* macro to wipe out a monster at a location */
#define disappear(x,y) (mitem[x][y]=know[x][y]=0)

#ifdef VT100
	/* macro to turn on bold display for the terminal */
#define setbold() (lprcat(boldon?"\33[1m":"\33[7m"))
	/* macro to turn off bold display for the terminal */
#define resetbold() (lprcat("\33[m"))
	/* macro to setup the scrolling region for the terminal */
#define setscroll() (lprcat("\33[20;24r"))
	/* macro to clear the scrolling region for the terminal */
#define resetscroll() (lprcat("\33[;24r"))
	/* macro to clear the screen and home the cursor */
#define clear() (lprcat("\33[2J\33[f"), cbak[SPELLS]= -50)
#define cltoeoln() lprcat("\33[K")
#else VT100
	/* defines below are for use in the termcap mode only */
#define ST_START 1
#define ST_END   2
#define BOLD     3
#define END_BOLD 4
#define CLEAR    5
#define CL_LINE  6
#define CL_DOWN 14
#define CURSOR  15
	/* macro to turn on bold display for the terminal */
#define setbold() (*lpnt++ = ST_START)
	/* macro to turn off bold display for the terminal */
#define resetbold() (*lpnt++ = ST_END)
	/* macro to setup the scrolling region for the terminal */
#define setscroll() enable_scroll=1
	/* macro to clear the scrolling region for the terminal */
#define resetscroll() enable_scroll=0
	/* macro to clear the screen and home the cursor */
#define clear() (*lpnt++ =CLEAR, cbak[SPELLS]= -50)
	/* macro to clear to end of line */
#define cltoeoln() (*lpnt++ = CL_LINE)
#endif VT100

	/* macro to output one byte to the output buffer */
#define lprc(ch) ((lpnt>=lpend)?(*lpnt++ =(ch), lflush()):(*lpnt++ =(ch)))

#ifdef MACRORND
extern unsigned long randx;
	/* macro to seed the random number generator */
#define srand(x) (randx=x)
	/* macros to generate random numbers   1<=rnd(N)<=N   0<=rund(N)<=N-1 */
#define rnd(x)  ((((randx=randx*1103515245+12345)>>7)%(x))+1)
#define rund(x) ((((randx=randx*1103515245+12345)>>7)%(x))  )
#endif MACRORND
	/* macros for miscellaneous data conversion */
#define min(x,y) (((x)>(y))?(y):(x))
#define max(x,y) (((x)>(y))?(x):(y))
#define isalpha(x) (is_alpha[x])
#define isdigit(x) (is_digit[x])
#define tolower(x) (to_lower[x])
#define toupper(x) (to_upper[x])
#define lcc(x) (to_lower[x])
#define ucc(x) (to_upper[x])
#endif NODEFS

