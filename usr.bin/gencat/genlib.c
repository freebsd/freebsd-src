/* -*-c++-*- */


/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/

/* Edit History

02/25/91   5 nazgul	Added flag for MS byteorder
01/14/91   4 nazgul	Off by one on number specified entries
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef SYSV
#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#include <memory.h>
static int bcopy(src, dst, length)
char *src, *dst;
int length;
{
    memcpy(dst, src, length);
}
static int bzero(b, length)
char *b;
int length;
{
    memset(b, '\0', length);
}
#endif
#include <sys/file.h>
#include <ctype.h>
#include <err.h>
#include "msgcat.h"
#include "gencat.h"

static char *curline = NULL;
static long lineno = 0;

static void warning(cptr, msg)
char *cptr;
char *msg;
{
    warnx("%s on line %d\n%s", msg, lineno, curline);
    if (cptr) {
	char	*tptr;
	for (tptr = curline; tptr < cptr; ++tptr) putc(' ', stderr);
	fprintf(stderr, "^\n");
    }
}

static void error(cptr, msg)
char *cptr;
char *msg;
{
    warning(cptr, msg);
    exit(1);
}

static void corrupt() {
    error(NULL, "corrupt message catalog");
}
static void nomem() {
    error(NULL, "out of memory");
}

static char *getline(fd)
int fd;
{
    static long	curlen = BUFSIZ;
    static char	buf[BUFSIZ], *bptr = buf, *bend = buf;
    char	*cptr, *cend;
    long	buflen;

    if (!curline) {
	curline = (char *) malloc(curlen);
	if (!curline) nomem();
    }
    ++lineno;

    cptr = curline;
    cend = curline + curlen;
    while (True) {
	for (; bptr < bend && cptr < cend; ++cptr, ++bptr) {
	    if (*bptr == '\n') {
		*cptr = '\0';
		++bptr;
		return(curline);
	    } else *cptr = *bptr;
	}
	if (bptr == bend) {
	    buflen = read(fd, buf, BUFSIZ);
	    if (buflen <= 0) {
		if (cptr > curline) {
		    *cptr = '\0';
		    return(curline);
		}
		return(NULL);
	    }
	    bend = buf + buflen;
	    bptr = buf;
	}
	if (cptr == cend) {
	    cptr = curline = (char *) realloc(curline, curlen *= 2);
	    cend = curline + curlen;
	}
    }
}


static char *token(cptr)
char *cptr;
{
    static char	tok[MAXTOKEN+1];
    char	*tptr = tok;

    while (*cptr && isspace(*cptr)) ++cptr;
    while (*cptr && !isspace(*cptr)) *tptr++ = *cptr++;
    *tptr = '\0';
    return(tok);
}
static char *wskip(cptr)
char *cptr;
{
    if (!*cptr || !isspace(*cptr)) {
	warning(cptr, "expected a space");
	return(cptr);
    }
    while (*cptr && isspace(*cptr)) ++cptr;
    return(cptr);
}
static char *cskip(cptr)
char *cptr;
{
    if (!*cptr || isspace(*cptr)) {
	warning(cptr, "wasn't expecting a space");
	return(cptr);
    }
    while (*cptr && !isspace(*cptr)) ++cptr;
    return(cptr);
}

static char *getmsg(fd, cptr, quote)
int fd;
char *cptr;
char quote;
{
    static char	*msg = NULL;
    static long	msglen = 0;
    long	clen, i;
    char	*tptr;

    int		needq;

    if (quote && *cptr == quote) {
	needq = True;
	++cptr;
    } else needq = False;

    clen = strlen(cptr) + 1;
    if (clen > msglen) {
	if (msglen) msg = (char *) realloc(msg, clen);
	else msg = (char *) malloc(clen);
	msglen = clen;
    }
    tptr = msg;

    while (*cptr) {
	if (quote && *cptr == quote) {
	    char	*tmp;
	    tmp = cptr+1;
	    if (*tmp && (!isspace(*tmp) || *wskip(tmp))) {
		warning(cptr, "unexpected quote character, ignoreing");
		*tptr++ = *cptr++;
	    } else {
		*cptr = '\0';
	    }
	} else if (*cptr == '\\') {
	    ++cptr;
	    switch (*cptr) {
	      case '\0':
		cptr = getline(fd);
		if (!cptr) error(NULL, "premature end of file");
		msglen += strlen(cptr);
		i = tptr - msg;
		msg = (char *) realloc(msg, msglen);
		tptr = msg + i;
		break;
	      case 'n':
		*tptr++ = '\n';
		++cptr;
		break;
	      case 't':
		*tptr++ = '\t';
		++cptr;
		break;
	      case 'v':
		*tptr++ = '\v';
		++cptr;
		break;
	      case 'b':
		*tptr++ = '\b';
		++cptr;
		break;
	      case 'r':
		*tptr++ = '\r';
		++cptr;
		break;
	      case 'f':
		*tptr++ = '\f';
		++cptr;
		break;
	      case '"':
		*tptr++ = '"';
		++cptr;
		break;
	      case '\'':
		*tptr++ = '\'';
		++cptr;
		break;
	      case '\\':
		*tptr++ = '\\';
		++cptr;
		break;
	      default:
		if (isdigit(*cptr)) {
		    *tptr = 0;
		    for (i = 0; i < 3; ++i) {
			if (!isdigit(*cptr)) break;
			if (*cptr > '7') warning(cptr, "octal number greater than 7?!");
			*tptr *= 8;
			*tptr += (*cptr - '0');
			++cptr;
		    }
		} else {
		    warning(cptr, "unrecognized escape sequence");
		}
	    }
	} else {
	    *tptr++ = *cptr++;
	}
    }
    *tptr = '\0';
    return(msg);
}



static char *dupstr(ostr)
char *ostr;
{
    char	*nstr;

    nstr = (char *) malloc(strlen(ostr) + 1);
    if (!nstr) error(NULL, "unable to allocate storage");
    strcpy(nstr, ostr);
    return(nstr);
}


/*
 * The Global Stuff
 */


typedef struct _msgT {
    long	msgId;
    char	*str;
    char	*hconst;
    long	offset;
    struct _msgT	*prev, *next;
} msgT;
typedef struct _setT {
    long	setId;
    char	*hconst;
    msgT	*first, *last;
    struct _setT	*prev, *next;
} setT;
typedef struct {
    setT	*first, *last;
} catT;

static setT	*curSet;
static catT	*cat;

/*
 * Find the current byte order.  There are of course some others, but this will do
 * for now.  Note that all we care about is "long".
 */
long MCGetByteOrder() {
    long	l = 0x00010203;
    char	*cptr = (char *) &l;

    if (cptr[0] == 0 && cptr[1] == 1 && cptr[2] == 2 && cptr[3] == 3)
      return MC68KByteOrder;
    else return MCn86ByteOrder;
}


void MCParse(
#if PROTO
		int fd)
#else
		fd)
int fd;
#endif
{
    char	*cptr, *str;
    int	setid, msgid = 0;
    char	hconst[MAXTOKEN+1];
    char	quote = 0;

    if (!cat) {
	cat = (catT *) malloc(sizeof(catT));
	if (!cat) nomem();
	bzero(cat, sizeof(catT));
    }

    hconst[0] = '\0';

    while (cptr = getline(fd)) {
	if (*cptr == '$') {
	    ++cptr;
	    if (strncmp(cptr, "set", 3) == 0) {
		cptr += 3;
		cptr = wskip(cptr);
		setid = atoi(cptr);
		cptr = cskip(cptr);
		if (*cptr) cptr = wskip(cptr);
		if (*cptr == '#') {
		    ++cptr;
		    MCAddSet(setid, token(cptr));
		} else MCAddSet(setid, NULL);
		msgid = 0;
	    } else if (strncmp(cptr, "delset", 6) == 0) {
		cptr += 6;
		cptr = wskip(cptr);
		setid = atoi(cptr);
		MCDelSet(setid);
	    } else if (strncmp(cptr, "quote", 5) == 0) {
		cptr += 5;
		if (!*cptr) quote = 0;
		else {
		    cptr = wskip(cptr);
		    if (!*cptr) quote = 0;
		    else quote = *cptr;
		}
	    } else if (isspace(*cptr)) {
		cptr = wskip(cptr);
		if (*cptr == '#') {
		    ++cptr;
		    strcpy(hconst, token(cptr));
		}
	    } else {
		if (*cptr) {
		    cptr = wskip(cptr);
		    if (*cptr) warning(cptr, "unrecognized line");
		}
	    }
	} else {
	    if (isdigit(*cptr) || *cptr == '#') {
		if (*cptr == '#') {
		    ++msgid;
		    ++cptr;
		    if (!*cptr) {
			MCAddMsg(msgid, "", hconst);
			hconst[0] = '\0';
			continue;
		    }
		    if (!isspace(*cptr)) warning(cptr, "expected a space");
		    ++cptr;
		    if (!*cptr) {
			MCAddMsg(msgid, "", hconst);
			hconst[0] = '\0';
			continue;
		    }
		} else {
		    msgid = atoi(cptr);
		    cptr = cskip(cptr);
		    cptr = wskip(cptr);
		    /* if (*cptr) ++cptr; */
		}
		if (!*cptr) MCDelMsg(msgid);
		else {
		    str = getmsg(fd, cptr, quote);
		    MCAddMsg(msgid, str, hconst);
		    hconst[0] = '\0';
		}
	    }
	}
    }
}

void MCReadCat(
#if PROTO
		int fd)
#else
		fd)
int fd;
#endif
{
    MCHeaderT	mcHead;
    MCMsgT	mcMsg;
    MCSetT	mcSet;
    msgT	*msg;
    setT	*set;
    int		i;
    char	*data;

    cat = (catT *) malloc(sizeof(catT));
    if (!cat) nomem();
    bzero(cat, sizeof(catT));

    if (read(fd, &mcHead, sizeof(mcHead)) != sizeof(mcHead)) corrupt();
    if (strncmp(mcHead.magic, MCMagic, MCMagicLen) != 0) corrupt();
    if (mcHead.majorVer != MCMajorVer) error(NULL, "unrecognized catalog version");
    if ((mcHead.flags & MCGetByteOrder()) == 0) error(NULL, "wrong byte order");

    if (lseek(fd, mcHead.firstSet, L_SET) == -1) corrupt();

    while (True) {
	if (read(fd, &mcSet, sizeof(mcSet)) != sizeof(mcSet)) corrupt();
	if (mcSet.invalid) continue;

	set = (setT *) malloc(sizeof(setT));
	if (!set) nomem();
	bzero(set, sizeof(*set));
	if (cat->first) {
	    cat->last->next = set;
	    set->prev = cat->last;
	    cat->last = set;
	} else cat->first = cat->last = set;

	set->setId = mcSet.setId;

	/* Get the data */
	if (mcSet.dataLen) {
	    data = (char *) malloc(mcSet.dataLen);
	    if (!data) nomem();
	    if (lseek(fd, mcSet.data.off, L_SET) == -1) corrupt();
	    if (read(fd, data, mcSet.dataLen) != mcSet.dataLen) corrupt();
	    if (lseek(fd, mcSet.u.firstMsg, L_SET) == -1) corrupt();

	    for (i = 0; i < mcSet.numMsgs; ++i) {
		if (read(fd, &mcMsg, sizeof(mcMsg)) != sizeof(mcMsg)) corrupt();
		if (mcMsg.invalid) {
		    --i;
		    continue;
		}

		msg = (msgT *) malloc(sizeof(msgT));
		if (!msg) nomem();
		bzero(msg, sizeof(*msg));
		if (set->first) {
		    set->last->next = msg;
		    msg->prev = set->last;
		    set->last = msg;
		} else set->first = set->last = msg;

		msg->msgId = mcMsg.msgId;
		msg->str = dupstr((char *) (data + mcMsg.msg.off));
	    }
	    free(data);
	}
	if (!mcSet.nextSet) break;
	if (lseek(fd, mcSet.nextSet, L_SET) == -1) corrupt();
    }
}


static void printS(fd, str)
int fd;
char *str;
{
    write(fd, str, strlen(str));
}
static void printL(fd, l)
int fd;
long l;
{
    char	buf[32];
    sprintf(buf, "%ld", l);
    write(fd, buf, strlen(buf));
}
static void printLX(fd, l)
int fd;
long l;
{
    char	buf[32];
    sprintf(buf, "%lx", l);
    write(fd, buf, strlen(buf));
}

static void genconst(fd, type, setConst, msgConst, val)
int fd;
int type;
char *setConst;
char *msgConst;
long val;
{
    switch (type) {
      case MCLangC:
	if (!msgConst) {
	    printS(fd, "\n#define ");
	    printS(fd, setConst);
	    printS(fd, "Set");
	} else {
	    printS(fd, "#define ");
	    printS(fd, setConst);
	    printS(fd, msgConst);
	}
	printS(fd, "\t0x");
	printLX(fd, val);
	printS(fd, "\n");
	break;
      case MCLangCPlusPlus:
      case MCLangANSIC:
	if (!msgConst) {
	    printS(fd, "\nconst long ");
	    printS(fd, setConst);
	    printS(fd, "Set");
	} else {
	    printS(fd, "const long ");
	    printS(fd, setConst);
	    printS(fd, msgConst);
	}
	printS(fd, "\t= ");
	printL(fd, val);
	printS(fd, ";\n");
	break;
      default:
	error(NULL, "not a recognized (programming) language type");
    }
}

void MCWriteConst(
#if PROTO
		int fd, int type, int orConsts)
#else
		fd, type, orConsts)
int fd;
int type;
int orConsts;
#endif
{
    msgT	*msg;
    setT	*set;
    long	id;

    if (orConsts && (type == MCLangC || type == MCLangCPlusPlus || type == MCLangANSIC)) {
	printS(fd, "/* Use these Macros to compose and decompose setId's and msgId's */\n");
	printS(fd, "#ifndef MCMakeId\n");
        printS(fd, "# define MCMakeId(s,m)\t(unsigned long)(((unsigned short)s<<(sizeof(short)*8))\\\n");
        printS(fd, "\t\t\t\t\t|(unsigned short)m)\n");
        printS(fd, "# define MCSetId(id)\t(unsigned int) (id >> (sizeof(short) * 8))\n");
        printS(fd, "# define MCMsgId(id)\t(unsigned int) ((id << (sizeof(short) * 8))\\\n");
        printS(fd, "\t\t\t\t\t>> (sizeof(short) * 8))\n");
	printS(fd, "#endif\n");
    }

    for (set = cat->first; set; set = set->next) {
	if (set->hconst) genconst(fd, type, set->hconst, NULL, set->setId);

	for (msg = set->first; msg; msg = msg->next) {
	    if (msg->hconst) {
		if (orConsts) id = MCMakeId(set->setId, msg->msgId);
		else id = msg->msgId;
		genconst(fd, type, set->hconst, msg->hconst, id);
		free(msg->hconst);
		msg->hconst = NULL;
	    }
	}
	if (set->hconst) {
	    free(set->hconst);
	    set->hconst = NULL;
	}
    }
}

void MCWriteCat(
#if PROTO
		int fd)
#else
		fd)
int fd;
#endif
{
    MCHeaderT	mcHead;
    int		cnt;
    setT	*set;
    msgT	*msg;
    MCSetT	mcSet;
    MCMsgT	mcMsg;
    off_t	pos;

    bcopy(MCMagic, mcHead.magic, MCMagicLen);
    mcHead.majorVer = MCMajorVer;
    mcHead.minorVer = MCMinorVer;
    mcHead.flags = MCGetByteOrder();
    mcHead.firstSet = 0;	/* We'll be back to set this in a minute */

    for (cnt = 0, set = cat->first; set; set = set->next) ++cnt;
    mcHead.numSets = cnt;

    lseek(fd, 0L, L_SET);
    write(fd, &mcHead, sizeof(mcHead));
    mcHead.firstSet = lseek(fd, 0, L_INCR);
    lseek(fd, 0L, L_SET);
    write(fd, &mcHead, sizeof(mcHead));

    for (set = cat->first; set; set = set->next) {
	bzero(&mcSet, sizeof(mcSet));

	mcSet.setId = set->setId;
	mcSet.invalid = False;

	/* The rest we'll have to come back and change in a moment */
	pos = lseek(fd, 0, L_INCR);
	write(fd, &mcSet, sizeof(mcSet));

	/* Now write all the string data */
	mcSet.data.off = lseek(fd, 0, L_INCR);
	cnt = 0;
	for (msg = set->first; msg; msg = msg->next) {
	    msg->offset = lseek(fd, 0, L_INCR) - mcSet.data.off;
	    mcSet.dataLen += write(fd, msg->str, strlen(msg->str) + 1);
	    ++cnt;
	}
	mcSet.u.firstMsg = lseek(fd, 0, L_INCR);
	mcSet.numMsgs = cnt;

	/* Now write the message headers */
	for (msg = set->first; msg; msg = msg->next) {
	    mcMsg.msgId = msg->msgId;
	    mcMsg.msg.off = msg->offset;
	    mcMsg.invalid = False;
	    write(fd, &mcMsg, sizeof(mcMsg));
	}

	/* Go back and fix things up */

	if (set == cat->last) {
	    mcSet.nextSet = 0;
	    lseek(fd, pos, L_SET);
	    write(fd, &mcSet, sizeof(mcSet));
	} else {
	    mcSet.nextSet = lseek(fd, 0, L_INCR);
	    lseek(fd, pos, L_SET);
	    write(fd, &mcSet, sizeof(mcSet));
	    lseek(fd, mcSet.nextSet, L_SET);
	}
    }
}


void MCAddSet(
#if PROTO
		int setId, char *hconst)
#else
		setId, hconst)
int setId;
char *hconst;
#endif
{
    setT	*set;

    if (setId <= 0) {
	error(NULL, "setId's must be greater than zero");
	return;
    }

    if (hconst && !*hconst) hconst = NULL;
    for (set = cat->first; set; set = set->next) {
	if (set->setId == setId) {
	    if (set->hconst && hconst) free(set->hconst);
	    set->hconst = NULL;
	    break;
	} else if (set->setId > setId) {
	    setT	*newSet;

	    newSet = (setT *) malloc(sizeof(setT));
	    if (!newSet) nomem();
	    bzero(newSet, sizeof(setT));
	    newSet->prev = set->prev;
	    newSet->next = set;
	    if (set->prev) set->prev->next = newSet;
	    else cat->first = newSet;
	    set->prev = newSet;
	    set = newSet;
	    break;
	}
    }
    if (!set) {
	set = (setT *) malloc(sizeof(setT));
	if (!set) nomem();
	bzero(set, sizeof(setT));

	if (cat->first) {
	    set->prev = cat->last;
	    set->next = NULL;
	    cat->last->next = set;
	    cat->last = set;
	} else {
	    set->prev = set->next = NULL;
	    cat->first = cat->last = set;
	}
    }
    set->setId = setId;
    if (hconst) set->hconst = dupstr(hconst);
    curSet = set;
}

void MCAddMsg(
#if PROTO
		int msgId, char *str, char *hconst)
#else
		msgId, str, hconst)
int msgId;
char *str;
char *hconst;
#endif
{
    msgT	*msg;

    if (!curSet) error(NULL, "can't specify a message when no set exists");

    if (msgId <= 0) {
	error(NULL, "msgId's must be greater than zero");
	return;
    }

    if (hconst && !*hconst) hconst = NULL;
    for (msg = curSet->first; msg; msg = msg->next) {
	if (msg->msgId == msgId) {
	    if (msg->hconst && hconst) free(msg->hconst);
	    if (msg->str) free(msg->str);
	    msg->hconst = msg->str = NULL;
	    break;
	} else if (msg->msgId > msgId) {
	    msgT	*newMsg;

	    newMsg = (msgT *) malloc(sizeof(msgT));
	    if (!newMsg) nomem();
	    bzero(newMsg, sizeof(msgT));
	    newMsg->prev = msg->prev;
	    newMsg->next = msg;
	    if (msg->prev) msg->prev->next = newMsg;
	    else curSet->first = newMsg;
	    msg->prev = newMsg;
	    msg = newMsg;
	    break;
	}
    }
    if (!msg) {
	msg = (msgT *) malloc(sizeof(msgT));
	if (!msg) nomem();
	bzero(msg, sizeof(msgT));

	if (curSet->first) {
	    msg->prev = curSet->last;
	    msg->next = NULL;
	    curSet->last->next = msg;
	    curSet->last = msg;
	} else {
	    msg->prev = msg->next = NULL;
	    curSet->first = curSet->last = msg;
	}
    }
    msg->msgId = msgId;
    if (hconst) msg->hconst = dupstr(hconst);
    msg->str = dupstr(str);
}

void MCDelSet(
#if PROTO
		int setId)
#else
		setId)
int setId;
#endif
{
    setT	*set;
    msgT	*msg;

    for (set = cat->first; set; set = set->next) {
	if (set->setId == setId) {
	    for (msg = set->first; msg; msg = msg->next) {
		if (msg->hconst) free(msg->hconst);
		if (msg->str) free(msg->str);
		free(msg);
	    }
	    if (set->hconst) free(set->hconst);

	    if (set->prev) set->prev->next = set->next;
	    else cat->first = set->next;

	    if (set->next) set->next->prev = set->prev;
	    else cat->last = set->prev;

	    free(set);
	    return;
	} else if (set->setId > setId) break;
    }
    warning(NULL, "specified set doesn't exist");
}

void MCDelMsg(
#if PROTO
		int msgId)
#else
		msgId)
int msgId;
#endif
{
    msgT	*msg;

    if (!curSet) error(NULL, "you can't delete a message before defining the set");

    for (msg = curSet->first; msg; msg = msg->next) {
	if (msg->msgId == msgId) {
	    if (msg->hconst) free(msg->hconst);
	    if (msg->str) free(msg->str);

	    if (msg->prev) msg->prev->next = msg->next;
	    else curSet->first = msg->next;

	    if (msg->next) msg->next->prev = msg->prev;
	    else curSet->last = msg->prev;

	    free(msg);
	    return;
	} else if (msg->msgId > msgId) break;
    }
    warning(NULL, "specified msg doesn't exist");
}

void MCDumpcat(fp)
FILE  *fp;
{
    msgT	*msg;
    setT	*set;

    if (!cat)
		errx(1, "no catalog open");

    for (set = cat->first; set; set = set->next) {
	fprintf(fp, "$set %d", set->setId);
	if (set->hconst)
	    fprintf(fp, " # %s", set->hconst);
	fprintf(fp, "\n\n");

	for (msg = set->first; msg; msg = msg->next) {
	    if (msg->hconst)
		fprintf(fp, "# %s\n", msg->hconst);
	    fprintf(fp, "%d\t%s\n", msg->msgId, msg->str);
	}
	fprintf(fp, "\n");
    }

}
