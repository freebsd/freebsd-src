/*
 *		PPP User command processing module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: command.c,v 1.50 1997/05/26 00:43:58 brian Exp $
 *
 */
#include <sys/types.h>
#include <ctype.h>
#include <termios.h>
#include <sys/wait.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <paths.h>
#include <alias.h>
#include "fsm.h"
#include "phase.h"
#include "lcp.h"
#include "ipcp.h"
#include "modem.h"
#include "filter.h"
#include "command.h"
#include "alias_cmd.h"
#include "hdlc.h"
#include "loadalias.h"
#include "vars.h"
#include "systems.h"
#include "chat.h"
#include "os.h"

extern void Cleanup(), TtyTermMode(), PacketMode();
extern int  EnableCommand(), DisableCommand(), DisplayCommand();
extern int  AcceptCommand(), DenyCommand();
static int  AliasCommand();
extern int  LocalAuthCommand();
extern int  LoadCommand(), SaveCommand();
extern int  ChangeParity(char *);
extern int  SelectSystem();
extern int  ShowRoute();
extern void TtyOldMode(), TtyCommandMode();
extern struct pppvars pppVars;
extern struct cmdtab const SetCommands[];

extern char *IfDevName;

struct in_addr ifnetmask;
int randinit;

static int ShowCommand(), TerminalCommand(), QuitCommand();
static int CloseCommand(), DialCommand(), DownCommand();
static int SetCommand(), AddCommand(), DeleteCommand();
static int ShellCommand();

static int
HelpCommand(list, argc, argv, plist)
struct cmdtab *list;
int argc;
char **argv;
struct cmdtab *plist;
{
  struct cmdtab *cmd;
  int n;

  if (argc > 0) {
    for (cmd = plist; cmd->name; cmd++) {
      if (strcasecmp(cmd->name, *argv) == 0 && (cmd->lauth & VarLocalAuth)) {
	if (plist == SetCommands)
		printf("set ");
        printf("%s %s\n", cmd->name, cmd->syntax);
        return(1);
      }
    }
    return(1);
  }
  n = 0;
  for (cmd = plist; cmd->func; cmd++) {
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      printf("  %-8s: %-20s\n", cmd->name, cmd->helpmes);
      n++;
    }
  }
  if (n & 1)
    printf("\n");
  return(1);
}

int
IsInteractive()
{
  char *mes = NULL;

  if (mode & MODE_DDIAL)
    mes = "Working in dedicated dial mode.";
  else if (mode & MODE_BACKGROUND)
    mes = "Working in background mode.";
  else if (mode & MODE_AUTO)
    mes = "Working in auto mode.";
  else if (mode & MODE_DIRECT)
    mes = "Working in direct mode.";
  else if (mode & MODE_DEDICATED)
    mes = "Working in dedicated mode.";
  if (mes) {
    printf("%s\n", mes);
    return(0);
  }
  return(1);
}

static int
DialCommand(cmdlist, argc, argv)
struct cmdtab *cmdlist;
int argc;
char **argv;
{
  int tries;

  if (LcpFsm.state > ST_CLOSED) {
    printf("LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return(1);
  }
  if (!IsInteractive())
    return(1);
  if (argc > 0) {
    if (SelectSystem(*argv, CONFFILE) < 0) {
      printf("%s: not found.\n", *argv);
      return(1);
    }
  }
  tries = 0;
  do {
    printf("Dial attempt %u of %d\n", ++tries, VarDialTries);
    modem = OpenModem(mode);
    if (modem < 0) {
      printf("failed to open modem.\n");
      break;
    }
    if (DialModem() == EX_DONE) {
      sleep(1);
      ModemTimeout();
      PacketMode();
      break;
    }
  } while (VarDialTries == 0 || tries < VarDialTries);
  return(1);
}

static int
ShellCommand(cmdlist, argc, argv)
struct cmdtab *cmdlist;
int argc;
char **argv;
{
  const char *shell;
  pid_t shpid;

  if((shell = getenv("SHELL")) == 0) {
    shell = _PATH_BSHELL;
  }
#ifdef SHELL_ONLY_INTERACTIVELY
#ifndef HAVE_SHELL_CMD_WITH_ANY_MODE
  if( mode != MODE_INTER) {
     fprintf(stdout,
             "Can only start a shell in interactive mode\n");
     return(1);
  }
#else
  if(argc == 0 && !(mode & MODE_INTER)) {
      fprintf(stderr,
             "Can only start an interactive shell in interactive mode\n");
      return(1);
  }
#endif /* HAVE_SHELL_CMD_WITH_ANY_MODE */
#else
  if ((mode & (MODE_AUTO|MODE_INTER)) == (MODE_AUTO|MODE_INTER)) {
     fprintf(stdout,
             "Shell is not allowed interactively in auto mode\n");
     return(1);
  }
#endif /* SHELL_ONLY_INTERACTIVELY */
  if((shpid = fork()) == 0) {
     int dtablesize, i ;

     for (dtablesize = getdtablesize(), i = 3; i < dtablesize; i++)
	(void)close(i);

     /*
      * We are running setuid, we should change to
      * real user for avoiding security problems.
      */
     if (setgid(getgid()) < 0) {
	perror("setgid");
	exit(1);
     }
     if (setuid(getuid()) < 0) {
	perror("setuid");
	exit(1);
     }
     TtyOldMode();
     if(argc > 0) {
       /* substitute pseudo args */
       for (i=1; i<argc; i++) {
         if (strcasecmp(argv[i], "HISADDR") == 0) {
           argv[i] = strdup(inet_ntoa(IpcpInfo.his_ipaddr));
         }
         if (strcasecmp(argv[i], "INTERFACE") == 0) {
           argv[i] = strdup(IfDevName);
         }
         if (strcasecmp(argv[i], "MYADDR") == 0) {
           argv[i] = strdup(inet_ntoa(IpcpInfo.want_ipaddr));
         }
       }
       (void)execvp(argv[0], argv);
     }
     else
       (void)execl(shell, shell, NULL);

     fprintf(stdout, "exec() of %s failed\n", argc > 0? argv[0]: shell);
     exit(255);
  }
  if( shpid == (pid_t)-1 ) {
    fprintf(stdout, "Fork failed\n");
  } else {
    int status;
    (void)waitpid(shpid, &status, 0);
  }

  TtyCommandMode(1);

  return(0);
}

static char StrOption[] = "option ..";
static char StrRemote[] = "[remote]";
char StrNull[] = "";

struct cmdtab const Commands[] = {
  { "accept",  NULL,    AcceptCommand,	LOCAL_AUTH,
  	"accept option request",	StrOption},
  { "add",     NULL,	AddCommand,	LOCAL_AUTH,
	"add route",			"dest mask gateway"},
  { "close",   NULL,    CloseCommand,	LOCAL_AUTH,
	"Close connection",		StrNull},
  { "delete",  NULL,    DeleteCommand,	LOCAL_AUTH,
	"delete route",                 "ALL | dest gateway [mask]"},
  { "deny",    NULL,    DenyCommand,	LOCAL_AUTH,
  	"Deny option request",		StrOption},
  { "dial",    "call",  DialCommand,	LOCAL_AUTH,
  	"Dial and login",		StrRemote},
  { "disable", NULL,    DisableCommand,	LOCAL_AUTH,
  	"Disable option",		StrOption},
  { "display", NULL,    DisplayCommand,	LOCAL_AUTH,
  	"Display option configs",	StrNull},
  { "enable",  NULL,    EnableCommand,	LOCAL_AUTH,
  	"Enable option",		StrOption},
  { "passwd",  NULL,	LocalAuthCommand,LOCAL_NO_AUTH,
  	"Password for manipulation", StrOption},
  { "load",    NULL,    LoadCommand,	LOCAL_AUTH,
  	"Load settings",		StrRemote},
  { "save",    NULL,    SaveCommand,	LOCAL_AUTH,
  	"Save settings", StrNull},
  { "set",     "setup", SetCommand,	LOCAL_AUTH,
  	"Set parameters",  "var value"},
  { "shell",   "!",     ShellCommand,   LOCAL_AUTH,
	"Run a subshell",  "[sh command]"},
  { "show",    NULL,    ShowCommand,	LOCAL_AUTH,
  	"Show status and statictics", "var"},
  { "term",    NULL,    TerminalCommand,LOCAL_AUTH,
  	"Enter to terminal mode", StrNull},
  { "alias",   NULL,    AliasCommand,   LOCAL_AUTH,
        "alias control",        "option [yes|no]"},
  { "quit",    "bye",   QuitCommand,	LOCAL_AUTH | LOCAL_NO_AUTH,
	"Quit PPP program", "[all]"},
  { "help",    "?",     HelpCommand,	LOCAL_AUTH | LOCAL_NO_AUTH,
	"Display this message", "[command]", (void *)Commands },
  { NULL,      "down",  DownCommand,	LOCAL_AUTH,
  	"Generate down event",		StrNull},
  { NULL,      NULL,    NULL },
};

extern int ReportCcpStatus();
extern int ReportLcpStatus();
extern int ReportIpcpStatus();
extern int ReportProtStatus();
extern int ReportCompress();
extern int ShowModemStatus();
extern int ReportHdlcStatus();
extern int ShowMemMap();

static char *LogLevelName[] = {
  LM_PHASE,   LM_CHAT,    LM_LQM,   LM_LCP,
  LM_TCPIP,   LM_HDLC,    LM_ASYNC, LM_LINK, 
  LM_CONNECT, LM_CARRIER,
};

static int ShowDebugLevel()
{
  int i;

  printf("%02x: ", loglevel);
  for (i = LOG_PHASE; i < MAXLOGLEVEL; i++) {
    if (loglevel & (1 << i))
      printf("%s ", LogLevelName[i]);
  }
  printf("\n");
  return(1);
}

static int ShowEscape()
{
  int code, bit;

  if (EscMap[32]) {
    for (code = 0; code < 32; code++) {
      if (EscMap[code]) {
        for (bit = 0; bit < 8; bit++) {
          if (EscMap[code] & (1<<bit)) {
            printf(" 0x%02x", (code << 3) + bit);
          }
        }
      }
    }
    printf("\n");
  }
  return(1);
}

static int ShowTimeout()
{
  printf(" Idle Timer: %d secs   LQR Timer: %d secs   Retry Timer: %d secs\n",
    VarIdleTimeout, VarLqrTimeout, VarRetryTimeout);
  return(1);
}

static int ShowAuthKey()
{
  printf("AuthName = %s\n", VarAuthName);
  printf("AuthKey  = %s\n", VarAuthKey);
  return(1);
}

static int ShowVersion()
{
  extern char VarVersion[];
  extern char VarLocalVersion[];

  printf("%s - %s \n", VarVersion, VarLocalVersion);
  return(1);
}

static int ShowLogList()
{
  ListLog();
  return(1);
}

static int ShowReconnect()
{
  printf(" Reconnect Timer:  %d,  %d tries\n",
         VarReconnectTimer, VarReconnectTries);
  return(1);
}

static int ShowRedial()
{
  printf(" Redial Timer: ");

  if (VarRedialTimeout >= 0) {
    printf(" %d seconds, ", VarRedialTimeout);
  }
  else {
    printf(" Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  printf(" Redial Next Timer: ");

  if (VarRedialNextTimeout >= 0) {
    printf(" %d seconds, ", VarRedialNextTimeout);
  }
  else {
    printf(" Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  if (VarDialTries)
      printf("%d dial tries", VarDialTries);

  printf("\n");

  return(1);
}

#ifdef MSEXT
static int ShowMSExt()
{
  printf(" MS PPP extention values \n" );
  printf("   Primary NS     : %s\n", inet_ntoa( ns_entries[0] ));
  printf("   Secondary NS   : %s\n", inet_ntoa( ns_entries[1] ));
  printf("   Primary NBNS   : %s\n", inet_ntoa( nbns_entries[0] ));
  printf("   Secondary NBNS : %s\n", inet_ntoa( nbns_entries[1] ));

  return(1);
}
#endif /* MSEXT */

extern int ShowIfilter(), ShowOfilter(), ShowDfilter(), ShowAfilter();

struct cmdtab const ShowCommands[] = {
  { "afilter",  NULL,     ShowAfilter,		LOCAL_AUTH,
	"Show keep Alive filters", StrOption},
  { "auth",     NULL,     ShowAuthKey,		LOCAL_AUTH,
	"Show auth name/key", StrNull},
  { "ccp",      NULL,     ReportCcpStatus,	LOCAL_AUTH,
	"Show CCP status", StrNull},
  { "compress", NULL,     ReportCompress,	LOCAL_AUTH,
	"Show compression statictics", StrNull},
  { "debug",	NULL,	  ShowDebugLevel,	LOCAL_AUTH,
	"Show current debug level", StrNull},
  { "dfilter",  NULL,     ShowDfilter,		LOCAL_AUTH,
	"Show Demand filters", StrOption},
  { "escape",   NULL,     ShowEscape,		LOCAL_AUTH,
	"Show escape characters", StrNull},
  { "hdlc",	NULL,	  ReportHdlcStatus,	LOCAL_AUTH,
	"Show HDLC error summary", StrNull},
  { "ifilter",  NULL,     ShowIfilter,		LOCAL_AUTH,
	"Show Input filters", StrOption},
  { "ipcp",     NULL,     ReportIpcpStatus,	LOCAL_AUTH,
	"Show IPCP status", StrNull},
  { "lcp",      NULL,     ReportLcpStatus,	LOCAL_AUTH,
	"Show LCP status", StrNull},
  { "log",      NULL,     ShowLogList,		LOCAL_AUTH,
	"Show log records", StrNull},
  { "mem",      NULL,     ShowMemMap,		LOCAL_AUTH,
	"Show memory map", StrNull},
  { "modem",    NULL,     ShowModemStatus,	LOCAL_AUTH,
	"Show modem setups", StrNull},
  { "ofilter",  NULL,     ShowOfilter,		LOCAL_AUTH,
	"Show Output filters", StrOption},
  { "proto",    NULL,     ReportProtStatus,	LOCAL_AUTH,
	"Show protocol summary", StrNull},
  { "reconnect",NULL,	  ShowReconnect,	LOCAL_AUTH,
	"Show Reconnect timer,tries", StrNull},
  { "redial",   NULL,	  ShowRedial,		LOCAL_AUTH,
	"Show Redial timeout value", StrNull},
  { "route",    NULL,     ShowRoute,		LOCAL_AUTH,
	"Show routing table", StrNull},
  { "timeout",  NULL,	  ShowTimeout,		LOCAL_AUTH,
	"Show Idle timeout value", StrNull},
#ifdef MSEXT
  { "msext", 	NULL,	  ShowMSExt,		LOCAL_AUTH,
	"Show MS PPP extentions", StrNull},
#endif /* MSEXT */
  { "version",  NULL,	  ShowVersion,		LOCAL_NO_AUTH | LOCAL_AUTH,
	"Show version string", StrNull},
  { "help",     "?",      HelpCommand,		LOCAL_NO_AUTH | LOCAL_AUTH,
	"Display this message", StrNull, (void *)ShowCommands},
  { NULL,       NULL,     NULL },
};

struct cmdtab *
FindCommand(cmds, str, pmatch)
struct cmdtab *cmds;
char *str;
int *pmatch;
{
  int nmatch = 0;
  int len = strlen(str);
  struct cmdtab *found = NULL;

  while (cmds->func) {
    if (cmds->name && strncasecmp(str, cmds->name, len) == 0) {
      nmatch++;
      found = cmds;
    } else if (cmds->alias && strncasecmp(str, cmds->alias, len) == 0) {
      nmatch++;
      found = cmds;
    }
    cmds++;
  }
  *pmatch = nmatch;
  return(found);
}

int
FindExec(cmdlist, argc, argv)
struct cmdtab *cmdlist;
int argc;
char **argv;
{
  struct cmdtab *cmd;
  int val = 1;
  int nmatch;

  cmd = FindCommand(cmdlist, *argv, &nmatch);
  if (nmatch > 1)
    printf("Ambiguous.\n");
  else if (cmd && ( cmd->lauth & VarLocalAuth ) )
    val = (cmd->func)(cmd, --argc, ++argv, cmd->args);
  else
    printf("what?\n");
  return(val);
}

int aft_cmd = 1;

void
Prompt()
{
  char *pconnect, *pauth;

  if (!(mode & MODE_INTER))
    return;

  if (!aft_cmd)
    printf("\n");
  else
    aft_cmd = 0;

  if ( VarLocalAuth == LOCAL_AUTH )
    pauth = " ON ";
  else
    pauth = " on ";
  if (IpcpFsm.state == ST_OPENED && phase == PHASE_NETWORK)
    pconnect = "PPP";
  else
    pconnect = "ppp";
  printf("%s%s%s> ", pconnect, pauth, VarShortHost);
  fflush(stdout);
}

void
DecodeCommand(buff, nb, prompt)
char *buff;
int nb;
int prompt;
{
  char *vector[20];
  char **argv;
  int argc, val;
  char *cp;

  val = 1;
  if (nb > 0) {
    cp = buff + strcspn(buff, "\r\n");
    if (cp)
      *cp = '\0';
    {
      argc = MakeArgs(buff, vector, VECSIZE(vector));
      argv = vector;

      if (argc > 0)
        val = FindExec(Commands, argc, argv);
    }
  }
  if (val && prompt)
    Prompt();
}

static int
ShowCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int val = 1;

  if (argc > 0)
    val = FindExec(ShowCommands, argc, argv);
  else
    printf("Use ``show ?'' to get a list.\n");
  return(val);
}

static int
TerminalCommand()
{
  if (LcpFsm.state > ST_CLOSED) {
    printf("LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return(1);
  }
  if (!IsInteractive())
    return(1);
  modem = OpenModem(mode);
  if (modem < 0) {
    printf("failed to open modem.\n");
    return(1);
  }
  printf("Enter to terminal mode.\n");
  printf("Type `~?' for help.\n");
  TtyTermMode();
  return(0);
}

static int
QuitCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (mode & (MODE_DIRECT|MODE_DEDICATED|MODE_AUTO)) {
    if (argc > 0 && (VarLocalAuth & LOCAL_AUTH)) {
      Cleanup(EX_NORMAL);
      mode &= ~MODE_INTER;
    } else {
      LogPrintf(LOG_PHASE_BIT, "client connection closed.\n");
      VarLocalAuth = LOCAL_NO_AUTH;
      close(netfd);
      close(1);
      dup2(2, 1);     /* Have to have something here or the modem will be 1 */
      netfd = -1;
      mode &= ~MODE_INTER;
    }
  } else
    Cleanup(EX_NORMAL);
  return(1);
}

static int
CloseCommand()
{
  reconnect(RECON_FALSE);
  LcpClose();
  if (mode & MODE_BACKGROUND)
      Cleanup(EX_NORMAL);
  return(1);
}

static int
DownCommand()
{
  LcpDown();
  return(1);
}

static int
SetModemSpeed(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int speed;

  if (argc > 0) {
    if (strcmp(*argv, "sync") == 0) {
      VarSpeed = 0;
      return(1);
    }
    speed = atoi(*argv);
    if (IntToSpeed(speed) != B0) {
      VarSpeed = speed;
      return(1);
    }
    printf("invalid speed.\n");
  }
  return(1);
}

static int
SetReconnect(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc == 2) {
    VarReconnectTimer = atoi(argv[0]);
    VarReconnectTries = atoi(argv[1]);
  } else
    printf("Usage: %s %s\n", list->name, list->syntax);
  return(1);
}

static int
SetRedialTimeout(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int timeout;
  int tries;
  char *dot;

  if (argc == 1 || argc == 2 ) {
    if (strncasecmp(argv[0], "random", 6) == 0 &&
	(argv[0][6] == '\0' || argv[0][6] == '.')) {
      VarRedialTimeout = -1;
      printf("Using random redial timeout.\n");
      if (!randinit) {
	randinit = 1;
	if (srandomdev() < 0)
	  srandom((unsigned long)(time(NULL) ^ getpid()));
      }
    }
    else {
      timeout = atoi(argv[0]);

      if (timeout >= 0) {
	VarRedialTimeout = timeout;
      }
      else {
	printf("invalid redial timeout\n");
	printf("Usage: %s %s\n", list->name, list->syntax);
      }
    }

    dot = index(argv[0],'.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
        VarRedialNextTimeout = -1;
        printf("Using random next redial timeout.\n");
        if (!randinit) {
          randinit = 1;
          if (srandomdev() < 0)
            srandom((unsigned long)(time(NULL) ^ getpid()));
        }
      }
      else {
        timeout = atoi(dot);
        if (timeout >= 0) {
          VarRedialNextTimeout = timeout;
        }
        else {
          printf("invalid next redial timeout\n");
          printf("Usage: %s %s\n", list->name, list->syntax);
        }
      }
    }
    else
      VarRedialNextTimeout = NEXT_REDIAL_PERIOD;   /* Default next timeout */

    if (argc == 2) {
      tries = atoi(argv[1]);

      if (tries >= 0) {
	  VarDialTries = tries;
      }
      else {
	printf("invalid retry value\n");
	printf("Usage: %s %s\n", list->name, list->syntax);
      }
    }
  }
  else {
    printf("Usage: %s %s\n", list->name, list->syntax);
  }
  return(1);
}

static int
SetModemParity(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int parity;

  if (argc > 0) {
    parity = ChangeParity(*argv);
    if (parity < 0)
      printf("Invalid parity.\n");
    else
      VarParity = parity;
  }
  return(1);
}

static int
SetDebugLevel(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int level, w;

  for (level = 0; argc-- > 0; argv++) {
    if (isdigit(**argv)) {
      w = atoi(*argv);
      if (w < 0 || w >= MAXLOGLEVEL) {
	printf("invalid log level.\n");
	break;
      } else
	level |= (1 << w);
    } else {
      for (w = 0; w < MAXLOGLEVEL; w++) {
	if (strcasecmp(*argv, LogLevelName[w]) == 0) {
	  level |= (1 << w);
	  continue;
	}
      }
    }
  }
  loglevel = level;
  return(1);
}

static int
SetEscape(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int code;

  for (code = 0; code < 33; code++)
    EscMap[code] = 0;
  while (argc-- > 0) {
    sscanf(*argv++, "%x", &code);
    code &= 0xff;
    EscMap[code >> 3] |= (1 << (code&7));
    EscMap[32] = 1;
  }
  return(1);
}

static int
SetInitialMRU(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int mru;

  if (argc > 0) {
    mru = atoi(*argv);
    if (mru < 100)
      printf("given value is too small.\n");
    else if (mru > MAX_MRU)
      printf("given value is too big.\n");
    else
      VarMRU = mru;
  }
  return(1);
}

static int
SetIdleTimeout(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc-- > 0) {
    VarIdleTimeout = atoi(*argv++);
    if (argc-- > 0) {
      VarLqrTimeout = atoi(*argv++);
      if (VarLqrTimeout < 1)
	VarLqrTimeout = 30;
      if (argc > 0) {
	VarRetryTimeout = atoi(*argv);
	if (VarRetryTimeout < 1 || VarRetryTimeout > 10)
	  VarRetryTimeout = 3;
      }
    }
  }
  return(1);
}

struct in_addr
GetIpAddr(cp)
char *cp;
{
  struct hostent *hp;
  struct in_addr ipaddr;

  hp = gethostbyname(cp);
  if (hp && hp->h_addrtype == AF_INET)
    bcopy(hp->h_addr, &ipaddr, hp->h_length);
  else if (inet_aton(cp, &ipaddr) == 0)
    ipaddr.s_addr = 0;
  return(ipaddr);
}

static int
SetInterfaceAddr(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{

  DefMyAddress.ipaddr.s_addr = DefHisAddress.ipaddr.s_addr = 0L;
  if (argc > 4) {
     printf("set ifaddr: too many arguments (%d > 4)\n", argc);
     return(0);
  }
  if (argc > 0) {
    if (ParseAddr(argc, argv++,
            &DefMyAddress.ipaddr,
	    &DefMyAddress.mask,
	    &DefMyAddress.width) == 0)
       return(0);
    if (--argc > 0) {
      if (ParseAddr(argc, argv++,
		    &DefHisAddress.ipaddr,
		    &DefHisAddress.mask,
		    &DefHisAddress.width) == 0)
	 return(0);
      if (--argc > 0) {
        ifnetmask = GetIpAddr(*argv);
    	if (--argc > 0) {
	   if (ParseAddr(argc, argv++,
			 &DefTriggerAddress.ipaddr,
			 &DefTriggerAddress.mask,
			 &DefTriggerAddress.width) == 0)
	      return(0);
	}
      }
    }
  }
  /*
   * For backwards compatibility, 0.0.0.0 means any address.
   */
  if (DefMyAddress.ipaddr.s_addr == 0) {
    DefMyAddress.mask.s_addr = 0;
    DefMyAddress.width = 0;
  }
  if (DefHisAddress.ipaddr.s_addr == 0) {
    DefHisAddress.mask.s_addr = 0;
    DefHisAddress.width = 0;
  }

  if ((mode & MODE_AUTO) ||
	((mode & MODE_DEDICATED) && dstsystem)) {
    if (OsSetIpaddress(DefMyAddress.ipaddr, DefHisAddress.ipaddr, ifnetmask) < 0)
       return(0);
  }
  return(1);
}

#ifdef MSEXT

void
SetMSEXT(pri_addr, sec_addr, argc, argv)
struct in_addr *pri_addr;
struct in_addr *sec_addr;
int argc;
char **argv;
{
  int dummyint;
  struct in_addr dummyaddr;

  pri_addr->s_addr = sec_addr->s_addr = 0L;

  if( argc > 0 ) {
    ParseAddr(argc, argv++, pri_addr, &dummyaddr, &dummyint);
    if( --argc > 0 ) 
      ParseAddr(argc, argv++, sec_addr, &dummyaddr, &dummyint);
    else
      sec_addr->s_addr = pri_addr->s_addr;
  }

 /*
  * if the primary/secondary ns entries are 0.0.0.0 we should 
  * set them to either the localhost's ip, or the values in
  * /etc/resolv.conf ??
  *
  * up to you if you want to implement this...
  */

}

static int
SetNS(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  SetMSEXT(&ns_entries[0], &ns_entries[1], argc, argv);
  return(1);
}

static int
SetNBNS(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  SetMSEXT(&nbns_entries[0], &nbns_entries[1], argc, argv);
  return(1);
}

#endif /* MS_EXT */

#define	VAR_AUTHKEY	0
#define	VAR_DIAL	1
#define	VAR_LOGIN	2
#define	VAR_AUTHNAME	3
#define	VAR_DEVICE	4
#define	VAR_ACCMAP	5
#define	VAR_PHONE	6

static int
SetVariable(list, argc, argv, param)
struct cmdtab *list;
int argc;
char **argv;
int param;
{
  u_long map;

  if (argc > 0) {
    switch (param) {
    case VAR_AUTHKEY:
      strncpy(VarAuthKey, *argv, sizeof(VarAuthKey)-1);
      VarAuthKey[sizeof(VarAuthKey)-1] = '\0';
      break;
    case VAR_AUTHNAME:
      strncpy(VarAuthName, *argv, sizeof(VarAuthName)-1);
      VarAuthName[sizeof(VarAuthName)-1] = '\0';
      break;
    case VAR_DIAL:
      strncpy(VarDialScript, *argv, sizeof(VarDialScript)-1);
      VarDialScript[sizeof(VarDialScript)-1] = '\0';
      break;
    case VAR_LOGIN:
      strncpy(VarLoginScript, *argv, sizeof(VarLoginScript)-1);
      VarLoginScript[sizeof(VarLoginScript)-1] = '\0';
      break;
    case VAR_DEVICE:
      strncpy(VarDevice, *argv, sizeof(VarDevice)-1);
      VarDevice[sizeof(VarDevice)-1] = '\0';
      VarBaseDevice = rindex(VarDevice, '/');
      VarBaseDevice = VarBaseDevice ? VarBaseDevice + 1 : "";
      break;
    case VAR_ACCMAP:
      sscanf(*argv, "%lx", &map);
      VarAccmap = map;
      break;
    case VAR_PHONE:
      strncpy(VarPhoneList, *argv, sizeof(VarPhoneList)-1);
      VarPhoneList[sizeof(VarPhoneList)-1] = '\0';
      strcpy(VarPhoneCopy, VarPhoneList);
      VarNextPhone = VarPhoneCopy;
      break;
    }
  }
  return(1);
}

static int SetCtsRts(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0) {
    if (strcmp(*argv, "on") == 0)
      VarCtsRts = TRUE;
    else if (strcmp(*argv, "off") == 0)
      VarCtsRts = FALSE;
    else
      printf("usage: set ctsrts [on|off].\n");
  }
  return(1);
}


static int SetOpenMode(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0) {
    if (strcmp(*argv, "active") == 0)
      VarOpenMode = OPEN_ACTIVE;
    else if (strcmp(*argv, "passive") == 0)
      VarOpenMode = OPEN_PASSIVE;
    else
      printf("Invalid mode.\n");
  }
  return(1);
}
static char StrChatStr[] = "chat-script";
static char StrValue[] = "value";

extern int SetIfilter(), SetOfilter(), SetDfilter(), SetAfilter();

struct cmdtab const SetCommands[] = {
  { "accmap",   NULL,	  SetVariable,		LOCAL_AUTH,
	"Set accmap value", "hex-value", (void *)VAR_ACCMAP},
  { "afilter",  NULL,     SetAfilter, 		LOCAL_AUTH,
	"Set keep Alive filter", "..."},
  { "authkey",  "key",     SetVariable,		LOCAL_AUTH,
	"Set authentication key", "key", (void *)VAR_AUTHKEY},
  { "authname", NULL,     SetVariable,		LOCAL_AUTH,
	"Set authentication name", "name", (void *)VAR_AUTHNAME},
  { "ctsrts", NULL,	  SetCtsRts,		LOCAL_AUTH,
	"Use CTS/RTS modem signalling", "[on|off]"},
  { "debug",    NULL,	  SetDebugLevel,	LOCAL_AUTH,
	"Set debug level", StrValue},
  { "device",     "line", SetVariable, 		LOCAL_AUTH,
	"Set modem device name", "device-name",	(void *)VAR_DEVICE},
  { "dfilter",  NULL,     SetDfilter,		 LOCAL_AUTH,
	"Set demand filter", "..."},
  { "dial",     NULL,     SetVariable, 		LOCAL_AUTH,
	"Set dialing script", StrChatStr, (void *)VAR_DIAL},
  { "escape",   NULL,	  SetEscape, 		LOCAL_AUTH,
	"Set escape characters", "hex-digit ..."},
  { "ifaddr",   NULL,   SetInterfaceAddr,	LOCAL_AUTH,
	"Set destination address", "[src-addr [dst-addr [netmask [trg-addr]]]]"},
  { "ifilter",  NULL,     SetIfilter, 		LOCAL_AUTH,
	"Set input filter", "..."},
  { "login",    NULL,     SetVariable,		LOCAL_AUTH,
	"Set login script", StrChatStr,	(void *)VAR_LOGIN },
  { "mru",      "mtu",    SetInitialMRU,	LOCAL_AUTH,
	"Set Initial MRU value", StrValue },
  { "ofilter",  NULL,	  SetOfilter,		LOCAL_AUTH,
	"Set output filter", "..." },
  { "openmode", NULL,	  SetOpenMode,		LOCAL_AUTH,
	"Set open mode", "[active|passive]"},
  { "parity",   NULL,     SetModemParity,	LOCAL_AUTH,
	"Set modem parity", "[odd|even|none]"},
  { "phone",    NULL,     SetVariable,		LOCAL_AUTH,
	"Set telephone number(s)", "phone1[:phone2[...]]", (void *)VAR_PHONE },
  { "reconnect",NULL,     SetReconnect,		LOCAL_AUTH,
	"Set Reconnect timeout", "value ntries"},
  { "redial",   NULL,     SetRedialTimeout,	LOCAL_AUTH,
	"Set Redial timeout", "value|random[.value|random] [dial_attempts]"},
  { "speed",    NULL,     SetModemSpeed,	LOCAL_AUTH,
	"Set modem speed", "speed"},
  { "timeout",  NULL,     SetIdleTimeout,	LOCAL_AUTH,
	"Set Idle timeout", StrValue},
#ifdef MSEXT
  { "ns",	NULL,	  SetNS,		LOCAL_AUTH,
	"Set NameServer", "pri-addr [sec-addr]"},
  { "nbns",	NULL,	  SetNBNS,		LOCAL_AUTH,
	"Set NetBIOS NameServer", "pri-addr [sec-addr]"},
#endif /* MSEXT */
  { "help",     "?",      HelpCommand,		LOCAL_AUTH | LOCAL_NO_AUTH,
	"Display this message", StrNull, (void *)SetCommands},
  { NULL,       NULL,     NULL },
};

static int
SetCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int val = 1;

  if (argc > 0)
    val = FindExec(SetCommands, argc, argv);
  else
    printf("Use `set ?' to get a list or `set ? <var>' for syntax help.\n");
  return(val);
}


static int
AddCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct in_addr dest, gateway, netmask;

  if (argc == 3) {
    dest = GetIpAddr(argv[0]);
    netmask = GetIpAddr(argv[1]);
    if (strcasecmp(argv[2], "HISADDR") == 0)
      gateway = IpcpInfo.his_ipaddr;
    else
      gateway = GetIpAddr(argv[2]);
    OsSetRoute(RTM_ADD, dest, gateway, netmask);
  } else {
    printf("Usage: %s %s\n", list->name, list->syntax);
  }
  return(1);
}

static int
DeleteCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct in_addr dest, gateway, netmask;

  if (argc >= 2) {
    dest = GetIpAddr(argv[0]);
    if (strcasecmp(argv[1], "HISADDR") == 0)
      gateway = IpcpInfo.his_ipaddr;
    else
      gateway = GetIpAddr(argv[1]);
    netmask.s_addr = 0;
    if (argc == 3) {
      if (inet_aton(argv[1], &netmask) == 0) {
	printf("bad netmask value.\n");
	return(1);
      }
    }
    OsSetRoute(RTM_DELETE, dest, gateway, netmask);
  } else if (argc == 1 && strcasecmp(argv[0], "ALL") == 0) {
    DeleteIfRoutes(0);
  } else {
    printf("Usage: %s %s\n", list->name, list->syntax);
  }
  return(1);
}


static int AliasEnable();
static int AliasOption();


static struct cmdtab const AliasCommands[] =
{
  { "enable",   NULL,     AliasEnable,          LOCAL_AUTH,
        "enable IP aliasing", "[yes|no]"},
  { "port",   NULL,     AliasRedirectPort,          LOCAL_AUTH,
        "port redirection", "[proto  addr_local:port_local  port_alias]"},
  { "addr",   NULL,     AliasRedirectAddr,          LOCAL_AUTH,
        "static address translation", "[addr_local  addr_alias]"},
  { "deny_incoming",  NULL,    AliasOption,     LOCAL_AUTH,
        "stop incoming connections",   "[yes|no]",
        (void*)PKT_ALIAS_DENY_INCOMING},
  { "log",  NULL,     AliasOption,              LOCAL_AUTH,
        "log aliasing link creation",           "[yes|no]",
        (void*)PKT_ALIAS_LOG},
  { "same_ports", NULL,     AliasOption,        LOCAL_AUTH,
        "try to leave port numbers unchanged", "[yes|no]",
        (void*)PKT_ALIAS_SAME_PORTS},
  { "use_sockets", NULL,     AliasOption,       LOCAL_AUTH,
        "allocate host sockets", "[yes|no]",
        (void*)PKT_ALIAS_USE_SOCKETS },
  { "unregistered_only", NULL,     AliasOption, LOCAL_AUTH,
        "alias unregistered (private) IP address space only", "[yes|no]",
        (void*)PKT_ALIAS_UNREGISTERED_ONLY},
  { "help",     "?",      HelpCommand,          LOCAL_AUTH | LOCAL_NO_AUTH,
        "Display this message", StrNull,
        (void *)AliasCommands},
  { NULL,       NULL,     NULL },
};


static int
AliasCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int val = 1;

  if (argc > 0)
    val = FindExec(AliasCommands, argc, argv);
  else
    printf("Use `alias help' to get a list or `alias help <option>' for syntax h
elp.\n");
  return(val);
}


static int
AliasEnable(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
    if (argc == 1 && strcmp(argv[0], "yes") == 0) {
        if (!(mode & MODE_ALIAS))
            if (loadAliasHandlers(&VarAliasHandlers) == 0)
                mode |= MODE_ALIAS;
            else
                printf("Cannot load alias library\n");
    } else if (argc == 1 && strcmp(argv[0], "no") == 0) {
        if (mode & MODE_ALIAS) {
            unloadAliasHandlers();
            mode &= ~MODE_ALIAS;
        }
    } else {
        printf("Usage: alias %s %s\n", list->name, list->syntax);
    }
    return(1);
}


static int
AliasOption(list, argc, argv, param)
struct cmdtab *list;
int argc;
char **argv;
void* param;
{
   if (argc == 1 && strcmp(argv[0], "yes") == 0) {
      if (mode & MODE_ALIAS)
         VarSetPacketAliasMode((unsigned)param, (unsigned)param);
      else
         printf("alias not enabled\n");
   } else if (argc == 1 && strcmp(argv[0], "no") == 0) {
      if (mode & MODE_ALIAS)
         VarSetPacketAliasMode(0, (unsigned)param);
      else
         printf("alias not enabled\n");
   } else {
      printf("Usage: alias %s %s\n", list->name, list->syntax);
   }
   return(1);
}
