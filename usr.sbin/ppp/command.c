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
 * $Id: command.c,v 1.124 1997/12/30 23:22:27 brian Exp $
 *
 */
#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netdb.h>

#ifndef NOALIAS
#include <alias.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "phase.h"
#include "lcp.h"
#include "iplist.h"
#include "ipcp.h"
#include "modem.h"
#include "filter.h"
#ifndef NOALIAS
#include "alias_cmd.h"
#endif
#include "hdlc.h"
#include "loadalias.h"
#include "vars.h"
#include "systems.h"
#include "chat.h"
#include "os.h"
#include "server.h"
#include "main.h"
#include "route.h"
#include "ccp.h"
#include "ip.h"
#include "slcompress.h"
#include "auth.h"

struct in_addr ifnetmask;
static const char *HIDDEN = "********";

static int ShowCommand(struct cmdargs const *arg);
static int TerminalCommand(struct cmdargs const *arg);
static int QuitCommand(struct cmdargs const *arg);
static int CloseCommand(struct cmdargs const *arg);
static int DialCommand(struct cmdargs const *arg);
static int DownCommand(struct cmdargs const *arg);
static int AllowCommand(struct cmdargs const *arg);
static int SetCommand(struct cmdargs const *arg);
static int AddCommand(struct cmdargs const *arg);
static int DeleteCommand(struct cmdargs const *arg);
static int BgShellCommand(struct cmdargs const *arg);
static int FgShellCommand(struct cmdargs const *arg);
#ifndef NOALIAS
static int AliasCommand(struct cmdargs const *arg);
static int AliasEnable(struct cmdargs const *arg);
static int AliasOption(struct cmdargs const *arg);
#endif

static int
HelpCommand(struct cmdargs const *arg)
{
  struct cmdtab const *cmd;
  int n, cmax, dmax, cols;

  if (!VarTerm)
    return 0;

  if (arg->argc > 0) {
    for (cmd = arg->cmd; cmd->name; cmd++)
      if (strcasecmp(cmd->name, *arg->argv) == 0 &&
          (cmd->lauth & VarLocalAuth)) {
	fprintf(VarTerm, "%s\n", cmd->syntax);
	return 0;
      }
    return -1;
  }
  cmax = dmax = 0;
  for (cmd = arg->cmd; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      if ((n = strlen(cmd->name)) > cmax)
        cmax = n;
      if ((n = strlen(cmd->helpmes)) > dmax)
        dmax = n;
    }

  cols = 80 / (dmax + cmax + 3);
  n = 0;
  for (cmd = arg->cmd; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      fprintf(VarTerm, " %-*.*s: %-*.*s",
              cmax, cmax, cmd->name, dmax, dmax, cmd->helpmes);
      if (++n % cols == 0)
        fprintf(VarTerm, "\n");
    }
  if (n % cols != 0)
    fprintf(VarTerm, "\n");

  return 0;
}

int
IsInteractive(int Display)
{
  const char *mes = NULL;

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
    if (Display && VarTerm)
      fprintf(VarTerm, "%s\n", mes);
    return 0;
  }
  return 1;
}

static int
DialCommand(struct cmdargs const *arg)
{
  int tries;
  int res;

  if (LcpFsm.state > ST_CLOSED) {
    if (VarTerm)
      fprintf(VarTerm, "LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return 0;
  }

  if (arg->argc > 0 && (res = LoadCommand(arg)) != 0)
    return res;

  tries = 0;
  do {
    if (VarTerm)
      fprintf(VarTerm, "Dial attempt %u of %d\n", ++tries, VarDialTries);
    if (OpenModem() < 0) {
      if (VarTerm)
	fprintf(VarTerm, "Failed to open modem.\n");
      break;
    }
    if ((res = DialModem()) == EX_DONE) {
      nointr_sleep(1);
      ModemTimeout(NULL);
      PacketMode();
      break;
    } else if (res == EX_SIG)
      return 1;
  } while (VarDialTries == 0 || tries < VarDialTries);

  return 0;
}

static int
SetLoopback(struct cmdargs const *arg)
{
  if (arg->argc == 1)
    if (!strcasecmp(*arg->argv, "on")) {
      VarLoopback = 1;
      return 0;
    }
    else if (!strcasecmp(*arg->argv, "off")) {
      VarLoopback = 0;
      return 0;
    }
  return -1;
}

static int
ShellCommand(struct cmdargs const *arg, int bg)
{
  const char *shell;
  pid_t shpid;
  int argc;
  char *argv[MAXARGS];

#ifdef SHELL_ONLY_INTERACTIVELY
  /* we're only allowed to shell when we run ppp interactively */
  if (mode != MODE_INTER) {
    LogPrintf(LogWARN, "Can only start a shell in interactive mode\n");
    return 1;
  }
#endif
#ifdef NO_SHELL_IN_AUTO_INTERACTIVE

  /*
   * we want to stop shell commands when we've got a telnet connection to an
   * auto mode ppp
   */
  if (VarTerm && !(mode & MODE_INTER)) {
    LogPrintf(LogWARN, "Shell is not allowed interactively in auto mode\n");
    return 1;
  }
#endif

  if (arg->argc == 0)
    if (!(mode & MODE_INTER)) {
      if (VarTerm)
        LogPrintf(LogWARN, "Can't start an interactive shell from"
		  " a telnet session\n");
      else
        LogPrintf(LogWARN, "Can only start an interactive shell in"
		  " interactive mode\n");
      return 1;
    } else if (bg) {
      LogPrintf(LogWARN, "Can only start an interactive shell in"
		" the foreground mode\n");
      return 1;
    }
  if ((shell = getenv("SHELL")) == 0)
    shell = _PATH_BSHELL;

  if ((shpid = fork()) == 0) {
    int dtablesize, i, fd;

    TermTimerService();
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGALRM, SIG_DFL);

    if (VarTerm)
      fd = fileno(VarTerm);
    else if ((fd = open("/dev/null", O_RDWR)) == -1) {
      LogPrintf(LogALERT, "Failed to open /dev/null: %s\n", strerror(errno));
      exit(1);
    }
    for (i = 0; i < 3; i++)
      dup2(fd, i);

    for (dtablesize = getdtablesize(), i = 3; i < dtablesize; i++)
      close(i);

    TtyOldMode();
    setuid(geteuid());
    if (arg->argc > 0) {
      /* substitute pseudo args */
      argv[0] = strdup(arg->argv[0]);
      for (argc = 1; argc < arg->argc; argc++) {
	if (strcasecmp(arg->argv[argc], "HISADDR") == 0)
	  argv[argc] = strdup(inet_ntoa(IpcpInfo.his_ipaddr));
	else if (strcasecmp(arg->argv[argc], "INTERFACE") == 0)
	  argv[argc] = strdup(IfDevName);
	else if (strcasecmp(arg->argv[argc], "MYADDR") == 0)
	  argv[argc] = strdup(inet_ntoa(IpcpInfo.want_ipaddr));
        else
          argv[argc] = strdup(arg->argv[argc]);
      }
      argv[argc] = NULL;
      if (bg) {
	pid_t p;

	p = getpid();
	if (daemon(1, 1) == -1) {
	  LogPrintf(LogERROR, "%d: daemon: %s\n", p, strerror(errno));
	  exit(1);
	}
      } else if (VarTerm)
        printf("ppp: Pausing until %s finishes\n", arg->argv[0]);
      execvp(argv[0], argv);
    } else {
      if (VarTerm)
        printf("ppp: Pausing until %s finishes\n", shell);
      execl(shell, shell, NULL);
    }

    LogPrintf(LogWARN, "exec() of %s failed\n",
              arg->argc > 0 ? arg->argv[0] : shell);
    exit(255);
  }
  if (shpid == (pid_t) - 1) {
    LogPrintf(LogERROR, "Fork failed: %s\n", strerror(errno));
  } else {
    int status;

    waitpid(shpid, &status, 0);
  }

  TtyCommandMode(0);

  return (0);
}

static int
BgShellCommand(struct cmdargs const *arg)
{
  if (arg->argc == 0)
    return -1;
  return ShellCommand(arg, 1);
}

static int
FgShellCommand(struct cmdargs const *arg)
{
  return ShellCommand(arg, 0);
}

static struct cmdtab const Commands[] = {
  {"accept", NULL, AcceptCommand, LOCAL_AUTH,
  "accept option request", "accept option .."},
  {"add", NULL, AddCommand, LOCAL_AUTH,
  "add route", "add dest mask gateway", NULL},
  {"add!", NULL, AddCommand, LOCAL_AUTH,
  "add or change route", "add! dest mask gateway", (void *)1},
  {"allow", "auth", AllowCommand, LOCAL_AUTH,
  "Allow ppp access", "allow users|modes ...."},
  {"bg", "!bg", BgShellCommand, LOCAL_AUTH,
  "Run a background command", "[!]bg command"},
  {"close", NULL, CloseCommand, LOCAL_AUTH,
  "Close connection", "close"},
  {"delete", NULL, DeleteCommand, LOCAL_AUTH,
  "delete route", "delete dest", NULL},
  {"delete!", NULL, DeleteCommand, LOCAL_AUTH,
  "delete a route if it exists", "delete! dest", (void *)1},
  {"deny", NULL, DenyCommand, LOCAL_AUTH,
  "Deny option request", "deny option .."},
  {"dial", "call", DialCommand, LOCAL_AUTH,
  "Dial and login", "dial|call [remote]"},
  {"disable", NULL, DisableCommand, LOCAL_AUTH,
  "Disable option", "disable option .."},
  {"display", NULL, DisplayCommand, LOCAL_AUTH,
  "Display option configs", "display"},
  {"enable", NULL, EnableCommand, LOCAL_AUTH,
  "Enable option", "enable option .."},
  {"passwd", NULL, LocalAuthCommand, LOCAL_NO_AUTH,
  "Password for manipulation", "passwd LocalPassword"},
  {"load", NULL, LoadCommand, LOCAL_AUTH,
  "Load settings", "load [remote]"},
  {"save", NULL, SaveCommand, LOCAL_AUTH,
  "Save settings", "save"},
  {"set", "setup", SetCommand, LOCAL_AUTH,
  "Set parameters", "set[up] var value"},
  {"shell", "!", FgShellCommand, LOCAL_AUTH,
  "Run a subshell", "shell|! [sh command]"},
  {"show", NULL, ShowCommand, LOCAL_AUTH,
  "Show status and stats", "show var"},
  {"term", NULL, TerminalCommand, LOCAL_AUTH,
  "Enter terminal mode", "term"},
#ifndef NOALIAS
  {"alias", NULL, AliasCommand, LOCAL_AUTH,
  "alias control", "alias option [yes|no]"},
#endif
  {"quit", "bye", QuitCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Quit PPP program", "quit|bye [all]"},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "help|? [command]", Commands},
  {NULL, "down", DownCommand, LOCAL_AUTH,
  "Generate down event", "down"},
  {NULL, NULL, NULL},
};

static int
ShowLoopback(struct cmdargs const *arg)
{
  if (VarTerm)
    fprintf(VarTerm, "Local loopback is %s\n", VarLoopback ? "on" : "off");

  return 0;
}

static int
ShowLogLevel(struct cmdargs const *arg)
{
  int i;

  if (!VarTerm)
    return 0;

  fprintf(VarTerm, "Log:  ");
  for (i = LogMIN; i <= LogMAX; i++)
    if (LogIsKept(i) & LOG_KEPT_SYSLOG)
      fprintf(VarTerm, " %s", LogName(i));

  fprintf(VarTerm, "\nLocal:");
  for (i = LogMIN; i <= LogMAX; i++)
    if (LogIsKept(i) & LOG_KEPT_LOCAL)
      fprintf(VarTerm, " %s", LogName(i));

  fprintf(VarTerm, "\n");

  return 0;
}

static int
ShowEscape(struct cmdargs const *arg)
{
  int code, bit;

  if (!VarTerm)
    return 0;
  if (EscMap[32]) {
    for (code = 0; code < 32; code++)
      if (EscMap[code])
	for (bit = 0; bit < 8; bit++)
	  if (EscMap[code] & (1 << bit))
	    fprintf(VarTerm, " 0x%02x", (code << 3) + bit);
    fprintf(VarTerm, "\n");
  }
  return 0;
}

static int
ShowTimeout(struct cmdargs const *arg)
{
  if (VarTerm) {
    int remaining;

    fprintf(VarTerm, " Idle Timer: %d secs   LQR Timer: %d secs"
	    "   Retry Timer: %d secs\n", VarIdleTimeout, VarLqrTimeout,
	    VarRetryTimeout);
    remaining = RemainingIdleTime();
    if (remaining != -1)
    fprintf(VarTerm, " %d secs remaining\n", remaining);
  }
  return 0;
}

static int
ShowStopped(struct cmdargs const *arg)
{
  if (!VarTerm)
    return 0;

  fprintf(VarTerm, " Stopped Timer:  LCP: ");
  if (!LcpFsm.StoppedTimer.load)
    fprintf(VarTerm, "Disabled");
  else
    fprintf(VarTerm, "%ld secs", LcpFsm.StoppedTimer.load / SECTICKS);

  fprintf(VarTerm, ", IPCP: ");
  if (!IpcpFsm.StoppedTimer.load)
    fprintf(VarTerm, "Disabled");
  else
    fprintf(VarTerm, "%ld secs", IpcpFsm.StoppedTimer.load / SECTICKS);

  fprintf(VarTerm, ", CCP: ");
  if (!CcpFsm.StoppedTimer.load)
    fprintf(VarTerm, "Disabled");
  else
    fprintf(VarTerm, "%ld secs", CcpFsm.StoppedTimer.load / SECTICKS);

  fprintf(VarTerm, "\n");

  return 0;
}

static int
ShowAuthKey(struct cmdargs const *arg)
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, "AuthName = %s\n", VarAuthName);
  fprintf(VarTerm, "AuthKey  = %s\n", HIDDEN);
#ifdef HAVE_DES
  fprintf(VarTerm, "Encrypt  = %s\n", VarMSChap ? "MSChap" : "MD5" );
#endif
  return 0;
}

static int
ShowVersion(struct cmdargs const *arg)
{
  if (VarTerm)
    fprintf(VarTerm, "%s - %s \n", VarVersion, VarLocalVersion);
  return 0;
}

static int
ShowInitialMRU(struct cmdargs const *arg)
{
  if (VarTerm)
    fprintf(VarTerm, " Initial MRU: %ld\n", VarMRU);
  return 0;
}

static int
ShowPreferredMTU(struct cmdargs const *arg)
{
  if (VarTerm)
    if (VarPrefMTU)
      fprintf(VarTerm, " Preferred MTU: %ld\n", VarPrefMTU);
    else
      fprintf(VarTerm, " Preferred MTU: unspecified\n");
  return 0;
}

static int
ShowReconnect(struct cmdargs const *arg)
{
  if (VarTerm)
    fprintf(VarTerm, " Reconnect Timer:  %d,  %d tries\n",
	    VarReconnectTimer, VarReconnectTries);
  return 0;
}

static int
ShowRedial(struct cmdargs const *arg)
{
  if (!VarTerm)
    return 0;
  fprintf(VarTerm, " Redial Timer: ");

  if (VarRedialTimeout >= 0) {
    fprintf(VarTerm, " %d seconds, ", VarRedialTimeout);
  } else {
    fprintf(VarTerm, " Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  fprintf(VarTerm, " Redial Next Timer: ");

  if (VarRedialNextTimeout >= 0) {
    fprintf(VarTerm, " %d seconds, ", VarRedialNextTimeout);
  } else {
    fprintf(VarTerm, " Random 0 - %d seconds, ", REDIAL_PERIOD);
  }

  if (VarDialTries)
    fprintf(VarTerm, "%d dial tries", VarDialTries);

  fprintf(VarTerm, "\n");

  return 0;
}

#ifndef NOMSEXT
static int
ShowMSExt(struct cmdargs const *arg)
{
  if (VarTerm) {
    fprintf(VarTerm, " MS PPP extention values \n");
    fprintf(VarTerm, "   Primary NS     : %s\n", inet_ntoa(ns_entries[0]));
    fprintf(VarTerm, "   Secondary NS   : %s\n", inet_ntoa(ns_entries[1]));
    fprintf(VarTerm, "   Primary NBNS   : %s\n", inet_ntoa(nbns_entries[0]));
    fprintf(VarTerm, "   Secondary NBNS : %s\n", inet_ntoa(nbns_entries[1]));
  }
  return 0;
}

#endif

static struct cmdtab const ShowCommands[] = {
  {"afilter", NULL, ShowAfilter, LOCAL_AUTH,
  "Show keep-alive filters", "show afilter option .."},
  {"auth", NULL, ShowAuthKey, LOCAL_AUTH,
  "Show auth details", "show auth"},
  {"ccp", NULL, ReportCcpStatus, LOCAL_AUTH,
  "Show CCP status", "show cpp"},
  {"compress", NULL, ReportCompress, LOCAL_AUTH,
  "Show compression stats", "show compress"},
  {"dfilter", NULL, ShowDfilter, LOCAL_AUTH,
  "Show Demand filters", "show dfilteroption .."},
  {"escape", NULL, ShowEscape, LOCAL_AUTH,
  "Show escape characters", "show escape"},
  {"hdlc", NULL, ReportHdlcStatus, LOCAL_AUTH,
  "Show HDLC errors", "show hdlc"},
  {"ifilter", NULL, ShowIfilter, LOCAL_AUTH,
  "Show Input filters", "show ifilter option .."},
  {"ipcp", NULL, ReportIpcpStatus, LOCAL_AUTH,
  "Show IPCP status", "show ipcp"},
  {"lcp", NULL, ReportLcpStatus, LOCAL_AUTH,
  "Show LCP status", "show lcp"},
  {"loopback", NULL, ShowLoopback, LOCAL_AUTH,
  "Show loopback setting", "show loopback"},
  {"log", NULL, ShowLogLevel, LOCAL_AUTH,
  "Show log levels", "show log"},
  {"mem", NULL, ShowMemMap, LOCAL_AUTH,
  "Show memory map", "show mem"},
  {"modem", NULL, ShowModemStatus, LOCAL_AUTH,
  "Show modem setups", "show modem"},
  {"mru", NULL, ShowInitialMRU, LOCAL_AUTH,
  "Show Initial MRU", "show mru"},
  {"mtu", NULL, ShowPreferredMTU, LOCAL_AUTH,
  "Show Preferred MTU", "show mtu"},
  {"ofilter", NULL, ShowOfilter, LOCAL_AUTH,
  "Show Output filters", "show ofilter option .."},
  {"proto", NULL, ReportProtStatus, LOCAL_AUTH,
  "Show protocol summary", "show proto"},
  {"reconnect", NULL, ShowReconnect, LOCAL_AUTH,
  "Show reconnect timer", "show reconnect"},
  {"redial", NULL, ShowRedial, LOCAL_AUTH,
  "Show Redial timeout", "show redial"},
  {"route", NULL, ShowRoute, LOCAL_AUTH,
  "Show routing table", "show route"},
  {"timeout", NULL, ShowTimeout, LOCAL_AUTH,
  "Show Idle timeout", "show timeout"},
  {"stopped", NULL, ShowStopped, LOCAL_AUTH,
  "Show STOPPED timeout", "show stopped"},
#ifndef NOMSEXT
  {"msext", NULL, ShowMSExt, LOCAL_AUTH,
  "Show MS PPP extentions", "show msext"},
#endif
  {"version", NULL, ShowVersion, LOCAL_NO_AUTH | LOCAL_AUTH,
  "Show version string", "show version"},
  {"help", "?", HelpCommand, LOCAL_NO_AUTH | LOCAL_AUTH,
  "Display this message", "show help|? [command]", ShowCommands},
  {NULL, NULL, NULL},
};

static struct cmdtab const *
FindCommand(struct cmdtab const *cmds, const char *str, int *pmatch)
{
  int nmatch;
  int len;
  struct cmdtab const *found;

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
    } else if (cmds->alias && strncasecmp(str, cmds->alias, len) == 0) {
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

static int
FindExec(struct cmdtab const *cmds, int argc, char const *const *argv,
         const char *prefix)
{
  struct cmdtab const *cmd;
  int val = 1;
  int nmatch;
  struct cmdargs arg;

  cmd = FindCommand(cmds, *argv, &nmatch);
  if (nmatch > 1)
    LogPrintf(LogWARN, "%s%s: Ambiguous command\n", prefix, *argv);
  else if (cmd && (cmd->lauth & VarLocalAuth)) {
    arg.cmd = cmds;
    arg.argc = argc-1;
    arg.argv = argv+1;
    arg.data = cmd->args;
    val = (cmd->func) (&arg);
  } else
    LogPrintf(LogWARN, "%s%s: Invalid command\n", prefix, *argv);

  if (val == -1)
    LogPrintf(LogWARN, "Usage: %s\n", cmd->syntax);
  else if (val)
    LogPrintf(LogWARN, "%s%s: Failed %d\n", prefix, *argv, val);

  return val;
}

int aft_cmd = 1;

void
Prompt()
{
  const char *pconnect, *pauth;

  if (!VarTerm || TermMode)
    return;

  if (!aft_cmd)
    fprintf(VarTerm, "\n");
  else
    aft_cmd = 0;

  if (VarLocalAuth == LOCAL_AUTH)
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
InterpretCommand(char *buff, int nb, int *argc, char ***argv)
{
  static char *vector[MAXARGS];
  char *cp;

  if (nb > 0) {
    cp = buff + strcspn(buff, "\r\n");
    if (cp)
      *cp = '\0';
    *argc = MakeArgs(buff, vector, VECSIZE(vector));
    *argv = vector;
  } else
    *argc = 0;
}

static int
arghidden(int argc, char const *const *argv, int n)
{
  /* Is arg n of the given command to be hidden from the log ? */

  /* set authkey xxxxx */
  /* set key xxxxx */
  if (n == 2 && !strncasecmp(argv[0], "se", 2) &&
      (!strncasecmp(argv[1], "authk", 5) || !strncasecmp(argv[1], "ke", 2)))
    return 1;

  /* passwd xxxxx */
  if (n == 1 && !strncasecmp(argv[0], "p", 1))
    return 1;

  return 0;
}

void
RunCommand(int argc, char const *const *argv, const char *label)
{
  if (argc > 0) {
    if (LogIsKept(LogCOMMAND)) {
      static char buf[LINE_LEN];
      int f, n;

      *buf = '\0';
      if (label) {
        strncpy(buf, label, sizeof buf - 3);
        buf[sizeof buf - 3] = '\0';
        strcat(buf, ": ");
      }
      n = strlen(buf);
      for (f = 0; f < argc; f++) {
        if (n < sizeof buf - 1 && f)
          buf[n++] = ' ';
        if (arghidden(argc, argv, f))
          strncpy(buf+n, HIDDEN, sizeof buf - n - 1);
        else
          strncpy(buf+n, argv[f], sizeof buf - n - 1);
        n += strlen(buf+n);
      }
      LogPrintf(LogCOMMAND, "%s\n", buf);
    }
    FindExec(Commands, argc, argv, "");
  }
}

void
DecodeCommand(char *buff, int nb, const char *label)
{
  int argc;
  char **argv;

  InterpretCommand(buff, nb, &argc, &argv);
  RunCommand(argc, (char const *const *)argv, label);
}

static int
ShowCommand(struct cmdargs const *arg)
{
  if (arg->argc > 0)
    FindExec(ShowCommands, arg->argc, arg->argv, "show ");
  else if (VarTerm)
    fprintf(VarTerm, "Use ``show ?'' to get a arg->cmd.\n");
  else
    LogPrintf(LogWARN, "show command must have arguments\n");

  return 0;
}

static int
TerminalCommand(struct cmdargs const *arg)
{
  if (LcpFsm.state > ST_CLOSED) {
    if (VarTerm)
      fprintf(VarTerm, "LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return 1;
  }
  if (!IsInteractive(1))
    return (1);
  if (OpenModem() < 0) {
    if (VarTerm)
      fprintf(VarTerm, "Failed to open modem.\n");
    return (1);
  }
  if (VarTerm) {
    fprintf(VarTerm, "Enter to terminal mode.\n");
    fprintf(VarTerm, "Type `~?' for help.\n");
  }
  TtyTermMode();
  return (0);
}

static int
QuitCommand(struct cmdargs const *arg)
{
  if (VarTerm) {
    DropClient(1);
    if (mode & MODE_INTER)
      Cleanup(EX_NORMAL);
    else if (arg->argc > 0 && !strcasecmp(*arg->argv, "all") && VarLocalAuth&LOCAL_AUTH)
      Cleanup(EX_NORMAL);
  }

  return 0;
}

static int
CloseCommand(struct cmdargs const *arg)
{
  reconnect(RECON_FALSE);
  LcpClose();
  return 0;
}

static int
DownCommand(struct cmdargs const *arg)
{
  LcpDown();
  return 0;
}

static int
SetModemSpeed(struct cmdargs const *arg)
{
  int speed;

  if (arg->argc > 0) {
    if (strcasecmp(*arg->argv, "sync") == 0) {
      VarSpeed = 0;
      return 0;
    }
    speed = atoi(*arg->argv);
    if (IntToSpeed(speed) != B0) {
      VarSpeed = speed;
      return 0;
    }
    LogPrintf(LogWARN, "%s: Invalid speed\n", *arg->argv);
  }
  return -1;
}

static int
SetReconnect(struct cmdargs const *arg)
{
  if (arg->argc == 2) {
    VarReconnectTimer = atoi(arg->argv[0]);
    VarReconnectTries = atoi(arg->argv[1]);
    return 0;
  }
  return -1;
}

static int
SetRedialTimeout(struct cmdargs const *arg)
{
  int timeout;
  int tries;
  char *dot;

  if (arg->argc == 1 || arg->argc == 2) {
    if (strncasecmp(arg->argv[0], "random", 6) == 0 &&
	(arg->argv[0][6] == '\0' || arg->argv[0][6] == '.')) {
      VarRedialTimeout = -1;
      randinit();
    } else {
      timeout = atoi(arg->argv[0]);

      if (timeout >= 0)
	VarRedialTimeout = timeout;
      else {
	LogPrintf(LogWARN, "Invalid redial timeout\n");
	return -1;
      }
    }

    dot = strchr(arg->argv[0], '.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
	VarRedialNextTimeout = -1;
	randinit();
      } else {
	timeout = atoi(dot);
	if (timeout >= 0)
	  VarRedialNextTimeout = timeout;
	else {
	  LogPrintf(LogWARN, "Invalid next redial timeout\n");
	  return -1;
	}
      }
    } else
      VarRedialNextTimeout = NEXT_REDIAL_PERIOD;	/* Default next timeout */

    if (arg->argc == 2) {
      tries = atoi(arg->argv[1]);

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
SetStoppedTimeout(struct cmdargs const *arg)
{
  LcpFsm.StoppedTimer.load = 0;
  IpcpFsm.StoppedTimer.load = 0;
  CcpFsm.StoppedTimer.load = 0;
  if (arg->argc <= 3) {
    if (arg->argc > 0) {
      LcpFsm.StoppedTimer.load = atoi(arg->argv[0]) * SECTICKS;
      if (arg->argc > 1) {
	IpcpFsm.StoppedTimer.load = atoi(arg->argv[1]) * SECTICKS;
	if (arg->argc > 2)
	  CcpFsm.StoppedTimer.load = atoi(arg->argv[2]) * SECTICKS;
      }
    }
    return 0;
  }
  return -1;
}

#define ismask(x) \
  (*x == '0' && strlen(x) == 4 && strspn(x+1, "0123456789.") == 3)

static int
SetServer(struct cmdargs const *arg)
{
  int res = -1;

  if (arg->argc > 0 && arg->argc < 4) {
    const char *port, *passwd, *mask;

    /* What's what ? */
    port = arg->argv[0];
    if (arg->argc == 2)
      if (ismask(arg->argv[1])) {
        passwd = NULL;
        mask = arg->argv[1];
      } else {
        passwd = arg->argv[1];
        mask = NULL;
      }
    else if (arg->argc == 3) {
      passwd = arg->argv[1];
      mask = arg->argv[2];
      if (!ismask(mask))
        return -1;
    } else
      passwd = mask = NULL;

    if (passwd == NULL)
      VarHaveLocalAuthKey = 0;
    else {
      strncpy(VarLocalAuthKey, passwd, sizeof VarLocalAuthKey - 1);
      VarLocalAuthKey[sizeof VarLocalAuthKey - 1] = '\0';
      VarHaveLocalAuthKey = 1;
    }
    LocalAuthInit();

    if (strcasecmp(port, "none") == 0) {
      int oserver;

      if (mask != NULL || passwd != NULL)
        return -1;
      oserver = server;
      ServerClose();
      if (oserver != -1)
        LogPrintf(LogPHASE, "Disabling server port.\n");
      res = 0;
    } else if (*port == '/') {
      mode_t imask;

      if (mask != NULL) {
	unsigned m;

	if (sscanf(mask, "%o", &m) == 1)
	  imask = m;
        else
          return -1;
      } else
        imask = (mode_t)-1;
      res = ServerLocalOpen(port, imask);
    } else {
      int iport;

      if (mask != NULL)
        return -1;

      if (strspn(port, "0123456789") != strlen(port)) {
        struct servent *s;

        if ((s = getservbyname(port, "tcp")) == NULL) {
	  iport = 0;
	  LogPrintf(LogWARN, "%s: Invalid port or service\n", port);
	} else
	  iport = ntohs(s->s_port);
      } else
        iport = atoi(port);
      res = iport ? ServerTcpOpen(iport) : -1;
    }
  }

  return res;
}

static int
SetModemParity(struct cmdargs const *arg)
{
  return arg->argc > 0 ? ChangeParity(*arg->argv) : -1;
}

static int
SetLogLevel(struct cmdargs const *arg)
{
  int i;
  int res;
  int argc;
  char const *const *argv, *argp;
  void (*Discard)(int), (*Keep)(int);
  void (*DiscardAll)(void);

  argc = arg->argc;
  argv = arg->argv;
  res = 0;
  if (argc == 0 || strcasecmp(argv[0], "local")) {
    Discard = LogDiscard;
    Keep = LogKeep;
    DiscardAll = LogDiscardAll;
  } else {
    argc--;
    argv++;
    Discard = LogDiscardLocal;
    Keep = LogKeepLocal;
    DiscardAll = LogDiscardAllLocal;
  }

  if (argc == 0 || (argv[0][0] != '+' && argv[0][0] != '-'))
    DiscardAll();
  while (argc--) {
    argp = **argv == '+' || **argv == '-' ? *argv + 1 : *argv;
    for (i = LogMIN; i <= LogMAX; i++)
      if (strcasecmp(argp, LogName(i)) == 0) {
	if (**argv == '-')
	  (*Discard)(i);
	else
	  (*Keep)(i);
	break;
      }
    if (i > LogMAX) {
      LogPrintf(LogWARN, "%s: Invalid log value\n", argp);
      res = -1;
    }
    argv++;
  }
  return res;
}

static int
SetEscape(struct cmdargs const *arg)
{
  int code;
  int argc = arg->argc;
  char const *const *argv = arg->argv;

  for (code = 0; code < 33; code++)
    EscMap[code] = 0;

  while (argc-- > 0) {
    sscanf(*argv++, "%x", &code);
    code &= 0xff;
    EscMap[code >> 3] |= (1 << (code & 7));
    EscMap[32] = 1;
  }
  return 0;
}

static int
SetInitialMRU(struct cmdargs const *arg)
{
  long mru;
  const char *err;

  if (arg->argc > 0) {
    mru = atol(*arg->argv);
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
SetPreferredMTU(struct cmdargs const *arg)
{
  long mtu;
  const char *err;

  if (arg->argc > 0) {
    mtu = atol(*arg->argv);
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
SetIdleTimeout(struct cmdargs const *arg)
{
  if (arg->argc > 0) {
    VarIdleTimeout = atoi(arg->argv[0]);
    UpdateIdleTimer();		/* If we're connected, restart the idle timer */
    if (arg->argc > 1) {
      VarLqrTimeout = atoi(arg->argv[1]);
      if (VarLqrTimeout < 1)
	VarLqrTimeout = 30;
      if (arg->argc > 2) {
	VarRetryTimeout = atoi(arg->argv[2]);
	if (VarRetryTimeout < 1 || VarRetryTimeout > 10)
	  VarRetryTimeout = 3;
      }
    }
    return 0;
  }
  return -1;
}

static struct in_addr
GetIpAddr(const char *cp)
{
  struct hostent *hp;
  struct in_addr ipaddr;

  if (inet_aton(cp, &ipaddr) == 0) {
    hp = gethostbyname(cp);
    if (hp && hp->h_addrtype == AF_INET)
      memcpy(&ipaddr, hp->h_addr, hp->h_length);
    else
      ipaddr.s_addr = 0;
  }
  return (ipaddr);
}

static int
SetInterfaceAddr(struct cmdargs const *arg)
{
  const char *hisaddr;

  hisaddr = NULL;
  DefMyAddress.ipaddr.s_addr = DefHisAddress.ipaddr.s_addr = 0L;

  if (arg->argc > 4)
    return -1;

  HaveTriggerAddress = 0;
  ifnetmask.s_addr = 0;
  iplist_reset(&DefHisChoice);

  if (arg->argc > 0) {
    if (!ParseAddr(arg->argc, arg->argv, &DefMyAddress.ipaddr,
		   &DefMyAddress.mask, &DefMyAddress.width))
      return 1;
    if (arg->argc > 1) {
      hisaddr = arg->argv[1];
      if (arg->argc > 2) {
	ifnetmask = GetIpAddr(arg->argv[2]);
	if (arg->argc > 3) {
	  TriggerAddress = GetIpAddr(arg->argv[3]);
	  HaveTriggerAddress = 1;
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
  IpcpInfo.want_ipaddr.s_addr = DefMyAddress.ipaddr.s_addr;
  if (DefHisAddress.ipaddr.s_addr == 0) {
    DefHisAddress.mask.s_addr = 0;
    DefHisAddress.width = 0;
  }

  if (hisaddr && !UseHisaddr(hisaddr, mode & MODE_AUTO))
    return 4;

  return 0;
}

#ifndef NOMSEXT

static void
SetMSEXT(struct in_addr * pri_addr,
	 struct in_addr * sec_addr,
	 int argc,
	 char const *const *argv)
{
  int dummyint;
  struct in_addr dummyaddr;

  pri_addr->s_addr = sec_addr->s_addr = 0L;

  if (argc > 0) {
    ParseAddr(argc, argv++, pri_addr, &dummyaddr, &dummyint);
    if (--argc > 0)
      ParseAddr(argc, argv++, sec_addr, &dummyaddr, &dummyint);
    else
      sec_addr->s_addr = pri_addr->s_addr;
  }

  /*
   * if the primary/secondary ns entries are 0.0.0.0 we should set them to
   * either the localhost's ip, or the values in /etc/resolv.conf ??
   * 
   * up to you if you want to implement this...
   */

}

static int
SetNS(struct cmdargs const *arg)
{
  SetMSEXT(&ns_entries[0], &ns_entries[1], arg->argc, arg->argv);
  return 0;
}

static int
SetNBNS(struct cmdargs const *arg)
{
  SetMSEXT(&nbns_entries[0], &nbns_entries[1], arg->argc, arg->argv);
  return 0;
}

#endif				/* MS_EXT */

int
SetVariable(struct cmdargs const *arg)
{
  u_long map;
  const char *argp;
  int param = (int)arg->data;

  if (arg->argc > 0)
    argp = *arg->argv;
  else
    argp = "";

  switch (param) {
  case VAR_AUTHKEY:
    strncpy(VarAuthKey, argp, sizeof VarAuthKey - 1);
    VarAuthKey[sizeof VarAuthKey - 1] = '\0';
    break;
  case VAR_AUTHNAME:
    strncpy(VarAuthName, argp, sizeof VarAuthName - 1);
    VarAuthName[sizeof VarAuthName - 1] = '\0';
    break;
  case VAR_DIAL:
    strncpy(VarDialScript, argp, sizeof VarDialScript - 1);
    VarDialScript[sizeof VarDialScript - 1] = '\0';
    break;
  case VAR_LOGIN:
    strncpy(VarLoginScript, argp, sizeof VarLoginScript - 1);
    VarLoginScript[sizeof VarLoginScript - 1] = '\0';
    break;
  case VAR_DEVICE:
    if (mode & MODE_INTER)
      HangupModem(0);
    if (modem != -1)
      LogPrintf(LogWARN, "Cannot change device to \"%s\" when \"%s\" is open\n",
                argp, VarDevice);
    else {
      strncpy(VarDeviceList, argp, sizeof VarDeviceList - 1);
      VarDeviceList[sizeof VarDeviceList - 1] = '\0';
    }
    break;
  case VAR_ACCMAP:
    sscanf(argp, "%lx", &map);
    VarAccmap = map;
    break;
  case VAR_PHONE:
    strncpy(VarPhoneList, argp, sizeof VarPhoneList - 1);
    VarPhoneList[sizeof VarPhoneList - 1] = '\0';
    strncpy(VarPhoneCopy, VarPhoneList, sizeof VarPhoneCopy - 1);
    VarPhoneCopy[sizeof VarPhoneCopy - 1] = '\0';
    VarNextPhone = VarPhoneCopy;
    VarAltPhone = NULL;
    break;
  case VAR_HANGUP:
    strncpy(VarHangupScript, argp, sizeof VarHangupScript - 1);
    VarHangupScript[sizeof VarHangupScript - 1] = '\0';
    break;
#ifdef HAVE_DES
  case VAR_ENC:
    VarMSChap = !strcasecmp(argp, "mschap");
    break;
#endif
  }
  return 0;
}

static int 
SetCtsRts(struct cmdargs const *arg)
{
  if (arg->argc > 0) {
    if (strcmp(*arg->argv, "on") == 0)
      VarCtsRts = 1;
    else if (strcmp(*arg->argv, "off") == 0)
      VarCtsRts = 0;
    else
      return -1;
    return 0;
  }
  return -1;
}


static int 
SetOpenMode(struct cmdargs const *arg)
{
  if (arg->argc > 0) {
    if (strcmp(*arg->argv, "active") == 0)
      VarOpenMode = OPEN_ACTIVE;
    else if (strcmp(*arg->argv, "passive") == 0)
      VarOpenMode = OPEN_PASSIVE;
    else
      return -1;
    return 0;
  }
  return -1;
}

static struct cmdtab const SetCommands[] = {
  {"accmap", NULL, SetVariable, LOCAL_AUTH,
  "Set accmap value", "set accmap hex-value", (const void *) VAR_ACCMAP},
  {"afilter", NULL, SetAfilter, LOCAL_AUTH,
  "Set keep Alive filter", "set afilter ..."},
  {"authkey", "key", SetVariable, LOCAL_AUTH,
  "Set authentication key", "set authkey|key key", (const void *) VAR_AUTHKEY},
  {"authname", NULL, SetVariable, LOCAL_AUTH,
  "Set authentication name", "set authname name", (const void *) VAR_AUTHNAME},
  {"ctsrts", NULL, SetCtsRts, LOCAL_AUTH,
  "Use CTS/RTS modem signalling", "set ctsrts [on|off]"},
  {"device", "line", SetVariable, LOCAL_AUTH, "Set modem device name",
  "set device|line device-name[,device-name]", (const void *) VAR_DEVICE},
  {"dfilter", NULL, SetDfilter, LOCAL_AUTH,
  "Set demand filter", "set dfilter ..."},
  {"dial", NULL, SetVariable, LOCAL_AUTH,
  "Set dialing script", "set dial chat-script", (const void *) VAR_DIAL},
#ifdef HAVE_DES
  {"encrypt", NULL, SetVariable, LOCAL_AUTH, "Set CHAP encryption algorithm",
  "set encrypt MSChap|MD5", (const void *) VAR_ENC},
#endif
  {"escape", NULL, SetEscape, LOCAL_AUTH,
  "Set escape characters", "set escape hex-digit ..."},
  {"hangup", NULL, SetVariable, LOCAL_AUTH,
  "Set hangup script", "set hangup chat-script", (const void *) VAR_HANGUP},
  {"ifaddr", NULL, SetInterfaceAddr, LOCAL_AUTH, "Set destination address",
  "set ifaddr [src-addr [dst-addr [netmask [trg-addr]]]]"},
  {"ifilter", NULL, SetIfilter, LOCAL_AUTH,
  "Set input filter", "set ifilter ..."},
  {"loopback", NULL, SetLoopback, LOCAL_AUTH,
  "Set loopback facility", "set loopback on|off"},
  {"log", NULL, SetLogLevel, LOCAL_AUTH,
  "Set log level", "set log [local] [+|-]value..."},
  {"login", NULL, SetVariable, LOCAL_AUTH,
  "Set login script", "set login chat-script", (const void *) VAR_LOGIN},
  {"mru", NULL, SetInitialMRU, LOCAL_AUTH,
  "Set Initial MRU value", "set mru value"},
  {"mtu", NULL, SetPreferredMTU, LOCAL_AUTH,
  "Set Preferred MTU value", "set mtu value"},
  {"ofilter", NULL, SetOfilter, LOCAL_AUTH,
  "Set output filter", "set ofilter ..."},
  {"openmode", NULL, SetOpenMode, LOCAL_AUTH,
  "Set open mode", "set openmode [active|passive]"},
  {"parity", NULL, SetModemParity, LOCAL_AUTH,
  "Set modem parity", "set parity [odd|even|none]"},
  {"phone", NULL, SetVariable, LOCAL_AUTH, "Set telephone number(s)",
  "set phone phone1[:phone2[...]]", (const void *) VAR_PHONE},
  {"reconnect", NULL, SetReconnect, LOCAL_AUTH,
  "Set Reconnect timeout", "set reconnect value ntries"},
  {"redial", NULL, SetRedialTimeout, LOCAL_AUTH, "Set Redial timeout",
  "set redial value|random[.value|random] [dial_attempts]"},
  {"stopped", NULL, SetStoppedTimeout, LOCAL_AUTH, "Set STOPPED timeouts",
  "set stopped [LCPseconds [IPCPseconds [CCPseconds]]]"},
  {"server", "socket", SetServer, LOCAL_AUTH,
  "Set server port", "set server|socket TcpPort|LocalName|none [mask]"},
  {"speed", NULL, SetModemSpeed, LOCAL_AUTH,
  "Set modem speed", "set speed value"},
  {"timeout", NULL, SetIdleTimeout, LOCAL_AUTH,
  "Set Idle timeout", "set timeout value"},
#ifndef NOMSEXT
  {"ns", NULL, SetNS, LOCAL_AUTH,
  "Set NameServer", "set ns pri-addr [sec-addr]"},
  {"nbns", NULL, SetNBNS, LOCAL_AUTH,
  "Set NetBIOS NameServer", "set nbns pri-addr [sec-addr]"},
#endif
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "set help|? [command]", SetCommands},
  {NULL, NULL, NULL},
};

static int
SetCommand(struct cmdargs const *arg)
{
  if (arg->argc > 0)
    FindExec(SetCommands, arg->argc, arg->argv, "set ");
  else if (VarTerm)
    fprintf(VarTerm, "Use `set ?' to get a arg->cmd or `set ? <var>' for"
	    " syntax help.\n");
  else
    LogPrintf(LogWARN, "set command must have arguments\n");

  return 0;
}


static int
AddCommand(struct cmdargs const *arg)
{
  struct in_addr dest, gateway, netmask;
  int gw;

  if (arg->argc != 3 && arg->argc != 2)
    return -1;

  if (arg->argc == 2)
    if (strcasecmp(arg->argv[0], "default"))
      return -1;
    else {
      dest.s_addr = netmask.s_addr = INADDR_ANY;
      gw = 1;
    }
  else {
    if (strcasecmp(arg->argv[0], "MYADDR") == 0)
      dest = IpcpInfo.want_ipaddr;
    else if (strcasecmp(arg->argv[0], "HISADDR") == 0)
      dest = IpcpInfo.his_ipaddr;
    else
      dest = GetIpAddr(arg->argv[0]);
    netmask = GetIpAddr(arg->argv[1]);
    gw = 2;
  }
  if (strcasecmp(arg->argv[gw], "HISADDR") == 0)
    gateway = IpcpInfo.his_ipaddr;
  else if (strcasecmp(arg->argv[gw], "INTERFACE") == 0)
    gateway.s_addr = INADDR_ANY;
  else
    gateway = GetIpAddr(arg->argv[gw]);
  OsSetRoute(RTM_ADD, dest, gateway, netmask, arg->data ? 1 : 0);
  return 0;
}

static int
DeleteCommand(struct cmdargs const *arg)
{
  struct in_addr dest, none;

  if (arg->argc == 1)
    if(strcasecmp(arg->argv[0], "all") == 0)
      DeleteIfRoutes(0);
    else {
      if (strcasecmp(arg->argv[0], "MYADDR") == 0)
        dest = IpcpInfo.want_ipaddr;
      else if (strcasecmp(arg->argv[0], "default") == 0)
        dest.s_addr = INADDR_ANY;
      else
        dest = GetIpAddr(arg->argv[0]);
      none.s_addr = INADDR_ANY;
      OsSetRoute(RTM_DELETE, dest, none, none, arg->data ? 1 : 0);
    }
  else
    return -1;

  return 0;
}

#ifndef NOALIAS
static struct cmdtab const AliasCommands[] =
{
  {"enable", NULL, AliasEnable, LOCAL_AUTH,
  "enable IP aliasing", "alias enable [yes|no]"},
  {"port", NULL, AliasRedirectPort, LOCAL_AUTH,
  "port redirection", "alias port [proto addr_local:port_local  port_alias]"},
  {"addr", NULL, AliasRedirectAddr, LOCAL_AUTH,
  "static address translation", "alias addr [addr_local addr_alias]"},
  {"deny_incoming", NULL, AliasOption, LOCAL_AUTH,
    "stop incoming connections", "alias deny_incoming [yes|no]",
  (const void *) PKT_ALIAS_DENY_INCOMING},
  {"log", NULL, AliasOption, LOCAL_AUTH,
    "log aliasing link creation", "alias log [yes|no]",
  (const void *) PKT_ALIAS_LOG},
  {"same_ports", NULL, AliasOption, LOCAL_AUTH,
    "try to leave port numbers unchanged", "alias same_ports [yes|no]",
  (const void *) PKT_ALIAS_SAME_PORTS},
  {"use_sockets", NULL, AliasOption, LOCAL_AUTH,
    "allocate host sockets", "alias use_sockets [yes|no]",
  (const void *) PKT_ALIAS_USE_SOCKETS},
  {"unregistered_only", NULL, AliasOption, LOCAL_AUTH,
    "alias unregistered (private) IP address space only",
    "alias unregistered_only [yes|no]",
  (const void *) PKT_ALIAS_UNREGISTERED_ONLY},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
    "Display this message", "alias help|? [command]", AliasCommands},
  {NULL, NULL, NULL},
};


static int
AliasCommand(struct cmdargs const *arg)
{
  if (arg->argc > 0)
    FindExec(AliasCommands, arg->argc, arg->argv, "alias ");
  else if (VarTerm)
    fprintf(VarTerm, "Use `alias help' to get a arg->cmd or `alias help <option>'"
	    " for syntax help.\n");
  else
    LogPrintf(LogWARN, "alias command must have arguments\n");

  return 0;
}

static int
AliasEnable(struct cmdargs const *arg)
{
  if (arg->argc == 1)
    if (strcasecmp(arg->argv[0], "yes") == 0) {
      if (!(mode & MODE_ALIAS)) {
	if (loadAliasHandlers(&VarAliasHandlers) == 0) {
	  mode |= MODE_ALIAS;
	  return 0;
	}
	LogPrintf(LogWARN, "Cannot load alias library\n");
	return 1;
      }
      return 0;
    } else if (strcasecmp(arg->argv[0], "no") == 0) {
      if (mode & MODE_ALIAS) {
	unloadAliasHandlers();
	mode &= ~MODE_ALIAS;
      }
      return 0;
    }
  return -1;
}


static int
AliasOption(struct cmdargs const *arg)
{
  unsigned param = (unsigned)arg->data;
  if (arg->argc == 1)
    if (strcasecmp(arg->argv[0], "yes") == 0) {
      if (mode & MODE_ALIAS) {
	VarPacketAliasSetMode(param, param);
	return 0;
      }
      LogPrintf(LogWARN, "alias not enabled\n");
    } else if (strcmp(arg->argv[0], "no") == 0) {
      if (mode & MODE_ALIAS) {
	VarPacketAliasSetMode(0, param);
	return 0;
      }
      LogPrintf(LogWARN, "alias not enabled\n");
    }
  return -1;
}
#endif /* #ifndef NOALIAS */

static struct cmdtab const AllowCommands[] = {
  {"users", "user", AllowUsers, LOCAL_AUTH,
  "Allow users access to ppp", "allow users logname..."},
  {"modes", "mode", AllowModes, LOCAL_AUTH,
  "Only allow certain ppp modes", "allow modes mode..."},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "allow help|? [command]", AllowCommands},
  {NULL, NULL, NULL},
};

static int
AllowCommand(struct cmdargs const *arg)
{
  if (arg->argc > 0)
    FindExec(AllowCommands, arg->argc, arg->argv, "allow ");
  else if (VarTerm)
    fprintf(VarTerm, "Use `allow ?' to get a arg->cmd or `allow ? <cmd>' for"
	    " syntax help.\n");
  else
    LogPrintf(LogWARN, "allow command must have arguments\n");

  return 0;
}
