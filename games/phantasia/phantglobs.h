/*
 * phantglobs.h - global declarations for Phantasia
 */

extern	double	Circle;		/* which circle player is in			*/
extern	double	Shield;		/* force field thrown up in monster battle	*/

extern	bool	Beyond;		/* set if player is beyond point of no return	*/
extern	bool	Marsh;		/* set if player is in dead marshes		*/
extern	bool	Throne;		/* set if player is on throne			*/
extern	bool	Changed;	/* set if important player stats have changed	*/
extern	bool	Wizard;		/* set if player is the 'wizard' of the game	*/
extern	bool	Timeout;	/* set if short timeout waiting for input	*/
extern	bool	Windows;	/* set if we are set up for curses stuff	*/
extern	bool	Luckout;	/* set if we have tried to luck out in fight	*/
extern	bool	Foestrikes;	/* set if foe gets a chance to hit in battleplayer()*/
extern	bool	Echo;		/* set if echo input to terminal		*/

extern	int	Users;		/* number of users currently playing		*/
extern	int	Whichmonster;	/* which monster we are fighting		*/
extern	int	Lines;		/* line on screen counter for fight routines	*/

extern	jmp_buf Fightenv;	/* used to jump into fight routine		*/
extern	jmp_buf Timeoenv;	/* used for timing out waiting for input	*/

extern	long	Fileloc;	/* location in file of player statistics	*/

extern	char	*Login;		/* pointer to login of current player		*/
extern	char	*Enemyname;	/* pointer name of monster/player we are battling*/

extern	struct player	Player;	/* stats for player				*/
extern	struct player	Other;	/* stats for another player			*/

extern	struct monster	Curmonster;/* stats for current monster			*/

extern	struct energyvoid Enrgyvoid;/* energy void buffer			*/

extern	struct charstats Stattable[];/* used for rolling and changing player stats*/

extern	struct charstats *Statptr;/* pointer into Stattable[]			*/

extern	struct menuitem	Menu[];	/* menu of items for purchase			*/

extern	FILE	*Playersfp;	/* pointer to open player file			*/
extern	FILE	*Monstfp;	/* pointer to open monster file			*/
extern	FILE	*Messagefp;	/* pointer to open message file			*/
extern	FILE	*Energyvoidfp;	/* pointer to open energy void file		*/

extern	char	Databuf[];	/* a place to read data into			*/

/* some canned strings for messages */
extern	char	Illcmd[];
extern	char	Illmove[];
extern	char	Illspell[];
extern	char	Nomana[];
extern	char	Somebetter[];
extern	char	Nobetter[];

/* library functions and system calls */
extern	long	time();
extern	char	*getlogin();
extern	char	*getpass();
extern	char	*strchr();
extern	char	*strcat();
extern	char	*strcpy();
extern	char	*strncpy();
extern	char	*getenv();
struct	passwd	*getpwuid();
extern	char	*fgets();

/* functions which we need to know about */
extern	int	interrupt();
extern	int	ill_sig();
extern	void	catchalarm();
extern	long	recallplayer();
extern	long	findname();
extern	long	allocrecord();
extern	long	rollnewplayer();
extern	long	allocvoid();
extern	double	drandom();
extern	double	distance();
extern	double	infloat();
extern	double	explevel();
extern	char	*descrlocation();
extern	char	*descrtype();
extern	char	*descrstatus();
