#ifndef _BLURB_
#define _BLURB_
/************************************************************************
          Copyright 1988, 1991 by Carnegie Mellon University

                          All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of Carnegie Mellon University not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
************************************************************************/
#endif /* _BLURB_ */


#ifndef lint
static char rcsid[] = "$Header: /home/cvs/386BSD/src/libexec/bootpd/readfile.c,v 1.1 1994/01/25 22:53:48 martin Exp $";
#endif


/*
 * bootpd configuration file reading code.
 *
 * The routines in this file deal with reading, interpreting, and storing
 * the information found in the bootpd configuration file (usually
 * /etc/bootptab).
 */


#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <netinet/in.h>
#ifdef SYSLOG
#include <syslog.h>
#endif
#include "bootp.h"
#include "hash.h"
#include "bootpd.h"


#define SUCCESS			  0
#define E_END_OF_ENTRY		(-1)
#define E_SYNTAX_ERROR		(-2)
#define E_UNKNOWN_SYMBOL	(-3)
#define E_BAD_IPADDR		(-4)
#define E_BAD_HADDR		(-5)
#define E_BAD_SMASK		(-6)
#define E_BAD_TIMEOFF		(-7)
#define E_BAD_VM_COOKIE		(-8)
#define E_BAD_HTYPE		(-9)
#define E_BAD_BOOTSIZE	       (-10)
#define E_BAD_BOOT_SERVER      (-11)
#define E_BAD_HOMEDIR	       (-12)
#define E_BAD_TFTPDIR	       (-13)
#define E_BAD_SWAP_SERVER      (-14)

#define SYM_NULL		  0
#define SYM_BOOTFILE		  1
#define SYM_COOKIE_SERVER	  2
#define SYM_DOMAIN_SERVER	  3
#define SYM_GATEWAY		  4
#define SYM_HADDR		  5
#define SYM_HOMEDIR		  6
#define SYM_HTYPE		  7
#define SYM_IMPRESS_SERVER	  8
#define SYM_IPADDR		  9
#define SYM_LOG_SERVER		 10
#define SYM_LPR_SERVER		 11
#define SYM_NAME_SERVER		 12
#define SYM_RLP_SERVER		 13
#define SYM_SUBNET_MASK		 14
#define SYM_TIME_OFFSET		 15
#define SYM_TIME_SERVER		 16
#define SYM_VENDOR_MAGIC	 17
#define SYM_SIMILAR_ENTRY	 18
#define SYM_NAME_SWITCH		 19
#define SYM_BOOTSIZE		 20
#define SYM_BOOT_SERVER		 22
#define SYM_TFTPDIR		 23
#define SYM_DUMPFILE		 24
#define SYM_DOMAIN_NAME          25
#define SYM_SWAP_SERVER          26
#define SYM_ROOTPATH             27

#define OP_ADDITION		  1	/* Operations on tags */
#define OP_DELETION		  2
#define OP_BOOLEAN		  3

#define MAXINADDRS		 16	/* Max size of an IP address list */
#define MAXBUFLEN		 64	/* Max temp buffer space */
#define MAXENTRYLEN	       2048	/* Max size of an entire entry */



/*
 * Structure used to map a configuration-file symbol (such as "ds") to a
 * unique integer.
 */

struct symbolmap {
    char *symbol;
    int symbolcode;
};


struct htypename {
    char *name;
    byte htype;
};


PRIVATE int nhosts;		/* Number of hosts (/w hw or IP address) */
PRIVATE int nentries;		/* Total number of entries */
PRIVATE long modtime = 0;	/* Last modification time of bootptab */


/*
 * List of symbolic names used in the bootptab file.  The order and actual
 * values of the symbol codes (SYM_. . .) are unimportant, but they must
 * all be unique.
 */

PRIVATE struct symbolmap symbol_list[] = {
    { "bf", SYM_BOOTFILE	},
    { "bs", SYM_BOOTSIZE	},
    { "cs", SYM_COOKIE_SERVER	},
    { "df", SYM_DUMPFILE	},
    { "dn", SYM_DOMAIN_NAME	},
    { "ds", SYM_DOMAIN_SERVER	},
    { "gw", SYM_GATEWAY		},
    { "ha", SYM_HADDR		},
    { "hd", SYM_HOMEDIR		},
    { "hn", SYM_NAME_SWITCH     },
    { "ht", SYM_HTYPE		},
    { "im", SYM_IMPRESS_SERVER	},
    { "ip", SYM_IPADDR		},
    { "lg", SYM_LOG_SERVER	},
    { "lp", SYM_LPR_SERVER	},
    { "ns", SYM_NAME_SERVER	},
    { "rl", SYM_RLP_SERVER	},
    { "rp", SYM_ROOTPATH	},
    { "sa", SYM_BOOT_SERVER	},
    { "sm", SYM_SUBNET_MASK	},
    { "sw", SYM_SWAP_SERVER	},
    { "tc", SYM_SIMILAR_ENTRY   },
    { "td", SYM_TFTPDIR		},
    { "to", SYM_TIME_OFFSET	},
    { "ts", SYM_TIME_SERVER	},
    { "vm", SYM_VENDOR_MAGIC	},
};


/*
 * List of symbolic names for hardware types.  Name translates into
 * hardware type code listed with it.  Names must begin with a letter
 * and must be all lowercase.  This is searched linearly, so put
 * commonly-used entries near the beginning.
 */

PRIVATE struct htypename htnamemap[] = {
    { "ethernet",   HTYPE_ETHERNET	},
    { "ethernet3",  HTYPE_EXP_ETHERNET	},
    { "ether",	    HTYPE_ETHERNET	},
    { "ether3",	    HTYPE_EXP_ETHERNET	},
    { "ieee802",    HTYPE_IEEE802	},
    { "tr",	    HTYPE_IEEE802	},
    { "token-ring", HTYPE_IEEE802	},
    { "pronet",	    HTYPE_PRONET	},
    { "chaos",	    HTYPE_CHAOS		},
    { "arcnet",	    HTYPE_ARCNET	},
    { "ax.25",	    HTYPE_AX25		},
    { "direct",     HTYPE_DIRECT	},
    { "serial",     HTYPE_DIRECT	},
    { "slip",       HTYPE_DIRECT	},
    { "ppp",        HTYPE_DIRECT	}
};



/*
 * Externals and forward declarations.
 */

extern char *malloc();
extern boolean iplookcmp();
extern int errno;

PRIVATE char *smalloc();
PRIVATE void fill_defaults();
PRIVATE void del_string();
PRIVATE void del_bindata();
PRIVATE void del_iplist();
PRIVATE void free_host();
PRIVATE u_long get_u_long();
PRIVATE void process_generic();
PRIVATE char *get_string();
PRIVATE struct shared_string *get_shared_string();
PRIVATE void read_entry();
PRIVATE boolean nullcmp();
PRIVATE boolean nmcmp();
PRIVATE boolean hwinscmp();
PRIVATE void adjust();
PRIVATE void eat_whitespace();
PRIVATE void makelower();
PRIVATE struct in_addr_list *get_addresses();
PRIVATE byte *prs_haddr();





/*
 * Read bootptab database file.  Avoid rereading the file if the
 * write date hasn't changed since the last time we read it.
 */

int readtab()
{
    struct host *hp;
    FILE *fp;
    struct stat st;
    unsigned hashcode, buflen;
    static char buffer[MAXENTRYLEN];
#ifdef DEBUG
    char timestr[26];
#endif

    /*
     * Check the last modification time.
     */
    if (stat(bootptab, &st) < 0) {
	report(LOG_ERR, "stat on \"%s\": %s\n", bootptab, get_errmsg());
	return -1;
    }
#ifdef DEBUG
    if (debug > 3) {
	strcpy(timestr, ctime(&(st.st_mtime)));
	report(LOG_INFO, "bootptab mtime is %s",
	       timestr);
    }
#endif
    if (st.st_mtime == modtime && st.st_nlink) {
	/*
	 * hasn't been modified or deleted yet.
	 */
	return 0;
    }
    report(LOG_INFO, "reading %s\"%s\"\n",
	   (modtime != 0L) ? "new " : "",
	   bootptab);

    /*
     * Open bootptab file.
     */
    if ((fp = fopen(bootptab, "r")) == NULL) {
	report(LOG_ERR, "error opening \"%s\": %s\n", bootptab, get_errmsg());
	return -1;
    }

    /*
     * Record file modification time.
     */
    if (fstat(fileno(fp), &st) < 0) {
	report(LOG_ERR, "fstat: %s\n", get_errmsg());
	fclose(fp);
	return -1;
    }
    modtime = st.st_mtime;

    /*
     * Entirely erase all hash tables.
     */
    hash_Reset(hwhashtable, free_host);
    hash_Reset(iphashtable, free_host);
    hash_Reset(nmhashtable, free_host);

    nhosts = 0;
    nentries = 0;
    while (TRUE) {
	buflen = sizeof(buffer);
	read_entry(fp, buffer, &buflen);
	if (buflen == 0) {		/* More entries? */
	    break;
	}
	hp = (struct host *) smalloc(sizeof(struct host));

	/*
	 * Get individual info
	 */
	hp->flags.vm_auto = TRUE;
	bcopy(vm_rfc1048, hp->vm_cookie, 4);
	if (process_entry(hp, buffer) < 0) {
	    free_host(hp);
	    continue;
	}

	if ((hp->flags.htype && hp->flags.haddr) || hp->flags.iaddr) {
	    nhosts++;
	}
	if (hp->flags.htype && hp->flags.haddr) {
	    hashcode = hash_HashFunction(hp->haddr, haddrlength(hp->htype));
	    if (hash_Insert(hwhashtable, hashcode, hwinscmp, hp, hp) < 0) {
		report(LOG_WARNING, "duplicate %s address: %s\n",
			netname(hp->htype),
			haddrtoa(hp->haddr, hp->htype));
		free_host(hp);
		continue;
	    }
	}
	if (hp->flags.iaddr) {
	    hashcode = hash_HashFunction(&(hp->iaddr.s_addr), 4);
	    if (hash_Insert(iphashtable, hashcode, nullcmp, hp, hp) < 0) {
		report(LOG_ERR,
			"hash_Insert() failed on IP address insertion\n");
	    }
	}

	hashcode = hash_HashFunction(hp->hostname->string,
				     strlen(hp->hostname->string));
	if (hash_Insert(nmhashtable, hashcode, nullcmp, hp->hostname->string, hp) < 0) {
	    report(LOG_ERR,
		   "hash_Insert() failed on insertion of hostname: \"%s\"\n",
		   hp->hostname->string);
	}
	nentries++;
    }

done:
    fclose(fp);
    report(LOG_INFO, "read %d entries (%d hosts) from \"%s\"\n",
		nentries, nhosts, bootptab);
    return 0;
}



/*
 * Read an entire host entry from the file pointed to by "fp" and insert it
 * into the memory pointed to by "buffer".  Leading whitespace and comments
 * starting with "#" are ignored (removed).  Backslashes (\) always quote
 * the next character except that newlines preceeded by a backslash cause
 * line-continuation onto the next line.  The entry is terminated by a
 * newline character which is not preceeded by a backslash.  Sequences
 * surrounded by double quotes are taken literally (including newlines, but
 * not backslashes).
 *
 * The "bufsiz" parameter points to an unsigned int which specifies the
 * maximum permitted buffer size.  Upon return, this value will be replaced
 * with the actual length of the entry (not including the null terminator).
 *
 * This code is a little scary. . . .  I don't like using gotos in C
 * either, but I first wrote this as an FSM diagram and gotos seemed like
 * the easiest way to implement it.  Maybe later I'll clean it up.
 */

PRIVATE void read_entry(fp, buffer, bufsiz)
FILE *fp;
char *buffer;
unsigned *bufsiz;
{
    int c, length;

    length = 0;

    /*
     * Eat whitespace, blank lines, and comment lines.
     */
top:
    c = fgetc(fp);
    if (c < 0) {
	goto done;			/* Exit if end-of-file */
    }
    if (isspace(c)) {
	goto top;			/* Skip over whitespace */
    }
    if (c == '#') {
	while (TRUE) {			/* Eat comments after # */
	    c = fgetc(fp);
	    if (c < 0) {
		goto done;		/* Exit if end-of-file */
	    }
	    if (c == '\n') {
		goto top;		/* Try to read the next line */
	    }
	}
    }
    ungetc(c, fp);	/* Other character, push it back to reprocess it */


    /*
     * Now we're actually reading a data entry.  Get each character and
     * assemble it into the data buffer, processing special characters like
     * double quotes (") and backslashes (\).
     */

mainloop:
    c = fgetc(fp);
    switch (c) {
	case EOF:		
	case '\n':
	    goto done;			/* Exit on EOF or newline */
	case '\\':
	    c = fgetc(fp);		/* Backslash, read a new character */
	    if (c < 0) {
		goto done;		/* Exit on EOF */
	    }
	    *buffer++ = c;		/* Store the literal character */
	    length++;
	    if (length < *bufsiz - 1) {
		goto mainloop;
	    } else {
		goto done;
	    }
	case '"':
	    *buffer++ = '"';		/* Store double-quote */
	    length++;
	    if (length >= *bufsiz - 1) {
		goto done;
	    }
	    while (TRUE) {		/* Special quote processing loop */
		c = fgetc(fp);
		switch (c) {
		    case EOF:
			goto done;	    /* Exit on EOF . . . */
		    case '"':
			*buffer++ = '"';    /* Store matching quote */
			length++;
			if (length < *bufsiz - 1) {
			    goto mainloop;	/* And continue main loop */
			} else {
			    goto done;
			}
		    case '\\':
			if ((c = fgetc(fp)) < 0) {	/* Backslash */
			    goto done;			/* EOF. . . .*/
			}		    /* else fall through */
		    default:
			*buffer++ = c;	    /* Other character, store it */
			length++;
			if (length >= *bufsiz - 1) {
			    goto done;
			}
		}
	    }
	case ':':
	    *buffer++ = c;		/* Store colons */
	    length++;
	    if (length >= *bufsiz - 1) {
		goto done;
	    }

	    do {			/* But remove whitespace after them */
		c = fgetc(fp);
		if ((c < 0) || (c == '\n')) {
		    goto done;
		}
	    } while (isspace(c));	/* Skip whitespace */

	    if (c == '\\') {		/* Backslash quotes next character */
		c = fgetc(fp);
		if (c < 0) {
		    goto done;
		}
		if (c == '\n') {
		    goto top;		/* Backslash-newline continuation */
		}
	    }
	    /* fall through if "other" character */
	default:
	    *buffer++ = c;		/* Store other characters */
	    length++;
	    if (length >= *bufsiz - 1) {
		goto done;
	    }
    }
    goto mainloop;			/* Keep going */

done:
    *buffer = '\0';			/* Terminate string */
    *bufsiz = length;			/* Tell the caller its length */
}



/*
 * Parse out all the various tags and parameters in the host entry pointed
 * to by "src".  Stuff all the data into the appropriate fields of the
 * host structure pointed to by "host".  If there is any problem with the
 * entry, an error message is reported via report(), no further processing
 * is done, and -1 is returned.  Successful calls return 0.
 *
 * (Some errors probably shouldn't be so completely fatal. . . .)
 */

PRIVATE int process_entry(host, src)
struct host *host;
char *src;
{
    int retval;

    if (!host || *src == '\0') {
	return -1;
    }
    host->hostname = get_shared_string(&src);
    if (!goodname(host->hostname->string)) {
	report(LOG_ERR, "bad hostname: \"%s\"\n", host->hostname->string);
	del_string(host->hostname);
	return -1;
    }
    adjust(&src);
    while (TRUE) {
 	retval = eval_symbol(&src, host);
	switch (retval) {
	    case SUCCESS:
		break;
	    case E_SYNTAX_ERROR:
		report(LOG_ERR, "syntax error in entry for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_UNKNOWN_SYMBOL:
		report(LOG_ERR, "unknown symbol in entry for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_IPADDR:
		report(LOG_ERR, "bad IP address for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_HADDR:
		report(LOG_ERR, "bad hardware address for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_SMASK:
		report(LOG_ERR, "bad subnet mask for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_TIMEOFF:
		report(LOG_ERR, "bad time offset for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_VM_COOKIE:
		report(LOG_ERR, "bad vendor magic cookie for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_HOMEDIR:
		report(LOG_ERR, "bad home directory for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_TFTPDIR:
		report(LOG_ERR, "bad TFTP directory for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_BOOT_SERVER:
		report(LOG_ERR, "bad boot server IP address for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_BAD_SWAP_SERVER:
		report(LOG_ERR, "bad swap server IP address for host \"%s\"\n",
				host->hostname->string);
		return -1;
	    case E_END_OF_ENTRY:
	    default:
#if 0
		/*
		 * For now, don't try to make-up a subnet mask if one
		 * wasn't specified.
		 *
		 * This algorithm is also not entirely correct.
		 */
		if (!(hp->flags.subnet_mask)) {
		    /*
		     * Try to deduce the subnet mask from the network class
		     */
		    value = (ntohl(value) >> 30) & 0x03;
		    switch (value) {
			case 0:
			case 1:
			    hp->subnet_mask.s_addr = htonl(0xFF000000L);
			    break;
			case 2:
			    hp->subnet_mask.s_addr = htonl(0xFFFF0000L);
			    break;
			case 3:
			    hp->subnet_mask.s_addr = htonl(0xFFFFFF00L);
			    break;
		    }
		    hp->flags.subnet_mask = TRUE;
		}
#endif
		/*
		 * And now we're done with this entry
		 */
		return 0;
	}
	adjust(&src);
    }
}



/*
 * Evaluate the two-character tag symbol pointed to by "symbol" and place
 * the data in the structure pointed to by "hp".  The pointer pointed to
 * by "symbol" is updated to point past the source string (but may not
 * point to the next tag entry).
 *
 * Obviously, this need a few more comments. . . .
 */

PRIVATE eval_symbol(symbol, hp)
char **symbol;
struct host *hp;
{
    char tmpstr[MAXSTRINGLEN];
    byte *tmphaddr, *ustr;
    struct shared_string *ss;
    struct symbolmap *symbolptr;
    u_long value;
    long timeoff;
    int i, numsymbols;
    unsigned len;
    int optype;		/* Indicates boolean, addition, or deletion */

    if ((*symbol)[0] == '\0') {
	return E_END_OF_ENTRY;
    }
    if ((*symbol)[0] == ':') {
	return SUCCESS;
    }
    if ((*symbol)[0] == 'T') {			/* generic symbol */
	(*symbol)++;
	value = get_u_long(symbol);
	eat_whitespace(symbol);
	if ((*symbol)[0] != '=') {
	    return E_SYNTAX_ERROR;
	}
	(*symbol)++;
	if (!(hp->generic)) {
	    hp->generic = (struct shared_bindata *)
			    smalloc(sizeof(struct shared_bindata));
	}
	process_generic(symbol, &(hp->generic), (byte) (value & 0xFF));
	hp->flags.generic = TRUE;
	return SUCCESS;
    }

    eat_whitespace(symbol);

    /*
     * Determine the type of operation to be done on this symbol
     */
    switch ((*symbol)[2]) {
	case '=':
	    optype = OP_ADDITION;
	    break;
	case '@':
	    optype = OP_DELETION;
	    break;
	case ':':
	case '\0':
	    optype = OP_BOOLEAN;
	    break;
	default:
	    return E_SYNTAX_ERROR;
    }

    symbolptr = symbol_list;
    numsymbols = sizeof(symbol_list) / sizeof(struct symbolmap);
    for (i = 0; i < numsymbols; i++) {
	if (((symbolptr->symbol)[0] == (*symbol)[0]) &&
	   ((symbolptr->symbol)[1] == (*symbol)[1])) {
	    break;
	}
	symbolptr++;
    }
    if (i >= numsymbols) {
	return E_UNKNOWN_SYMBOL;
    }

    /*
     * Skip past the = or @ character (to point to the data) if this
     * isn't a boolean operation.  For boolean operations, just skip
     * over the two-character tag symbol (and nothing else. . . .).
     */
    (*symbol) += (optype == OP_BOOLEAN) ? 2 : 3;

    switch (symbolptr->symbolcode) {
	case SYM_BOOTFILE:
	    switch (optype) {
		case OP_ADDITION:
		    if (ss = get_shared_string(symbol)) {
			if (hp->bootfile) {
			    del_string(hp->bootfile);
			}
			hp->bootfile = ss;
			hp->flags.bootfile = TRUE;
		    }
		    break;
		case OP_DELETION:
		    if (hp->bootfile) {
			del_string(hp->bootfile);
		    }
		    hp->bootfile = NULL;
		    hp->flags.bootfile = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_COOKIE_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.cookie_server && hp->cookie_server) {
			del_iplist(hp->cookie_server);
		    }
		    hp->cookie_server = get_addresses(symbol);
		    hp->flags.cookie_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.cookie_server && hp->cookie_server) {
			del_iplist(hp->cookie_server);
		    }
		    hp->cookie_server = NULL;
		    hp->flags.cookie_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_DOMAIN_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.domain_server && hp->domain_server) {
			del_iplist(hp->domain_server);
		    }
		    hp->domain_server = get_addresses(symbol);
		    hp->flags.domain_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.domain_server && hp->domain_server) {
			del_iplist(hp->domain_server);
		    }
		    hp->domain_server = NULL;
		    hp->flags.domain_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_GATEWAY:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.gateway && hp->gateway) {
			del_iplist(hp->gateway);
		    }
		    hp->gateway = get_addresses(symbol);
		    hp->flags.gateway = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.gateway && hp->gateway) {
			del_iplist(hp->gateway);
		    }
		    hp->gateway = NULL;
		    hp->flags.gateway = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_HADDR:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.htype && hp->htype &&
			(tmphaddr = prs_haddr(symbol, hp->htype))) {
			bcopy(tmphaddr, hp->haddr, haddrlength(hp->htype));
			hp->flags.haddr = TRUE;
		    } else {
			return E_BAD_HADDR;
		    }
		    break;
		case OP_DELETION:
		    hp->flags.haddr = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_HOMEDIR:
	    switch (optype) {
		case OP_ADDITION:
		    if (ss = get_shared_string(symbol)) {
			if ((ss->string)[0] == '/') {
			    if (hp->homedir) {
				del_string(hp->homedir);
			    }
			    hp->homedir = ss;
			    hp->flags.homedir = TRUE;
			} else {
			    return E_BAD_HOMEDIR;
			}
		    } else {
			return E_BAD_HOMEDIR;
		    }
		    break;
		case OP_DELETION:
		    if (hp->homedir) {
			del_string(hp->homedir);
		    }
		    hp->homedir = NULL;
		    hp->flags.homedir = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_HTYPE:
	    switch (optype) {
		case OP_ADDITION:
		    value = 0L;			/* Assume an illegal value */
		    eat_whitespace(symbol);
		    if (isdigit(**symbol)) {
			value = get_u_long(symbol);
		    } else {
			len = sizeof(tmpstr);
			(void) get_string(symbol, tmpstr, &len);
			makelower(tmpstr);
			numsymbols = sizeof(htnamemap) /
				     sizeof(struct htypename);
			for (i = 0; i < numsymbols; i++) {
			    if (!strcmp(htnamemap[i].name, tmpstr)) {
				break;
			    }
			}
			if (i < numsymbols) {
			    value = htnamemap[i].htype;
			}
		    }
		    if ((value < 0) || (value > MAXHTYPES)) {
			return E_BAD_HTYPE;
		    }
		    hp->htype = (byte) (value & 0xFF);
		    hp->flags.htype = TRUE;
		    break;
		case OP_DELETION:
		    hp->flags.htype = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_IMPRESS_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.impress_server && hp->impress_server) {
			del_iplist(hp->impress_server);
		    }
		    hp->impress_server = get_addresses(symbol);
		    hp->flags.impress_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.impress_server && hp->impress_server) {
			del_iplist(hp->impress_server);
		    }
		    hp->impress_server = NULL;
		    hp->flags.impress_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_IPADDR:
	    switch (optype) {
		case OP_ADDITION:
		    if (prs_inetaddr(symbol, &value) < 0) {
			return E_BAD_IPADDR;
		    } else {
			hp->iaddr.s_addr = value;
			hp->flags.iaddr = TRUE;
		    }
		    break;
		case OP_DELETION:
		    hp->flags.iaddr = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_LOG_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.log_server && hp->log_server) {
			del_iplist(hp->log_server);
		    }
		    hp->log_server = get_addresses(symbol);
		    hp->flags.log_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.log_server && hp->log_server) {
			del_iplist(hp->log_server);
		    }
		    hp->log_server = NULL;
		    hp->flags.log_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_LPR_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.lpr_server && hp->lpr_server) {
			del_iplist(hp->lpr_server);
		    }
		    hp->lpr_server = get_addresses(symbol);
		    hp->flags.lpr_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.lpr_server && hp->lpr_server) {
			del_iplist(hp->lpr_server);
		    }
		    hp->lpr_server = NULL;
		    hp->flags.lpr_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_NAME_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.name_server && hp->name_server) {
			del_iplist(hp->name_server);
		    }
		    hp->name_server = get_addresses(symbol);
		    hp->flags.name_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.name_server && hp->name_server) {
			del_iplist(hp->name_server);
		    }
		    hp->name_server = NULL;
		    hp->flags.name_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_RLP_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.rlp_server && hp->rlp_server) {
			del_iplist(hp->rlp_server);
		    }
		    hp->rlp_server = get_addresses(symbol);
		    hp->flags.rlp_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.rlp_server && hp->rlp_server) {
			del_iplist(hp->rlp_server);
		    }
		    hp->rlp_server = NULL;
		    hp->flags.rlp_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_SUBNET_MASK:
	    switch (optype) {
		case OP_ADDITION:
		    if (prs_inetaddr(symbol, &value) < 0) {
			return E_BAD_SMASK;
		    } else {
			hp->subnet_mask.s_addr = value;
			hp->flags.subnet_mask = TRUE;
		    }
		    break;
		case OP_DELETION:
		    hp->flags.subnet_mask = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_TIME_OFFSET:
	    switch (optype) {
		case OP_ADDITION:
		    len = sizeof(tmpstr);
		    (void) get_string(symbol, tmpstr, &len);
		    if (!strncmp(tmpstr, "auto", 4)) {
			hp->flags.timeoff_auto = TRUE;
			hp->flags.time_offset = TRUE;
		    } else {
			if (sscanf(tmpstr, "%ld", &timeoff) != 1) {
			    return E_BAD_TIMEOFF;
			} else {
			    hp->time_offset = timeoff;
			    hp->flags.timeoff_auto = FALSE;
			    hp->flags.time_offset = TRUE;
			}
		    }
		    break;
		case OP_DELETION:
		    hp->flags.time_offset = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_TIME_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (hp->flags.time_server && hp->time_server) {
			del_iplist(hp->time_server);
		    }
		    hp->time_server = get_addresses(symbol);
		    hp->flags.time_server = TRUE;
		    break;
		case OP_DELETION:
		    if (hp->flags.time_server && hp->time_server) {
			del_iplist(hp->time_server);
		    }
		    hp->time_server = NULL;
		    hp->flags.time_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_VENDOR_MAGIC:
	    switch (optype) {
		case OP_ADDITION:
		    if (!strncmp(*symbol, "auto", 4)) {
			hp->flags.vm_auto = TRUE;	/* Make it auto */
		    } else if (!strncmp(*symbol, "rfc1048", 7) ||
			       !strncmp(*symbol, "rfc1084", 7)) {
			hp->flags.vm_auto = FALSE;	/* Make it manual */
			bcopy(vm_rfc1048, hp->vm_cookie, 4);
		    } else if (!strncmp(*symbol, "cmu", 3)) {
			hp->flags.vm_auto = FALSE;	/* Make it manual */
			bcopy(vm_cmu, hp->vm_cookie, 4);
		    } else {
			if (prs_inetaddr(symbol, &value) < 0) {
			    return E_BAD_VM_COOKIE;
			}
			hp->flags.vm_auto = FALSE;	/* Make it manual */
			ustr = hp->vm_cookie;
			insert_u_long(value, &ustr);
		    }
		    hp->flags.vendor_magic = TRUE;
		    break;
		case OP_DELETION:
		    hp->flags.vendor_magic = FALSE;
		    break;
		case OP_BOOLEAN:
		    hp->flags.vm_auto = TRUE;
		    hp->flags.vendor_magic = TRUE;
		    break;
	    }
	    break;

	case SYM_SIMILAR_ENTRY:
	    switch (optype) {
		case OP_ADDITION:
		    fill_defaults(hp, symbol);
		    break;
		default:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_NAME_SWITCH:
	    switch (optype) {
		case OP_ADDITION:
		    return E_SYNTAX_ERROR;
		case OP_DELETION:
		    hp->flags.send_name = FALSE;
		    hp->flags.name_switch = FALSE;
		    break;
		case OP_BOOLEAN:
		    hp->flags.send_name = TRUE;
		    hp->flags.name_switch = TRUE;
		    break;
	    }
	    break;

	case SYM_BOOTSIZE:
	    switch (optype) {
		case OP_ADDITION:
		    if (!strncmp(*symbol, "auto", 4)) {
			hp->flags.bootsize = TRUE;
			hp->flags.bootsize_auto = TRUE;
		    } else {
			hp->bootsize = (unsigned int) get_u_long(symbol);
			hp->flags.bootsize = TRUE;
			hp->flags.bootsize_auto = FALSE;
		    }
		    break;
		case OP_DELETION:
		    hp->flags.bootsize = FALSE;
		    break;
		case OP_BOOLEAN:
		    hp->flags.bootsize = TRUE;
		    hp->flags.bootsize_auto = TRUE;
		    break;
	    }
	    break;

	case SYM_BOOT_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (prs_inetaddr(symbol, &value) < 0) {
			return E_BAD_BOOT_SERVER;
		    } else {
			hp->bootserver.s_addr = value;
			hp->flags.bootserver = TRUE;
		    }
		    break;
		case OP_DELETION:
		    hp->flags.bootserver = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_TFTPDIR:
	    switch (optype) {
		case OP_ADDITION:
		    if (ss = get_shared_string(symbol)) {
			if ((ss->string)[0] == '/') {
			    if (hp->tftpdir) {
				del_string(hp->tftpdir);
			    }
			    hp->tftpdir = ss;
			    hp->flags.tftpdir = TRUE;
			} else {
			    return E_BAD_TFTPDIR;
			}
		    } else {
			return E_BAD_TFTPDIR;
		    }
		    break;
		case OP_DELETION:
		    if (hp->tftpdir) {
			del_string(hp->tftpdir);
		    }
		    hp->tftpdir = NULL;
		    hp->flags.tftpdir = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_DUMPFILE:
	    switch (optype) {
		case OP_ADDITION:
		    if (ss = get_shared_string(symbol)) {
			if (hp->dumpfile) {
			    del_string(hp->dumpfile);
			}
			hp->dumpfile = ss;
			hp->flags.dumpfile = TRUE;
		    }
		    break;
		case OP_DELETION:
		    if (hp->dumpfile) {
			del_string(hp->dumpfile);
		    }
		    hp->dumpfile = NULL;
		    hp->flags.dumpfile = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_DOMAIN_NAME:
	    switch (optype) {
		case OP_ADDITION:
		    if (ss = get_shared_string(symbol)) {
			if (hp->domainname) {
			    del_string(hp->domainname);
			}
			hp->domainname = ss;
			hp->flags.domainname = TRUE;
		    }
		    break;
		case OP_DELETION:
		    if (hp->domainname) {
			del_string(hp->domainname);
		    }
		    hp->domainname = NULL;
		    hp->flags.domainname = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	case SYM_ROOTPATH:
	    switch (optype) {
		case OP_ADDITION:
		    if (ss = get_shared_string(symbol)) {
			if (hp->rootpath) {
			    del_string(hp->rootpath);
			}
			hp->rootpath = ss;
			hp->flags.rootpath = TRUE;
		    }
		    break;
		case OP_DELETION:
		    if (hp->rootpath) {
			del_string(hp->rootpath);
		    }
		    hp->rootpath = NULL;
		    hp->flags.rootpath = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;
            
	case SYM_SWAP_SERVER:
	    switch (optype) {
		case OP_ADDITION:
		    if (prs_inetaddr(symbol, &value) < 0) {
			return E_BAD_SWAP_SERVER;
		    } else {
			hp->swapserver.s_addr = value;
			hp->flags.swap_server = TRUE;
		    }
		    break;
		case OP_DELETION:
		    hp->flags.swap_server = FALSE;
		    break;
		case OP_BOOLEAN:
		    return E_SYNTAX_ERROR;
	    }
	    break;

	default:
	    return E_UNKNOWN_SYMBOL;
    }
    return SUCCESS;
}



/*
 * Read a string from the buffer indirectly pointed to through "src" and
 * move it into the buffer pointed to by "dest".  A pointer to the maximum
 * allowable length of the string (including null-terminator) is passed as
 * "length".  The actual length of the string which was read is returned in
 * the unsigned integer pointed to by "length".  This value is the same as
 * that which would be returned by applying the strlen() function on the
 * destination string (i.e the terminating null is not counted as a
 * character).  Trailing whitespace is removed from the string.  For
 * convenience, the function returns the new value of "dest".
 *
 * The string is read until the maximum number of characters, an unquoted
 * colon (:), or a null character is read.  The return string in "dest" is
 * null-terminated.
 */

PRIVATE char *get_string(src, dest, length)
char **src, *dest;
unsigned *length;
{
    int n, len, quoteflag;

    quoteflag = FALSE;
    n = 0;
    len = *length - 1;
    while ((n < len) && (**src)) {
	if (!quoteflag && (**src == ':')) {
	     break;
	}
	if (**src == '"') {
	    (*src)++;
	    quoteflag = !quoteflag;
	    continue;
	}
	if (**src == '\\') {
	    (*src)++;
	    if (! **src) {
		break;
	    }
	}
	*dest++ = *(*src)++;
	n++;
    }

    /*
     * Remove that troublesome trailing whitespace. . .
     */
    while ((n > 0) && isspace(dest[-1])) {
	dest--;
	n--;
    }

    *dest = '\0';
    *length = n;
    return dest;
}



/*
 * Read the string indirectly pointed to by "src", update the caller's
 * pointer, and return a pointer to a malloc'ed shared_string structure
 * containing the string.
 *
 * The string is read using the same rules as get_string() above.
 */

PRIVATE struct shared_string *get_shared_string(src)
char **src;
{
    char retstring[MAXSTRINGLEN];
    struct shared_string *s;
    unsigned length;

    length = sizeof(retstring);
    (void) get_string(src, retstring, &length);

    s = (struct shared_string *) smalloc(sizeof(struct shared_string)
					 + length);
    s->linkcount = 1;
    strcpy(s->string, retstring);

    return s;
}



/*
 * Load RFC1048 generic information directly into a memory buffer.
 *
 * "src" indirectly points to the ASCII representation of the generic data.
 * "dest" points to a string structure which is updated to point to a new
 * string with the new data appended to the old string.  The old string is
 * freed.
 *
 * The given tag value is inserted with the new data.
 *
 * The data may be represented as either a stream of hexadecimal numbers
 * representing bytes (any or all bytes may optionally start with '0x' and
 * be separated with periods ".") or as a quoted string of ASCII
 * characters (the quotes are required).
 */

PRIVATE void process_generic(src, dest, tagvalue)
char **src;
struct shared_bindata **dest;
byte tagvalue;
{
    byte tmpbuf[MAXBUFLEN];
    byte *str;
    struct shared_bindata *bdata;
    int newlength, oldlength;

    str = tmpbuf;
    *str++ = tagvalue;	    /* Store tag value */
    str++;		    /* Skip over length field */
    if ((*src)[0] == '"') {				/* ASCII data */
	newlength = sizeof(tmpbuf) - 2;	/* Set maximum allowed length */
	(void) get_string(src, str, &newlength);
    } else {						/* Numeric data */
	newlength = 0;
	while (newlength < sizeof(tmpbuf) - 2) {
	    if (interp_byte(src, str++) < 0) {
		break;
	    } else {
		newlength++;
	    }
	    if (**src == '.') {
		(*src)++;
	    }
	}    
    }
    tmpbuf[1] = (byte) (newlength & 0xFF);
    oldlength = ((*dest)->length);
    bdata = (struct shared_bindata *) smalloc(sizeof(struct shared_bindata)
						+ oldlength + newlength + 1);
    if (oldlength > 0) {
	bcopy((*dest)->data, bdata->data, oldlength);
    }
    bcopy(tmpbuf, bdata->data + oldlength, newlength + 2);
    bdata->length = oldlength + newlength + 2;
    bdata->linkcount = 1;
    if (*dest) {
	del_bindata(*dest);
    }
    *dest = bdata;
}



/*
 * Verify that the given string makes sense as a hostname (according to
 * Appendix 1, page 29 of RFC882).
 *
 * Return TRUE for good names, FALSE otherwise.
 */

PRIVATE boolean goodname(hostname)
register char *hostname;
{
    do {
	if (!isalpha(*hostname++)) {	/* First character must be a letter */
	    return FALSE;
	}
	while (isalnum(*hostname) || (*hostname == '-')) {
	    hostname++;			/* Alphanumeric or a hyphen */
	}
	if (!isalnum(hostname[-1])) {	/* Last must be alphanumeric */
	    return FALSE;
	}
	if (*hostname == '\0') {	/* Done? */
	    return TRUE;
	}
    } while (*hostname++ == '.');	/* Dot, loop for next label */

    return FALSE;			/* If it's not a dot, lose */
}



/*
 * Null compare function -- always returns FALSE so an element is always
 * inserted into a hash table (i.e. there is never a collision with an
 * existing element).
 */

PRIVATE boolean nullcmp(host1, host2)
struct host *host1, *host2;
{
    return FALSE;
}


/*
 * Function for comparing a string with the hostname field of a host
 * structure.
 */

PRIVATE boolean nmcmp(name, hp)
char *name;
struct host *hp;
{
    return !strcmp(name, hp->hostname->string);
}


/*
 * Compare function to determine whether two hardware addresses are
 * equivalent.  Returns TRUE if "host1" and "host2" are equivalent, FALSE
 * otherwise.
 *
 * If the hardware addresses of "host1" and "host2" are identical, but
 * they are on different IP subnets, this function returns FALSE.
 *
 * This function is used when inserting elements into the hardware address
 * hash table.
 */

PRIVATE boolean hwinscmp(host1, host2)
struct host *host1, *host2;
{
    if (host1->htype != host2->htype) {
	return FALSE;
    }
    if (bcmp(host1->haddr, host2->haddr, haddrlength(host1->htype))) {
	return FALSE;
    }
    if ((host1->subnet_mask.s_addr) == (host2->subnet_mask.s_addr)) {
	if (((host1->iaddr.s_addr) & (host1->subnet_mask.s_addr)) !=
	    ((host2->iaddr.s_addr) & (host2->subnet_mask.s_addr))) {
	    return FALSE;
	}
    }
    return TRUE;
}



/*
 * Process the "similar entry" symbol.
 *
 * The host specified as the value of the "tc" symbol is used as a template
 * for the current host entry.  Symbol values not explicitly set in the
 * current host entry are inferred from the template entry.
 */

PRIVATE void fill_defaults(hp, src)
struct host *hp;
char **src;
{
    unsigned tlen, hashcode;
    struct host *hp2, thp;
    char tstring[MAXSTRINGLEN];

    tlen = sizeof(tstring);
    (void) get_string(src, tstring, &tlen);
    if (goodname(tstring)) {
	hashcode = hash_HashFunction(tstring, tlen);
	hp2 = (struct host *) hash_Lookup(nmhashtable, hashcode, nmcmp,
					  tstring);
    } else {
	thp.iaddr.s_addr = inet_addr(tstring);
	hashcode = hash_HashFunction(&(thp.iaddr.s_addr), 4);
	hp2 = (struct host *) hash_Lookup(iphashtable, hashcode, iplookcmp,
					  &thp);
    }
    if (hp2 == NULL) {
	report(LOG_ERR, "can't find tc=\"%s\"\n", tstring);
    } else {
	/*
	 * Assignments inside "if" conditionals are intended here.
	 */
	if (!hp->flags.cookie_server) {
	    if (hp->flags.cookie_server = hp2->flags.cookie_server) {
		hp->cookie_server = hp2->cookie_server;
		(hp->cookie_server->linkcount)++;
	    }
	}
	if (!hp->flags.domain_server) {
	    if (hp->flags.domain_server = hp2->flags.domain_server) {
		hp->domain_server = hp2->domain_server;
		(hp->domain_server->linkcount)++;
	    }
	}
	if (!hp->flags.gateway) {
	    if (hp->flags.gateway = hp2->flags.gateway) {
		hp->gateway = hp2->gateway;
		(hp->gateway->linkcount)++;
	    }
	}
	if (!hp->flags.impress_server) {
	    if (hp->flags.impress_server = hp2->flags.impress_server) {
		hp->impress_server = hp2->impress_server;
		(hp->impress_server->linkcount)++;
	    }
	}
	if (!hp->flags.log_server) {
	    if (hp->flags.log_server = hp2->flags.log_server) {
		hp->log_server = hp2->log_server;
		(hp->log_server->linkcount)++;
	    }
	}
	if (!hp->flags.lpr_server) {
	    if (hp->flags.lpr_server = hp2->flags.lpr_server) {
		hp->lpr_server = hp2->lpr_server;
		(hp->lpr_server->linkcount)++;
	    }
	}
	if (!hp->flags.name_server) {
	    if (hp->flags.name_server = hp2->flags.name_server) {
		hp->name_server = hp2->name_server;
		(hp->name_server->linkcount)++;
	    }
	}
	if (!hp->flags.rlp_server) {
	    if (hp->flags.rlp_server = hp2->flags.rlp_server) {
		hp->rlp_server = hp2->rlp_server;
		(hp->rlp_server->linkcount)++;
	    }
	}
	if (!hp->flags.time_server) {
	    if (hp->flags.time_server = hp2->flags.time_server) {
		hp->time_server = hp2->time_server;
		(hp->time_server->linkcount)++;
	    }
	}
	if (!hp->flags.homedir) {
	    if (hp->flags.homedir = hp2->flags.homedir) {
		hp->homedir = hp2->homedir;
		(hp->homedir->linkcount)++;
	    }
	}
	if (!hp->flags.bootfile) {
	    if (hp->flags.bootfile = hp2->flags.bootfile) {
		hp->bootfile = hp2->bootfile;
		(hp->bootfile->linkcount)++;
	    }
	}
	if (!hp->flags.generic) {
	    if (hp->flags.generic = hp2->flags.generic) {
		hp->generic = hp2->generic;
		(hp->generic->linkcount)++;
	    }
	}
	if (!hp->flags.vendor_magic) {
	    hp->flags.vm_auto = hp2->flags.vm_auto;
	    if (hp->flags.vendor_magic = hp2->flags.vendor_magic) {
		bcopy(hp2->vm_cookie, hp->vm_cookie, 4);
	    }
	}
	if (!hp->flags.name_switch) {
	    if (hp->flags.name_switch = hp2->flags.name_switch) {
		hp->flags.send_name = hp2->flags.send_name;
	    }
	}
	if (!hp->flags.htype) {
	    if (hp->flags.htype = hp2->flags.htype) {
		hp->htype = hp2->htype;
	    }
	}
	if (!hp->flags.time_offset) {
	    if (hp->flags.time_offset = hp2->flags.time_offset) {
		hp->flags.timeoff_auto = hp2->flags.timeoff_auto;
		hp->time_offset = hp2->time_offset;
	    }
	}
	if (!hp->flags.subnet_mask) {
	    if (hp->flags.subnet_mask = hp2->flags.subnet_mask) {
		hp->subnet_mask.s_addr = hp2->subnet_mask.s_addr;
	    }
	}
	if (!hp->flags.swap_server) {
	    if (hp->flags.swap_server = hp2->flags.swap_server) {
		hp->swapserver.s_addr = hp2->swapserver.s_addr;
	    }
	}
	if (!hp->flags.bootsize) {
	    if (hp->flags.bootsize = hp2->flags.bootsize) {
		hp->flags.bootsize_auto = hp2->flags.bootsize_auto;
		hp->bootsize = hp2->bootsize;
	    }
	}
	if (!hp->flags.tftpdir) {
	    if (hp->flags.tftpdir = hp2->flags.tftpdir) {
		hp->tftpdir = hp2->tftpdir;
		(hp->tftpdir->linkcount)++;
	    }
	}
	if (!hp->flags.rootpath) {
	    if (hp->flags.rootpath = hp2->flags.rootpath) {
		hp->rootpath = hp2->rootpath;
		(hp->rootpath->linkcount)++;
	    }
	}
	if (!hp->flags.domainname) {
	    if (hp->flags.domainname = hp2->flags.domainname) {
		hp->domainname = hp2->domainname;
		(hp->domainname->linkcount)++;
	    }
	}
	if (!hp->flags.dumpfile) {
	    if (hp->flags.dumpfile = hp2->flags.dumpfile) {
		hp->dumpfile = hp2->dumpfile;
		(hp->dumpfile->linkcount)++;
	    }
	}
    }
}



/*
 * This function adjusts the caller's pointer to point just past the
 * first-encountered colon.  If it runs into a null character, it leaves
 * the pointer pointing to it.
 */

PRIVATE void adjust(s)
char **s;
{
    register char *t;

    t = *s;
    while (*t && (*t != ':')) {
	t++;
    }
    if (*t) {
	t++;
    }
    *s = t;
}




/*
 * This function adjusts the caller's pointer to point to the first
 * non-whitespace character.  If it runs into a null character, it leaves
 * the pointer pointing to it.
 */

PRIVATE void eat_whitespace(s)
char **s;
{
    register char *t;

    t = *s;
    while (*t && isspace(*t)) {
	t++;
    }
    *s = t;
}



/*
 * This function converts the given string to all lowercase.
 */

PRIVATE void makelower(s)
char *s;
{
    while (*s) {
	if (isupper(*s)) {
	    *s = tolower(*s);
	}
	s++;
    }
}



/*
 *
 *	N O T E :
 *
 *	In many of the functions which follow, a parameter such as "src" or
 *	"symbol" is passed as a pointer to a pointer to something.  This is
 *	done for the purpose of letting the called function update the
 *	caller's copy of the parameter (i.e. to effect call-by-reference
 *	parameter passing).  The value of the actual parameter is only used
 *	to locate the real parameter of interest and then update this indirect
 *	parameter.
 *
 *	I'm sure somebody out there won't like this. . . .
 *
 *
 */



/*
 * "src" points to a character pointer which points to an ASCII string of
 * whitespace-separated IP addresses.  A pointer to an in_addr_list
 * structure containing the list of addresses is returned.  NULL is
 * returned if no addresses were found at all.  The pointer pointed to by
 * "src" is updated to point to the first non-address (illegal) character.
 */

PRIVATE struct in_addr_list *get_addresses(src)
char **src;
{
    struct in_addr tmpaddrlist[MAXINADDRS];
    struct in_addr *address1, *address2;
    struct in_addr_list *result;
    unsigned addrcount, totalsize;

    address1 = tmpaddrlist;
    for (addrcount = 0; addrcount < MAXINADDRS; addrcount++) {
	while (**src && isspace(**src)) {	/* Skip whitespace */
	    (*src)++;
	}
	if (! **src) {				/* Quit if nothing more */
	    break;
	}
	if (prs_inetaddr(src, &(address1->s_addr)) < 0) {
	    break;
	}
	address1++;			/* Point to next address slot */
    }
    if (addrcount < 1) {
	result = NULL;
    } else {
	totalsize = sizeof(struct in_addr_list)
		    + (addrcount - 1) * sizeof(struct in_addr);
	result = (struct in_addr_list *) smalloc(totalsize);
	result->linkcount = 1;
	result->addrcount = addrcount;
	address1 = tmpaddrlist;
	address2 = result->addr;
	for (; addrcount > 0; addrcount--) {
	    address2->s_addr = address1->s_addr;
	    address1++;
	    address2++;
	}
    }
    return result;
}



/*
 * prs_inetaddr(src, result)
 *
 * "src" is a value-result parameter; the pointer it points to is updated
 * to point to the next data position.   "result" points to an unsigned long
 * in which an address is returned.
 *
 * This function parses the IP address string in ASCII "dot notation" pointed
 * to by (*src) and places the result (in network byte order) in the unsigned
 * long pointed to by "result".  For malformed addresses, -1 is returned,
 * (*src) points to the first illegal character, and the unsigned long pointed
 * to by "result" is unchanged.  Successful calls return 0.
 */

PRIVATE prs_inetaddr(src, result)
char **src;
u_long *result;
{
    register u_long value;
    u_long parts[4], *pp = parts;
    int n;

    if (!isdigit(**src)) {
	return -1;
    }
loop:
    value = get_u_long(src);
    if (**src == '.') {
	/*
	 * Internet format:
	 *	a.b.c.d
	 *	a.b.c	(with c treated as 16-bits)
	 *	a.b	(with b treated as 24 bits)
	 */
	if (pp >= parts + 4) {
	    return (-1);
	}
	*pp++ = value;
	(*src)++;
	goto loop;
    }
    /*
     * Check for trailing characters.
     */
    if (**src && !(isspace(**src) || (**src == ':'))) {
	return (-1);
    }
    *pp++ = value;
    /*
     * Construct the address according to
     * the number of parts specified.
     */
    n = pp - parts;
    switch (n) {
	case 1:				/* a -- 32 bits */
	    value = parts[0];
	    break;
	case 2:				/* a.b -- 8.24 bits */
	    value = (parts[0] << 24) | (parts[1] & 0xFFFFFF);
	    break;
	case 3:				/* a.b.c -- 8.8.16 bits */
	    value = (parts[0] << 24) | ((parts[1] & 0xFF) << 16) |
		    (parts[2] & 0xFFFF);
	    break;
	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
	    value = (parts[0] << 24) | ((parts[1] & 0xFF) << 16) |
		    ((parts[2] & 0xFF) << 8) | (parts[3] & 0xFF);
	    break;
	default:
	    return (-1);
    }
    *result = htonl(value);
    return (0);
}



/*
 * "src" points to a pointer which in turn points to a hexadecimal ASCII
 * string.  This string is interpreted as a hardware address and returned
 * as a pointer to the actual hardware address, represented as an array of
 * bytes.
 *
 * The ASCII string must have the proper number of digits for the specified
 * hardware type (e.g. twelve digits for a 48-bit Ethernet address).
 * Two-digit sequences (bytes) may be separated with periods (.)  and/or
 * prefixed with '0x' for readability, but this is not required.
 *
 * For bad addresses, the pointer which "src" points to is updated to point
 * to the start of the first two-digit sequence which was bad, and the
 * function returns a NULL pointer.
 */

PRIVATE byte *prs_haddr(src, htype)
char **src;
byte htype;
{
    static byte haddr[MAXHADDRLEN];
    byte *hptr;
    unsigned hlen;

    hlen = haddrlength(htype);		/* Get length of this address type */
    hptr = haddr;
    while  (hptr < haddr + hlen) {
	if (**src == '.') {
	    (*src)++;
	}
	if (interp_byte(src, hptr++) < 0) {
	    return NULL;
	}
    }
    return haddr;       
}



/*
 * "src" is a pointer to a character pointer which in turn points to a
 * hexadecimal ASCII representation of a byte.  This byte is read, the
 * character pointer is updated, and the result is deposited into the
 * byte pointed to by "retbyte".
 *
 * The usual '0x' notation is allowed but not required.  The number must be
 * a two digit hexadecimal number.  If the number is invalid, "src" and
 * "retbyte" are left untouched and -1 is returned as the function value.
 * Successful calls return 0.
 */

PRIVATE int interp_byte(src, retbyte)
char **src;
byte *retbyte;
{
    int v;

    if ((*src)[0] == '0' && (*src)[1] == 'x' || (*src)[1] == 'X') {
	(*src) += 2;	/* allow 0x for hex, but don't require it */
    }
    if (!isxdigit((*src)[0]) || !isxdigit((*src)[1])) {
	return -1;
    }
    if (sscanf(*src, "%2x", &v) != 1) {
	return -1;
    }
    (*src) += 2;
    *retbyte = (byte) (v & 0xFF);
    return 0;
}



/*
 * The parameter "src" points to a character pointer which points to an
 * ASCII string representation of an unsigned number.  The number is
 * returned as an unsigned long and the character pointer is updated to
 * point to the first illegal character.
 */

PRIVATE u_long get_u_long(src)
char **src;
{
    register u_long value, base;
    char c;

    /*
     * Collect number up to first illegal character.  Values are specified
     * as for C:  0x=hex, 0=octal, other=decimal.
     */
    value = 0;
    base = 10;
    if (**src == '0') {
	base = 8, (*src)++;
    }
    if (**src == 'x' || **src == 'X') {
	base = 16, (*src)++;
    }
    while (c = **src) {
	if (isdigit(c)) {
	    value = (value * base) + (c - '0');
	    (*src)++;
	    continue;
	}
	if (base == 16 && isxdigit(c)) {
	    value = (value << 4) + ((c & ~32) + 10 - 'A');
	    (*src)++;
	    continue;
	}
	break;
    }
    return value;
}



/*
 * Routines for deletion of data associated with the main data structure.
 */


/*
 * Frees the entire host data structure given.  Does nothing if the passed
 * pointer is NULL.
 */

PRIVATE void free_host(hostptr)
struct host *hostptr;
{
    if (hostptr) {
	del_iplist(hostptr->cookie_server);
	del_iplist(hostptr->domain_server);
	del_iplist(hostptr->gateway);
	del_iplist(hostptr->impress_server);
	del_iplist(hostptr->log_server);
	del_iplist(hostptr->lpr_server);
	del_iplist(hostptr->name_server);
	del_iplist(hostptr->rlp_server);
	del_iplist(hostptr->time_server);
	del_string(hostptr->hostname);
	del_string(hostptr->homedir);
	del_string(hostptr->bootfile);
	del_string(hostptr->tftpdir);
	del_string(hostptr->rootpath);
	del_string(hostptr->domainname);
	del_string(hostptr->dumpfile);
	del_bindata(hostptr->generic);
	free((char *) hostptr);
    }
}



/*
 * Decrements the linkcount on the given IP address data structure.  If the
 * linkcount goes to zero, the memory associated with the data is freed.
 */

PRIVATE void del_iplist(iplist)
struct in_addr_list *iplist;
{
    if (iplist) {
	if (! (--(iplist->linkcount))) {
	    free((char *) iplist);
	}
    }
}



/*
 * Decrements the linkcount on a string data structure.  If the count
 * goes to zero, the memory associated with the string is freed.  Does
 * nothing if the passed pointer is NULL.
 */

PRIVATE void del_string(stringptr)
struct shared_string *stringptr;
{
    if (stringptr) {
	if (! (--(stringptr->linkcount))) {
	    free((char *) stringptr);
	}
    }
}



/*
 * Decrements the linkcount on a shared_bindata data structure.  If the
 * count goes to zero, the memory associated with the data is freed.  Does
 * nothing if the passed pointer is NULL.
 */

PRIVATE void del_bindata(dataptr)
struct shared_bindata *dataptr;
{
    if (dataptr) {
	if (! (--(dataptr->linkcount))) {
	    free((char *) dataptr);
	}
    }
}




/* smalloc()  --  safe malloc()
 *
 * Always returns a valid pointer (if it returns at all).  The allocated
 * memory is initialized to all zeros.  If malloc() returns an error, a
 * message is printed using the report() function and the program aborts
 * with a status of 1.
 */

PRIVATE char *smalloc(nbytes)
unsigned nbytes;
{
    char *retvalue;

    retvalue = malloc(nbytes);
    if (!retvalue) {
	report(LOG_ERR, "malloc() failure -- exiting\n");
	exit(1);	
    }
    bzero(retvalue, nbytes);
    return retvalue;
}
