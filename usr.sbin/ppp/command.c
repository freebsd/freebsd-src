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
 * $Id: command.c,v 1.149 1998/06/25 22:33:15 brian Exp $
 *
 */
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netdb.h>
#include <sys/un.h>

#ifndef NOALIAS
#include <alias.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "modem.h"
#ifndef NOALIAS
#include "alias_cmd.h"
#endif
#include "lqr.h"
#include "hdlc.h"
#include "loadalias.h"
#include "systems.h"
#include "filter.h"
#include "descriptor.h"
#include "main.h"
#include "route.h"
#include "ccp.h"
#include "auth.h"
#include "async.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#include "bundle.h"
#include "server.h"
#include "prompt.h"
#include "chat.h"
#include "chap.h"
#include "datalink.h"

/* ``set'' values */
#define	VAR_AUTHKEY	0
#define	VAR_DIAL	1
#define	VAR_LOGIN	2
#define	VAR_AUTHNAME	3
#define	VAR_AUTOLOAD	4
#define	VAR_WINSIZE	5
#define	VAR_DEVICE	6
#define	VAR_ACCMAP	7
#define	VAR_MRRU	8
#define	VAR_MRU		9
#define	VAR_MTU		10
#define	VAR_OPENMODE	11
#define	VAR_PHONE	12
#define	VAR_HANGUP	13
#define	VAR_IDLETIMEOUT	14
#define	VAR_LQRPERIOD	15
#define	VAR_LCPRETRY	16
#define	VAR_CHAPRETRY	17
#define	VAR_PAPRETRY	18
#define	VAR_CCPRETRY	19
#define	VAR_IPCPRETRY	20
#define	VAR_DNS		21
#define	VAR_NBNS	22
#define	VAR_MODE	23

/* ``accept|deny|disable|enable'' masks */
#define NEG_HISMASK (1)
#define NEG_MYMASK (2)

/* ``accept|deny|disable|enable'' values */
#define NEG_ACFCOMP	40
#define NEG_CHAP	41
#define NEG_DEFLATE	42
#define NEG_LQR		43
#define NEG_PAP		44
#define NEG_PPPDDEFLATE	45
#define NEG_PRED1	46
#define NEG_PROTOCOMP	47
#define NEG_SHORTSEQ	48
#define NEG_VJCOMP	49
#define NEG_DNS		50

const char Version[] = "2.0-beta";
const char VersionDate[] = "$Date: 1998/06/25 22:33:15 $";

static int ShowCommand(struct cmdargs const *);
static int TerminalCommand(struct cmdargs const *);
static int QuitCommand(struct cmdargs const *);
static int OpenCommand(struct cmdargs const *);
static int CloseCommand(struct cmdargs const *);
static int DownCommand(struct cmdargs const *);
static int AllowCommand(struct cmdargs const *);
static int SetCommand(struct cmdargs const *);
static int LinkCommand(struct cmdargs const *);
static int AddCommand(struct cmdargs const *);
static int DeleteCommand(struct cmdargs const *);
static int NegotiateCommand(struct cmdargs const *);
static int ClearCommand(struct cmdargs const *);
#ifndef NOALIAS
static int AliasCommand(struct cmdargs const *);
static int AliasEnable(struct cmdargs const *);
static int AliasOption(struct cmdargs const *);
#endif

static const char *
showcx(struct cmdtab const *cmd)
{
  if (cmd->lauth & LOCAL_CX)
    return "(c)";
  else if (cmd->lauth & LOCAL_CX_OPT)
    return "(o)";

  return "";
}

static int
HelpCommand(struct cmdargs const *arg)
{
  struct cmdtab const *cmd;
  int n, cmax, dmax, cols, cxlen;
  const char *cx;

  if (!arg->prompt) {
    log_Printf(LogWARN, "help: Cannot help without a prompt\n");
    return 0;
  }

  if (arg->argc > arg->argn) {
    for (cmd = arg->cmdtab; cmd->name || cmd->alias; cmd++)
      if ((cmd->lauth & arg->prompt->auth) &&
          ((cmd->name && !strcasecmp(cmd->name, arg->argv[arg->argn])) ||
           (cmd->alias && !strcasecmp(cmd->alias, arg->argv[arg->argn])))) {
	prompt_Printf(arg->prompt, "%s %s\n", cmd->syntax, showcx(cmd));
	return 0;
      }
    return -1;
  }

  cmax = dmax = 0;
  for (cmd = arg->cmdtab; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & arg->prompt->auth)) {
      if ((n = strlen(cmd->name) + strlen(showcx(cmd))) > cmax)
        cmax = n;
      if ((n = strlen(cmd->helpmes)) > dmax)
        dmax = n;
    }

  cols = 80 / (dmax + cmax + 3);
  n = 0;
  prompt_Printf(arg->prompt, "(o) = Optional context,"
                " (c) = Context required\n");
  for (cmd = arg->cmdtab; cmd->func; cmd++)
    if (cmd->name && (cmd->lauth & arg->prompt->auth)) {
      cx = showcx(cmd);
      cxlen = cmax - strlen(cmd->name);
      prompt_Printf(arg->prompt, " %s%-*.*s: %-*.*s",
              cmd->name, cxlen, cxlen, cx, dmax, dmax, cmd->helpmes);
      if (++n % cols == 0)
        prompt_Printf(arg->prompt, "\n");
    }
  if (n % cols != 0)
    prompt_Printf(arg->prompt, "\n");

  return 0;
}

static int
CloneCommand(struct cmdargs const *arg)
{
  char namelist[LINE_LEN];
  char *name;
  int f;

  if (arg->argc == arg->argn)
    return -1;

  namelist[sizeof namelist - 1] = '\0';
  for (f = arg->argn; f < arg->argc; f++) {
    strncpy(namelist, arg->argv[f], sizeof namelist - 1);
    for(name = strtok(namelist, ", "); name; name = strtok(NULL,", "))
      bundle_DatalinkClone(arg->bundle, arg->cx, name);
  }

  return 0;
}

static int
RemoveCommand(struct cmdargs const *arg)
{
  if (arg->argc != arg->argn)
    return -1;

  if (arg->cx->state != DATALINK_CLOSED) {
    log_Printf(LogWARN, "remove: Cannot delete links that aren't closed\n");
    return 2;
  }

  bundle_DatalinkRemove(arg->bundle, arg->cx);
  return 0;
}

static int
RenameCommand(struct cmdargs const *arg)
{
  if (arg->argc != arg->argn + 1)
    return -1;

  if (bundle_RenameDatalink(arg->bundle, arg->cx, arg->argv[arg->argn]))
    return 0;

  log_Printf(LogWARN, "%s -> %s: target name already exists\n", 
             arg->cx->name, arg->argv[arg->argn]);
  return 1;
}

int
LoadCommand(struct cmdargs const *arg)
{
  const char *name;

  if (arg->argc > arg->argn)
    name = arg->argv[arg->argn];
  else
    name = "default";

  if (!system_IsValid(name, arg->prompt, arg->bundle->phys_type.all)) {
    log_Printf(LogWARN, "%s: Label not allowed\n", name);
    return 1;
  } else {
    /*
     * Set the label before & after so that `set enddisc' works and
     * we handle nested `load' commands.
     */
    bundle_SetLabel(arg->bundle, arg->argc > arg->argn ? name : NULL);
    if (system_Select(arg->bundle, name, CONFFILE, arg->prompt, arg->cx) < 0) {
      bundle_SetLabel(arg->bundle, NULL);
      log_Printf(LogWARN, "%s: label not found.\n", name);
      return -1;
    }
    bundle_SetLabel(arg->bundle, arg->argc > arg->argn ? name : NULL);
  }
  return 0;
}

int
SaveCommand(struct cmdargs const *arg)
{
  log_Printf(LogWARN, "save command is not implemented (yet).\n");
  return 1;
}

static int
DialCommand(struct cmdargs const *arg)
{
  int res;

  if ((arg->cx && !(arg->cx->physical->type & (PHYS_INTERACTIVE|PHYS_AUTO)))
      || (!arg->cx &&
          (arg->bundle->phys_type.all & ~(PHYS_INTERACTIVE|PHYS_AUTO)))) {
    log_Printf(LogWARN, "Manual dial is only available for auto and"
              " interactive links\n");
    return 1;
  }

  if (arg->argc > arg->argn && (res = LoadCommand(arg)) != 0)
    return res;

  bundle_Open(arg->bundle, arg->cx ? arg->cx->name : NULL, PHYS_ALL);

  return 0;
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
  if (arg->prompt && arg->prompt->owner) {
    log_Printf(LogWARN, "Can't start a shell from a network connection\n");
    return 1;
  }
#endif

  if (arg->argc == arg->argn) {
    if (!arg->prompt) {
      log_Printf(LogWARN, "Can't start an interactive shell from"
                " a config file\n");
      return 1;
    } else if (arg->prompt->owner) {
      log_Printf(LogWARN, "Can't start an interactive shell from"
                " a socket connection\n");
      return 1;
    } else if (bg) {
      log_Printf(LogWARN, "Can only start an interactive shell in"
		" the foreground mode\n");
      return 1;
    }
  }

  if ((shpid = fork()) == 0) {
    int i, fd;

    if ((shell = getenv("SHELL")) == 0)
      shell = _PATH_BSHELL;

    timer_TermService();

    if (arg->prompt)
      fd = arg->prompt->fd_out;
    else if ((fd = open(_PATH_DEVNULL, O_RDWR)) == -1) {
      log_Printf(LogALERT, "Failed to open %s: %s\n",
                _PATH_DEVNULL, strerror(errno));
      exit(1);
    }
    for (i = 0; i < 3; i++)
      dup2(fd, i);

    fcntl(3, F_SETFD, 1);	/* Set close-on-exec flag */

    setuid(geteuid());
    if (arg->argc > arg->argn) {
      /* substitute pseudo args */
      argv[0] = strdup(arg->argv[arg->argn]);
      for (argc = 1; argc < arg->argc - arg->argn; argc++) {
	if (strcasecmp(arg->argv[argc + arg->argn], "HISADDR") == 0)
	  argv[argc] = strdup(inet_ntoa(arg->bundle->ncp.ipcp.peer_ip));
	else if (strcasecmp(arg->argv[argc + arg->argn], "INTERFACE") == 0)
	  argv[argc] = strdup(arg->bundle->ifp.Name);
	else if (strcasecmp(arg->argv[argc + arg->argn], "MYADDR") == 0)
	  argv[argc] = strdup(inet_ntoa(arg->bundle->ncp.ipcp.my_ip));
        else
          argv[argc] = strdup(arg->argv[argc + arg->argn]);
      }
      argv[argc] = NULL;
      if (bg) {
	pid_t p;

	p = getpid();
	if (daemon(1, 1) == -1) {
	  log_Printf(LogERROR, "%d: daemon: %s\n", (int)p, strerror(errno));
	  exit(1);
	}
      } else if (arg->prompt)
        printf("ppp: Pausing until %s finishes\n", arg->argv[arg->argn]);
      execvp(argv[0], argv);
    } else {
      if (arg->prompt)
        printf("ppp: Pausing until %s finishes\n", shell);
      prompt_TtyOldMode(arg->prompt);
      execl(shell, shell, NULL);
    }

    log_Printf(LogWARN, "exec() of %s failed\n",
              arg->argc > arg->argn ? arg->argv[arg->argn] : shell);
    exit(255);
  }

  if (shpid == (pid_t) - 1)
    log_Printf(LogERROR, "Fork failed: %s\n", strerror(errno));
  else {
    int status;
    waitpid(shpid, &status, 0);
  }

  if (arg->prompt && !arg->prompt->owner)
    prompt_TtyCommandMode(arg->prompt);

  return 0;
}

static int
BgShellCommand(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn)
    return -1;
  return ShellCommand(arg, 1);
}

static int
FgShellCommand(struct cmdargs const *arg)
{
  return ShellCommand(arg, 0);
}

static struct cmdtab const Commands[] = {
  {"accept", NULL, NegotiateCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "accept option request", "accept option .."},
  {"add", NULL, AddCommand, LOCAL_AUTH,
  "add route", "add dest mask gateway", NULL},
  {NULL, "add!", AddCommand, LOCAL_AUTH,
  "add or change route", "add! dest mask gateway", (void *)1},
#ifndef NOALIAS
  {"alias", NULL, AliasCommand, LOCAL_AUTH,
  "alias control", "alias option [yes|no]"},
#endif
  {"allow", "auth", AllowCommand, LOCAL_AUTH,
  "Allow ppp access", "allow users|modes ...."},
  {"bg", "!bg", BgShellCommand, LOCAL_AUTH,
  "Run a background command", "[!]bg command"},
  {"clear", NULL, ClearCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Clear throughput statistics", "clear ipcp|modem [current|overall|peak]..."},
  {"clone", NULL, CloneCommand, LOCAL_AUTH | LOCAL_CX,
  "Clone a link", "clone newname..."},
  {"close", NULL, CloseCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Close an FSM", "close [lcp|ccp]"},
  {"delete", NULL, DeleteCommand, LOCAL_AUTH,
  "delete route", "delete dest", NULL},
  {NULL, "delete!", DeleteCommand, LOCAL_AUTH,
  "delete a route if it exists", "delete! dest", (void *)1},
  {"deny", NULL, NegotiateCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Deny option request", "deny option .."},
  {"dial", "call", DialCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Dial and login", "dial|call [remote]"},
  {"disable", NULL, NegotiateCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Disable option", "disable option .."},
  {"down", NULL, DownCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Generate a down event", "down"},
  {"enable", NULL, NegotiateCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Enable option", "enable option .."},
  {"link", "datalink", LinkCommand, LOCAL_AUTH,
  "Link specific commands", "link name command ..."},
  {"load", NULL, LoadCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Load settings", "load [remote]"},
  {"open", NULL, OpenCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Open an FSM", "open [lcp|ccp|ipcp]"},
  {"passwd", NULL, PasswdCommand, LOCAL_NO_AUTH,
  "Password for manipulation", "passwd LocalPassword"},
  {"quit", "bye", QuitCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Quit PPP program", "quit|bye [all]"},
  {"remove", "rm", RemoveCommand, LOCAL_AUTH | LOCAL_CX,
  "Remove a link", "remove"},
  {"rename", "mv", RenameCommand, LOCAL_AUTH | LOCAL_CX,
  "Rename a link", "rename name"},
  {"save", NULL, SaveCommand, LOCAL_AUTH,
  "Save settings", "save"},
  {"set", "setup", SetCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Set parameters", "set[up] var value"},
  {"shell", "!", FgShellCommand, LOCAL_AUTH,
  "Run a subshell", "shell|! [sh command]"},
  {"show", NULL, ShowCommand, LOCAL_AUTH | LOCAL_CX_OPT,
  "Show status and stats", "show var"},
  {"term", NULL, TerminalCommand, LOCAL_AUTH | LOCAL_CX,
  "Enter terminal mode", "term"},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "help|? [command]", Commands},
  {NULL, NULL, NULL},
};

static int
ShowEscape(struct cmdargs const *arg)
{
  if (arg->cx->physical->async.cfg.EscMap[32]) {
    int code, bit;
    const char *sep = "";

    for (code = 0; code < 32; code++)
      if (arg->cx->physical->async.cfg.EscMap[code])
	for (bit = 0; bit < 8; bit++)
	  if (arg->cx->physical->async.cfg.EscMap[code] & (1 << bit)) {
	    prompt_Printf(arg->prompt, "%s0x%02x", sep, (code << 3) + bit);
            sep = ", ";
          }
    prompt_Printf(arg->prompt, "\n");
  }
  return 0;
}

static int
ShowTimerList(struct cmdargs const *arg)
{
  timer_Show(0, arg->prompt);
  return 0;
}

static int
ShowStopped(struct cmdargs const *arg)
{
  prompt_Printf(arg->prompt, " Stopped Timer:  LCP: ");
  if (!arg->cx->physical->link.lcp.fsm.StoppedTimer.load)
    prompt_Printf(arg->prompt, "Disabled");
  else
    prompt_Printf(arg->prompt, "%ld secs",
                  arg->cx->physical->link.lcp.fsm.StoppedTimer.load / SECTICKS);

  prompt_Printf(arg->prompt, ", CCP: ");
  if (!arg->cx->physical->link.ccp.fsm.StoppedTimer.load)
    prompt_Printf(arg->prompt, "Disabled");
  else
    prompt_Printf(arg->prompt, "%ld secs",
                  arg->cx->physical->link.ccp.fsm.StoppedTimer.load / SECTICKS);

  prompt_Printf(arg->prompt, "\n");

  return 0;
}

static int
ShowVersion(struct cmdargs const *arg)
{
  prompt_Printf(arg->prompt, "PPP Version %s - %s\n", Version, VersionDate);
  return 0;
}

static int
ShowProtocolStats(struct cmdargs const *arg)
{
  struct link *l = command_ChooseLink(arg);

  if (!l)
    return -1;
  prompt_Printf(arg->prompt, "%s:\n", l->name);
  link_ReportProtocolStatus(l, arg->prompt);
  return 0;
}

static struct cmdtab const ShowCommands[] = {
  {"bundle", NULL, bundle_ShowStatus, LOCAL_AUTH,
  "bundle details", "show bundle"},
  {"ccp", NULL, ccp_ReportStatus, LOCAL_AUTH | LOCAL_CX_OPT,
  "CCP status", "show cpp"},
  {"compress", NULL, sl_Show, LOCAL_AUTH,
  "VJ compression stats", "show compress"},
  {"escape", NULL, ShowEscape, LOCAL_AUTH | LOCAL_CX,
  "escape characters", "show escape"},
  {"filter", NULL, filter_Show, LOCAL_AUTH,
  "packet filters", "show filter [in|out|dial|alive]"},
  {"hdlc", NULL, hdlc_ReportStatus, LOCAL_AUTH | LOCAL_CX,
  "HDLC errors", "show hdlc"},
  {"ipcp", NULL, ipcp_Show, LOCAL_AUTH,
  "IPCP status", "show ipcp"},
  {"lcp", NULL, lcp_ReportStatus, LOCAL_AUTH | LOCAL_CX,
  "LCP status", "show lcp"},
  {"link", "datalink", datalink_Show, LOCAL_AUTH | LOCAL_CX,
  "(high-level) link info", "show link"},
  {"links", NULL, bundle_ShowLinks, LOCAL_AUTH,
  "available link names", "show links"},
  {"log", NULL, log_ShowLevel, LOCAL_AUTH,
  "log levels", "show log"},
  {"mem", NULL, mbuf_Show, LOCAL_AUTH,
  "mbuf allocations", "show mem"},
  {"modem", NULL, modem_ShowStatus, LOCAL_AUTH | LOCAL_CX,
  "(low-level) link info", "show modem"},
  {"mp", "multilink", mp_ShowStatus, LOCAL_AUTH,
  "multilink setup", "show mp"},
  {"proto", NULL, ShowProtocolStats, LOCAL_AUTH | LOCAL_CX_OPT,
  "protocol summary", "show proto"},
  {"route", NULL, route_Show, LOCAL_AUTH,
  "routing table", "show route"},
  {"stopped", NULL, ShowStopped, LOCAL_AUTH | LOCAL_CX,
  "STOPPED timeout", "show stopped"},
  {"timers", NULL, ShowTimerList, LOCAL_AUTH,
  "alarm timers", "show timers"},
  {"version", NULL, ShowVersion, LOCAL_NO_AUTH | LOCAL_AUTH,
  "version string", "show version"},
  {"who", NULL, log_ShowWho, LOCAL_AUTH,
  "client list", "show who"},
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

static const char *
mkPrefix(int argc, char const *const *argv, char *tgt, int sz)
{
  int f, tlen, len;

  tlen = 0;
  for (f = 0; f < argc && tlen < sz - 2; f++) {
    if (f)
      tgt[tlen++] = ' ';
    len = strlen(argv[f]);
    if (len > sz - tlen - 1)
      len = sz - tlen - 1;
    strncpy(tgt+tlen, argv[f], len);
    tlen += len;
  }
  tgt[tlen] = '\0';
  return tgt;
}

static int
FindExec(struct bundle *bundle, struct cmdtab const *cmds, int argc, int argn,
         char const *const *argv, struct prompt *prompt, struct datalink *cx)
{
  struct cmdtab const *cmd;
  int val = 1;
  int nmatch;
  struct cmdargs arg;
  char prefix[100];

  cmd = FindCommand(cmds, argv[argn], &nmatch);
  if (nmatch > 1)
    log_Printf(LogWARN, "%s: Ambiguous command\n",
              mkPrefix(argn+1, argv, prefix, sizeof prefix));
  else if (cmd && (!prompt || (cmd->lauth & prompt->auth))) {
    if ((cmd->lauth & LOCAL_CX) && !cx)
      /* We've got no context, but we require it */
      cx = bundle2datalink(bundle, NULL);

    if ((cmd->lauth & LOCAL_CX) && !cx)
      log_Printf(LogWARN, "%s: No context (use the `link' command)\n",
                mkPrefix(argn+1, argv, prefix, sizeof prefix));
    else {
      if (cx && !(cmd->lauth & (LOCAL_CX|LOCAL_CX_OPT))) {
        log_Printf(LogWARN, "%s: Redundant context (%s) ignored\n",
                  mkPrefix(argn+1, argv, prefix, sizeof prefix), cx->name);
        cx = NULL;
      }
      arg.cmdtab = cmds;
      arg.cmd = cmd;
      arg.argc = argc;
      arg.argn = argn+1;
      arg.argv = argv;
      arg.bundle = bundle;
      arg.cx = cx;
      arg.prompt = prompt;
      val = (*cmd->func) (&arg);
    }
  } else
    log_Printf(LogWARN, "%s: Invalid command\n",
              mkPrefix(argn+1, argv, prefix, sizeof prefix));

  if (val == -1)
    log_Printf(LogWARN, "Usage: %s\n", cmd->syntax);
  else if (val)
    log_Printf(LogWARN, "%s: Failed %d\n",
              mkPrefix(argn+1, argv, prefix, sizeof prefix), val);

  return val;
}

int
command_Interpret(char *buff, int nb, char *argv[MAXARGS])
{
  char *cp;

  if (nb > 0) {
    cp = buff + strcspn(buff, "\r\n");
    if (cp)
      *cp = '\0';
    return MakeArgs(buff, argv, MAXARGS);
  }
  return 0;
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

  /* set server port xxxxx .... */
  if (n == 3 && !strncasecmp(argv[0], "se", 2) &&
      !strncasecmp(argv[1], "se", 2))
    return 1;

  return 0;
}

void
command_Run(struct bundle *bundle, int argc, char const *const *argv,
           struct prompt *prompt, const char *label, struct datalink *cx)
{
  if (argc > 0) {
    if (log_IsKept(LogCOMMAND)) {
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
          strncpy(buf+n, "********", sizeof buf - n - 1);
        else
          strncpy(buf+n, argv[f], sizeof buf - n - 1);
        n += strlen(buf+n);
      }
      log_Printf(LogCOMMAND, "%s\n", buf);
    }
    FindExec(bundle, Commands, argc, 0, argv, prompt, cx);
  }
}

void
command_Decode(struct bundle *bundle, char *buff, int nb, struct prompt *prompt,
              const char *label)
{
  int argc;
  char *argv[MAXARGS];

  argc = command_Interpret(buff, nb, argv);
  command_Run(bundle, argc, (char const *const *)argv, prompt, label, NULL);
}

static int
ShowCommand(struct cmdargs const *arg)
{
  if (!arg->prompt)
    log_Printf(LogWARN, "show: Cannot show without a prompt\n");
  else if (arg->argc > arg->argn)
    FindExec(arg->bundle, ShowCommands, arg->argc, arg->argn, arg->argv,
             arg->prompt, arg->cx);
  else
    prompt_Printf(arg->prompt, "Use ``show ?'' to get a list.\n");

  return 0;
}

static int
TerminalCommand(struct cmdargs const *arg)
{
  if (!arg->prompt) {
    log_Printf(LogWARN, "term: Need a prompt\n");
    return 1;
  }

  if (arg->cx->physical->link.lcp.fsm.state > ST_CLOSED) {
    prompt_Printf(arg->prompt, "LCP state is [%s]\n",
                  State2Nam(arg->cx->physical->link.lcp.fsm.state));
    return 1;
  }

  datalink_Up(arg->cx, 0, 0);
  prompt_TtyTermMode(arg->prompt, arg->cx);
  return 0;
}

static int
QuitCommand(struct cmdargs const *arg)
{
  if (!arg->prompt || prompt_IsController(arg->prompt) ||
      (arg->argc > arg->argn && !strcasecmp(arg->argv[arg->argn], "all") &&
       (arg->prompt->auth & LOCAL_AUTH)))
    Cleanup(EX_NORMAL);
  if (arg->prompt)
    prompt_Destroy(arg->prompt, 1);

  return 0;
}

static int
OpenCommand(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn)
    bundle_Open(arg->bundle, arg->cx ? arg->cx->name : NULL, PHYS_ALL);
  else if (arg->argc == arg->argn + 1) {
    if (!strcasecmp(arg->argv[arg->argn], "lcp")) {
      if (arg->cx) {
        if (arg->cx->physical->link.lcp.fsm.state == ST_OPENED)
          fsm_Reopen(&arg->cx->physical->link.lcp.fsm);
        else
          bundle_Open(arg->bundle, arg->cx->name, PHYS_ALL);
      } else
        log_Printf(LogWARN, "open lcp: You must specify a link\n");
    } else if (!strcasecmp(arg->argv[arg->argn], "ccp")) {
      struct link *l;
      struct fsm *fp;

      if (!(l = command_ChooseLink(arg)))
        return -1;
      fp = &l->ccp.fsm;

      if (fp->link->lcp.fsm.state != ST_OPENED)
        log_Printf(LogWARN, "open: LCP must be open before opening CCP\n");
      else if (fp->state == ST_OPENED)
        fsm_Reopen(fp);
      else {
        fp->open_mode = 0;	/* Not passive any more */
        if (fp->state == ST_STOPPED) {
          fsm_Down(fp);
          fsm_Up(fp);
        } else {
          fsm_Up(fp);
          fsm_Open(fp);
        }
      }
    } else if (!strcasecmp(arg->argv[arg->argn], "ipcp")) {
      if (arg->cx)
        log_Printf(LogWARN, "open ipcp: You need not specify a link\n");
      if (arg->bundle->ncp.ipcp.fsm.state == ST_OPENED)
        fsm_Reopen(&arg->bundle->ncp.ipcp.fsm);
      else
        bundle_Open(arg->bundle, NULL, PHYS_ALL);
    } else
      return -1;
  } else
    return -1;

  return 0;
}

static int
CloseCommand(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn)
    bundle_Close(arg->bundle, arg->cx ? arg->cx->name : NULL, CLOSE_STAYDOWN);
  else if (arg->argc == arg->argn + 1) {
    if (!strcasecmp(arg->argv[arg->argn], "lcp"))
      bundle_Close(arg->bundle, arg->cx ? arg->cx->name : NULL, CLOSE_LCP);
    else if (!strcasecmp(arg->argv[arg->argn], "ccp") ||
             !strcasecmp(arg->argv[arg->argn], "ccp!")) {
      struct link *l;
      struct fsm *fp;

      if (!(l = command_ChooseLink(arg)))
        return -1;
      fp = &l->ccp.fsm;

      if (fp->state == ST_OPENED) {
        fsm_Close(fp);
        if (arg->argv[arg->argn][3] == '!')
          fp->open_mode = 0;		/* Stay ST_CLOSED */
        else
          fp->open_mode = OPEN_PASSIVE;	/* Wait for the peer to start */
      }
    } else
      return -1;
  } else
    return -1;

  return 0;
}

static int
DownCommand(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn) {
      if (arg->cx)
        datalink_Down(arg->cx, CLOSE_STAYDOWN);
      else
        bundle_Down(arg->bundle, CLOSE_STAYDOWN);
  } else if (arg->argc == arg->argn + 1) {
    if (!strcasecmp(arg->argv[arg->argn], "lcp")) {
      if (arg->cx)
        datalink_Down(arg->cx, CLOSE_LCP);
      else
        bundle_Down(arg->bundle, CLOSE_LCP);
    } else if (!strcasecmp(arg->argv[arg->argn], "ccp")) {
      struct fsm *fp = arg->cx ? &arg->cx->physical->link.ccp.fsm :
                                 &arg->bundle->ncp.mp.link.ccp.fsm;
      fsm2initial(fp);
    } else
      return -1;
  } else
    return -1;

  return 0;
}

static int
SetModemSpeed(struct cmdargs const *arg)
{
  long speed;
  char *end;

  if (arg->argc > arg->argn && *arg->argv[arg->argn]) {
    if (arg->argc > arg->argn+1) {
      log_Printf(LogWARN, "SetModemSpeed: Too many arguments");
      return -1;
    }
    if (strcasecmp(arg->argv[arg->argn], "sync") == 0) {
      physical_SetSync(arg->cx->physical);
      return 0;
    }
    end = NULL;
    speed = strtol(arg->argv[arg->argn], &end, 10);
    if (*end) {
      log_Printf(LogWARN, "SetModemSpeed: Bad argument \"%s\"",
                arg->argv[arg->argn]);
      return -1;
    }
    if (physical_SetSpeed(arg->cx->physical, speed))
      return 0;
    log_Printf(LogWARN, "%s: Invalid speed\n", arg->argv[arg->argn]);
  } else
    log_Printf(LogWARN, "SetModemSpeed: No speed specified\n");

  return -1;
}

static int
SetStoppedTimeout(struct cmdargs const *arg)
{
  struct link *l = &arg->cx->physical->link;

  l->lcp.fsm.StoppedTimer.load = 0;
  l->ccp.fsm.StoppedTimer.load = 0;
  if (arg->argc <= arg->argn+2) {
    if (arg->argc > arg->argn) {
      l->lcp.fsm.StoppedTimer.load = atoi(arg->argv[arg->argn]) * SECTICKS;
      if (arg->argc > arg->argn+1)
        l->ccp.fsm.StoppedTimer.load = atoi(arg->argv[arg->argn+1]) * SECTICKS;
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

  if (arg->argc > arg->argn && arg->argc < arg->argn+4) {
    const char *port, *passwd, *mask;

    /* What's what ? */
    port = arg->argv[arg->argn];
    if (arg->argc == arg->argn + 2) {
      passwd = arg->argv[arg->argn+1];
      mask = NULL;
    } else if (arg->argc == arg->argn + 3) {
      passwd = arg->argv[arg->argn+1];
      mask = arg->argv[arg->argn+2];
      if (!ismask(mask))
        return -1;
    } else if (strcasecmp(port, "none") == 0) {
      if (server_Close(arg->bundle))
        log_Printf(LogPHASE, "Disabled server port.\n");
      return 0;
    } else
      return -1;

    strncpy(server.passwd, passwd, sizeof server.passwd - 1);
    server.passwd[sizeof server.passwd - 1] = '\0';

    if (*port == '/') {
      mode_t imask;
      char *ptr, name[LINE_LEN + 12];

      if (mask != NULL) {
	unsigned m;

	if (sscanf(mask, "%o", &m) == 1)
	  imask = m;
        else
          return -1;
      } else
        imask = (mode_t)-1;

      ptr = strstr(port, "%d");
      if (ptr) {
        snprintf(name, sizeof name, "%.*s%d%s",
                 ptr - port, port, arg->bundle->unit, ptr + 2);
        port = name;
      }
      res = server_LocalOpen(arg->bundle, port, imask);
    } else {
      int iport, add = 0;

      if (mask != NULL)
        return -1;

      if (*port == '+') {
        port++;
        add = 1;
      }
      if (strspn(port, "0123456789") != strlen(port)) {
        struct servent *s;

        if ((s = getservbyname(port, "tcp")) == NULL) {
	  iport = 0;
	  log_Printf(LogWARN, "%s: Invalid port or service\n", port);
	} else
	  iport = ntohs(s->s_port);
      } else
        iport = atoi(port);

      if (iport) {
        if (add)
          iport += arg->bundle->unit;
        res = server_TcpOpen(arg->bundle, iport);
      } else
        res = -1;
    }
  }

  return res;
}

static int
SetModemParity(struct cmdargs const *arg)
{
  return arg->argc > arg->argn ? modem_SetParity(arg->cx->physical,
                                                 arg->argv[arg->argn]) : -1;
}

static int
SetEscape(struct cmdargs const *arg)
{
  int code;
  int argc = arg->argc - arg->argn;
  char const *const *argv = arg->argv + arg->argn;

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
  struct ipcp *ipcp = &arg->bundle->ncp.ipcp;
  const char *hisaddr;

  hisaddr = NULL;
  ipcp->cfg.my_range.ipaddr.s_addr = INADDR_ANY;
  ipcp->cfg.peer_range.ipaddr.s_addr = INADDR_ANY;

  if (arg->argc > arg->argn + 4)
    return -1;

  ipcp->cfg.HaveTriggerAddress = 0;
  ipcp->cfg.netmask.s_addr = INADDR_ANY;
  iplist_reset(&ipcp->cfg.peer_list);

  if (arg->argc > arg->argn) {
    if (!ParseAddr(ipcp, arg->argc - arg->argn, arg->argv + arg->argn,
                   &ipcp->cfg.my_range.ipaddr, &ipcp->cfg.my_range.mask,
                   &ipcp->cfg.my_range.width))
      return 1;
    if (arg->argc > arg->argn+1) {
      hisaddr = arg->argv[arg->argn+1];
      if (arg->argc > arg->argn+2) {
        ipcp->cfg.netmask = GetIpAddr(arg->argv[arg->argn+2]);
	if (arg->argc > arg->argn+3) {
	  ipcp->cfg.TriggerAddress = GetIpAddr(arg->argv[arg->argn+3]);
	  ipcp->cfg.HaveTriggerAddress = 1;
	}
      }
    }
  }

  /*
   * For backwards compatibility, 0.0.0.0 means any address.
   */
  if (ipcp->cfg.my_range.ipaddr.s_addr == INADDR_ANY) {
    ipcp->cfg.my_range.mask.s_addr = INADDR_ANY;
    ipcp->cfg.my_range.width = 0;
  }
  ipcp->my_ip.s_addr = ipcp->cfg.my_range.ipaddr.s_addr;

  if (ipcp->cfg.peer_range.ipaddr.s_addr == INADDR_ANY) {
    ipcp->cfg.peer_range.mask.s_addr = INADDR_ANY;
    ipcp->cfg.peer_range.width = 0;
  }

  if (hisaddr && !ipcp_UseHisaddr(arg->bundle, hisaddr,
                                  arg->bundle->phys_type.all & PHYS_AUTO))
    return 4;

  return 0;
}

static int
SetVariable(struct cmdargs const *arg)
{
  u_long ulong_val;
  const char *argp;
  int param = (int)arg->cmd->args, mode;
  struct datalink *cx = arg->cx;	/* LOCAL_CX uses this */
  const char *err = NULL;
  struct link *l = command_ChooseLink(arg);	/* LOCAL_CX_OPT uses this */
  int dummyint;
  struct in_addr dummyaddr, *addr;

  if (!l)
    return -1;

  if (arg->argc > arg->argn)
    argp = arg->argv[arg->argn];
  else
    argp = "";

  if ((arg->cmd->lauth & LOCAL_CX) && !cx) {
    log_Printf(LogWARN, "set %s: No context (use the `link' command)\n",
              arg->cmd->name);
    return 1;
  } else if (cx && !(arg->cmd->lauth & (LOCAL_CX|LOCAL_CX_OPT))) {
    log_Printf(LogWARN, "set %s: Redundant context (%s) ignored\n",
              arg->cmd->name, cx->name);
    cx = NULL;
  }

  switch (param) {
  case VAR_AUTHKEY:
    if (bundle_Phase(arg->bundle) == PHASE_DEAD) {
      strncpy(arg->bundle->cfg.auth.key, argp,
              sizeof arg->bundle->cfg.auth.key - 1);
      arg->bundle->cfg.auth.key[sizeof arg->bundle->cfg.auth.key - 1] = '\0';
    } else {
      err = "set authkey: Only available at phase DEAD\n";
      log_Printf(LogWARN, err);
    }
    break;
  case VAR_AUTHNAME:
    if (bundle_Phase(arg->bundle) == PHASE_DEAD) {
      strncpy(arg->bundle->cfg.auth.name, argp,
              sizeof arg->bundle->cfg.auth.name - 1);
      arg->bundle->cfg.auth.name[sizeof arg->bundle->cfg.auth.name - 1] = '\0';
    } else {
      err = "set authname: Only available at phase DEAD\n";
      log_Printf(LogWARN, err);
    }
    break;
  case VAR_AUTOLOAD:
    if (arg->argc == arg->argn + 2 || arg->argc == arg->argn + 4) {
      arg->bundle->autoload.running = 1;
      arg->bundle->cfg.autoload.max.timeout = atoi(arg->argv[arg->argn]);
      arg->bundle->cfg.autoload.max.packets = atoi(arg->argv[arg->argn + 1]);
      if (arg->argc == arg->argn + 4) {
        arg->bundle->cfg.autoload.min.timeout = atoi(arg->argv[arg->argn + 2]);
        arg->bundle->cfg.autoload.min.packets = atoi(arg->argv[arg->argn + 3]);
      } else {
        arg->bundle->cfg.autoload.min.timeout = 0;
        arg->bundle->cfg.autoload.min.packets = 0;
      }
    } else {
      err = "Set autoload requires two or four arguments\n";
      log_Printf(LogWARN, err);
    }
    break;
  case VAR_DIAL:
    strncpy(cx->cfg.script.dial, argp, sizeof cx->cfg.script.dial - 1);
    cx->cfg.script.dial[sizeof cx->cfg.script.dial - 1] = '\0';
    break;
  case VAR_LOGIN:
    strncpy(cx->cfg.script.login, argp, sizeof cx->cfg.script.login - 1);
    cx->cfg.script.login[sizeof cx->cfg.script.login - 1] = '\0';
    break;
  case VAR_WINSIZE:
    if (arg->argc > arg->argn) {
      l->ccp.cfg.deflate.out.winsize = atoi(arg->argv[arg->argn]);
      if (l->ccp.cfg.deflate.out.winsize < 8 ||
          l->ccp.cfg.deflate.out.winsize > 15) {
          log_Printf(LogWARN, "%d: Invalid outgoing window size\n",
                    l->ccp.cfg.deflate.out.winsize);
          l->ccp.cfg.deflate.out.winsize = 15;
      }
      if (arg->argc > arg->argn+1) {
        l->ccp.cfg.deflate.in.winsize = atoi(arg->argv[arg->argn+1]);
        if (l->ccp.cfg.deflate.in.winsize < 8 ||
            l->ccp.cfg.deflate.in.winsize > 15) {
            log_Printf(LogWARN, "%d: Invalid incoming window size\n",
                      l->ccp.cfg.deflate.in.winsize);
            l->ccp.cfg.deflate.in.winsize = 15;
        }
      } else
        l->ccp.cfg.deflate.in.winsize = 0;
    } else {
      err = "No window size specified\n";
      log_Printf(LogWARN, err);
    }
    break;
  case VAR_DEVICE:
    physical_SetDeviceList(cx->physical, arg->argc - arg->argn,
                           arg->argv + arg->argn);
    break;
  case VAR_ACCMAP:
    if (arg->argc > arg->argn) {
      sscanf(argp, "%lx", &ulong_val);
      cx->physical->link.lcp.cfg.accmap = ulong_val;
    } else {
      err = "No accmap specified\n";
      log_Printf(LogWARN, err);
    }
    break;
  case VAR_MODE:
    mode = Nam2mode(argp);
    if (mode == PHYS_NONE || mode == PHYS_ALL) {
      log_Printf(LogWARN, "%s: Invalid mode\n", argp);
      return -1;
    }
    bundle_SetMode(arg->bundle, cx, mode);
    break;
  case VAR_MRRU:
    if (bundle_Phase(arg->bundle) != PHASE_DEAD)
      log_Printf(LogWARN, "mrru: Only changable at phase DEAD\n");
    else {
      ulong_val = atol(argp);
      if (ulong_val && ulong_val < MIN_MRU)
        err = "Given MRRU value (%lu) is too small.\n";
      else if (ulong_val > MAX_MRU)
        err = "Given MRRU value (%lu) is too big.\n";
      else
        arg->bundle->ncp.mp.cfg.mrru = ulong_val;
      if (err)
        log_Printf(LogWARN, err, ulong_val);
    }
    break;
  case VAR_MRU:
    ulong_val = atol(argp);
    if (ulong_val < MIN_MRU)
      err = "Given MRU value (%lu) is too small.\n";
    else if (ulong_val > MAX_MRU)
      err = "Given MRU value (%lu) is too big.\n";
    else
      l->lcp.cfg.mru = ulong_val;
    if (err)
      log_Printf(LogWARN, err, ulong_val);
    break;
  case VAR_MTU:
    ulong_val = atol(argp);
    if (ulong_val == 0)
      arg->bundle->cfg.mtu = 0;
    else if (ulong_val < MIN_MTU)
      err = "Given MTU value (%lu) is too small.\n";
    else if (ulong_val > MAX_MTU)
      err = "Given MTU value (%lu) is too big.\n";
    else
      arg->bundle->cfg.mtu = ulong_val;
    if (err)
      log_Printf(LogWARN, err, ulong_val);
    break;
  case VAR_OPENMODE:
    if (strcasecmp(argp, "active") == 0)
      cx->physical->link.lcp.cfg.openmode = arg->argc > arg->argn+1 ?
        atoi(arg->argv[arg->argn+1]) : 1;
    else if (strcasecmp(argp, "passive") == 0)
      cx->physical->link.lcp.cfg.openmode = OPEN_PASSIVE;
    else {
      err = "%s: Invalid openmode\n";
      log_Printf(LogWARN, err, argp);
    }
    break;
  case VAR_PHONE:
    strncpy(cx->cfg.phone.list, argp, sizeof cx->cfg.phone.list - 1);
    cx->cfg.phone.list[sizeof cx->cfg.phone.list - 1] = '\0';
    break;
  case VAR_HANGUP:
    strncpy(cx->cfg.script.hangup, argp, sizeof cx->cfg.script.hangup - 1);
    cx->cfg.script.hangup[sizeof cx->cfg.script.hangup - 1] = '\0';
    break;
  case VAR_IDLETIMEOUT:
    if (arg->argc > arg->argn+1)
      err = "Too many idle timeout values\n";
    else if (arg->argc == arg->argn+1)
      bundle_SetIdleTimer(arg->bundle, atoi(argp));
    if (err)
      log_Printf(LogWARN, err);
    break;
  case VAR_LQRPERIOD:
    ulong_val = atol(argp);
    if (ulong_val <= 0) {
      err = "%s: Invalid lqr period\n";
      log_Printf(LogWARN, err, argp);
    } else
      l->lcp.cfg.lqrperiod = ulong_val;
    break;
  case VAR_LCPRETRY:
    ulong_val = atol(argp);
    if (ulong_val <= 0) {
      err = "%s: Invalid LCP FSM retry period\n";
      log_Printf(LogWARN, err, argp);
    } else
      cx->physical->link.lcp.cfg.fsmretry = ulong_val;
    break;
  case VAR_CHAPRETRY:
    ulong_val = atol(argp);
    if (ulong_val <= 0) {
      err = "%s: Invalid CHAP retry period\n";
      log_Printf(LogWARN, err, argp);
    } else
      cx->chap.auth.cfg.fsmretry = ulong_val;
    break;
  case VAR_PAPRETRY:
    ulong_val = atol(argp);
    if (ulong_val <= 0) {
      err = "%s: Invalid PAP retry period\n";
      log_Printf(LogWARN, err, argp);
    } else
      cx->pap.cfg.fsmretry = ulong_val;
    break;
  case VAR_CCPRETRY:
    ulong_val = atol(argp);
    if (ulong_val <= 0) {
      err = "%s: Invalid CCP FSM retry period\n";
      log_Printf(LogWARN, err, argp);
    } else
      l->ccp.cfg.fsmretry = ulong_val;
    break;
  case VAR_IPCPRETRY:
    ulong_val = atol(argp);
    if (ulong_val <= 0) {
      err = "%s: Invalid IPCP FSM retry period\n";
      log_Printf(LogWARN, err, argp);
    } else
      arg->bundle->ncp.ipcp.cfg.fsmretry = ulong_val;
    break;
  case VAR_NBNS:
  case VAR_DNS:
    if (param == VAR_DNS)
      addr = arg->bundle->ncp.ipcp.cfg.ns.dns;
    else
      addr = arg->bundle->ncp.ipcp.cfg.ns.nbns;

    addr[0].s_addr = addr[1].s_addr = INADDR_ANY;

    if (arg->argc > arg->argn) {
      ParseAddr(&arg->bundle->ncp.ipcp, 1, arg->argv + arg->argn,
                addr, &dummyaddr, &dummyint);
      if (arg->argc > arg->argn+1)
        ParseAddr(&arg->bundle->ncp.ipcp, 1, arg->argv + arg->argn + 1,
                  addr + 1, &dummyaddr, &dummyint);

      if (addr[1].s_addr == INADDR_ANY)
        addr[1].s_addr = addr[0].s_addr;
      if (addr[0].s_addr == INADDR_ANY)
        addr[0].s_addr = addr[1].s_addr;
    }
    break;
  }

  return err ? 1 : 0;
}

static int 
SetCtsRts(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn+1) {
    if (strcmp(arg->argv[arg->argn], "on") == 0)
      physical_SetRtsCts(arg->cx->physical, 1);
    else if (strcmp(arg->argv[arg->argn], "off") == 0)
      physical_SetRtsCts(arg->cx->physical, 0);
    else
      return -1;
    return 0;
  }
  return -1;
}

static struct cmdtab const SetCommands[] = {
  {"accmap", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "accmap value", "set accmap hex-value", (const void *)VAR_ACCMAP},
  {"authkey", "key", SetVariable, LOCAL_AUTH,
  "authentication key", "set authkey|key key", (const void *)VAR_AUTHKEY},
  {"authname", NULL, SetVariable, LOCAL_AUTH,
  "authentication name", "set authname name", (const void *)VAR_AUTHNAME},
  {"autoload", NULL, SetVariable, LOCAL_AUTH,
  "auto link [de]activation", "set autoload maxtime maxload mintime minload",
  (const void *)VAR_AUTOLOAD},
  {"ccpretry", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX_OPT,
  "FSM retry period", "set ccpretry value", (const void *)VAR_CCPRETRY},
  {"chapretry", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "CHAP retry period", "set chapretry value", (const void *)VAR_CHAPRETRY},
  {"ctsrts", "crtscts", SetCtsRts, LOCAL_AUTH | LOCAL_CX,
  "Use hardware flow control", "set ctsrts [on|off]"},
  {"deflate", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX_OPT,
  "deflate window sizes", "set deflate out-winsize in-winsize",
  (const void *) VAR_WINSIZE},
  {"device", "line", SetVariable, LOCAL_AUTH | LOCAL_CX,
  "modem device name", "set device|line device-name[,device-name]",
  (const void *) VAR_DEVICE},
  {"dial", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "dialing script", "set dial chat-script", (const void *) VAR_DIAL},
  {"dns", NULL, SetVariable, LOCAL_AUTH, "Domain Name Server",
  "set dns pri-addr [sec-addr]", (const void *)VAR_DNS},
  {"enddisc", NULL, mp_SetEnddisc, LOCAL_AUTH,
  "Endpoint Discriminator", "set enddisc [IP|magic|label|psn value]"},
  {"escape", NULL, SetEscape, LOCAL_AUTH | LOCAL_CX,
  "escape characters", "set escape hex-digit ..."},
  {"filter", NULL, filter_Set, LOCAL_AUTH,
  "packet filters", "set filter alive|dial|in|out rule-no permit|deny "
  "[src_addr[/width]] [dst_addr[/width]] [tcp|udp|icmp [src [lt|eq|gt port]] "
  "[dst [lt|eq|gt port]] [estab] [syn] [finrst]]"},
  {"hangup", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "hangup script", "set hangup chat-script", (const void *) VAR_HANGUP},
  {"ifaddr", NULL, SetInterfaceAddr, LOCAL_AUTH, "destination address",
  "set ifaddr [src-addr [dst-addr [netmask [trg-addr]]]]"},
  {"ipcpretry", NULL, SetVariable, LOCAL_AUTH,
  "FSM retry period", "set ipcpretry value", (const void *)VAR_IPCPRETRY},
  {"lcpretry", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "FSM retry period", "set lcpretry value", (const void *)VAR_LCPRETRY},
  {"log", NULL, log_SetLevel, LOCAL_AUTH, "log level",
  "set log [local] [+|-]async|ccp|chat|command|connect|debug|hdlc|id0|ipcp|"
  "lcp|lqm|phase|tcp/ip|timer|tun..."},
  {"login", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "login script", "set login chat-script", (const void *) VAR_LOGIN},
  {"lqrperiod", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX_OPT,
  "LQR period", "set lqrperiod value", (const void *)VAR_LQRPERIOD},
  {"mode", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX, "mode value",
  "set mode interactive|auto|ddial|background", (const void *)VAR_MODE},
  {"mrru", NULL, SetVariable, LOCAL_AUTH, "MRRU value",
  "set mrru value", (const void *)VAR_MRRU},
  {"mru", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX_OPT,
  "MRU value", "set mru value", (const void *)VAR_MRU},
  {"mtu", NULL, SetVariable, LOCAL_AUTH,
  "interface MTU value", "set mtu value", (const void *)VAR_MTU},
  {"nbns", NULL, SetVariable, LOCAL_AUTH, "NetBIOS Name Server",
  "set nbns pri-addr [sec-addr]", (const void *)VAR_NBNS},
  {"openmode", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX, "open mode",
  "set openmode active|passive [secs]", (const void *)VAR_OPENMODE},
  {"papretry", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX,
  "PAP retry period", "set papretry value", (const void *)VAR_PAPRETRY},
  {"parity", NULL, SetModemParity, LOCAL_AUTH | LOCAL_CX,
  "modem parity", "set parity [odd|even|none]"},
  {"phone", NULL, SetVariable, LOCAL_AUTH | LOCAL_CX, "telephone number(s)",
  "set phone phone1[:phone2[...]]", (const void *)VAR_PHONE},
  {"reconnect", NULL, datalink_SetReconnect, LOCAL_AUTH | LOCAL_CX,
  "Reconnect timeout", "set reconnect value ntries"},
  {"redial", NULL, datalink_SetRedial, LOCAL_AUTH | LOCAL_CX,
  "Redial timeout", "set redial value|random[.value|random] [attempts]"},
  {"server", "socket", SetServer, LOCAL_AUTH,
  "server port", "set server|socket TcpPort|LocalName|none password [mask]"},
  {"speed", NULL, SetModemSpeed, LOCAL_AUTH | LOCAL_CX,
  "modem speed", "set speed value"},
  {"stopped", NULL, SetStoppedTimeout, LOCAL_AUTH | LOCAL_CX,
  "STOPPED timeouts", "set stopped [LCPseconds [CCPseconds]]"},
  {"timeout", NULL, SetVariable, LOCAL_AUTH, "Idle timeout",
  "set timeout idletime", (const void *)VAR_IDLETIMEOUT},
  {"vj", NULL, ipcp_vjset, LOCAL_AUTH,
  "vj values", "set vj slots|slotcomp [value]"},
  {"weight", NULL, mp_SetDatalinkWeight, LOCAL_AUTH | LOCAL_CX,
  "datalink weighting", "set weight n"},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "set help|? [command]", SetCommands},
  {NULL, NULL, NULL},
};

static int
SetCommand(struct cmdargs const *arg)
{
  if (arg->argc > arg->argn)
    FindExec(arg->bundle, SetCommands, arg->argc, arg->argn, arg->argv,
             arg->prompt, arg->cx);
  else if (arg->prompt)
    prompt_Printf(arg->prompt, "Use `set ?' to get a list or `set ? <var>' for"
	    " syntax help.\n");
  else
    log_Printf(LogWARN, "set command must have arguments\n");

  return 0;
}


static int
AddCommand(struct cmdargs const *arg)
{
  struct in_addr dest, gateway, netmask;
  int gw, addrs;

  if (arg->argc != arg->argn+3 && arg->argc != arg->argn+2)
    return -1;

  addrs = 0;
  if (arg->argc == arg->argn+2) {
    if (!strcasecmp(arg->argv[arg->argn], "default"))
      dest.s_addr = netmask.s_addr = INADDR_ANY;
    else {
      int width;

      if (!ParseAddr(&arg->bundle->ncp.ipcp, 1, arg->argv + arg->argn,
	             &dest, &netmask, &width))
        return -1;
      if (!strncasecmp(arg->argv[arg->argn], "MYADDR", 6))
        addrs = ROUTE_DSTMYADDR;
      else if (!strncasecmp(arg->argv[arg->argn], "HISADDR", 7))
        addrs = ROUTE_DSTHISADDR;
    }
    gw = 1;
  } else {
    if (strcasecmp(arg->argv[arg->argn], "MYADDR") == 0) {
      addrs = ROUTE_DSTMYADDR;
      dest = arg->bundle->ncp.ipcp.my_ip;
    } else if (strcasecmp(arg->argv[arg->argn], "HISADDR") == 0) {
      addrs = ROUTE_DSTHISADDR;
      dest = arg->bundle->ncp.ipcp.peer_ip;
    } else
      dest = GetIpAddr(arg->argv[arg->argn]);
    netmask = GetIpAddr(arg->argv[arg->argn+1]);
    gw = 2;
  }

  if (strcasecmp(arg->argv[arg->argn+gw], "HISADDR") == 0) {
    gateway = arg->bundle->ncp.ipcp.peer_ip;
    addrs |= ROUTE_GWHISADDR;
  } else if (strcasecmp(arg->argv[arg->argn+gw], "INTERFACE") == 0)
    gateway.s_addr = INADDR_ANY;
  else
    gateway = GetIpAddr(arg->argv[arg->argn+gw]);

  if (bundle_SetRoute(arg->bundle, RTM_ADD, dest, gateway, netmask,
                  arg->cmd->args ? 1 : 0))
    route_Add(&arg->bundle->ncp.ipcp.route, addrs, dest, netmask, gateway);

  return 0;
}

static int
DeleteCommand(struct cmdargs const *arg)
{
  struct in_addr dest, none;
  int addrs;

  if (arg->argc == arg->argn+1) {
    if(strcasecmp(arg->argv[arg->argn], "all") == 0) {
      route_IfDelete(arg->bundle, 0);
      route_DeleteAll(&arg->bundle->ncp.ipcp.route);
    } else {
      addrs = 0;
      if (strcasecmp(arg->argv[arg->argn], "MYADDR") == 0) {
        dest = arg->bundle->ncp.ipcp.my_ip;
        addrs = ROUTE_DSTMYADDR;
      } else if (strcasecmp(arg->argv[arg->argn], "HISADDR") == 0) {
        dest = arg->bundle->ncp.ipcp.peer_ip;
        addrs = ROUTE_DSTHISADDR;
      } else {
        if (strcasecmp(arg->argv[arg->argn], "default") == 0)
          dest.s_addr = INADDR_ANY;
        else
          dest = GetIpAddr(arg->argv[arg->argn]);
        addrs = ROUTE_STATIC;
      }
      none.s_addr = INADDR_ANY;
      bundle_SetRoute(arg->bundle, RTM_DELETE, dest, none, none,
                      arg->cmd->args ? 1 : 0);
      route_Delete(&arg->bundle->ncp.ipcp.route, addrs, dest);
    }
  } else
    return -1;

  return 0;
}

#ifndef NOALIAS
static struct cmdtab const AliasCommands[] =
{
  {"addr", NULL, alias_RedirectAddr, LOCAL_AUTH,
   "static address translation", "alias addr [addr_local addr_alias]"},
  {"deny_incoming", NULL, AliasOption, LOCAL_AUTH,
   "stop incoming connections", "alias deny_incoming [yes|no]",
   (const void *) PKT_ALIAS_DENY_INCOMING},
  {"enable", NULL, AliasEnable, LOCAL_AUTH,
   "enable IP aliasing", "alias enable [yes|no]"},
  {"log", NULL, AliasOption, LOCAL_AUTH,
   "log aliasing link creation", "alias log [yes|no]",
   (const void *) PKT_ALIAS_LOG},
  {"port", NULL, alias_RedirectPort, LOCAL_AUTH,
   "port redirection", "alias port [proto addr_local:port_local  port_alias]"},
  {"same_ports", NULL, AliasOption, LOCAL_AUTH,
   "try to leave port numbers unchanged", "alias same_ports [yes|no]",
   (const void *) PKT_ALIAS_SAME_PORTS},
  {"unregistered_only", NULL, AliasOption, LOCAL_AUTH,
   "alias unregistered (private) IP address space only",
   "alias unregistered_only [yes|no]",
   (const void *) PKT_ALIAS_UNREGISTERED_ONLY},
  {"use_sockets", NULL, AliasOption, LOCAL_AUTH,
   "allocate host sockets", "alias use_sockets [yes|no]",
   (const void *) PKT_ALIAS_USE_SOCKETS},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
   "Display this message", "alias help|? [command]", AliasCommands},
  {NULL, NULL, NULL},
};


static int
AliasCommand(struct cmdargs const *arg)
{
  if (arg->argc > arg->argn)
    FindExec(arg->bundle, AliasCommands, arg->argc, arg->argn, arg->argv,
             arg->prompt, arg->cx);
  else if (arg->prompt)
    prompt_Printf(arg->prompt, "Use `alias help' to get a list or `alias help"
            " <option>' for syntax help.\n");
  else
    log_Printf(LogWARN, "alias command must have arguments\n");

  return 0;
}

static int
AliasEnable(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn+1) {
    if (strcasecmp(arg->argv[arg->argn], "yes") == 0) {
      arg->bundle->AliasEnabled = 1;
      return 0;
    } else if (strcasecmp(arg->argv[arg->argn], "no") == 0) {
      arg->bundle->AliasEnabled = 0;
      return 0;
    }
  }

  return -1;
}


static int
AliasOption(struct cmdargs const *arg)
{
  unsigned param = (unsigned)arg->cmd->args;
  if (arg->argc == arg->argn+1) {
    if (strcasecmp(arg->argv[arg->argn], "yes") == 0) {
      if (arg->bundle->AliasEnabled) {
	PacketAliasSetMode(param, param);
	return 0;
      }
      log_Printf(LogWARN, "alias not enabled\n");
    } else if (strcmp(arg->argv[arg->argn], "no") == 0) {
      if (arg->bundle->AliasEnabled) {
	PacketAliasSetMode(0, param);
	return 0;
      }
      log_Printf(LogWARN, "alias not enabled\n");
    }
  }
  return -1;
}
#endif /* #ifndef NOALIAS */

static struct cmdtab const AllowCommands[] = {
  {"modes", "mode", AllowModes, LOCAL_AUTH,
  "Only allow certain ppp modes", "allow modes mode..."},
  {"users", "user", AllowUsers, LOCAL_AUTH,
  "Allow users access to ppp", "allow users logname..."},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "allow help|? [command]", AllowCommands},
  {NULL, NULL, NULL},
};

static int
AllowCommand(struct cmdargs const *arg)
{
  /* arg->bundle may be NULL (see system_IsValid()) ! */
  if (arg->argc > arg->argn)
    FindExec(arg->bundle, AllowCommands, arg->argc, arg->argn, arg->argv,
             arg->prompt, arg->cx);
  else if (arg->prompt)
    prompt_Printf(arg->prompt, "Use `allow ?' to get a list or `allow ? <cmd>'"
                  " for syntax help.\n");
  else
    log_Printf(LogWARN, "allow command must have arguments\n");

  return 0;
}

static int
LinkCommand(struct cmdargs const *arg)
{
  if (arg->argc > arg->argn+1) {
    char namelist[LINE_LEN];
    struct datalink *cx;
    char *name;
    int result = 0;

    if (!strcmp(arg->argv[arg->argn], "*")) {
      struct datalink *dl;

      cx = arg->bundle->links;
      while (cx) {
        /* Watch it, the command could be a ``remove'' */
        dl = cx->next;
        FindExec(arg->bundle, Commands, arg->argc, arg->argn+1, arg->argv,
                 arg->prompt, cx);
        for (cx = arg->bundle->links; cx; cx = cx->next)
          if (cx == dl)
            break;		/* Pointer's still valid ! */
      }
    } else {
      strncpy(namelist, arg->argv[arg->argn], sizeof namelist - 1);
      namelist[sizeof namelist - 1] = '\0';
      for(name = strtok(namelist, ", "); name; name = strtok(NULL,", "))
        if (!bundle2datalink(arg->bundle, name)) {
          log_Printf(LogWARN, "link: %s: Invalid link name\n", name);
          return 1;
        }

      strncpy(namelist, arg->argv[arg->argn], sizeof namelist - 1);
      namelist[sizeof namelist - 1] = '\0';
      for(name = strtok(namelist, ", "); name; name = strtok(NULL,", ")) {
        cx = bundle2datalink(arg->bundle, name);
        if (cx)
          FindExec(arg->bundle, Commands, arg->argc, arg->argn+1, arg->argv,
                   arg->prompt, cx);
        else {
          log_Printf(LogWARN, "link: %s: Invalidated link name !\n", name);
          result++;
        }
      }
    }
    return result;
  }

  log_Printf(LogWARN, "Usage: %s\n", arg->cmd->syntax);
  return 2;
}

struct link *
command_ChooseLink(struct cmdargs const *arg)
{
  if (arg->cx)
    return &arg->cx->physical->link;
  else if (arg->bundle->ncp.mp.cfg.mrru)
    return &arg->bundle->ncp.mp.link;
  else {
    struct datalink *dl = bundle2datalink(arg->bundle, NULL);
    return dl ? &dl->physical->link : NULL;
  }
}

static const char *
ident_cmd(const char *cmd, unsigned *keep, unsigned *add)
{
  const char *result;

  switch (*cmd) {
    case 'A':
    case 'a':
      result = "accept";
      *keep = NEG_MYMASK;
      *add = NEG_ACCEPTED;
      break;
    case 'D':
    case 'd':
      switch (cmd[1]) {
        case 'E':
        case 'e':
          result = "deny";
          *keep = NEG_MYMASK;
          *add = 0;
          break;
        case 'I':
        case 'i':
          result = "disable";
          *keep = NEG_HISMASK;
          *add = 0;
          break;
        default:
          return NULL;
      }
      break;
    case 'E':
    case 'e':
      result = "enable";
      *keep = NEG_HISMASK;
      *add = NEG_ENABLED;
      break;
    default:
      return NULL;
  }

  return result;
}

static int
OptSet(struct cmdargs const *arg)
{
  int bit = (int)arg->cmd->args;
  const char *cmd;
  unsigned keep;			/* Keep these bits */
  unsigned add;				/* Add these bits */

  if ((cmd = ident_cmd(arg->argv[arg->argn-2], &keep, &add)) == NULL)
    return 1;

  if (add)
    arg->bundle->cfg.opt |= bit;
  else
    arg->bundle->cfg.opt &= ~bit;
  return 0;
}

static int
NegotiateSet(struct cmdargs const *arg)
{
  int param = (int)arg->cmd->args;
  struct link *l = command_ChooseLink(arg);	/* LOCAL_CX_OPT uses this */
  struct datalink *cx = arg->cx;	/* LOCAL_CX uses this */
  const char *cmd;
  unsigned keep;			/* Keep these bits */
  unsigned add;				/* Add these bits */

  if (!l)
    return -1;

  if ((cmd = ident_cmd(arg->argv[arg->argn-2], &keep, &add)) == NULL)
    return 1;

  if ((arg->cmd->lauth & LOCAL_CX) && !cx) {
    log_Printf(LogWARN, "%s %s: No context (use the `link' command)\n",
              cmd, arg->cmd->name);
    return 2;
  } else if (cx && !(arg->cmd->lauth & (LOCAL_CX|LOCAL_CX_OPT))) {
    log_Printf(LogWARN, "%s %s: Redundant context (%s) ignored\n",
              cmd, arg->cmd->name, cx->name);
    cx = NULL;
  }

  switch (param) {
    case NEG_ACFCOMP:
      cx->physical->link.lcp.cfg.acfcomp &= keep;
      cx->physical->link.lcp.cfg.acfcomp |= add;
      break;
    case NEG_CHAP:
      cx->physical->link.lcp.cfg.chap &= keep;
      cx->physical->link.lcp.cfg.chap |= add;
      break;
    case NEG_DEFLATE:
      l->ccp.cfg.neg[CCP_NEG_DEFLATE] &= keep;
      l->ccp.cfg.neg[CCP_NEG_DEFLATE] |= add;
      break;
    case NEG_DNS:
      arg->bundle->ncp.ipcp.cfg.ns.dns_neg &= keep;
      arg->bundle->ncp.ipcp.cfg.ns.dns_neg |= add;
      break;
    case NEG_LQR:
      cx->physical->link.lcp.cfg.lqr &= keep;
      cx->physical->link.lcp.cfg.lqr |= add;
      break;
    case NEG_PAP:
      cx->physical->link.lcp.cfg.pap &= keep;
      cx->physical->link.lcp.cfg.pap |= add;
      break;
    case NEG_PPPDDEFLATE:
      l->ccp.cfg.neg[CCP_NEG_DEFLATE24] &= keep;
      l->ccp.cfg.neg[CCP_NEG_DEFLATE24] |= add;
      break;
    case NEG_PRED1:
      l->ccp.cfg.neg[CCP_NEG_PRED1] &= keep;
      l->ccp.cfg.neg[CCP_NEG_PRED1] |= add;
      break;
    case NEG_PROTOCOMP:
      cx->physical->link.lcp.cfg.protocomp &= keep;
      cx->physical->link.lcp.cfg.protocomp |= add;
      break;
    case NEG_SHORTSEQ:
      if (bundle_Phase(arg->bundle) != PHASE_DEAD)
        log_Printf(LogWARN, "shortseq: Only changable at phase DEAD\n");
      else {
        arg->bundle->ncp.mp.cfg.shortseq &= keep;
        arg->bundle->ncp.mp.cfg.shortseq |= add;
      }
      break;
    case NEG_VJCOMP:
      arg->bundle->ncp.ipcp.cfg.vj.neg &= keep;
      arg->bundle->ncp.ipcp.cfg.vj.neg |= add;
      break;
  }

  return 0;
}

static struct cmdtab const NegotiateCommands[] = {
  {"idcheck", NULL, OptSet, LOCAL_AUTH, "Check FSM reply ids",
  "disable|enable", (const void *)OPT_IDCHECK},
  {"loopback", NULL, OptSet, LOCAL_AUTH, "Loop packets for local iface",
  "disable|enable", (const void *)OPT_LOOPBACK},
  {"passwdauth", NULL, OptSet, LOCAL_AUTH, "Use passwd file",
  "disable|enable", (const void *)OPT_PASSWDAUTH},
  {"proxy", NULL, OptSet, LOCAL_AUTH, "Create proxy ARP entry",
  "disable|enable", (const void *)OPT_PROXY},
  {"sroutes", NULL, OptSet, LOCAL_AUTH, "Use sticky routes",
  "disable|enable", (const void *)OPT_SROUTES},
  {"throughput", NULL, OptSet, LOCAL_AUTH, "Rolling throughput",
  "disable|enable", (const void *)OPT_THROUGHPUT},
  {"utmp", NULL, OptSet, LOCAL_AUTH, "Log connections in utmp",
  "disable|enable", (const void *)OPT_UTMP},

#define OPT_MAX 7	/* accept/deny allowed below and not above */

  {"acfcomp", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX,
  "Address & Control field compression", "accept|deny|disable|enable",
  (const void *)NEG_ACFCOMP},
  {"chap", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX,
  "Challenge Handshake Authentication Protocol", "accept|deny|disable|enable",
  (const void *)NEG_CHAP},
  {"deflate", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX_OPT,
  "Deflate compression", "accept|deny|disable|enable",
  (const void *)NEG_DEFLATE},
  {"deflate24", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX_OPT,
  "Deflate (type 24) compression", "accept|deny|disable|enable",
  (const void *)NEG_PPPDDEFLATE},
  {"dns", NULL, NegotiateSet, LOCAL_AUTH,
  "DNS specification", "accept|deny|disable|enable", (const void *)NEG_DNS},
  {"lqr", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX,
  "Link Quality Reports", "accept|deny|disable|enable",
  (const void *)NEG_LQR},
  {"pap", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX,
  "Password Authentication protocol", "accept|deny|disable|enable",
  (const void *)NEG_PAP},
  {"pred1", "predictor1", NegotiateSet, LOCAL_AUTH | LOCAL_CX_OPT,
  "Predictor 1 compression", "accept|deny|disable|enable",
  (const void *)NEG_PRED1},
  {"protocomp", NULL, NegotiateSet, LOCAL_AUTH | LOCAL_CX,
  "Protocol field compression", "accept|deny|disable|enable",
  (const void *)NEG_PROTOCOMP},
  {"shortseq", NULL, NegotiateSet, LOCAL_AUTH,
  "MP Short Sequence Numbers", "accept|deny|disable|enable",
  (const void *)NEG_SHORTSEQ},
  {"vjcomp", NULL, NegotiateSet, LOCAL_AUTH,
  "Van Jacobson header compression", "accept|deny|disable|enable",
  (const void *)NEG_VJCOMP},
  {"help", "?", HelpCommand, LOCAL_AUTH | LOCAL_NO_AUTH,
  "Display this message", "accept|deny|disable|enable help|? [value]",
  NegotiateCommands},
  {NULL, NULL, NULL},
};

static int
NegotiateCommand(struct cmdargs const *arg)
{
  if (arg->argc > arg->argn) {
    char const *argv[3];
    unsigned keep, add;
    int n;

    if ((argv[0] = ident_cmd(arg->argv[arg->argn-1], &keep, &add)) == NULL)
      return -1;
    argv[2] = NULL;

    for (n = arg->argn; n < arg->argc; n++) {
      argv[1] = arg->argv[n];
      FindExec(arg->bundle, NegotiateCommands + (keep == NEG_HISMASK ?
               0 : OPT_MAX), 2, 1, argv, arg->prompt, arg->cx);
    }
  } else if (arg->prompt)
    prompt_Printf(arg->prompt, "Use `%s ?' to get a list.\n",
	    arg->argv[arg->argn-1]);
  else
    log_Printf(LogWARN, "%s command must have arguments\n",
              arg->argv[arg->argn] );

  return 0;
}

const char *
command_ShowNegval(unsigned val)
{
  switch (val&3) {
    case 1: return "disabled & accepted";
    case 2: return "enabled & denied";
    case 3: return "enabled & accepted";
  }
  return "disabled & denied";
}

static int
ClearCommand(struct cmdargs const *arg)
{
  struct pppThroughput *t;
  struct datalink *cx;
  int i, clear_type;

  if (arg->argc < arg->argn + 1)
    return -1;

  if (strcasecmp(arg->argv[arg->argn], "modem") == 0) {
    cx = arg->cx;
    if (!cx)
      cx = bundle2datalink(arg->bundle, NULL);
    if (!cx) {
      log_Printf(LogWARN, "A link must be specified for ``clear modem''\n");
      return 1;
    }
    t = &cx->physical->link.throughput;
  } else if (strcasecmp(arg->argv[arg->argn], "ipcp") == 0)
    t = &arg->bundle->ncp.ipcp.throughput;
  else
    return -1;

  if (arg->argc > arg->argn + 1) {
    clear_type = 0;
    for (i = arg->argn + 1; i < arg->argc; i++)
      if (strcasecmp(arg->argv[i], "overall") == 0)
        clear_type |= THROUGHPUT_OVERALL;
      else if (strcasecmp(arg->argv[i], "current") == 0)
        clear_type |= THROUGHPUT_CURRENT;
      else if (strcasecmp(arg->argv[i], "peak") == 0)
        clear_type |= THROUGHPUT_PEAK;
      else
        return -1;
  } else 
    clear_type = THROUGHPUT_ALL;

  throughput_clear(t, clear_type, arg->prompt);
  return 0;
}
