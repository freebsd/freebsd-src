/* cmds.h */

#ifndef _cmd_h_
#define _cmd_h_

/*  $RCSfile: cmds.h,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/07/09 10:58:19 $
 */

/* Verbosity levels. */
#define V_QUIET		-1
#define V_ERRS		0
#define V_TERSE		1
#define V_VERBOSE	2
#define V_IMPLICITCD 4
#define IS_VQUIET	(verbose <= V_QUIET)
#define IS_VERRS	(verbose == V_ERRS)
#define IS_VTERSE	(verbose == V_TERSE)
#define IS_VVERBOSE	(verbose == V_VERBOSE)
#define NOT_VQUIET	(verbose > V_QUIET)

/* Open modes. */
#define OPEN_A 1
#define OPEN_U 0

#define LS_FLAGS_AND_FILE '\1'

/* Possible values returned by GetDateAndTime. */
#define SIZE_UNKNOWN (-1L)
#define MDTM_UNKNOWN (0L)

/* Command result codes. */
#define USAGE (88)
#define NOERR (0)
#define CMDERR (-1)

/*
 * Format of command table.
 */
struct cmd {
	char	*c_name;	/* name of command */
	char	c_conn;		/* must be connected to use command */
	char	c_hidden;	/* a hidden command or alias (won't show up in help) */
	int		(*c_handler)(int, char **);	/* function to call */
	char	*c_help;	/* help string */
	char	*c_usage;	/* usage string or NULL, to ask the function itself. */
};

#define NCMDS ((int) ((sizeof (cmdtab) / sizeof (struct cmd)) - 1))

struct macel {
	char mac_name[9];	/* macro name */
	char *mac_start;	/* start of macro in macbuf */
	char *mac_end;		/* end of macro in macbuf */
};

struct types {
	char	*t_name;
	char	*t_mode;
	int		t_type;
	char	*t_arg;
};

struct lslist {
	char			*string;
	struct lslist	*next;
};

int settype(int argc, char **argv);
int _settype(char *typename);
int setbinary(int argc, char **argv);
int setascii(int argc, char **argv);
int put(int argc, char **argv);
int mput(int argc, char **argv);
int rem_glob_one(char *pattern);
int get(int argc, char **argv);
void mabort SIG_PARAMS;
int mget(int argc, char **argv);
char *remglob(char *argv[], int *);
int setverbose(int argc, char **argv);
int setprompt(int argc, char **argv);
int setdebug(int argc, char **argv);
void fix_options(void);
int cd(int argc, char **argv);
int implicit_cd(char *dir);
int _cd(char *dir);
int lcd(int argc, char **argv);
int do_delete(int argc, char **argv);
int mdelete(int argc, char **argv);
int renamefile(int argc, char **argv);
int ls(int argc, char **argv);
int shell(int argc, char **argv);
int do_user(int argc, char **argv);
int pwd(int argc, char **argv);
int makedir(int argc, char **argv);
int removedir(int argc, char **argv);
int quote(int argc, char **argv);
int rmthelp(int argc, char **argv);
int quit(int argc, char **argv);
void close_streams(int wantShutDown);
int disconnect(int argc, char **argv);
int close_up_shop(void);
int globulize(char **cpp);
int cdup(int argc, char **argv);
int syst(int argc, char **argv);
int make_macro(char *name, FILE *fp);
int macdef(int argc, char **argv);
int domacro(int argc, char **argv);
int sizecmd(int argc, char **argv);
int modtime(int argc, char **argv);
int lookup(int argc, char **argv);
int rmtstatus(int argc, char **argv);
int create(int argc, char **argv);
int getlocalhostname(char *host, size_t size);
int show_version(int argc, char **argv);
void PurgeLineBuffer(void);
int ShowLineBuffer(int argc, char **argv);
int MallocStatusCmd(int argc, char **argv);
int unimpl(int argc, char **argv);
long GetDateSizeFromLSLine(char *fName, unsigned long *mod_time);
long GetDateAndSize(char *fName, unsigned long *mod_time);
int SetTypeByNumber(int i);
int setpassive(int argc, char **argv);
int setrestrict(int argc, char **argv);


/* In util.c: */
void cmd_help(struct cmd *c);
void cmd_usage(struct cmd *c);
struct cmd *getcmd(char *name);

#endif	/* _cmd_h_ */
