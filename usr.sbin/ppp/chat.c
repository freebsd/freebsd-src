/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 *  Most of codes are derived from chat.c by Karl Fox (karl@MorningStar.Com).
 *
 *	Chat -- a program for automatic session establishment (i.e. dial
 *		the phone and log in).
 *
 *	This software is in the public domain.
 *
 *	Please send all bug reports, requests for information, etc. to:
 *
 *		Karl Fox <karl@MorningStar.Com>
 *		Morning Star Technologies, Inc.
 *		1760 Zollinger Road
 *		Columbus, OH  43221
 *		(614)451-1883
 *
 * $Id:$
 *
 *  TODO:
 *	o Support more UUCP compatible control sequences.
 *	o Dialing shoud not block monitor process.
 */
#include "defs.h"
#include <ctype.h>
#include <sys/uio.h>
#ifndef isblank
#define	isblank(c)	((c) == '\t' || (c) == ' ')
#endif
#include <sys/time.h>
#include <fcntl.h>
#include "timeout.h"
#include "vars.h"

static int TimeoutSec;
static int abort_next, timeout_next;
static int numaborts;
char *AbortStrings[50];

extern int ChangeParity(char *);

#define	MATCH	1
#define	NOMATCH	0
#define	ABORT	-1

static char *
findblank(p, instring)
char *p;
int instring;
{
  if (instring) {
    while (*p) {
      if (*p == '\\') {
	strcpy(p, p + 1);
	if (!*p)
	  break;
      } else if (*p == '"')
	return(p);
      p++;
    }
  } else {
    while (*p) {
      if (isblank(*p))
	return(p);
      p++;
    }
  }
  return p;
}

int
MakeArgs(script, pvect)
char *script;
char **pvect;
{
  int nargs, nb;
  int instring;

  nargs = 0;
  while (*script) {
    nb = strspn(script, " \t");
    script += nb;
    if (*script) {
      if (*script == '"') {
	instring = 1;
	script++;
	if (*script == '\0')
	  return(nargs);
      } else
	instring = 0;
      *pvect++ = script;
      nargs++;
      script = findblank(script, instring);
      if (*script)
	*script++ = '\0';
    }
  }
  *pvect = NULL;
  return nargs;
}

/*
 *  \r	Carrige return character
 *  \s  Space character
 *  \n  Line feed character
 *  \T  Telephone number (defined via `set phone'
 *  \t  Tab character
 */
char *
ExpandString(str, result, sendmode)
char *str;
char *result;
int sendmode;
{
  int addcr = 0;

  if (sendmode)
    addcr = 1;
  while (*str) {
    switch (*str) {
    case '\\':
      str++;
      switch (*str) {
      case 'c':
	if (sendmode)
	  addcr = 0;
	break;
      case 'd':		/* Delay 2 seconds */
        sleep(2); break;
      case 'p':
        usleep(250000); break;	/* Pause 0.25 sec */
      case 'n':
	*result++ = '\n'; break;
      case 'r':
	*result++ = '\r'; break;
      case 's':
	*result++ = ' '; break;
      case 't':
	*result++ = '\t'; break;
      case 'P':
	bcopy(VarAuthKey, result, strlen(VarAuthKey));
	result += strlen(VarAuthKey);
	break;
      case 'T':
	bcopy(VarPhone, result, strlen(VarPhone));
	result += strlen(VarPhone);
	break;
      case 'U':
	bcopy(VarAuthName, result, strlen(VarAuthName));
	result += strlen(VarAuthName);
	break;
      default:
	*result++ = *str; break;
      }
      if (*str) str++;
      break;
    case '^':
      str++;
      if (*str)
	*result++ = *str++ & 0x1f;
      break;
    default:
      *result++ = *str++;
      break;
    }
  }
  if (addcr)
    *result++ = '\r';
  *result++ = '\0';
  return(result);
}

int
WaitforString(estr)
char *estr;
{
#define	IBSIZE 200
  struct timeval timeout;
  char *s, *str, ch;
  char *inp;
  fd_set rfds;
  int i, nfds;
  char buff[200];
  char inbuff[IBSIZE];

  (void) ExpandString(estr, buff, 0);
  LogPrintf(LOG_CHAT, "Wait for (%d): %s --> %s\n", TimeoutSec, estr, buff);
  str = buff;
  inp = inbuff;

  nfds = modem + 1;
  s = str;
  for (;;) {
    FD_ZERO(&rfds);
    FD_SET(modem, &rfds);
    /*
     *  Because it is not clear whether select() modifies timeout value,
     *  it is better to initialize timeout values everytime.
     */
    timeout.tv_sec = TimeoutSec;
    timeout.tv_usec = 0;

    i = select(nfds, &rfds, NULL, NULL, &timeout);
#ifdef notdef
    TimerService();
#endif
    if (i < 0) {
      perror("select");
      return(NOMATCH);
    } else if (i == 0) { 	/* Timeout reached! */
      LogPrintf(LOG_CHAT, "can't get (%d).\n", timeout.tv_sec);
      return(NOMATCH);
    }
    if (FD_ISSET(modem, &rfds)) {	/* got something */
      read(modem, &ch, 1);
      *inp++ = ch;
      if (ch == *s) {
	s++;
	if (*s == '\0') {
	  return(MATCH);
	}
      } else {
	s = str;
	if (inp == inbuff+ IBSIZE) {
	  bcopy(inp - 100, inbuff, 100);
	  inp = inbuff + 100;
	}
	for (i = 0; i < numaborts; i++) {	/* Look for Abort strings */
	  int len;
	  char *s1;

	  s1 = AbortStrings[i];
	  len = strlen(s1);
	  if ((len <= inp - inbuff) && (strncmp(inp - len, s1, len) == 0)) {
	    LogPrintf(LOG_CHAT, "Abort: %s\n", s1);
	    return(ABORT);
	  }
	}
      }
    }
  }
}

void
SendString(str)
char *str;
{
  char buff[200];

  if (abort_next) {
    abort_next = 0;
    ExpandString(str, buff, 0);
    AbortStrings[numaborts++] = strdup(buff);
  } else if (timeout_next) {
    timeout_next = 0;
    TimeoutSec = atoi(str);
    if (TimeoutSec <= 0)
      TimeoutSec = 30;
  } else {
    (void) ExpandString(str, buff, 1);
    LogPrintf(LOG_CHAT, "sending: %s\n", buff);
    write(modem, buff, strlen(buff));
  }
}

int
ExpectString(str)
char *str;
{
  char *minus;
  int state;

  if (strcmp(str, "ABORT") == 0) {
    ++abort_next;
    return(MATCH);
  }
  if (strcmp(str, "TIMEOUT") == 0) {
    ++timeout_next;
    return(MATCH);
  }
  LogPrintf(LOG_CHAT, "Expecting %s\n", str);
  while (*str) {
    /*
     *  Check whether if string contains sub-send-expect.
     */
    for (minus = str; *minus; minus++) {
      if (*minus == '-') {
	if (minus == str || minus[-1] != '\\')
	  break;
      }
    }
    if (*minus == '-') {      /* We have sub-send-expect. */
      *minus++ = '\0';
      state = WaitforString(str);
      if (state != NOMATCH)
	return(state);
      /*
       * Can't get expect string. Sendout send part.
       */
      str = minus;
      for (minus = str; *minus; minus++) {
	if (*minus == '-') {
	  if (minus == str || minus[-1] != '\\')
	    break;
	}
      }
      if (*minus == '-') {
	*minus++ = '\0';
	SendString(str);
	str = minus;
      } else {
	SendString(str);
	return(MATCH);
      }
    } else {
      /*
       *  Simple case. Wait for string.
       */
      return(WaitforString(str));
    }
  }
  return(MATCH);
}

int
DoChat(script)
char *script;
{
  char *vector[20];
  char **argv;
  int argc, n, state;
#ifdef DEBUG
  int i;
#endif
  
  timeout_next = abort_next = 0;
  for (n = 0; AbortStrings[n]; n++) {
    free(AbortStrings[n]);
    AbortStrings[n] = NULL;
  }
  numaborts = 0;

  bzero(vector, sizeof(vector));
  n = MakeArgs(script, &vector);
#ifdef DEBUG
  logprintf("n = %d\n", n);
  for (i = 0; i < n; i++)
    logprintf("  %s\n", vector[i]);
#endif
  argc = n;
  argv = vector;
  TimeoutSec = 30;
  while (*argv) {
    if (strcmp(*argv, "P_ZERO") == 0 ||
	strcmp(*argv, "P_ODD") == 0 || strcmp(*argv, "P_EVEN") == 0) {
      ChangeParity(*argv++);
      continue;
    }
    state = ExpectString(*argv++);
    switch (state) {
    case MATCH:
      if (*argv)
	SendString(*argv++);
      break;
    case ABORT:
#ifdef notdef
      HangupModem();
#endif
    case NOMATCH:
      return(NOMATCH);
    }
  }
  return(MATCH);
}
