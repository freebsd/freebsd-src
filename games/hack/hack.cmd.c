/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.cmd.c - version 1.0.3 */
/* $FreeBSD$ */

#include	"hack.h"
#include	"def.func_tab.h"

int doredraw(),doredotopl(),dodrop(),dodrink(),doread(),dosearch(),dopickup(),
doversion(),doweararm(),dowearring(),doremarm(),doremring(),dopay(),doapply(),
dosave(),dowield(),ddoinv(),dozap(),ddocall(),dowhatis(),doengrave(),dotele(),
dohelp(),doeat(),doddrop(),do_mname(),doidtrap(),doprwep(),doprarm(),
doprring(),doprgold(),dodiscovered(),dotypeinv(),dolook(),doset(),
doup(), dodown(), done1(), donull(), dothrow(), doextcmd(), dodip(), dopray();
#ifdef SHELL
int dosh();
#endif SHELL
#ifdef SUSPEND
int dosuspend();
#endif SUSPEND

struct func_tab cmdlist[]={
	'\020', doredotopl,
	'\022', doredraw,
	'\024', dotele,
#ifdef SUSPEND
	'\032', dosuspend,
#endif SUSPEND
	'a', doapply,
/*	'A' : UNUSED */
/*	'b', 'B' : go sw */
	'c', ddocall,
	'C', do_mname,
	'd', dodrop,
	'D', doddrop,
	'e', doeat,
	'E', doengrave,
/*	'f', 'F' : multiple go (might become 'fight') */
/*	'g', 'G' : UNUSED */
/*	'h', 'H' : go west */
	'I', dotypeinv,		/* Robert Viduya */
	'i', ddoinv,
/*	'j', 'J', 'k', 'K', 'l', 'L', 'm', 'M', 'n', 'N' : move commands */
/*	'o', doopen,	*/
	'O', doset,
	'p', dopay,
	'P', dowearring,
	'q', dodrink,
	'Q', done1,
	'r', doread,
	'R', doremring,
	's', dosearch,
	'S', dosave,
	't', dothrow,
	'T', doremarm,
/*	'u', 'U' : go ne */
	'v', doversion,
/*	'V' : UNUSED */
	'w', dowield,
	'W', doweararm,
/*	'x', 'X' : UNUSED */
/*	'y', 'Y' : go nw */
	'z', dozap,
/*	'Z' : UNUSED */
	'<', doup,
	'>', dodown,
	'/', dowhatis,
	'?', dohelp,
#ifdef SHELL
	'!', dosh,
#endif SHELL
	'.', donull,
	' ', donull,
	',', dopickup,
	':', dolook,
	'^', doidtrap,
	'\\', dodiscovered,		/* Robert Viduya */
	 WEAPON_SYM,  doprwep,
	 ARMOR_SYM,  doprarm,
	 RING_SYM,  doprring,
	'$', doprgold,
	'#', doextcmd,
	0,0,0
};

struct ext_func_tab extcmdlist[] = {
	"dip", dodip,
	"pray", dopray,
	(char *) 0, donull
};

extern char *parse(), lowc(), unctrl(), quitchars[];

rhack(cmd)
char *cmd;
{
	struct func_tab *tlist = cmdlist;
	boolean firsttime = FALSE;
	int res;

	if(!cmd) {
		firsttime = TRUE;
		flags.nopick = 0;
		cmd = parse();
	}
	if(!*cmd || (*cmd & 0377) == 0377 ||
	   (flags.no_rest_on_space && *cmd == ' ')){
		bell();
		flags.move = 0;
		return;		/* probably we just had an interrupt */
	}
	if(movecmd(*cmd)) {
	walk:
		if(multi) flags.mv = 1;
		domove();
		return;
	}
	if(movecmd(lowc(*cmd))) {
		flags.run = 1;
	rush:
		if(firsttime){
			if(!multi) multi = COLNO;
			u.last_str_turn = 0;
		}
		flags.mv = 1;
#ifdef QUEST
		if(flags.run >= 4) finddir();
		if(firsttime){
			u.ux0 = u.ux + u.dx;
			u.uy0 = u.uy + u.dy;
		}
#endif QUEST
		domove();
		return;
	}
	if((*cmd == 'f' && movecmd(cmd[1])) || movecmd(unctrl(*cmd))) {
		flags.run = 2;
		goto rush;
	}
	if(*cmd == 'F' && movecmd(lowc(cmd[1]))) {
		flags.run = 3;
		goto rush;
	}
	if(*cmd == 'm' && movecmd(cmd[1])) {
		flags.run = 0;
		flags.nopick = 1;
		goto walk;
	}
	if(*cmd == 'M' && movecmd(lowc(cmd[1]))) {
		flags.run = 1;
		flags.nopick = 1;
		goto rush;
	}
#ifdef QUEST
	if(*cmd == cmd[1] && (*cmd == 'f' || *cmd == 'F')) {
		flags.run = 4;
		if(*cmd == 'F') flags.run += 2;
		if(cmd[2] == '-') flags.run += 1;
		goto rush;
	}
#endif QUEST
	while(tlist->f_char) {
		if(*cmd == tlist->f_char){
			res = (*(tlist->f_funct))();
			if(!res) {
				flags.move = 0;
				multi = 0;
			}
			return;
		}
		tlist++;
	}
	{ char expcmd[10];
	  char *cp = expcmd;
	  while(*cmd && cp-expcmd < sizeof(expcmd)-2) {
		if(*cmd >= 040 && *cmd < 0177)
			*cp++ = *cmd++;
		else {
			*cp++ = '^';
			*cp++ = *cmd++ ^ 0100;
		}
	  }
	  *cp++ = 0;
	  pline("Unknown command '%s'.", expcmd);
	}
	multi = flags.move = 0;
}

doextcmd()	/* here after # - now read a full-word command */
{
	char buf[BUFSZ];
	struct ext_func_tab *efp = extcmdlist;

	pline("# ");
	getlin(buf);
	clrlin();
	if(buf[0] == '\033')
		return(0);
	while(efp->ef_txt) {
		if(!strcmp(efp->ef_txt, buf))
			return((*(efp->ef_funct))());
		efp++;
	}
	pline("%s: unknown command.", buf);
	return(0);
}

char
lowc(sym)
char sym;
{
    return( (sym >= 'A' && sym <= 'Z') ? sym+'a'-'A' : sym );
}

char
unctrl(sym)
char sym;
{
    return( (sym >= ('A' & 037) && sym <= ('Z' & 037)) ? sym + 0140 : sym );
}

/* 'rogue'-like direction commands */
char sdir[] = "hykulnjb><";
schar xdir[10] = { -1,-1, 0, 1, 1, 1, 0,-1, 0, 0 };
schar ydir[10] = {  0,-1,-1,-1, 0, 1, 1, 1, 0, 0 };
schar zdir[10] = {  0, 0, 0, 0, 0, 0, 0, 0, 1,-1 };

movecmd(sym)	/* also sets u.dz, but returns false for <> */
char sym;
{
	char *dp;

	u.dz = 0;
	if(!(dp = index(sdir, sym))) return(0);
	u.dx = xdir[dp-sdir];
	u.dy = ydir[dp-sdir];
	u.dz = zdir[dp-sdir];
	return(!u.dz);
}

getdir(s)
boolean s;
{
	char dirsym;

	if(s) pline("In what direction?");
	dirsym = readchar();
	if(!movecmd(dirsym) && !u.dz) {
		if(!index(quitchars, dirsym))
			pline("What a strange direction!");
		return(0);
	}
	if(Confusion && !u.dz)
		confdir();
	return(1);
}

confdir()
{
	int x = rn2(8);
	u.dx = xdir[x];
	u.dy = ydir[x];
}

#ifdef QUEST
finddir(){
int i, ui = u.di;
	for(i = 0; i <= 8; i++){
		if(flags.run & 1) ui++; else ui += 7;
		ui %= 8;
		if(i == 8){
			pline("Not near a wall.");
			flags.move = multi = 0;
			return(0);
		}
		if(!isroom(u.ux+xdir[ui], u.uy+ydir[ui]))
			break;
	}
	for(i = 0; i <= 8; i++){
		if(flags.run & 1) ui += 7; else ui++;
		ui %= 8;
		if(i == 8){
			pline("Not near a room.");
			flags.move = multi = 0;
			return(0);
		}
		if(isroom(u.ux+xdir[ui], u.uy+ydir[ui]))
			break;
	}
	u.di = ui;
	u.dx = xdir[ui];
	u.dy = ydir[ui];
}

isroom(x,y)  x,y; {		/* what about POOL? */
	return(isok(x,y) && (levl[x][y].typ == ROOM ||
				(levl[x][y].typ >= LDOOR && flags.run >= 6)));
}
#endif QUEST

isok(x,y) int x,y; {
	/* x corresponds to curx, so x==1 is the first column. Ach. %% */
	return(x >= 1 && x <= COLNO-1 && y >= 0 && y <= ROWNO-1);
}
