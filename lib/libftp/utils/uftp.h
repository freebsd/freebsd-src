#include <FtpLibrary.h>
#include <strings.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <arpa/telnet.h>
#include <pwd.h>
#include <errno.h>
#include <glob.h>


#define SYSTEMRC "/usr/share/etc/uftprc"
#define LINK ftp[frame]
#define NFRAMES 10
#define TIME(proc) settimer(), status = proc , showtimer(), status
#define ARGS char *w1,char *w2,char *w3,char *w4,char *w5,char *w6
#define log(x) FtpLog("uftp",x)

typedef struct
{
  String host;
  String user;
  String pass;
  String pwd;
  int lock;
} LINKINFO;

typedef struct
{
  char *cmd;
  int (*func)();
  int need;
  char *help;
} CMDS;

typedef struct _alias
{
  String name,str;
  struct _alias *next;
} ALIAS;

extern ALIAS *firstalias;
extern FTP *ftp[NFRAMES];
extern LINKINFO iftp[NFRAMES];
extern int frame;
extern int lastcmd;
extern int glassmode;
extern int trymode;
extern int hashmode;
extern int restmode;
extern int sleeptime;
extern time_t noopinterval,nooptimeout;
extern CMDS cmds[];
extern int status;
extern String prompt;
extern String defaultuser;
extern jmp_buf start;

char *word(char *,int);
char *readline(char *);
char *getpass(char *);
char *getrcname();
char *getaliasrcname();
char *makestr();
char *expandalias(char *str);
char *getprompt();
char *makefilename(char *,char *);

void intr(int);
void noop();
int myhash(FTP *,unsigned int);
STATUS my_error(FTP *, int, char *);









