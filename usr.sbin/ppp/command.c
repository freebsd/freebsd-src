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
 * $Id: command.c,v 1.131.2.35 1998/03/13 00:44:41 brian Exp $
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
#include "lcp.h"
#include "iplist.h"
#include "throughput.h"
#include "ipcp.h"
#include "modem.h"
#include "filter.h"
#ifndef NOALIAS
#include "alias_cmd.h"
#endif
#include "lqr.h"
#include "hdlc.h"
#include "loadalias.h"
#include "vars.h"
#include "systems.h"
#include "bundle.h"
#include "main.h"
#include "route.h"
#include "ccp.h"
#include "ip.h"
#include "slcompress.h"
#include "auth.h"
#include "async.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "server.h"
#include "prompt.h"
#include "chat.h"
#include "chap.h"
#include "datalink.h"

struct in_addr ifnetmask;
static const char *HIDDEN = "********";

static int ShowCommand(struct cmdargs const *);
static int TerminalCommand(struct cmdargs const *);
static int QuitCommand(struct cmdargs const *);
static int CloseCommand(struct cmdargs const *);
static int DialCommand(struct cmdargs const *);
static int DownCommand(struct cmdargs const *);
static int AllowCommand(struct cmdargs const *);
static int SetCommand(struct cmdargs const *);
static int LinkCommand(struct cmdargs const *);
static int AddCommand(struct cmdargs const *);
static int DeleteCommand(struct cmdargs const *);
static int BgShellCommand(struct cmdargs const *);
static int FgShellCommand(struct cmdargs const *);
#ifndef NOALIAS
static int AliasCommand(struct cmdargs const *);
static int AliasEnable(struct cmdargs const *);
static int AliasOption(struct cmdargs const *);
#endif

static int
HelpCommand(struct cmdargs const *arg)
{
  struct cmdtab const *cmd;
  int n, cmax, dmax, cols;

  if (arg->argc > 0) {
    for (cmd = arg->cmdtab; cmd->name; cmd++)
      if (strcasecmp(cmd->name, *arg->argv) == 0 &&
          (cmd->lauth & VarLocalAuth)) {
	prompt_Printf(&prompt, "%s\n", cmd->syntax);
	return 0;
      }
    return -1;
  }
  cmax = dmax = 0;
  for (cmd = arg->cmdtab; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      if ((n = strlen(cmd->name)) > cmax)
        cmax = n;
      if ((n = strlen(cmd->helpmes)) > dmax)
        dmax = n;
    }

  cols = 80 / (dmax + cmax + 3);
  n = 0;
  for (cmd = arg->cmdtab; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & VarLocalAuth)) {
      prompt_Printf(&prompt, " %-*.*s: %-*.*s",
              cmax, cmax, cmd->name, dmax, dmax, cmd->helpmes);
      if (++n % cols == 0)
        prompt_Printf(&prompt, "\n");
    }
  if (n % cols != 0)
    prompt_Printf(&prompt, "\n");

  return 0;
}

int
IsInteractive(int Display)
{
  const char *m = NULL;

  if (mode & MODE_DDIAL)
    m = "direct dial";
  else if (mode & MODE_BACKGROUND)
    m = "background";
  else if (mode & MODE_AUTO)
    m = "auto";
  else if (mode & MODE_DIRECT)
    m = "direct";
  else if (mode & MODE_DEDICATED)
    m = "dedicated";
  else if (mode & MODE_INTER)
    m = "interactive";
  if (m) {
    if (Display)
      prompt_Printf(&prompt, "Working in %s mode\n", m);
  }
  return mode & MODE_INTER;
}

static int
DialCommand(struct cmdargs const *arg)
{
  int res;

  if ((mode & MODE_DAEMON) && !(mode & MODE_AUTO)) {
    LogPrintf(LogWARN,
              "Manual dial is only available in auto and interactive mode\n");
    return 1;
  }

  if (arg->argc > 0 && (res = LoadCommand(arg)) != 0)
    return res;

  bundle_Open(arg->bundle, arg->cx ? arg->cx->name : NULL);

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
  if (prompt_Active(&prompt) && !(mode & MODE_INTER)) {
    LogPrintf(LogWARN, "Shell is not allowed interactively in auto mode\n");
    return 1;
  }
#endif

  if (arg->argc == 0)
    if (!(mode & MODE_INTER)) {
      if (prompt_Active(&prompt))
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

    if (prompt_Active(&prompt))
      fd = prompt.fd_out;
    else if ((fd = open("/dev/null", O_RDWR)) == -1) {
      LogPrintf(LogALERT, "Failed to open /dev/null: %s\n", strerror(errno));
      exit(1);
    }
    for (i = 0; i < 3; i++)
      dup2(fd, i);

    for (dtablesize = getdtablesize(), i = 3; i < dtablesize; i++)
      close(i);

    prompt_TtyOldMode(&prompt);
    setuid(geteuid());
    if (arg->argc > 0) {
      /* substitute pseudo args */
      argv[0] = strdup(arg->argv[0]);
      for (argc = 1; argc < arg->argc; argc++) {
	if (strcasecmp(arg->argv[argc], "HISADDR") == 0)
	  argv[argc] = strdup(inet_ntoa(IpcpInfo.peer_ip));
	else if (strcasecmp(arg->argv[argc], "INTERFACE") == 0)
	  argv[argc] = strdup(arg->bundle->ifname);
	else if (strcasecmp(arg->argv[argc], "MYADDR") == 0)
	  argv[argc] = strdup(inet_ntoa(IpcpInfo.my_ip));
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
      } else if (prompt_Active(&prompt))
        printf("ppp: Pausing until %s finishes\n", arg->argv[0]);
      execvp(argv[0], argv);
    } else {
      if (prompt_Active(&prompt))
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

  prompt_TtyCommandMode(&prompt);

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
  {"close", NULL, CloseCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Close connection", "close"},
  {"delete", NULL, DeleteCommand, LOCAL_AUTH,
  "delete route", "delete dest", NULL},
  {"delete!", NULL, DeleteCommand, LOCAL_AUTH,
  "delete a route if it exists", "delete! dest", (void *)1},
  {"deny", NULL, DenyCommand, LOCAL_AUTH,
  "Deny option request", "deny option .."},
  {"dial", "call", DialCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Dial and login", "dial|call [remote]"},
  {"disable", NULL, DisableCommand, LOCAL_AUTH,
  "Disable option", "disable option .."},
  {"display", NULL, DisplayCommand, LOCAL_AUTH,
  "Display option configs", "display"},
  {"enable", NULL, EnableCommand, LOCAL_AUTH,
  "Enable option", "enable option .."},
  {"passwd", NULL, LocalAuthCommand, LOCAL_NO_AUTH,
  "Password for manipulation", "passwd LocalPassword"},
  {"link", NULL, LinkCommand, LOCAL_AUTH,
  "Link specific commands", "link name command ..."},
  {"load", NULL, LoadCommand, LOCAL_AUTH,
  "Load settings", "load [remote]"},
  {"save", NULL, SaveCommand, LOCAL_AUTH,
  "Save settings", "save"},
  {"set", "setup", SetCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Set parameters", "set[up] var value"},
  {"shell", "!", FgShellCommand, LOCAL_AUTH,
  "Run a subshell", "shell|! [sh command]"},
  {"show", NULL, ShowCommand, LOCAL_AUTH,
  "Show status and stats", "show var"},
  {"term", NULL, TerminalCommand, LOCAL_AUTH | LOCAL_CX,
  "Enter terminal mode", "term"},
#ifndef NOALIAS
  {"alias", NULL, AliasCommand, LOCAL_AUTH,
  "alias control", "alias option [yes|no]"},
#endif
  {"quit", "bye", QuitCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Quit PPP program", "quit|bye [all]"},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "help|? [command]", Commands},
  {NULL, "down", DownCommand, LOCAL_AUTH | LOCAL_CX,
  "Generate down event", "down"},
  {NULL, NULL, NULL},
};

static int
ShowLoopback(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, "Local loopback is %s\n", VarLoopback ? "on" : "off");
  return 0;
}

static int
ShowLogLevel(struct cmdargs const *arg)
{
  int i;

  prompt_Printf(&prompt, "Log:  ");
  for (i = LogMIN; i <= LogMAX; i++)
    if (LogIsKept(i) & LOG_KEPT_SYSLOG)
      prompt_Printf(&prompt, " %s", LogName(i));

  prompt_Printf(&prompt, "\nLocal:");
  for (i = LogMIN; i <= LogMAX; i++)
    if (LogIsKept(i) & LOG_KEPT_LOCAL)
      prompt_Printf(&prompt, " %s", LogName(i));

  prompt_Printf(&prompt, "\n");

  return 0;
}

static int
ShowEscape(struct cmdargs const *arg)
{
  if (arg->cx->physical->async.cfg.EscMap[32]) {
    int code, bit;
    char *sep = "";

    for (code = 0; code < 32; code++)
      if (arg->cx->physical->async.cfg.EscMap[code])
	for (bit = 0; bit < 8; bit++)
	  if (arg->cx->physical->async.cfg.EscMap[code] & (1 << bit)) {
	    prompt_Printf(&prompt, "%s0x%02x", sep, (code << 3) + bit);
            sep = ", ";
          }
    prompt_Printf(&prompt, "\n");
  }
  return 0;
}

static int
ShowTimeout(struct cmdargs const *arg)
{
  int remaining;

  prompt_Printf(&prompt, " Idle Timer: %d secs   LQR Timer: %d secs"
	        "   Retry Timer: %d secs\n", arg->bundle->cfg.idle_timeout,
                VarLqrTimeout, VarRetryTimeout);
  remaining = bundle_RemainingIdleTime(arg->bundle);
  if (remaining != -1)
    prompt_Printf(&prompt, " %d secs remaining\n", remaining);

  return 0;
}

static int
ShowStopped(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, " Stopped Timer:  LCP: ");
  if (!arg->cx->lcp.fsm.StoppedTimer.load)
    prompt_Printf(&prompt, "Disabled");
  else
    prompt_Printf(&prompt, "%ld secs",
                  arg->cx->lcp.fsm.StoppedTimer.load / SECTICKS);

  prompt_Printf(&prompt, ", CCP: ");
  if (!arg->cx->ccp.fsm.StoppedTimer.load)
    prompt_Printf(&prompt, "Disabled");
  else
    prompt_Printf(&prompt, "%ld secs",
                  arg->cx->ccp.fsm.StoppedTimer.load / SECTICKS);

  prompt_Printf(&prompt, "\n");

  return 0;
}

static int
ShowAuthKey(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, "AuthName = %s\n", VarAuthName);
  prompt_Printf(&prompt, "AuthKey  = %s\n", HIDDEN);
#ifdef HAVE_DES
  prompt_Printf(&prompt, "Encrypt  = %s\n", VarMSChap ? "MSChap" : "MD5" );
#endif
  return 0;
}

static int
ShowVersion(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, "%s - %s \n", VarVersion, VarLocalVersion);
  return 0;
}

static int
ShowInitialMRU(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, " Initial MRU: %d\n", VarMRU);
  return 0;
}

static int
ShowPreferredMTU(struct cmdargs const *arg)
{
  if (VarPrefMTU)
    prompt_Printf(&prompt, " Preferred MTU: %d\n", VarPrefMTU);
  else
    prompt_Printf(&prompt, " Preferred MTU: unspecified\n");
  return 0;
}

static int
ShowReconnect(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, "%s: Reconnect Timer:  %d,  %d tries\n",
                arg->cx->name, arg->cx->cfg.reconnect_timeout,
                arg->cx->cfg.max_reconnect);
  return 0;
}

static int
ShowRedial(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, " Redial Timer: ");

  if (arg->cx->cfg.dial_timeout >= 0)
    prompt_Printf(&prompt, " %d seconds, ", arg->cx->cfg.dial_timeout);
  else
    prompt_Printf(&prompt, " Random 0 - %d seconds, ", DIAL_TIMEOUT);

  prompt_Printf(&prompt, " Redial Next Timer: ");

  if (arg->cx->cfg.dial_next_timeout >= 0)
    prompt_Printf(&prompt, " %d seconds, ", arg->cx->cfg.dial_next_timeout);
  else
    prompt_Printf(&prompt, " Random 0 - %d seconds, ", DIAL_TIMEOUT);

  if (arg->cx->cfg.max_dial)
    prompt_Printf(&prompt, "%d dial tries", arg->cx->cfg.max_dial);

  prompt_Printf(&prompt, "\n");

  return 0;
}

#ifndef NOMSEXT
static int
ShowMSExt(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, " MS PPP extention values \n");
  prompt_Printf(&prompt, "   Primary NS     : %s\n",
                inet_ntoa(IpcpInfo.cfg.ns_entries[0]));
  prompt_Printf(&prompt, "   Secondary NS   : %s\n",
                inet_ntoa(IpcpInfo.cfg.ns_entries[1]));
  prompt_Printf(&prompt, "   Primary NBNS   : %s\n",
                inet_ntoa(IpcpInfo.cfg.nbns_entries[0]));
  prompt_Printf(&prompt, "   Secondary NBNS : %s\n",
                inet_ntoa(IpcpInfo.cfg.nbns_entries[1]));

  return 0;
}

#endif

static struct cmdtab const ShowCommands[] = {
  {"afilter", NULL, ShowAfilter, LOCAL_AUTH,
  "Show keep-alive filters", "show afilter option .."},
  {"auth", NULL, ShowAuthKey, LOCAL_AUTH,
  "Show auth details", "show auth"},
  {"ccp", NULL, ccp_ReportStatus, LOCAL_AUTH | LOCAL_CX_OPT,
  "Show CCP status", "show cpp"},
  {"compress", NULL, ReportCompress, LOCAL_AUTH,
  "Show compression stats", "show compress"},
  {"dfilter", NULL, ShowDfilter, LOCAL_AUTH,
  "Show Demand filters", "show dfilteroption .."},
  {"escape", NULL, ShowEscape, LOCAL_AUTH | LOCAL_CX,
  "Show escape characters", "show escape"},
  {"hdlc", NULL, hdlc_ReportStatus, LOCAL_AUTH | LOCAL_CX,
  "Show HDLC errors", "show hdlc"},
  {"ifilter", NULL, ShowIfilter, LOCAL_AUTH,
  "Show Input filters", "show ifilter option .."},
  {"ipcp", NULL, ReportIpcpStatus, LOCAL_AUTH,
  "Show IPCP status", "show ipcp"},
  {"lcp", NULL, lcp_ReportStatus, LOCAL_AUTH | LOCAL_CX,
  "Show LCP status", "show lcp"},
  {"links", "link", bundle_ShowLinks, LOCAL_AUTH,
  "Show available link names", "show links"},
  {"loopback", NULL, ShowLoopback, LOCAL_AUTH,
  "Show loopback setting", "show loopback"},
  {"log", NULL, ShowLogLevel, LOCAL_AUTH,
  "Show log levels", "show log"},
  {"mem", NULL, ShowMemMap, LOCAL_AUTH,
  "Show memory map", "show mem"},
  {"modem", NULL, modem_ShowStatus, LOCAL_AUTH | LOCAL_CX,
  "Show modem setups", "show modem"},
  {"mru", NULL, ShowInitialMRU, LOCAL_AUTH,
  "Show Initial MRU", "show mru"},
#ifndef NOMSEXT
  {"msext", NULL, ShowMSExt, LOCAL_AUTH,
  "Show MS PPP extentions", "show msext"},
#endif
  {"mtu", NULL, ShowPreferredMTU, LOCAL_AUTH,
  "Show Preferred MTU", "show mtu"},
  {"ofilter", NULL, ShowOfilter, LOCAL_AUTH,
  "Show Output filters", "show ofilter option .."},
  {"proto", NULL, Physical_ReportProtocolStatus, LOCAL_AUTH,
  "Show protocol summary", "show proto"},
  {"reconnect", NULL, ShowReconnect, LOCAL_AUTH | LOCAL_CX,
  "Show reconnect timer", "show reconnect"},
  {"redial", NULL, ShowRedial, LOCAL_AUTH | LOCAL_CX,
  "Show Redial timeout", "show redial"},
  {"route", NULL, ShowRoute, LOCAL_AUTH,
  "Show routing table", "show route"},
  {"timeout", NULL, ShowTimeout, LOCAL_AUTH,
  "Show Idle timeout", "show timeout"},
  {"stopped", NULL, ShowStopped, LOCAL_AUTH | LOCAL_CX,
  "Show STOPPED timeout", "show stopped"},
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
FindExec(struct bundle *bundle, struct cmdtab const *cmds, int argc,
         char const *const *argv, const char *prefix, struct datalink *cx)
{
  struct cmdtab const *cmd;
  int val = 1;
  int nmatch;
  struct cmdargs arg;

  cmd = FindCommand(cmds, *argv, &nmatch);
  if (nmatch > 1)
    LogPrintf(LogWARN, "%s%s: Ambiguous command\n", prefix, *argv);
  else if (cmd && (cmd->lauth & VarLocalAuth)) {
    if ((cmd->lauth & LOCAL_CX) && !cx)
      /* We've got no context, but we require it */
      cx = bundle2datalink(bundle, NULL);

    if ((cmd->lauth & LOCAL_CX) && !cx)
      LogPrintf(LogWARN, "%s%s: No context (use the `link' command)\n",
                prefix, *argv);
    else {
      if (cx && !(cmd->lauth & (LOCAL_CX|LOCAL_CX_OPT))) {
        LogPrintf(LogWARN, "%s%s: Redundant context (%s) ignored\n",
                  prefix, *argv, cx->name);
        cx = NULL;
      }
      arg.cmdtab = cmds;
      arg.cmd = cmd;
      arg.argc = argc-1;
      arg.argv = argv+1;
      arg.bundle = bundle;
      arg.cx = cx;
      val = (cmd->func) (&arg);
    }
  } else
    LogPrintf(LogWARN, "%s%s: Invalid command\n", prefix, *argv);

  if (val == -1)
    LogPrintf(LogWARN, "Usage: %s\n", cmd->syntax);
  else if (val)
    LogPrintf(LogWARN, "%s%s: Failed %d\n", prefix, *argv, val);

  return val;
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
RunCommand(struct bundle *bundle, int argc, char const *const *argv,
           const char *label)
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
    FindExec(bundle, Commands, argc, argv, "", NULL);
  }
}

void
DecodeCommand(struct bundle *bundle, char *buff, int nb, const char *label)
{
  int argc;
  char **argv;

  InterpretCommand(buff, nb, &argc, &argv);
  RunCommand(bundle, argc, (char const *const *)argv, label);
}

static int
ShowCommand(struct cmdargs const *arg)
{
  if (!prompt_Active(&prompt))
    LogPrintf(LogWARN, "show: Cannot show without a prompt\n");
  else if (arg->argc > 0)
    FindExec(arg->bundle, ShowCommands, arg->argc, arg->argv, "show ", arg->cx);
  else if (prompt_Active(&prompt))
    prompt_Printf(&prompt, "Use ``show ?'' to get a list.\n");
  else
    LogPrintf(LogWARN, "show command must have arguments\n");

  return 0;
}

static int
TerminalCommand(struct cmdargs const *arg)
{
  if (arg->cx->lcp.fsm.state > ST_CLOSED) {
    prompt_Printf(&prompt, "LCP state is [%s]\n",
                  StateNames[arg->cx->lcp.fsm.state]);
    return 1;
  }

  if (!IsInteractive(1))
    return (1);

  datalink_Up(arg->cx, 0, 0);
  prompt_Printf(&prompt, "Entering terminal mode.\n");
  prompt_Printf(&prompt, "Type `~?' for help.\n");
  prompt_TtyTermMode(&prompt, arg->cx);
  return 0;
}

static int
QuitCommand(struct cmdargs const *arg)
{
  if (prompt_Active(&prompt)) {
    prompt_Drop(&prompt, 1);
    if ((mode & MODE_INTER) ||
        (arg->argc > 0 && !strcasecmp(*arg->argv, "all") &&
         (VarLocalAuth & LOCAL_AUTH)))
      Cleanup(EX_NORMAL);
  }

  return 0;
}

static int
CloseCommand(struct cmdargs const *arg)
{
  bundle_Close(arg->bundle, arg->cx ? arg->cx->name : NULL, 1);
  return 0;
}

static int
DownCommand(struct cmdargs const *arg)
{
  link_Close(&arg->cx->physical->link, arg->bundle, 0, 1);
  return 0;
}

static int
SetModemSpeed(struct cmdargs const *arg)
{
  long speed;
  char *end;

  if (arg->argc > 0 && **arg->argv) {
    if (arg->argc > 1) {
      LogPrintf(LogWARN, "SetModemSpeed: Too many arguments");
      return -1;
    }
    if (strcasecmp(*arg->argv, "sync") == 0) {
      Physical_SetSync(arg->cx->physical);
      return 0;
    }
    end = NULL;
    speed = strtol(*arg->argv, &end, 10);
    if (*end) {
      LogPrintf(LogWARN, "SetModemSpeed: Bad argument \"%s\"", *arg->argv);
      return -1;
    }
    if (Physical_SetSpeed(arg->cx->physical, speed))
      return 0;
    LogPrintf(LogWARN, "%s: Invalid speed\n", *arg->argv);
  } else
    LogPrintf(LogWARN, "SetModemSpeed: No speed specified\n");

  return -1;
}

static int
SetReconnect(struct cmdargs const *arg)
{
  if (arg->argc == 2) {
    arg->cx->cfg.reconnect_timeout = atoi(arg->argv[0]);
    arg->cx->cfg.max_reconnect = (mode & MODE_DIRECT) ? 0 : atoi(arg->argv[1]);
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
      arg->cx->cfg.dial_timeout = -1;
      randinit();
    } else {
      timeout = atoi(arg->argv[0]);

      if (timeout >= 0)
	arg->cx->cfg.dial_timeout = timeout;
      else {
	LogPrintf(LogWARN, "Invalid redial timeout\n");
	return -1;
      }
    }

    dot = strchr(arg->argv[0], '.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
	arg->cx->cfg.dial_next_timeout = -1;
	randinit();
      } else {
	timeout = atoi(dot);
	if (timeout >= 0)
	  arg->cx->cfg.dial_next_timeout = timeout;
	else {
	  LogPrintf(LogWARN, "Invalid next redial timeout\n");
	  return -1;
	}
      }
    } else
      /* Default next timeout */
      arg->cx->cfg.dial_next_timeout = DIAL_NEXT_TIMEOUT;

    if (arg->argc == 2) {
      tries = atoi(arg->argv[1]);

      if (tries >= 0) {
	arg->cx->cfg.max_dial = tries;
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
  arg->cx->lcp.fsm.StoppedTimer.load = 0;
  arg->cx->ccp.fsm.StoppedTimer.load = 0;
  if (arg->argc <= 2) {
    if (arg->argc > 0) {
      arg->cx->lcp.fsm.StoppedTimer.load = atoi(arg->argv[0]) * SECTICKS;
      if (arg->argc > 1)
        arg->cx->ccp.fsm.StoppedTimer.load = atoi(arg->argv[1]) * SECTICKS;
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
      if (mask != NULL || passwd != NULL)
        return -1;

      if (ServerClose())
        LogPrintf(LogPHASE, "Disabled server port.\n");
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
  return arg->argc > 0 ? modem_SetParity(arg->cx->physical, *arg->argv) : -1;
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
    arg->cx->physical->async.cfg.EscMap[code] = 0;

  while (argc-- > 0) {
    sscanf(*argv++, "%x", &code);
    code &= 0xff;
    arg->cx->physical->async.cfg.EscMap[code >> 3] |= (1 << (code & 7));
    arg->cx->physical->async.cfg.EscMap[32] = 1;
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
SetTimeout(struct cmdargs const *arg)
{
  if (arg->argc > 0) {
    bundle_SetIdleTimer(arg->bundle, atoi(arg->argv[0]));
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
  IpcpInfo.cfg.my_range.ipaddr.s_addr = INADDR_ANY;
  IpcpInfo.cfg.peer_range.ipaddr.s_addr = INADDR_ANY;

  if (arg->argc > 4)
    return -1;

  IpcpInfo.cfg.HaveTriggerAddress = 0;
  ifnetmask.s_addr = 0;
  iplist_reset(&IpcpInfo.cfg.peer_list);

  if (arg->argc > 0) {
    if (!ParseAddr(arg->argc, arg->argv, &IpcpInfo.cfg.my_range.ipaddr,
		   &IpcpInfo.cfg.my_range.mask, &IpcpInfo.cfg.my_range.width))
      return 1;
    if (arg->argc > 1) {
      hisaddr = arg->argv[1];
      if (arg->argc > 2) {
	ifnetmask = GetIpAddr(arg->argv[2]);
	if (arg->argc > 3) {
	  IpcpInfo.cfg.TriggerAddress = GetIpAddr(arg->argv[3]);
	  IpcpInfo.cfg.HaveTriggerAddress = 1;
	}
      }
    }
  }

  /*
   * For backwards compatibility, 0.0.0.0 means any address.
   */
  if (IpcpInfo.cfg.my_range.ipaddr.s_addr == INADDR_ANY) {
    IpcpInfo.cfg.my_range.mask.s_addr = INADDR_ANY;
    IpcpInfo.cfg.my_range.width = 0;
  }
  IpcpInfo.my_ip.s_addr = IpcpInfo.cfg.my_range.ipaddr.s_addr;

  if (IpcpInfo.cfg.peer_range.ipaddr.s_addr == INADDR_ANY) {
    IpcpInfo.cfg.peer_range.mask.s_addr = INADDR_ANY;
    IpcpInfo.cfg.peer_range.width = 0;
  }

  if (hisaddr && !UseHisaddr(arg->bundle, hisaddr, mode & MODE_AUTO))
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
  SetMSEXT(&IpcpInfo.cfg.ns_entries[0], &IpcpInfo.cfg.ns_entries[1],
           arg->argc, arg->argv);
  return 0;
}

static int
SetNBNS(struct cmdargs const *arg)
{
  SetMSEXT(&IpcpInfo.cfg.nbns_entries[0], &IpcpInfo.cfg.nbns_entries[1],
           arg->argc, arg->argv);
  return 0;
}

#endif				/* MS_EXT */

static int
SetVariable(struct cmdargs const *arg)
{
  u_long map;
  const char *argp;
  int param = (int)arg->cmd->args;
  struct datalink *cx = arg->cx;

  if (arg->argc > 0)
    argp = *arg->argv;
  else
    argp = "";

  if ((arg->cmd->lauth & LOCAL_CX) && !cx) {
    LogPrintf(LogWARN, "set %s: No context (use the `link' command)\n",
              arg->cmd->name);
    return 1;
  } else if (cx && !(arg->cmd->lauth & (LOCAL_CX|LOCAL_CX_OPT))) {
    LogPrintf(LogWARN, "set %s: Redundant context (%s) ignored\n",
              arg->cmd->name, cx->name);
    cx = NULL;
  }

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
    if (!(mode & (MODE_DIRECT|MODE_DEDICATED))) {
      strncpy(cx->cfg.script.dial, argp, sizeof cx->cfg.script.dial - 1);
      cx->cfg.script.dial[sizeof cx->cfg.script.dial - 1] = '\0';
    }
    break;
  case VAR_LOGIN:
    if (!(mode & (MODE_DIRECT|MODE_DEDICATED))) {
      strncpy(cx->cfg.script.login, argp, sizeof cx->cfg.script.login - 1);
      cx->cfg.script.login[sizeof cx->cfg.script.login - 1] = '\0';
    }
    break;
  case VAR_DEVICE:
    Physical_SetDeviceList(cx->physical, argp);
    break;
  case VAR_ACCMAP:
    sscanf(argp, "%lx", &map);
    VarAccmap = map;
    break;
  case VAR_PHONE:
    strncpy(cx->cfg.phone.list, argp, sizeof cx->cfg.phone.list - 1);
    cx->cfg.phone.list[sizeof cx->cfg.phone.list - 1] = '\0';
    break;
  case VAR_HANGUP:
    if (!(mode & (MODE_DIRECT|MODE_DEDICATED))) {
      strncpy(cx->cfg.script.hangup, argp, sizeof cx->cfg.script.hangup - 1);
      cx->cfg.script.hangup[sizeof cx->cfg.script.hangup - 1] = '\0';
    }
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
	if (arg->argc > 1) {
	  LogPrintf(LogWARN, "SetCtsRts: Too many arguments\n");
	  return -1;
	}

    if (strcmp(*arg->argv, "on") == 0)
      Physical_SetRtsCts(bundle2physical(arg->bundle, NULL), 1);
    else if (strcmp(*arg->argv, "off") == 0)
      Physical_SetRtsCts(bundle2physical(arg->bundle, NULL), 0);
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
    if (strcasecmp(*arg->argv, "active") == 0)
      VarOpenMode = arg->argc > 1 ? atoi(arg->argv[1]) : 1;
    else if (strcasecmp(*arg->argv, "passive") == 0)
      VarOpenMode = OPEN_PASSIVE;
    else
      return -1;
    return 0;
  }
  return -1;
}

static struct cmdtab const SetCommands[] = {
  {"accmap", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "Set accmap value", "set accmap hex-value", (const void *) VAR_ACCMAP},
  {"afilter", NULL, SetAfilter, LOCAL_AUTH,
  "Set keep Alive filter", "set afilter ..."},
  {"authkey", "key", SetVariable, LOCAL_AUTH,
  "Set authentication key", "set authkey|key key", (const void *) VAR_AUTHKEY},
  {"authname", NULL, SetVariable, LOCAL_AUTH,
  "Set authentication name", "set authname name", (const void *) VAR_AUTHNAME},
  {"ctsrts", NULL, SetCtsRts, LOCAL_AUTH | LOCAL_CX,
  "Use CTS/RTS modem signalling", "set ctsrts [on|off]"},
  {"device", "line", SetVariable, LOCAL_AUTH | LOCAL_CX,
  "Set modem device name", "set device|line device-name[,device-name]",
  (const void *) VAR_DEVICE},
  {"dfilter", NULL, SetDfilter, LOCAL_AUTH,
  "Set demand filter", "set dfilter ..."},
  {"dial", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "Set dialing script", "set dial chat-script", (const void *) VAR_DIAL},
#ifdef HAVE_DES
  {"encrypt", NULL, SetVariable, LOCAL_AUTH, "Set CHAP encryption algorithm",
  "set encrypt MSChap|MD5", (const void *) VAR_ENC},
#endif
  {"escape", NULL, SetEscape, LOCAL_AUTH | LOCAL_CX,
  "Set escape characters", "set escape hex-digit ..."},
  {"hangup", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "Set hangup script", "set hangup chat-script", (const void *) VAR_HANGUP},
  {"ifaddr", NULL, SetInterfaceAddr, LOCAL_AUTH, "Set destination address",
  "set ifaddr [src-addr [dst-addr [netmask [trg-addr]]]]"},
  {"ifilter", NULL, SetIfilter, LOCAL_AUTH,
  "Set input filter", "set ifilter ..."},
  {"loopback", NULL, SetLoopback, LOCAL_AUTH,
  "Set loopback facility", "set loopback on|off"},
  {"log", NULL, SetLogLevel, LOCAL_AUTH,
  "Set log level", "set log [local] [+|-]value..."},
  {"login", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "Set login script", "set login chat-script", (const void *) VAR_LOGIN},
  {"mru", NULL, SetInitialMRU, LOCAL_AUTH,
  "Set Initial MRU value", "set mru value"},
  {"mtu", NULL, SetPreferredMTU, LOCAL_AUTH,
  "Set Preferred MTU value", "set mtu value"},
#ifndef NOMSEXT
  {"nbns", NULL, SetNBNS, LOCAL_AUTH,
  "Set NetBIOS NameServer", "set nbns pri-addr [sec-addr]"},
  {"ns", NULL, SetNS, LOCAL_AUTH,
  "Set NameServer", "set ns pri-addr [sec-addr]"},
#endif
  {"ofilter", NULL, SetOfilter, LOCAL_AUTH,
  "Set output filter", "set ofilter ..."},
  {"openmode", NULL, SetOpenMode, LOCAL_AUTH | LOCAL_CX,
  "Set open mode", "set openmode [active|passive]"},
  {"parity", NULL, SetModemParity, LOCAL_AUTH | LOCAL_CX,
  "Set modem parity", "set parity [odd|even|none]"},
  {"phone", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX, "Set telephone number(s)",
  "set phone phone1[:phone2[...]]", (const void *) VAR_PHONE},
  {"reconnect", NULL, SetReconnect, LOCAL_AUTH | LOCAL_CX,
  "Set Reconnect timeout", "set reconnect value ntries"},
  {"redial", NULL, SetRedialTimeout, LOCAL_AUTH | LOCAL_CX,
  "Set Redial timeout", "set redial value|random[.value|random] [attempts]"},
  {"stopped", NULL, SetStoppedTimeout, LOCAL_AUTH | LOCAL_CX,
  "Set STOPPED timeouts", "set stopped [LCPseconds [CCPseconds]]"},
  {"server", "socket", SetServer, LOCAL_AUTH,
  "Set server port", "set server|socket TcpPort|LocalName|none [mask]"},
  {"speed", NULL, SetModemSpeed, LOCAL_AUTH | LOCAL_CX,
  "Set modem speed", "set speed value"},
  {"timeout", NULL, SetTimeout, LOCAL_AUTH,
  "Set Idle timeout", "set timeout idle LQR FSM-resend"},
  {"vj", NULL, SetInitVJ, LOCAL_AUTH,
  "Set vj values", "set vj slots|slotcomp"},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "set help|? [command]", SetCommands},
  {NULL, NULL, NULL},
};

static int
SetCommand(struct cmdargs const *arg)
{
  if (arg->argc > 0)
    FindExec(arg->bundle, SetCommands, arg->argc, arg->argv, "set ", arg->cx);
  else if (prompt_Active(&prompt))
    prompt_Printf(&prompt, "Use `set ?' to get a list or `set ? <var>' for"
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
      dest = IpcpInfo.my_ip;
    else if (strcasecmp(arg->argv[0], "HISADDR") == 0)
      dest = IpcpInfo.peer_ip;
    else
      dest = GetIpAddr(arg->argv[0]);
    netmask = GetIpAddr(arg->argv[1]);
    gw = 2;
  }
  if (strcasecmp(arg->argv[gw], "HISADDR") == 0)
    gateway = IpcpInfo.peer_ip;
  else if (strcasecmp(arg->argv[gw], "INTERFACE") == 0)
    gateway.s_addr = INADDR_ANY;
  else
    gateway = GetIpAddr(arg->argv[gw]);
  bundle_SetRoute(arg->bundle, RTM_ADD, dest, gateway, netmask,
                  arg->cmd->args ? 1 : 0);
  return 0;
}

static int
DeleteCommand(struct cmdargs const *arg)
{
  struct in_addr dest, none;

  if (arg->argc == 1)
    if(strcasecmp(arg->argv[0], "all") == 0)
      DeleteIfRoutes(arg->bundle, 0);
    else {
      if (strcasecmp(arg->argv[0], "MYADDR") == 0)
        dest = IpcpInfo.my_ip;
      else if (strcasecmp(arg->argv[0], "default") == 0)
        dest.s_addr = INADDR_ANY;
      else
        dest = GetIpAddr(arg->argv[0]);
      none.s_addr = INADDR_ANY;
      bundle_SetRoute(arg->bundle, RTM_DELETE, dest, none, none,
                      arg->cmd->args ? 1 : 0);
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
    FindExec(arg->bundle, AliasCommands, arg->argc, arg->argv, "alias ", arg->cx);
  else if (prompt_Active(&prompt))
    prompt_Printf(&prompt, "Use `alias help' to get a list or `alias help"
            " <option>' for syntax help.\n");
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
  unsigned param = (unsigned)arg->cmd->args;
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
  /* arg->bundle may be NULL (see ValidSystem()) ! */
  if (arg->argc > 0)
    FindExec(arg->bundle, AllowCommands, arg->argc, arg->argv, "allow ", arg->cx);
  else if (prompt_Active(&prompt))
    prompt_Printf(&prompt, "Use `allow ?' to get a list or `allow ? <cmd>' for"
	    " syntax help.\n");
  else
    LogPrintf(LogWARN, "allow command must have arguments\n");

  return 0;
}

static int
LinkCommand(struct cmdargs const *arg)
{
  if (arg->argc > 1) {
    struct datalink *cx = bundle2datalink(arg->bundle, arg->argv[0]);
    if (cx)
      FindExec(arg->bundle, Commands, arg->argc - 1, arg->argv + 1, "", cx);
    else {
      LogPrintf(LogWARN, "link: %s: Invalid link name\n", arg->argv[0]);
      return 1;
    }
  } else {
    LogPrintf(LogWARN, "Usage: %s\n", arg->cmd->syntax);
    return 2;
  }

  return 0;
}
