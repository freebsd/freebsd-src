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
 * $Id:$
 *
 */
#include <ctype.h>
#include "fsm.h"
#include "phase.h"
#include "lcp.h"
#include "ipcp.h"
#include "modem.h"
#include "command.h"
#include "hdlc.h"
#include "vars.h"
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/route.h>
#include "os.h"

extern int  MakeArgs();
extern void Cleanup(), TtyTermMode(), PacketMode();
extern int  EnableCommand(), DisableCommand(), DisplayCommand();
extern int  AcceptCommand(), DenyCommand();
extern int  LoadCommand(), SaveCommand();
extern int  ChangeParity(char *);
extern int  SelectSystem();
extern int  ShowRoute();

struct in_addr ifnetmask;

static int ShowCommand(), TerminalCommand(), QuitCommand();
static int CloseCommand(), DialCommand(), DownCommand();
static int SetCommand(), AddCommand(), DeleteCommand();

static int
HelpCommand(list, argc, argv, plist)
struct cmdtab *list;
int argc;
char **argv;
struct cmdtab *plist;
{
  struct cmdtab *cmd;
  int n;
  char c;

  if (argc > 0) {
    for (cmd = plist; cmd->name; cmd++) {
      if (strcmp(cmd->name, *argv) == 0) {
        printf("%s %s\n", cmd->name, cmd->syntax);
        return(1);
      }
    }
    return(1);
  }
  n = 0;
  for (cmd = plist; cmd->func; cmd++) {
    c = (n & 1)? '\n' : '\t';
    if (cmd->name) {
      printf("  %-8s: %-20s%c", cmd->name, cmd->helpmes, c);
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

  if (mode & MODE_AUTO)
    mes = "Working as auto mode.";
  else if (mode & MODE_DIRECT)
    mes = "Working as direct mode.";
  else if (mode & MODE_DEDICATED)
    mes = "Workring as dedicated mode.";
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
  if (LcpFsm.state > ST_CLOSED) {
    printf("LCP state is [%s]\n", StateNames[LcpFsm.state]);
    return(1);
  }
  if (!IsInteractive())
    return(1);
  modem = OpenModem(mode);
  if (modem < 0) {
    printf("failed to open modem.\n");
    modem = 0;
    return(1);
  }
  if (argc > 0) {
    if (SelectSystem(*argv, CONFFILE) < 0) {
      printf("%s: not found.\n", *argv);
      return(1);
    }
  }
  if (DialModem()) {
    sleep(1);
    ModemTimeout();
    PacketMode();
  }
  return(1);
}

static char StrOption[] = "option ..";
static char StrRemote[] = "[remote]";
char StrNull[] = "";

struct cmdtab Commands[] = {
  { "accept",  NULL,    AcceptCommand,
  	"accept option request",	StrOption },
  { "add",     NULL,	AddCommand,
	"add route",			"dest mask gateway" },
  { "close",   NULL,    CloseCommand,
	"Close connection",		StrNull },
  { "delete",  NULL,    DeleteCommand,
  	"delete route",			"dest gateway" },
  { "deny",    NULL,    DenyCommand,
  	"Deny option request",		StrOption },
  { "dial",    "call",  DialCommand,
  	"Dial and login",		StrRemote },
  { "disable", NULL,    DisableCommand,
  	"Disable option",		StrOption },
  { "display", NULL,    DisplayCommand,
  	"Display option configs",	StrNull },
  { "enable",  NULL,    EnableCommand,
  	"Enable option",		StrOption },
  { "load",    NULL,    LoadCommand,
  	"Load settings",		StrRemote },
  { "save",    NULL,    SaveCommand,
  	"Save settings", StrNull },
  { "set",     "setup", SetCommand,
  	"Set parameters",  "var value" },
  { "show",    NULL,    ShowCommand,
  	"Show status and statictics", "var" },
  { "term",    NULL,    TerminalCommand,
  	"Enter to terminal mode", StrNull },
  { "quit",    "bye",   QuitCommand,
  	"Quit PPP program", StrNull },
  { "help",    "?",     HelpCommand,
	"Display this message", "[command]", (void *)Commands },
  { NULL,      "down",  DownCommand,
  	"Generate down event",		StrNull },
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
  LM_PHASE, LM_CHAT, LM_LQM,   LM_LCP,
  LM_TCPIP, LM_HDLC, LM_ASYNC,
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
  printf(" Idle Timer: %d secs   LQR Timer: %d secs\n",
    VarIdleTimeout, VarLqrTimeout);
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
  extern char *VarVersion[];

  printf("%s\n", VarVersion);
  return(1);
}

static int ShowLogList()
{
  ListLog();
  return(1);
}

extern int ShowIfilter(), ShowOfilter(), ShowDfilter();

struct cmdtab ShowCommands[] = {
  { "auth",     NULL,     ShowAuthKey,       "Show auth name/key",
	StrNull, },
  { "ccp",      NULL,     ReportCcpStatus,   "Show CCP status",
	StrNull, },
  { "compress", NULL,     ReportCompress,    "Show compression statictics",
	StrNull },
  { "debug",	NULL,	  ShowDebugLevel,    "Show current debug level",
	StrNull },
  { "dfilter",  NULL,     ShowDfilter,       "Show Demand filters",
	StrOption },
  { "escape",   NULL,     ShowEscape,        "Show escape characters",
	StrNull },
  { "hdlc",	NULL,	  ReportHdlcStatus,  "Show HDLC error summary",
  	StrNull },
  { "ifilter",  NULL,     ShowIfilter,       "Show Input filters",
	StrOption },
  { "ipcp",     NULL,     ReportIpcpStatus,  "Show IPCP status",
	StrNull, },
  { "lcp",      NULL,     ReportLcpStatus,   "Show LCP status",
	StrNull, },
  { "log",      NULL,     ShowLogList,       "Show log records",
	StrNull, },
  { "mem",      NULL,     ShowMemMap,        "Show memory map",
	StrNull, },
  { "modem",    NULL,     ShowModemStatus,   "Show modem setups",
	StrNull, },
  { "ofilter",  NULL,     ShowOfilter,       "Show Output filters",
	StrOption },
  { "proto",    NULL,     ReportProtStatus,  "Show protocol summary",
	StrNull, },
  { "route",    NULL,     ShowRoute,	     "Show routing table",
	StrNull, },
  { "timeout",  NULL,	  ShowTimeout,       "Show Idle timeout value",
	StrNull, },
  { "version",  NULL,	  ShowVersion,       "Show version string",
	StrNull, },
  { "help",     "?",      HelpCommand,       "Display this message",
	StrNull, (void *)ShowCommands },
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
    if (cmds->name && strncmp(str, cmds->name, len) == 0) {
      nmatch++;
      found = cmds;
    } else if (cmds->alias && strncmp(str, cmds->alias, len) == 0) {
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
    printf("Anbiguous.\n");
  else if (cmd)
    val = (cmd->func)(cmd, --argc, ++argv, cmd->args);
  else
    printf("what?\n");
  return(val);
}

void
Prompt(flag)
int flag;
{
  if (!(mode & MODE_INTER))
    return;
  if (flag) printf("\n");
  if (IpcpFsm.state == ST_OPENED && phase == PHASE_NETWORK)
    printf("PPP> ");
  else
    printf("ppp> ");
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
      argc = MakeArgs(buff, &vector);
      argv = vector;

      if (argc > 0)
        val = FindExec(Commands, argc, argv);
    }
  }
  if (val && prompt)
    Prompt(0);
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
    modem = 0;
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
    if (argc > 0) {
      Cleanup(EX_NORMAL);
    } else {
      close(netfd);
      close(1);
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
  LcpClose();
  return(1);
}

static int
DownCommand()
{
  LcpDown();
  return(1);
}

static int validspeed[] = {
  1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 0
};

static int SetModemSpeed(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  int speed;
  int *sp;

  if (argc > 0) {
    speed = atoi(*argv);
    for (sp = validspeed; *sp; sp++) {
      if (*sp == speed) {
	VarSpeed = speed;
	return(1);
      }
    }
    printf("invalid speed.\n");
  }
  return(1);
}

static int SetModemParity(list, argc, argv)
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
    if (argc > 0)
      VarLqrTimeout = atoi(*argv);
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
  int width;

  DefMyAddress.ipaddr.s_addr = DefHisAddress.ipaddr.s_addr = 0L;
  if (argc > 0) {
    ParseAddr(argc, argv++,
      &DefMyAddress.ipaddr, &DefMyAddress.mask, &DefMyAddress.width);
    if (--argc > 0) {
      ParseAddr(argc, argv++,
	&DefHisAddress.ipaddr, &DefHisAddress.mask, &DefHisAddress.width);
      if (--argc > 0) {
        ifnetmask = GetIpAddr(*argv);
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

  if ((mode & MODE_AUTO) |
	((mode & MODE_DEDICATED) && dstsystem)) {
    OsSetIpaddress(DefMyAddress.ipaddr, DefHisAddress.ipaddr, ifnetmask);
  }
  return(1);
}


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
      break;
    case VAR_AUTHNAME:
      strncpy(VarAuthName, *argv, sizeof(VarAuthName)-1);
      break;
    case VAR_DIAL:
      strncpy(VarDialScript, *argv, sizeof(VarDialScript)-1);
      break;
    case VAR_LOGIN:
      strncpy(VarLoginScript, *argv, sizeof(VarDialScript)-1);
      break;
    case VAR_DEVICE:
      strncpy(VarDevice, *argv, sizeof(VarDevice)-1);
      break;
    case VAR_ACCMAP:
      sscanf(*argv, "%x", &map);
      VarAccmap = map;
      break;
    case VAR_PHONE:
      strncpy(VarPhone, *argv, sizeof(VarPhone)-1);
      break;
    }
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

extern int SetIfilter(), SetOfilter(), SetDfilter();

struct cmdtab SetCommands[] = {
  { "accmap",   NULL,	  SetVariable,	  "Set accmap value",
	"hex-value",	(void *)VAR_ACCMAP },
  { "authkey",  "key",     SetVariable,	  "Set authentication key",
	"key",		(void *)VAR_AUTHKEY },
  { "authname", NULL,     SetVariable,    "Set authentication name",
	"name",		(void *)VAR_AUTHNAME },
  { "debug",    NULL,	  SetDebugLevel,  "Set debug level",
	StrValue },
  { "device",     "line", SetVariable, "Set modem device name",
	"device-name",	(void *)VAR_DEVICE },
  { "dfilter",  NULL,     SetDfilter,  "Set demand filter",
        "..." },
  { "dial",     NULL,     SetVariable, 	 "Set dialing script",
	StrChatStr,	(void *)VAR_DIAL },
  { "escape",   NULL,	  SetEscape,       "Set escape characters",
        "hex-digit ..."},
  { "ifaddr",   NULL,   SetInterfaceAddr,  "Set destination address",
	"src-addr dst-addr netmask" },
  { "ifilter",  NULL,     SetIfilter,  "Set input filter",
        "..." },
  { "login",    NULL,     SetVariable,	"Set login script",
	 StrChatStr,	(void *)VAR_LOGIN },
  { "mru",      "mtu",    SetInitialMRU,  "Set Initial MRU value",
	StrValue },
  { "ofilter",  NULL,	  SetOfilter,	  "Set output filter",
        "..." },
  { "openmode", NULL,	  SetOpenMode,	  "Set open mode",
	"[active|passive]" },
  { "parity",   NULL,     SetModemParity, "Set modem parity",
	"[odd|even|none]" },
  { "phone",    NULL,     SetVariable,	  "Set telephone number",
        "phone-number",	(void *)VAR_PHONE },
  { "speed",    NULL,     SetModemSpeed,  "Set modem speed",
	"speed" },
  { "timeout",  NULL,     SetIdleTimeout, "Set Idle timeout",
	StrValue },
  { "help",     "?",      HelpCommand,    "Display this message",
	StrNull, (void *)SetCommands },
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
    printf("Use ``set ?'' to get a list.\n");
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
    if (strcmp(argv[2], "HISADDR") == 0)
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
    if (strcmp(argv[1], "HISADDR") == 0)
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
  } else if (argc == 1 && strcmp(argv[0], "ALL") == 0) {
    DeleteIfRoutes(0);
  } else {
    printf("Usage: %s %s\n", list->name, list->syntax);
  }
  return(1);
}

