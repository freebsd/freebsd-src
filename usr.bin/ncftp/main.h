/* main.h */

#ifndef _main_h_
#define _main_h_

/*  $RCSfile: main.h,v $
 *  $Revision: 14020.12 $
 *  $Date: 93/05/21 05:45:33 $
 */

struct userinfo {
	str32   username;
	string  homedir;
	string  shell;
	string  hostname;
	int		uid;
};

void intr SIG_PARAMS;
int getuserinfo(void);
int init_arrays(void);
void init_transfer_buffer(void);
void init_prompt(void);
void lostpeer SIG_PARAMS;
int cmdscanner(int top);
char *strprompt(void);
void makeargv(void);
char *slurpstring(void);
int help(int argc, char **argv);
void trim_log(void);
int CheckNewMail(void);

#ifdef CURSES
	void tcap_put(char *cap);
	void termcap_init(void);
	int termcap_get(char **dest, char *attr);
#	ifndef TERMH /* <term.h> would take care of this. */
#		ifdef NO_CONST
			extern char *tgetstr(char *, char **);
#		else
			extern char *tgetstr(const char *, char **);
#		endif
#	endif	/* TERMH */
#endif	/* CURSES */

/* Should be in a 'tips.h,' but... */
void PrintTip(void);

#endif	/* _main_h_ */

