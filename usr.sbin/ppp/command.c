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
 * $Id: command.c,v 1.65 1997/06/30 03:03:29 brian Exp $
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
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
#include <fcntl.h>
#include <errno.h>
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
#include "timeout.h"
#include "server.h"

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

  if (!VarTerm)
    return 0;

  if (argc > 0) {
    for (cmd = plist; cmd->name; cmd++)
      if (strcasecmp(cmd->name, *argv) == 0 && (cmd->lauth & VarLocalAuth)) {
        fprintf(VarTerm, "%s\n", cmd->syntax);
        return 0;
      }

    return -1;
  }

  n = 0;
  for (cmd = plist; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      fprintf(VarTerm, "  %-9s: %-20s\n", cmd->name, cmd->helpmes);
      n++;
    }

  if (n & 1)
    fprintf(VarTerm, "\n");

  return 0;
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
    if (VarTerm)
      fprintf(VarTerm, "%s\n", mes);
    return 0;
  }
  return 1;
}

static int
DialCommand(cmdlist, argc, argv)
struct cmdtab *cmdlist;
int argc;
char **argv;
{
  int tries;
  int res;

  if (LcpFsm.state > ST_CLOSED) {
    if (VarTerm)
      fprintf(VarTerm, "LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return 0;
  }

  if (!IsInteractive())
    return(1);

  if (argc > 0) {
    if (SelectSystem(*argv, CONFFILE) < 0) {
      if (VarTerm)
        fprintf(VarTerm, "%s: not found.\n", *argv);
      return -1;
    }
  }

  tries = 0;
  do {
    if (VarTerm)
      fprintf(VarTerm, "Dial attempt %u of %d\n", ++tries, VarDialTries);
    modem = OpenModem(mode);
    if (modem < 0) {
      if (VarTerm)
        fprintf(VarTerm, "Failed to open modem.\n");
      break;
    }
    if ((res = DialModem()) == EX_DONE) {
      sleep(1);
      ModemTimeout();
      PacketMode();
      break;
    } else if (res == EX_SIG)
      return 1;
  } while (VarDialTries == 0 || tries < VarDialTries);

  return 0;
}

static int
ShellCommand(cmdlist, argc, argv)
struct cmdtab *cmdlist;
int argc;
char **argv;
{
  const char *shell;
  pid_t shpid;
  FILE *oVarTerm;

#ifdef SHELL_ONLY_INTERACTIVELY
  /* we're only allowed to shell when we run ppp interactively */
  if (mode != MODE_INTER) {
    LogPrintf(LogWARN, "Can only start a shell in interactive mode\n");
    return 1;
  }
#endif
#ifdef NO_SHELL_IN_AUTO_INTERACTIVE
  /*
   * we want to stop shell commands when we've got a telnet connection
   * to an auto mode ppp
   */
  if ((mode & (MODE_AUTO|MODE_INTER)) == (MODE_AUTO|MODE_INTER)) {
    LogPrintf(LogWARN,  "Shell is not allowed interactively in auto mode\n");
    return 1;
  }
#endif

  if(argc == 0 && !(mode & MODE_INTER)) {
    LogPrintf(LogWARN, "Can only start an interactive shell in"
	      " interactive mode\n");
    return 1;
  }

  if((shell = getenv("SHELL")) == 0)
    shell = _PATH_BSHELL;

  if((shpid = fork()) == 0) {
     int dtablesize, i, fd;

     if (VarTerm)
       fd = fileno(VarTerm);
     else if ((fd = open("/dev/null", O_RDWR)) == -1) {
       LogPrintf(LogALERT, "Failed to open /dev/null: %s\n", strerror(errno));
       exit(1);
     }

     for (i = 0; i < 3; i++)
       dup2(fd, i);

     if (fd > 2)
       if (VarTerm) {
	 oVarTerm = VarTerm;
	 VarTerm = 0;
         if (oVarTerm && oVarTerm != stdout)
           fclose(oVarTerm);
       } else
         close(fd);

     for (dtablesize = getdtablesize(), i = 3; i < dtablesize; i++)
	(void)close(i);

     /*
      * We are running setuid, we should change to
      * real user for avoiding security problems.
      */
     if (setgid(getgid()) < 0) {
        LogPrintf(LogERROR, "setgid: %s\n", strerror(errno));
	exit(1);
     }
     if (setuid(getuid()) < 0) {
        LogPrintf(LogERROR, "setuid: %s\n", strerror(errno));
	exit(1);
     }
     TtyOldMode();
     if(argc > 0) {
       /* substitute pseudo args */
       for (i=1; i<argc; i++)
         if (strcasecmp(argv[i], "HISADDR") == 0)
           argv[i] = strdup(inet_ntoa(IpcpInfo.his_ipaddr));
         else if (strcasecmp(argv[i], "INTERFACE") == 0)
           argv[i] = strdup(IfDevName);
         else if (strcasecmp(argv[i], "MYADDR") == 0)
           argv[i] = strdup(inet_ntoa(IpcpInfo.want_ipaddr));
       (void)execvp(argv[0], argv);
     }
     else
       (void)execl(shell, shell, NULL);

     LogPrintf(LogWARN, "exec() of %s failed\n", argc > 0 ? argv[0] : shell);
     exit(255);
  }

  if( shpid == (pid_t)-1 ) {
    LogPrintf(LogERROR, "Fork failed: %s\n", strerror(errno));
  } else {
    int status;
    (void)waitpid(shpid, &status, 0);
  }

  TtyCommandMode(1);

  return(0);
}

struct cmdtab const Commands[] = {
  { "accept",  NULL,    AcceptCommand,	LOCAL_AUTH,
  	"accept option request",	"accept option .."},
  { "add",     NULL,	AddCommand,	LOCAL_AUTH,
	"add route",			"add dest mask gateway"},
  { "close",   NULL,    CloseCommand,	LOCAL_AUTH,
	"Close connection",		"close"},
  { "delete",  NULL,    DeleteCommand,	LOCAL_AUTH,
	"delete route",                 "delete ALL | dest [gateway [mask]]"},
  { "deny",    NULL,    DenyCommand,	LOCAL_AUTH,
  	"Deny option request",		"deny option .."},
  { "dial",    "call",  DialCommand,	LOCAL_AUTH,
  	"Dial and login",		"dial|call [remote]"},
  { "disable", NULL,    DisableCommand,	LOCAL_AUTH,
  	"Disable option",		"disable option .."},
  { "display", NULL,    DisplayCommand,	LOCAL_AUTH,
  	"Display option configs",	"display"},
  { "enable",  NULL,    EnableCommand,	LOCAL_AUTH,
  	"Enable option",		"enable option .."},
  { "passwd",  NULL,	LocalAuthCommand,LOCAL_NO_AUTH,
  	"Password for manipulation", "passwd option .."},
  { "load",    NULL,    LoadCommand,	LOCAL_AUTH,
  	"Load settings",		"load [remote]"},
  { "save",    NULL,    SaveCommand,	LOCAL_AUTH,
  	"Save settings", "save"},
  { "set",     "setup", SetCommand,	LOCAL_AUTH,
  	"Set parameters",  "set[up] var value"},
  { "shell",   "!",     ShellCommand,   LOCAL_AUTH,
	"Run a subshell",  "shell|! [sh command]"},
  { "show",    NULL,    ShowCommand,	LOCAL_AUTH,
  	"Show status and statictics", "show var"},
  { "term",    NULL,    TerminalCommand,LOCAL_AUTH,
  	"Enter to terminal mode", "term"},
  { "alias",   NULL,    AliasCommand,   LOCAL_AUTH,
        "alias control",        "alias option [yes|no]"},
  { "quit",    "bye",   QuitCommand,	LOCAL_AUTH | LOCAL_NO_AUTH,
	"Quit PPP program", "quit|bye [all]"},
  { "help",    "?",     HelpCommand,	LOCAL_AUTH | LOCAL_NO_AUTH,
	"Display this message", "help|? [command]", (void *)Commands },
  { NULL,      "down",  DownCommand,	LOCAL_AUTH,
  	"Generate down event",		"down"},
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

static int ShowLogLevel()
{
  int i;

  if (!VarTerm)
    return 0;
  fprintf(VarTerm, "Log:");
  for (i = LogMIN; i < LogMAXCONF; i++) {
    if (LogIsKept(i))
      fprintf(VarTerm, " %s", LogName(i));
  }
  fprintf(VarTerm, "\n");

  return 0;
}

static int ShowEscape()
{
  int code, bit;

  if (!VarTerm)
    return 0;
  if (EscMap[32]) {
    for (code = 0; code < 32; code++)
      if (EscMap[code])
        for (bit = 0; bit < 8; bit++)
          if (EscMap[code] & (1<<bit))
            fprintf(VarTerm, " 0x%02x", (code << 3) + bit);
    fprintf(VarTerm, "\n");
  }
  return 1;
}

static int ShowTimeout()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " Idle Timer: %d secs   LQR Timer: %d secs"
          "   Retry Timer: %d secs\n", VarIdleTimeout, VarLqrTimeout,
          VarRetryTimeout);
  return 1;
}

static int ShowAuthKey()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, "AuthName = %s\n", VarAuthName);
  fprintf(VarTerm, "AuthKey  = %s\n", VarAuthKey);
  return 1;
}

static int ShowVersion()
{
  extern char VarVersion[];
  extern char VarLocalVersion[];

  if (!VarTerm)
    return 0;
  fprintf(VarTerm, "%s - %s \n", VarVersion, VarLocalVersion);
  return 1;
}

static int ShowInitialMRU()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " Initial MRU: %ld\n", VarMRU);
  return 1;
}

static int ShowPreferredMTU()
{
  if (!VarTerm)
    return 0;
  if (VarPrefMTU)
    fprintf(VarTerm, " Preferred MTU: %ld\n", VarPrefMTU);
  else
    fprintf(VarTerm, " Preferred MTU: unspecified\n");
  return 1;
}

static int ShowReconnect()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " Reconnect Timer:  %d,  %d tries\n",
         VarReconnectTimer, VarReconnectTries);
  return 1;
}

static int ShowRedial()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " Redial Timer: ");

  if (VarRedialTimeout >= 0) {
    fprintf(VarTerm, " %d seconds, ", VarRedialTimeout);
  }
  else {
    fprintf(VarTerm, " Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  fprintf(VarTerm, " Redial Next Timer: ");

  if (VarRedialNextTimeout >= 0) {
    fprintf(VarTerm, " %d seconds, ", VarRedialNextTimeout);
  }
  else {
    fprintf(VarTerm, " Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  if (VarDialTries)
      fprintf(VarTerm, "%d dial tries", VarDialTries);

  fprintf(VarTerm, "\n");

  return 1;
}

#ifndef NOMSEXT
static int ShowMSExt()
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " MS PPP extention values \n" );
  fprintf(VarTerm, "   Primary NS     : %s\n", inet_ntoa( ns_entries[0] ));
  fprintf(VarTerm, "   Secondary NS   : %s\n", inet_ntoa( ns_entries[1] ));
  fprintf(VarTerm, "   Primary NBNS   : %s\n", inet_ntoa( nbns_entries[0] ));
  fprintf(VarTerm, "   Secondary NBNS : %s\n", inet_ntoa( nbns_entries[1] ));
  return 1;
}
#endif

extern int ShowIfilter(), ShowOfilter(), ShowDfilter(), ShowAfilter();

struct cmdtab const ShowCommands[] = {
  { "afilter",  NULL,     ShowAfilter,		LOCAL_AUTH,
	"Show keep Alive filters", "show afilter option .."},
  { "auth",     NULL,     ShowAuthKey,		LOCAL_AUTH,
	"Show auth name/key", "show auth"},
  { "ccp",      NULL,     ReportCcpStatus,	LOCAL_AUTH,
	"Show CCP status", "show cpp"},
  { "compress", NULL,     ReportCompress,	LOCAL_AUTH,
	"Show compression statictics", "show compress"},
  { "dfilter",  NULL,     ShowDfilter,		LOCAL_AUTH,
	"Show Demand filters", "show dfilteroption .."},
  { "escape",   NULL,     ShowEscape,		LOCAL_AUTH,
	"Show escape characters", "show escape"},
  { "hdlc",	NULL,	  ReportHdlcStatus,	LOCAL_AUTH,
	"Show HDLC error summary", "show hdlc"},
  { "ifilter",  NULL,     ShowIfilter,		LOCAL_AUTH,
	"Show Input filters", "show ifilter option .."},
  { "ipcp",     NULL,     ReportIpcpStatus,	LOCAL_AUTH,
	"Show IPCP status", "show ipcp"},
  { "lcp",      NULL,     ReportLcpStatus,	LOCAL_AUTH,
	"Show LCP status", "show lcp"},
  { "log",	NULL,	  ShowLogLevel,	LOCAL_AUTH,
	"Show current log level", "show log"},
  { "mem",      NULL,     ShowMemMap,		LOCAL_AUTH,
	"Show memory map", "show mem"},
  { "modem",    NULL,     ShowModemStatus,	LOCAL_AUTH,
	"Show modem setups", "show modem"},
  { "mru",      NULL,     ShowInitialMRU,	LOCAL_AUTH,
	"Show Initial MRU", "show mru"},
  { "mtu",      NULL,     ShowPreferredMTU,	LOCAL_AUTH,
	"Show Preferred MTU", "show mtu"},
  { "ofilter",  NULL,     ShowOfilter,		LOCAL_AUTH,
	"Show Output filters", "show ofilter option .."},
  { "proto",    NULL,     ReportProtStatus,	LOCAL_AUTH,
	"Show protocol summary", "show proto"},
  { "reconnect",NULL,	  ShowReconnect,	LOCAL_AUTH,
	"Show Reconnect timer,tries", "show reconnect"},
  { "redial",   NULL,	  ShowRedial,		LOCAL_AUTH,
	"Show Redial timeout value", "show redial"},
  { "route",    NULL,     ShowRoute,		LOCAL_AUTH,
	"Show routing table", "show route"},
  { "timeout",  NULL,	  ShowTimeout,		LOCAL_AUTH,
	"Show Idle timeout value", "show timeout"},
#ifndef NOMSEXT
  { "msext", 	NULL,	  ShowMSExt,		LOCAL_AUTH,
	"Show MS PPP extentions", "show msext"},
#endif
  { "version",  NULL,	  ShowVersion,		LOCAL_NO_AUTH | LOCAL_AUTH,
	"Show version string", "show version"},
  { "help",     "?",      HelpCommand,		LOCAL_NO_AUTH | LOCAL_AUTH,
	"Display this message", "show help|? [command]", (void *)ShowCommands},
  { NULL,       NULL,     NULL },
};

struct cmdtab *
FindCommand(cmds, str, pmatch)
struct cmdtab *cmds;
char *str;
int *pmatch;
{
  int nmatch;
  int len;
  struct cmdtab *found;

  found = NULL;
  len = strlen(str);
  nmatch = 0;
  while (cmds->func) {
    if (cmds->name && strncasecmp(str, cmds->name, len) == 0) {
      if (cmds->name[len] == '\0') {
        *pmatch = 1;
        return cmds;
      }
      nmatch++;
      found = cmds;
    } else if(cmds->alias && strncasecmp(str, cmds->alias, len) == 0) {
      if (cmds->alias[len] == '\0') {
        *pmatch = 1;
        return cmds;
      }
      nmatch++;
      found = cmds;
    }
    cmds++;
  }
  *pmatch = nmatch;
  return found;
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
    LogPrintf(LogWARN, "%s: Ambiguous command\n", *argv);
  else if (cmd && ( cmd->lauth & VarLocalAuth ) )
    val = (cmd->func)(cmd, --argc, ++argv, cmd->args);
  else
    LogPrintf(LogWARN, "%s: Invalid command\n", *argv);

  if (val == -1)
    LogPrintf(LogWARN, "Usage: %s\n", cmd->syntax);
  else if(val)
    LogPrintf(LogCOMMAND, "%s: Failed %d\n", *argv, val);

  return val;
}

int aft_cmd = 1;
extern int TermMode;

void
Prompt()
{
  char *pconnect, *pauth;

  if (!(mode & MODE_INTER) || !VarTerm || TermMode)
    return;

  if (!aft_cmd)
    fprintf(VarTerm, "\n");
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
  fprintf(VarTerm, "%s%s%s> ", pconnect, pauth, VarShortHost);
  fflush(VarTerm);
}

void
DecodeCommand(buff, nb, prompt)
char *buff;
int nb;
int prompt;
{
  char *vector[20];
  char **argv;
  int argc;
  char *cp;

  if (nb > 0) {
    cp = buff + strcspn(buff, "\r\n");
    if (cp)
      *cp = '\0';
    argc = MakeArgs(buff, vector, VECSIZE(vector));
    argv = vector;

    if (argc > 0)
      FindExec(Commands, argc, argv);
  }
  if (prompt)
    Prompt();
}

static int
ShowCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0)
    FindExec(ShowCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use ``show ?'' to get a list.\n");
  else
    LogPrintf(LogWARN, "show command must have arguments\n");

  return 0;
}

static int
TerminalCommand()
{
  if (LcpFsm.state > ST_CLOSED) {
    if (VarTerm)
      fprintf(VarTerm, "LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return 1;
  }
  if (!IsInteractive())
    return(1);
  modem = OpenModem(mode);
  if (modem < 0) {
    if (VarTerm)
      fprintf(VarTerm, "Failed to open modem.\n");
    return(1);
  }
  if (VarTerm) {
    fprintf(VarTerm, "Enter to terminal mode.\n");
    fprintf(VarTerm, "Type `~?' for help.\n");
  }
  TtyTermMode();
  return(0);
}

static int
QuitCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  FILE *oVarTerm;

  if (mode & (MODE_DIRECT|MODE_DEDICATED|MODE_AUTO)) {
    if (argc > 0 && (VarLocalAuth & LOCAL_AUTH)) {
      Cleanup(EX_NORMAL);
      mode &= ~MODE_INTER;
      oVarTerm = VarTerm;
      VarTerm = 0;
      if (oVarTerm && oVarTerm != stdout)
        fclose(oVarTerm);
    } else {
      LogPrintf(LogPHASE, "Client connection closed.\n");
      VarLocalAuth = LOCAL_NO_AUTH;
      mode &= ~MODE_INTER;
      oVarTerm = VarTerm;
      VarTerm = 0;
      if (oVarTerm && oVarTerm != stdout)
        fclose(oVarTerm);
      close(netfd);
      netfd = -1;
    }
  } else
    Cleanup(EX_NORMAL);

  return 0;
}

static int
CloseCommand()
{
  reconnect(RECON_FALSE);
  LcpClose();
  if (mode & MODE_BACKGROUND)
      Cleanup(EX_NORMAL);
  return 0;
}

static int
DownCommand()
{
  LcpDown();
  return 0;
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
      return 0;
    }
    speed = atoi(*argv);
    if (IntToSpeed(speed) != B0) {
      VarSpeed = speed;
      return 0;
    }
    LogPrintf(LogWARN, "%s: Invalid speed\n", *argv);
  }
  return -1;
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
    return 0;
  }

  return -1;
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
      if (!randinit) {
	randinit = 1;
	srandomdev();
      }
    } else {
      timeout = atoi(argv[0]);

      if (timeout >= 0)
	VarRedialTimeout = timeout;
      else {
	LogPrintf(LogWARN, "Invalid redial timeout\n");
        return -1;
      }
    }

    dot = index(argv[0],'.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
        VarRedialNextTimeout = -1;
        if (!randinit) {
          randinit = 1;
	  srandomdev();
        }
      }
      else {
        timeout = atoi(dot);
        if (timeout >= 0)
          VarRedialNextTimeout = timeout;
        else {
          LogPrintf(LogWARN, "Invalid next redial timeout\n");
	  return -1;
        }
      }
    }
    else
      VarRedialNextTimeout = NEXT_REDIAL_PERIOD;   /* Default next timeout */

    if (argc == 2) {
      tries = atoi(argv[1]);

      if (tries >= 0) {
	VarDialTries = tries;
      } else {
	LogPrintf(LogWARN, "Invalid retry value\n");
	return 1;
      }
    }
    return 0;
  }

  return -1;
}

static int
SetServer(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int res = -1;

  if (argc > 0 && argc < 3)
    if (strcasecmp(argv[0], "none") == 0) {
      ServerClose();
      LogPrintf(LogPHASE, "Disabling server port.\n");
      res = 0;
    } else if (*argv[0] == '/') {
      mode_t mask;
      umask(mask = umask(0));
      if (argc == 2) {
        unsigned m;
        if (sscanf(argv[1], "%o", &m) == 1)
          mask = m;
      }
      res = ServerLocalOpen(argv[0], mask);
    } else {
      int port;
      if (strspn(argv[0], "0123456789") != strlen(argv[0])) {
        struct servent *s;
        if ((s = getservbyname(argv[0], "tcp")) == NULL) {
          port = 0;
          LogPrintf(LogWARN, "%s: Invalid port or service\n", argv[0]);
        } else
          port = ntohs(s->s_port);
      } else
        port = atoi(argv[0]);
      if (port)
        res = ServerTcpOpen(port);
    }

  return res;
}

static int
SetModemParity(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  return argc > 0 ? ChangeParity(*argv) : -1;
}

static int
SetLogLevel(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int i;
  int res;
  char *arg;

  res = 0;
  if (argc == 0 || (argv[0][0] != '+' && argv[0][0] != '-'))
    LogDiscardAll();
  while (argc--) {
    arg = **argv == '+' || **argv == '-' ? *argv + 1 : *argv;
    for (i = LogMIN; i <= LogMAX; i++)
      if (strcasecmp(arg, LogName(i)) == 0) {
        if (**argv == '-')
          LogDiscard(i);
        else
	  LogKeep(i);
	break;
      }
    if (i > LogMAX) {
      LogPrintf(LogWARN, "%s: Invalid log value\n", arg);
      res = -1;
    }
    argv++;
  }
  return res;
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
  return 0;
}

static int
SetInitialMRU(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  long mru;
  char *err;

  if (argc > 0) {
    mru = atol(*argv);
    if (mru < MIN_MRU)
      err = "Given MRU value (%ld) is too small.\n";
    else if (mru > MAX_MRU)
      err = "Given MRU value (%ld) is too big.\n";
    else {
      VarMRU = mru;
      return 0;
    }
    LogPrintf(LogWARN, err, mru);
  }

  return -1;
}

static int
SetPreferredMTU(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  long mtu;
  char *err;

  if (argc > 0) {
    mtu = atol(*argv);
    if (mtu == 0) {
      VarPrefMTU = 0;
      return 0;
    } else if (mtu < MIN_MTU)
      err = "Given MTU value (%ld) is too small.\n";
    else if (mtu > MAX_MTU)
      err = "Given MTU value (%ld) is too big.\n";
    else {
      VarPrefMTU = mtu;
      return 0;
    }
    LogPrintf(LogWARN, err, mtu);
  }

  return -1;
}

static int
SetIdleTimeout(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc-- > 0) {
    VarIdleTimeout = atoi(*argv++);
    UpdateIdleTimer();  /* If we're connected, restart the idle timer */
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
    return 0;
  }
 
  return -1;
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
  if (argc > 4)
     return -1;

  if (argc > 0) {
    if (ParseAddr(argc, argv++,
            &DefMyAddress.ipaddr,
	    &DefMyAddress.mask,
	    &DefMyAddress.width) == 0)
       return 1;
    if (--argc > 0) {
      if (ParseAddr(argc, argv++,
		    &DefHisAddress.ipaddr,
		    &DefHisAddress.mask,
		    &DefHisAddress.width) == 0)
	 return 2;
      if (--argc > 0) {
        ifnetmask = GetIpAddr(*argv);
    	if (--argc > 0) {
	   if (ParseAddr(argc, argv++,
			 &DefTriggerAddress.ipaddr,
			 &DefTriggerAddress.mask,
			 &DefTriggerAddress.width) == 0)
	      return 3;
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
       return 4;
  }
  return 0;
}

#ifndef NOMSEXT

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
  return 0;
}

static int
SetNBNS(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  SetMSEXT(&nbns_entries[0], &nbns_entries[1], argc, argv);
  return 0;
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
  char *arg;

  if (argc > 0)
    arg = *argv;
  else
    arg = "";

  switch (param) {
    case VAR_AUTHKEY:
      strncpy(VarAuthKey, arg, sizeof(VarAuthKey)-1);
      VarAuthKey[sizeof(VarAuthKey)-1] = '\0';
      break;
    case VAR_AUTHNAME:
      strncpy(VarAuthName, arg, sizeof(VarAuthName)-1);
      VarAuthName[sizeof(VarAuthName)-1] = '\0';
      break;
    case VAR_DIAL:
      strncpy(VarDialScript, arg, sizeof(VarDialScript)-1);
      VarDialScript[sizeof(VarDialScript)-1] = '\0';
      break;
    case VAR_LOGIN:
      strncpy(VarLoginScript, arg, sizeof(VarLoginScript)-1);
      VarLoginScript[sizeof(VarLoginScript)-1] = '\0';
      break;
    case VAR_DEVICE:
      CloseModem();
      strncpy(VarDevice, arg, sizeof(VarDevice)-1);
      VarDevice[sizeof(VarDevice)-1] = '\0';
      VarBaseDevice = rindex(VarDevice, '/');
      VarBaseDevice = VarBaseDevice ? VarBaseDevice + 1 : "";
      break;
    case VAR_ACCMAP:
      sscanf(arg, "%lx", &map);
      VarAccmap = map;
      break;
    case VAR_PHONE:
      strncpy(VarPhoneList, arg, sizeof(VarPhoneList)-1);
      VarPhoneList[sizeof(VarPhoneList)-1] = '\0';
      strcpy(VarPhoneCopy, VarPhoneList);
      VarNextPhone = VarPhoneCopy;
      break;
  }
  return 0;
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
      return -1;
    return 0;
  }
  return -1;
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
      return -1;
    return 0;
  }
  return -1;
}

extern int SetIfilter(), SetOfilter(), SetDfilter(), SetAfilter();

struct cmdtab const SetCommands[] = {
  { "accmap",   NULL,	  SetVariable,		LOCAL_AUTH,
	"Set accmap value", "set accmap hex-value", (void *)VAR_ACCMAP},
  { "afilter",  NULL,     SetAfilter, 		LOCAL_AUTH,
	"Set keep Alive filter", "set afilter ..."},
  { "authkey",  "key",     SetVariable,		LOCAL_AUTH,
	"Set authentication key", "set authkey|key key", (void *)VAR_AUTHKEY},
  { "authname", NULL,     SetVariable,		LOCAL_AUTH,
	"Set authentication name", "set authname name", (void *)VAR_AUTHNAME},
  { "ctsrts", NULL,	  SetCtsRts,		LOCAL_AUTH,
	"Use CTS/RTS modem signalling", "set ctsrts [on|off]"},
  { "device",     "line", SetVariable, 		LOCAL_AUTH,
	"Set modem device name", "set device|line device-name", (void *)VAR_DEVICE},
  { "dfilter",  NULL,     SetDfilter,		 LOCAL_AUTH,
	"Set demand filter", "set dfilter ..."},
  { "dial",     NULL,     SetVariable, 		LOCAL_AUTH,
	"Set dialing script", "set dial chat-script", (void *)VAR_DIAL},
  { "escape",   NULL,	  SetEscape, 		LOCAL_AUTH,
	"Set escape characters", "set escape hex-digit ..."},
  { "ifaddr",   NULL,   SetInterfaceAddr,	LOCAL_AUTH,
	"Set destination address", "set ifaddr [src-addr [dst-addr [netmask [trg-addr]]]]"},
  { "ifilter",  NULL,     SetIfilter, 		LOCAL_AUTH,
	"Set input filter", "set ifilter ..."},
  { "log",    NULL,	  SetLogLevel,	LOCAL_AUTH,
	"Set log level", "set log [+|-]value..."},
  { "login",    NULL,     SetVariable,		LOCAL_AUTH,
	"Set login script", "set login chat-script",	(void *)VAR_LOGIN },
  { "mru",      NULL,     SetInitialMRU,	LOCAL_AUTH,
	"Set Initial MRU value", "set mru value" },
  { "mtu",      NULL,     SetPreferredMTU,	LOCAL_AUTH,
	"Set Preferred MTU value", "set mtu value" },
  { "ofilter",  NULL,	  SetOfilter,		LOCAL_AUTH,
	"Set output filter", "set ofilter ..." },
  { "openmode", NULL,	  SetOpenMode,		LOCAL_AUTH,
	"Set open mode", "set openmode [active|passive]"},
  { "parity",   NULL,     SetModemParity,	LOCAL_AUTH,
	"Set modem parity", "set parity [odd|even|none]"},
  { "phone",    NULL,     SetVariable,		LOCAL_AUTH,
	"Set telephone number(s)", "set phone phone1[:phone2[...]]", (void *)VAR_PHONE },
  { "reconnect",NULL,     SetReconnect,		LOCAL_AUTH,
	"Set Reconnect timeout", "set reconnect value ntries"},
  { "redial",   NULL,     SetRedialTimeout,	LOCAL_AUTH,
	"Set Redial timeout", "set redial value|random[.value|random] [dial_attempts]"},
  { "server",    "socket",     SetServer,	LOCAL_AUTH,
	"Set server port", "set server|socket TcpPort|LocalName|none [mask]"},
  { "speed",    NULL,     SetModemSpeed,	LOCAL_AUTH,
	"Set modem speed", "set speed value"},
  { "timeout",  NULL,     SetIdleTimeout,	LOCAL_AUTH,
	"Set Idle timeout", "set timeout value"},
#ifndef NOMSEXT
  { "ns",	NULL,	  SetNS,		LOCAL_AUTH,
	"Set NameServer", "set ns pri-addr [sec-addr]"},
  { "nbns",	NULL,	  SetNBNS,		LOCAL_AUTH,
	"Set NetBIOS NameServer", "set nbns pri-addr [sec-addr]"},
#endif
  { "help",     "?",      HelpCommand,		LOCAL_AUTH | LOCAL_NO_AUTH,
	"Display this message", "set help|? [command]", (void *)SetCommands},
  { NULL,       NULL,     NULL },
};

static int
SetCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0)
    FindExec(SetCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use `set ?' to get a list or `set ? <var>' for"
	    " syntax help.\n");
  else
    LogPrintf(LogWARN, "set command must have arguments\n");

  return 0;
}


static int
AddCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct in_addr dest, gateway, netmask;

  if (argc == 3) {
    if (strcasecmp(argv[0], "MYADDR") == 0)
      dest = IpcpInfo.want_ipaddr;
    else
      dest = GetIpAddr(argv[0]);
    netmask = GetIpAddr(argv[1]);
    if (strcasecmp(argv[2], "HISADDR") == 0)
      gateway = IpcpInfo.his_ipaddr;
    else
      gateway = GetIpAddr(argv[2]);
    OsSetRoute(RTM_ADD, dest, gateway, netmask);
    return 0;
  }

  return -1;
}

static int
DeleteCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct in_addr dest, gateway, netmask;

  if (argc == 1 && strcasecmp(argv[0], "all") == 0)
    DeleteIfRoutes(0);
  else if (argc > 0 && argc < 4) {
    if (strcasecmp(argv[0], "MYADDR") == 0)
      dest = IpcpInfo.want_ipaddr;
    else
      dest = GetIpAddr(argv[0]);
    netmask.s_addr = INADDR_ANY;
    if (argc > 1) {
      if (strcasecmp(argv[1], "HISADDR") == 0)
        gateway = IpcpInfo.his_ipaddr;
      else
        gateway = GetIpAddr(argv[1]);
      if (argc == 3) {
        if (inet_aton(argv[2], &netmask) == 0) {
	  LogPrintf(LogWARN, "Bad netmask value.\n");
	  return -1;
        }
      }
    } else
      gateway.s_addr = INADDR_ANY;
    OsSetRoute(RTM_DELETE, dest, gateway, netmask);
  } else
    return -1;

  return 0;
}

static int AliasEnable();
static int AliasOption();

static struct cmdtab const AliasCommands[] =
{
  { "enable",   NULL,     AliasEnable,          LOCAL_AUTH,
        "enable IP aliasing", "alias enable [yes|no]"},
  { "port",   NULL,     AliasRedirectPort,          LOCAL_AUTH,
        "port redirection", "alias port [proto addr_local:port_local  port_alias]"},
  { "addr",   NULL,     AliasRedirectAddr,          LOCAL_AUTH,
        "static address translation", "alias addr [addr_local addr_alias]"},
  { "deny_incoming",  NULL,    AliasOption,     LOCAL_AUTH,
        "stop incoming connections",   "alias deny_incoming [yes|no]",
        (void*)PKT_ALIAS_DENY_INCOMING},
  { "log",  NULL,     AliasOption,              LOCAL_AUTH,
        "log aliasing link creation",           "alias log [yes|no]",
        (void*)PKT_ALIAS_LOG},
  { "same_ports", NULL,     AliasOption,        LOCAL_AUTH,
        "try to leave port numbers unchanged", "alias same_ports [yes|no]",
        (void*)PKT_ALIAS_SAME_PORTS},
  { "use_sockets", NULL,     AliasOption,       LOCAL_AUTH,
        "allocate host sockets", "alias use_sockets [yes|no]",
        (void*)PKT_ALIAS_USE_SOCKETS },
  { "unregistered_only", NULL,     AliasOption, LOCAL_AUTH,
        "alias unregistered (private) IP address space only",
        "alias unregistered_only [yes|no]",
        (void*)PKT_ALIAS_UNREGISTERED_ONLY},
  { "help",     "?",      HelpCommand,          LOCAL_AUTH | LOCAL_NO_AUTH,
        "Display this message", "alias help|? [command]",
        (void *)AliasCommands},
  { NULL,       NULL,     NULL },
};


static int
AliasCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0)
    FindExec(AliasCommands, argc, argv);
  else if (VarTerm)
    fprintf(VarTerm, "Use `alias help' to get a list or `alias help <option>'"
	    " for syntax help.\n");
  else
    LogPrintf(LogWARN, "alias command must have arguments\n");

  return 0;
}

static int
AliasEnable(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc == 1)
    if (strcasecmp(argv[0], "yes") == 0) {
      if (!(mode & MODE_ALIAS)) {
        if (loadAliasHandlers(&VarAliasHandlers) == 0) {
          mode |= MODE_ALIAS;
          return 0;
        }
        LogPrintf(LogWARN, "Cannot load alias library\n");
        return 1;
      }
      return 0;
    } else if (strcasecmp(argv[0], "no") == 0) {
      if (mode & MODE_ALIAS) {
        unloadAliasHandlers();
        mode &= ~MODE_ALIAS;
      }
      return 0;
    }

  return -1;
}


static int
AliasOption(list, argc, argv, param)
struct cmdtab *list;
int argc;
char **argv;
void* param;
{
   if (argc == 1)
     if (strcasecmp(argv[0], "yes") == 0) {
       if (mode & MODE_ALIAS) {
         VarSetPacketAliasMode((unsigned)param, (unsigned)param);
         return 0;
       }
       LogPrintf(LogWARN, "alias not enabled\n");
     } else if (strcmp(argv[0], "no") == 0) {
       if (mode & MODE_ALIAS) {
         VarSetPacketAliasMode(0, (unsigned)param);
         return 0;
       }
       LogPrintf(LogWARN, "alias not enabled\n");
     }

   return -1;
}
