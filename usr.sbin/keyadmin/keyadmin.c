/*----------------------------------------------------------------------
  key.c:   Main routines for the key(8) tool for manually managing
           cryptographic keys and security associations inside the
           Key Engine of the operating system.

	   Future Enhancements should support:
	        multiple sensitivity levels
		OSPFv2 keys
		RIPv2 keys
		Triple DES for ESP
		DES+MD5 for ESP
 
           Copyright 1995 by Bao Phan, Randall Atkinson, & Dan McDonald,
           All Rights Reserved.  All Rights have been assigned to the
           US Naval Research Laboratory.  The NRL Copyright Notice and
           License govern distribution and use of this software.
----------------------------------------------------------------------*/
/*----------------------------------------------------------------------
#       @(#)COPYRIGHT   1.1a (NRL) 17 August 1995
 
COPYRIGHT NOTICE
 
All of the documentation and software included in this software
distribution from the US Naval Research Laboratory (NRL) are
copyrighted by their respective developers.
 
This software and documentation were developed at NRL by various
people.  Those developers have each copyrighted the portions that they
developed at NRL and have assigned All Rights for those portions to
NRL.  Outside the USA, NRL also has copyright on the software
developed at NRL. The affected files all contain specific copyright
notices and those notices must be retained in any derived work.
 
NRL LICENSE
 
NRL grants permission for redistribution and use in source and binary
forms, with or without modification, of the software and documentation
created at NRL provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:
 
        This product includes software developed at the Information
        Technology Division, US Naval Research Laboratory.
 
4. Neither the name of the NRL nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.
 
THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
The views and conclusions contained in the software and documentation
are those of the authors and should not be interpreted as representing
official policies, either expressed or implied, of the US Naval
Research Laboratory (NRL).

----------------------------------------------------------------------*/

/*
 *	$ANA: keyadmin.c,v 1.2 1996/06/13 19:42:40 wollman Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#ifdef INET6
#include <netinet6/in6.h>
#else /* INET6 */
#if 0
#include <netinet6/in6_types.h>
#endif
#endif /* INET6 */

#ifdef IPSEC
#include <netsec/ipsec.h>
#endif
#include <netkey/key.h>

#ifdef INET6
#include <netinet6/support.h>
#endif

#ifndef INET6 /* XXX */
#define hostname2addr(a, b, c) gethostbyname(a)
#define addr2hostname(a, b, c, d) gethostbyaddr((a), (b), (c))
#endif

int parse7 __P((int, char **));
int parse4 __P((int, char **));
int docmd __P((int, char **));

#define KEYCMD_ARG_MAX  10

#define KEYCMD_LOAD   1
#define KEYCMD_SAVE   2
#define KEYCMD_ADD    3
#define KEYCMD_DEL    4
#define KEYCMD_FLUSH  5
#define KEYCMD_GET    6
#define KEYCMD_DUMP   7
#define KEYCMD_HELP   8
#define KEYCMD_EXIT   9
#define KEYCMD_SHELL  10

struct nametonum {
  char *name;
  int num;
  int flags;
};

char parse7usage[] = "<type> <spi> <src> <dst> <transform> <key> [iv]";
char parse4usage[] = "<type> <spi> <src> <dst>";

struct keycmd {
  char *name;
  int num;
  int (*parse) __P((int, char **));
  char *usage;
  char *help;
} keycmds[] = {
  { "add", KEYCMD_ADD, parse7, parse7usage, 
      "Adds a specific key entry to the kernel key table." },
  { "del", KEYCMD_DEL, parse4, parse4usage, 
      "Removes a specific key entry from the kernel key table." },
  { "get", KEYCMD_GET, parse4, parse4usage,
      "Retrieves a key entry from the kernel key table." },
  { "dump", KEYCMD_DUMP, NULL, " ",
      "Retrieves all key entries from the kernel key table." },
  { "load", KEYCMD_LOAD, NULL, "{ <filename> | - }",
      "Loads keys from a file or stdin into the kernel key table." },
  { "save", KEYCMD_SAVE, NULL, "{ <filename> | - }",
      "Saves keys from the kernel key table to a file or stdout." },
  { "help", KEYCMD_HELP, NULL, "[command]",
      "Provides limited on-line help. Read the man page for more." },
  { "flush", KEYCMD_FLUSH, NULL, NULL,
      "Clears the kernel key table." },
  { "!", KEYCMD_SHELL, NULL, "[command]",
      "Executes a subshell." },
  { "exit", KEYCMD_EXIT, NULL, NULL,
      "Exits the program." },
  { "quit", KEYCMD_EXIT, NULL, NULL,
      "Exits the program." },
  { NULL, 0, NULL, NULL, NULL }
};

/* flags: index into algorithmtabs */

struct nametonum keytypes[] = {
#ifdef IPSEC
  { "ah", KEY_TYPE_AH, 0 },
  { "esp", KEY_TYPE_ESP, 1 },
#endif
  { "rsvp", KEY_TYPE_RSVP, 2 },
  { "ospf", KEY_TYPE_OSPF, 3 },
  { "rip", KEY_TYPE_RIPV2, 4 },
  { NULL, 0, 0 }
};

#ifndef	IPSEC_ALGTYPE_AUTH_MD5	/* XXX */
#define	IPSEC_ALGTYPE_AUTH_MD5	1
#endif

/* flags: true = iv req. */
struct nametonum authalgorithms[] = {
  { "md5", IPSEC_ALGTYPE_AUTH_MD5, 0 },
#ifdef DEBUG
  /* These provide no security but are useful for debugging the
     kernel's ESP and Key Engine and PF_KEY routines */
  { "dummy", IPSEC_ALGTYPE_AUTH_DUMMY, 0 },
  { "cksum", IPSEC_ALGTYPE_AUTH_CKSUM, 0 },
#endif
  { NULL, 0, 0 }
};

#ifndef IPSEC_ALGTYPE_ESP_DES_CBC /* XXX */
#define	IPSEC_ALGTYPE_ESP_DES_CBC	1
#endif

/* flags: true = iv req. */
struct nametonum encralgorithms[] = {
  { "des-cbc", IPSEC_ALGTYPE_ESP_DES_CBC, 1 },
#ifdef DEBUG
  /* These provide no security but are useful for debugging the
     kernel's ESP and Key Engine and PF_KEY routines */
  { "dummy", IPSEC_ALGTYPE_ESP_DUMMY, 0 },
#endif
  { NULL, 0, 0 }
};

/*
 * These numbers should be defined in a header file somewhere
 * and shared with the consuming programs, once someone has
 * actually written the support in those programs (rspvd,
 * gated, and routed).  Probably <protocols/*>...?
 */
#define RSVP_AUTHTYPE_MD5	1	/* XXX */
struct nametonum rsvpalgorithms[] = {
	{ "md5", RSVP_AUTHTYPE_MD5, 0 },
	{ NULL, 0, 0 }
};

#define OSPF_AUTHTYPE_MD5	1 	/* XXX */
struct nametonum ospfalgorithms[] = {
	{ "md5", OSPF_AUTHTYPE_MD5, 0 },
	{ NULL, 0, 0 }
};

#define	RIPV2_AUTHTYPE_MD5	1	/* XXX */
struct nametonum ripalgorithms[] = {
	{ "md5", RIPV2_AUTHTYPE_MD5, 0 },
	{ NULL, 0, 0 }
};

/* NB:  It is the ordering here that determines the values for the
   flags specified above that are used to index into algorithmtabs[] */
struct nametonum *algorithmtabs[] = {
  authalgorithms,
  encralgorithms,
  rsvpalgorithms,
  ospfalgorithms,
  ripalgorithms,
  NULL
};

char buffer[1024] = "\0";

#define MAXRCVBUFSIZE      8 * 1024

char key_message[sizeof(struct key_msghdr) + MAX_SOCKADDR_SZ * 3 
		 + MAX_KEY_SZ + MAX_IV_SZ];
int key_messageptr;

int keysock = -1;

int keygetseqno = 1;
int keydumpseqno = 1;
pid_t mypid;


/*----------------------------------------------------------------------
  help:   Print appropriate help message on stdout.

----------------------------------------------------------------------*/
help(cmdname)
     char *cmdname;
{
  int i;

  if (cmdname) {
    for (i = 0; keycmds[i].name; i++)
      if (!strcasecmp(keycmds[i].name, cmdname))
	break;

    if (!keycmds[i].name) {
      fprintf(stderr, "Unknown command: %s\n", cmdname);
      return 0;
    }

    printf("%s%s%s\n", keycmds[i].name, keycmds[i].usage ? " " : "",
	   keycmds[i].usage ? keycmds[i].usage : "");

    if (keycmds[i].help)
      puts(keycmds[i].help);
  } else {
    for (i = 0; keycmds[i].name; i++)
      printf("\t%s%s%s\n", keycmds[i].name, keycmds[i].usage ? " " : "",
	     keycmds[i].usage ? keycmds[i].usage : "");
  }

  return 0;
}

/*----------------------------------------------------------------------
  usage:  print suitable usage message on stdout.

----------------------------------------------------------------------*/
usage(myname)
     char *myname;
{
  int i;

  fprintf(stderr, "usage: %s <command> <args>\n", myname);
  printf("where <command> is one of:\n");
  help(NULL);
}

/*----------------------------------------------------------------------
  parsekey:  parse argument into a binary key and also record
             the length of the resulting key.
----------------------------------------------------------------------*/
int parsekey(key, keylen, arg)
     u_int8_t *key;
     u_int8_t *keylen;
     char *arg;
{
  int i, j, k, l;
  u_int8_t thisbyte;

  i = strlen(arg);

  if ((i == 1) && (arg[0] == '0')) {
    *keylen = 0;
    return 0;
  }
    
  if ((i % 2)) {
    printf("Invalid number \"%s\"\n", arg);
    return -1;
  }

  j = 1;
  k = l = thisbyte = 0;

  while(arg[k]) {
    if ((arg[k] >= '0') && (arg[k] <= '9'))
      thisbyte |= arg[k] - '0';
    else
      if ((arg[k] >= 'a') && (arg[k] <= 'f'))
	thisbyte |= arg[k] - 'a' + 10;
      else
	if ((arg[k] >= 'A') && (arg[k] <= 'F'))
	  thisbyte |= arg[k] - 'A' + 10;
	else {
	  printf("Invalid hex number \"%s\"\n", arg);
	  return 1;
	}

    if (!(j % 2)) 
      key[l++] = thisbyte;

    thisbyte = (thisbyte << 4);
    j++;
    k++;
  }

  *keylen = l;

  return 0;
}

/*----------------------------------------------------------------------
  parsenametonum:   converts command-line name into index number.

----------------------------------------------------------------------*/
int parsenametonum(tab, arg)
     struct nametonum *tab;
     char *arg;
{
  int i;

  for (i = 0; tab[i].name; i++)
    if (!strcasecmp(tab[i].name, arg))
      return i;

  if (!tab[i].name)
    return -1;
}

/*----------------------------------------------------------------------
  parsesockaddr:  Convert hostname arg into an appropriate sockaddr.

----------------------------------------------------------------------*/
int parsesockaddr(sockaddr, arg)
     struct sockaddr *sockaddr;
     char *arg;
{
  struct hostent *hostent;
  struct in_addr in_addr, *in_addrp;
#ifdef INET6
  struct in_addr6 in_addr6, *in_addr6p;
#endif /* INET6 */

  if (hostent = hostname2addr(arg, AF_INET, 0))
    if ((hostent->h_addrtype == AF_INET) && 
	(hostent->h_length == sizeof(struct in_addr))) {
      in_addrp = ((struct in_addr *)hostent->h_addr_list[0]);
      goto fillin4;
    }
  
  if (ascii2addr(AF_INET, arg, (char *)&in_addr) == 
      sizeof(struct in_addr)) {
    in_addrp = &in_addr;
    goto fillin4;
  }

#ifdef INET6
  if (hostent = hostname2addr(arg, AF_INET6))
    if ((hostent->h_addrtype == AF_INET6) && 
	(hostent->h_length == sizeof(struct in_addr6))) {
      in_addr6p = ((struct in_addr6 *)hostent->h_addr_list[0]);
      goto fillin6;
    }
  
  if (ascii2addr(AF_INET6, arg, (char *)&in_addr6)
      == sizeof(struct in_addr6)) {
    in_addr6p = &in_addr6;
    goto fillin6;
  }
#endif /* INET6 */

  fprintf(stderr,"Unknown host \"%s\"\n", arg);
  return 1;

 fillin4:
  bzero(sockaddr, sizeof(struct sockaddr_in));
  sockaddr->sa_len = sizeof(struct sockaddr_in);
  sockaddr->sa_family = AF_INET;
  ((struct sockaddr_in *)sockaddr)->sin_addr = *in_addrp;
  return 0;

#ifdef INET6
 fillin6:
  bzero(sockaddr, sizeof(struct sockaddr_in6));
  sockaddr->sa_len = sizeof(struct sockaddr_in6);
  sockaddr->sa_family = AF_INET6;
  ((struct sockaddr_in6 *)sockaddr)->sin6_addr = *in_addr6p;
  return 0;
#endif /* INET6 */
}

/*----------------------------------------------------------------------
  dummyfromaddr:  Creates a zeroed sockaddr of family af.

----------------------------------------------------------------------*/
int dummyfromaddr(sa, af)
     struct sockaddr *sa;
     int af;
{
  int size;
#ifdef INET6
  size = (af == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
#else /* INET6 */
  size = sizeof(struct sockaddr_in);
#endif /* INET6 */
  bzero((char *)sa, size);
  sa->sa_family = af;
  sa->sa_len = size;
}

/* 
 * Macros to ensure word alignment.
 */
#define ROUNDUP(a) \
  ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) \
  { x += ROUNDUP(n); }


/*----------------------------------------------------------------------
  parse4:  parse keytype, spi, src addr, and dest addr from argv (command line)
           and stick in structure pointed to by key_messageptr.
----------------------------------------------------------------------*/
int parse4(argc, argv)
     int argc;
     char *argv[];
{
  int i;

  if (argc < 4)
    return 1;

  if ((i = parsenametonum(keytypes, argv[0])) < 0)
    return i;

  ((struct key_msghdr *)key_message)->type = keytypes[i].num;

  /* Should we zero check? */
  ((struct key_msghdr *)key_message)->spi = atoi(argv[1]);

  if (parsesockaddr(key_message + key_messageptr, argv[2]) != 0)
    return 1;
  ADVANCE(key_messageptr, ((struct sockaddr *)(key_message +
					       key_messageptr))->sa_len);
  
  if (parsesockaddr(key_message + key_messageptr, argv[3]) != 0)
    return 1;
  ADVANCE(key_messageptr, ((struct sockaddr *)(key_message +
					       key_messageptr))->sa_len);

  /*
   *  We need to put a dummy from address since the kernel expects
   *  this to be in the message.
   */
#ifdef INET6
  dummyfromaddr(key_message + key_messageptr, AF_INET6);
#else /* INET6 */
  dummyfromaddr(key_message + key_messageptr, AF_INET);
#endif /* INET6 */
  ADVANCE(key_messageptr, ((struct sockaddr *)(key_message +
					       key_messageptr))->sa_len);

  return 0;
}

/*----------------------------------------------------------------------
  parse7:  parse keytype, spi, src addr, dest addr, alg type, key, and iv
           from argv (command line)
           and stick in structure pointed to by key_messageptr.
----------------------------------------------------------------------*/
int parse7(argc, argv)
     int argc;
     char *argv[];
{
  int i, j;

  if (argc < 6)
    return 1;

  if ((i = parsenametonum(keytypes, argv[0])) < 0)
    return i;

  ((struct key_msghdr *)key_message)->type = keytypes[i].num;

/* Should we zero check? */
  ((struct key_msghdr *)key_message)->spi = atoi(argv[1]);

  if (parsesockaddr(key_message + key_messageptr, argv[2]) != 0)
    return 1;
  ADVANCE(key_messageptr, ((struct sockaddr *)(key_message +
					       key_messageptr))->sa_len);

  if (parsesockaddr(key_message + key_messageptr, argv[3]) != 0)
    return 1;
  ADVANCE(key_messageptr, ((struct sockaddr *)(key_message +
					       key_messageptr))->sa_len);

  /*
   *  We need to put a dummy from address since the kernel expects
   *  this to be in the message.
   */
#ifdef INET6
  dummyfromaddr(key_message + key_messageptr, AF_INET6);
#else /* INET6 */
  dummyfromaddr(key_message + key_messageptr, AF_INET);
#endif /* INET6 */
  ADVANCE(key_messageptr, ((struct sockaddr *)(key_message +
					       key_messageptr))->sa_len);  

  if ((j = parsenametonum(algorithmtabs[keytypes[i].flags], argv[4])) < 0)
    return j;

  ((struct key_msghdr *)key_message)->algorithm =
        algorithmtabs[keytypes[i].flags][j].num;

  if ((argc < 7) && algorithmtabs[keytypes[i].flags][j].flags)
    return 1;

  if (parsekey(key_message + key_messageptr, 
	   &(((struct key_msghdr *)key_message)->keylen), argv[5]) != 0)
    return 1;
  ADVANCE(key_messageptr, ((struct key_msghdr *)key_message)->keylen);  

  if (argc >= 7) {
    if (parsekey(key_message + key_messageptr,
	     &(((struct key_msghdr *)key_message)->ivlen), argv[6]) != 0)
      return 1;
    ADVANCE(key_messageptr, ((struct key_msghdr *)key_message)->ivlen);
  }

  return 0;
}

/*----------------------------------------------------------------------
  parsecmdline:

----------------------------------------------------------------------*/
int parsecmdline(buffer, argv, argc)
     char *buffer;
     char **argv;
     int *argc;
{
  int i = 0, iargc = 0;
  char *head;

  head = buffer;

  while(buffer[i] && (iargc < KEYCMD_ARG_MAX)) {
    if ((buffer[i] == '\n') || (buffer[i] == '#')) {
      buffer[i] = 0;
      if (*head)
	argv[iargc++] = head;
      break;
    }
    if ((buffer[i] == ' ') || (buffer[i] == '\t')) {
      buffer[i] = 0;
      if (*head)
	argv[iargc++] = head;
      head = &(buffer[++i]);
    } else
      i++;
  };
  argv[iargc] = NULL;
  *argc = iargc;

  return iargc ? 0 : 1;
}


/*----------------------------------------------------------------------
  load:   load keys from file filename into Key Engine.

----------------------------------------------------------------------*/
int load(filename)
     char *filename;
{
  FILE *fh;
  char buffer[1024], *buf, *largv[KEYCMD_ARG_MAX], *c;
  int i, largc, left, line = 0;

  if (strcmp(filename, "-")) {
    if (!(fh = fopen(filename, "r")))
      return -1;
  } else
    fh = stdin;

  largv[0] = "add";

  buf = buffer;
  left = sizeof(buffer);

  while(fgets(buf, left, fh)) {
    line++;
    if (c = strchr(buffer, '\\')) {
      left = (sizeof(buffer) - 1) - (--c - buffer);
      buf = c;
    } else {
      buffer[sizeof(buffer)-1] = 0;

      if ((i = parsecmdline(buffer, &(largv[1]), &largc)) < 0)
	return i;

      if (!i) {
	if (i = docmd(++largc, largv)) {
	  if (i > 0) {
	    fprintf(stderr, "Parse error on line %d of %s.\n", line, filename);
	    return 0;
	  }
          return i;
        }
      }

      buf = buffer;
      left = sizeof(buffer);
    }
  };

  return 0;
}

/*----------------------------------------------------------------------
  parsedata:

----------------------------------------------------------------------*/
int
parsedata(km, kip)
     struct key_msghdr *km;
     struct key_msgdata *kip;
{
  char *cp, *cpmax;
 
  if (!km)
    return (-1);
  if (!(km->key_msglen))
    return (-1);
 
  cp = (caddr_t)(km + 1);
  cpmax = (caddr_t)km + km->key_msglen;
 
#define NEXTDATA(x, n) \
    { x += ROUNDUP(n); if (cp >= cpmax) { fprintf(stderr, "key: kernel returned a truncated message!\n"); return(-1); } }
 
  /* Grab src addr */
  kip->src = (struct sockaddr *)cp;
  if (!kip->src->sa_len)
    return(-1);
 
  NEXTDATA(cp, kip->src->sa_len);
 
  /* Grab dest addr */
  kip->dst = (struct sockaddr *)cp;
  if (!kip->dst->sa_len)
     return(-1);
 
  NEXTDATA(cp, kip->dst->sa_len);
 
  /* Grab from addr */
  kip->from = (struct sockaddr *)cp;
  if (!kip->from->sa_len)
    return(-1);
 
  NEXTDATA(cp, kip->from->sa_len);
 
  /* Grab key */
  if (kip->keylen = km->keylen) {
    kip->key = cp;
    NEXTDATA(cp, km->keylen);
  }
 
  /* Grab iv */
  if (kip->ivlen = km->ivlen)
    kip->iv = cp;

  cp += kip->ivlen;

  printf("key: parsedata: difference=%d\n", cp - cpmax);
  return (0);
}


/*----------------------------------------------------------------------
  printkeyiv:

----------------------------------------------------------------------*/
void printkeyiv(fp, cp, len)
     FILE *fp;
     caddr_t cp;
     int len;
{
  int i;
  for (i=0; i<len; i++)
    fprintf(fp, "%02x", (u_int8_t)*(cp+i));
}

/*----------------------------------------------------------------------
  printsockaddr:

----------------------------------------------------------------------*/
void printsockaddr(fp, sa)
     FILE *fp;
     struct sockaddr *sa;
{
  struct hostent *hp;
  char *addrp;
  int len;

#ifdef INET6
  if (sa->sa_family == AF_INET6) {
    len = sizeof(struct in_addr6); 
    addrp = (char *)&(((struct sockaddr_in6 *)sa)->sin6_addr);
  } else {
#endif /* INET6 */
    len = sizeof(struct in_addr);
    addrp = (char *)&(((struct sockaddr_in *)sa)->sin_addr);
#ifdef INET6
  }
#endif /* INET6 */

  if((hp = addr2hostname(addrp, len, sa->sa_family, 0)) != NULL)
    fprintf(fp, "%s", hp->h_name);
  else
    fprintf(fp, "%s", addr2ascii(sa->sa_family, addrp, len, NULL));
}

/*----------------------------------------------------------------------
  parsenumtoname:

----------------------------------------------------------------------*/
char *
parsenumtoname(tab, num)
     struct nametonum *tab;
     int num;
{
  int i;
  for (i = 0; tab[i].name; i++) {
    if (num == tab[i].num)
      return(tab[i].name);
  }
  return(0);
}

/*----------------------------------------------------------------------
  parsenumtoflag:

----------------------------------------------------------------------*/
int
parsenumtoflag(tab, num)
     struct nametonum *tab;
     int num;
{
  int i;
  for (i = 0; tab[i].name; i++) {
    if (num == tab[i].num)
      return(tab[i].flags);
  }
  return(-1);
}


/*----------------------------------------------------------------------
  printkeymsg:

----------------------------------------------------------------------*/
void printkeymsg(kmp, kdp)
     struct key_msghdr *kmp;
     struct key_msgdata *kdp;
{
  int i;
  char *cp;

  printf("type=%d(%s) ",kmp->type, parsenumtoname(keytypes, kmp->type)); 
  printf("spi=%u ", kmp->spi);
  printf("alogrithm=%u(%s) ", kmp->algorithm,
	 parsenumtoname(algorithmtabs[parsenumtoflag(keytypes, kmp->type)],
			kmp->algorithm));
  printf("state=0x%x ",kmp->state); 

  if (kmp->state != 0) {
    if (kmp->state & K_USED)
      printf("USED ");
    if (kmp->state & K_UNIQUE)
      printf("UNIQUE ");    
    if (kmp->state & K_LARVAL)
      printf("LARVAL ");
    if (kmp->state & K_ZOMBIE)
      printf("ZOMBIE ");
    if (kmp->state & K_DEAD)
      printf("DEAD ");
    if (kmp->state & K_INBOUND)
      printf("INBOUND ");
    if (kmp->state & K_OUTBOUND)
      printf("OUTBOUND");
  }
  printf("\n");
  printf("sensitivity_label=%d ",kmp->label);
  printf("lifetype=%d ",kmp->lifetype);
  printf("lifetime1=%d ",kmp->lifetime1);
  printf("lifetime2=%d\n",kmp->lifetime2);
  printf("key (len=%d):\t",kdp->keylen);
  printkeyiv(stdout, kdp->key, kdp->keylen);
  printf("\n");
  printf("iv (len=%d):\t", kdp->ivlen);
  printkeyiv(stdout, kdp->iv, kdp->ivlen);
  printf("\n");
  printf("src:\t");
  printsockaddr(stdout, (struct sockaddr *)kdp->src);
  printf("\n");
  printf("dst:\t");
  printsockaddr(stdout, (struct sockaddr *)kdp->dst);
  printf("\n");
/*  printf("from:\t");
  printsockaddr(stdout, (struct sockaddr *)kdp->from); */
  printf("\n");
}

/*----------------------------------------------------------------------
  docmd:

----------------------------------------------------------------------*/
int docmd(argc, argv)
     int argc;
     char **argv;
{
  int i, j, seqno;
  int fd;
  FILE *fp;

  if (!argv[0] || !argc)
    return -1;

  if (!argv[0][0])
    return -1;

  bzero(&key_message, sizeof(key_message));
  key_messageptr = sizeof(struct key_msghdr);

  for (i = 0; keycmds[i].name; i++)
    if (!strcasecmp(keycmds[i].name, argv[0]))
      break;

  if (!keycmds[i].name)
    return -1;

  if (keycmds[i].parse)
    if (j = keycmds[i].parse(argc - 1, &(argv[1])))
      return j;

  ((struct key_msghdr *)key_message)->key_msglen = key_messageptr;
  ((struct key_msghdr *)key_message)->key_msgvers = 1;
  ((struct key_msghdr *)key_message)->key_seq = 1;

  switch(keycmds[i].num) {

  case KEYCMD_ADD:
#ifndef NOSANITYCHK
    /*
     * For now, we do sanity check of security association 
     * information here until we find a better place.
     */
    {
      struct key_msghdr *kmp = (struct key_msghdr *)key_message;

      if ((kmp->type == KEY_TYPE_AH || 
	   kmp->type == KEY_TYPE_ESP) && (kmp->spi < 256)) {
	fprintf(stderr, "add: spi must be greater than 255\n");
	return(0);
      }

      if (kmp->type == KEY_TYPE_ESP && 
	  (kmp->algorithm == IPSEC_ALGTYPE_ESP_DES_CBC
#ifdef IPSEC_ALGTYPE_ESP_3DES
	   || kmp->algorithm == IPSEC_ALGTYPE_ESP_3DES
#endif
	   )) {
	if (kmp->keylen != 8) {
	  fprintf(stderr, "add: key must be 8 bytes\n");
	  return (0);
	}
	if (kmp->ivlen != 4 && kmp->ivlen != 8) {
	  fprintf(stderr, "add: iv must be 4 or 8 bytes\n");
	  return (0);
	}
      }

      if (kmp->type == KEY_TYPE_AH &&
	  kmp->algorithm == IPSEC_ALGTYPE_AUTH_MD5 && kmp->keylen == 0) {
	fprintf(stderr, "add: no key specified\n");
	return (0);
      }
    }
#endif
    ((struct key_msghdr *)key_message)->key_msgtype = KEY_ADD;
    if (write(keysock, key_message, 
	      ((struct key_msghdr *)key_message)->key_msglen) != 
	((struct key_msghdr *)key_message)->key_msglen) {
      if (errno == EEXIST)
	fprintf(stderr, "add: security association already exists\n");
      else
	perror("add");
      return -1;
    }
    read(keysock, key_message, sizeof(key_message));
    return (0);

  case KEYCMD_DEL:
    ((struct key_msghdr *)key_message)->key_msgtype = KEY_DELETE;
    if (write(keysock, key_message, 
	      ((struct key_msghdr *)key_message)->key_msglen) != 
	((struct key_msghdr *)key_message)->key_msglen) {
      if (errno == ESRCH) {
	fprintf(stderr, "delete: Security association not found\n");
	return 0;
      } else {
	perror("delete");
	return -1;
      }
    }
    read(keysock, key_message, sizeof(key_message));
    return (0);

  case KEYCMD_GET:
    ((struct key_msghdr *)key_message)->key_msgtype = KEY_GET;
    ((struct key_msghdr *)key_message)->key_seq = seqno = keygetseqno++;
    
    if (write(keysock, key_message, 
	      ((struct key_msghdr *)key_message)->key_msglen) != 
	((struct key_msghdr *)key_message)->key_msglen) {
      if (errno == ESRCH) {
	fprintf(stderr, "get: Security association not found\n");
	return 0;
      } else {
	perror("get");
	return (-1);
      } /* endif ESRCH */
    } /* endif write() */

    {
      int len;
      struct key_msgdata keymsgdata;

      len = sizeof(struct key_msghdr) + MAX_SOCKADDR_SZ * 3 
		 + MAX_KEY_SZ + MAX_IV_SZ;

readmesg:
      if (read(keysock, key_message, len) < 0) {
	perror("read");
	return -1;
      }

      if (!((((struct key_msghdr *)&key_message)->key_pid==mypid)
	    && (((struct key_msghdr *)&key_message)->key_msgtype==KEY_GET)
	    && (((struct key_msghdr *)&key_message)->key_seq==seqno))) {
	fprintf(stderr, ".");
	goto readmesg;
      }

      if (((struct key_msghdr *)&key_message)->key_errno != 0) {
	printf("Error:  kernel reporting errno=%d\n",
	       ((struct key_msghdr *)&key_message)->key_errno);
	return 0;
      }

      if (parsedata((struct key_msghdr *)&key_message, &keymsgdata) < 0) {
	printf("get: can't parse reply\n");
	return -1;
      }
      printf("\n");
      printkeymsg((struct key_msghdr *)&key_message, &keymsgdata);
    }
    return (0);

  case KEYCMD_FLUSH:
    ((struct key_msghdr *)key_message)->key_msgtype = KEY_FLUSH;
    if (write(keysock, key_message, 
	      ((struct key_msghdr *)key_message)->key_msglen) != 
	((struct key_msghdr *)key_message)->key_msglen) {
      perror("write");
      return -1;
    }
    read(keysock, key_message, sizeof(key_message));
    return (0);

  case KEYCMD_HELP:
    return help((argc > 1) ? argv[1] : NULL);

  case KEYCMD_SHELL:
    if (argc > 1) {
      char buffer[1024], *ap, *ep, *c;
      int i;

      ep = buffer + sizeof(buffer) - 1;
      for (i = 1, ap = buffer; (i < argc) && (ap < ep); i++) {
        c = argv[i];
        while ((*(ap++) = *(c++)) && (ap < ep));
        *(ap - 1) = ' ';
      }
      *(ap - 1) = 0;
      system(buffer);
    } else {
      char *c = getenv("SHELL");
      if (!c)
        c = "/bin/sh";
      system(c);
    }
    return 0;

  case KEYCMD_EXIT:
    exit(0);

  case KEYCMD_LOAD:
    if (argc != 2)
      return 1;
    return load(argv[1]);

  case KEYCMD_SAVE:  
    if (argc != 2)
      return 1;
    if (!strcmp(argv[1], "-")) 
      fp = stdout;
    else if ((fd = open(argv[1], O_CREAT | O_RDWR | O_EXCL, 
			S_IRUSR | S_IWUSR)) < 0) {
      perror("open");
      return 1;
    } else if (!(fp = fdopen(fd, "w"))) {
      perror("fdopen");
      return 1;
    }

  case KEYCMD_DUMP:
    ((struct key_msghdr *)key_message)->key_msgtype = KEY_DUMP;
    if (write(keysock, key_message, 
	      ((struct key_msghdr *)key_message)->key_msglen) != 
	((struct key_msghdr *)key_message)->key_msglen) {
      perror("write");
      return -1;
    }

    {
      struct key_msgdata keymsgdata;

readmesg2:
      if (read(keysock, key_message, sizeof(key_message)) < 0) {
	perror("read");
	return -1;
      }

      if (!((((struct key_msghdr *)&key_message)->key_pid==mypid)
	    && (((struct key_msghdr *)&key_message)->key_msgtype==KEY_DUMP))) 
	goto readmesg2;

      /*
       *  Kernel is done sending secassoc if key_seq == 0
       */
      if (((struct key_msghdr *)&key_message)->key_seq == 0) {
	if ((keycmds[i].num == KEYCMD_SAVE) && (fp != stdout)) 
	  fclose(fp);
	return 0;
      }

      if (parsedata((struct key_msghdr *)&key_message, &keymsgdata) < 0) {
	printf("get: can't parse reply\n");
	goto readmesg2;
      }
      if (keycmds[i].num == KEYCMD_SAVE) {
	char *keytype, *algorithm;
	
	keytype = parsenumtoname(keytypes, 
		 ((struct key_msghdr *)&key_message)->type); 

	algorithm = parsenumtoname(algorithmtabs[parsenumtoflag(keytypes, 
			((struct key_msghdr *)&key_message)->type)],
			((struct key_msghdr *)&key_message)->algorithm);

	fprintf(fp, "%s %u ", keytype, 
		((struct key_msghdr *)&key_message)->spi);
	printsockaddr(fp, (struct sockaddr *)(keymsgdata.src));
	fprintf(fp, " ");
	printsockaddr(fp, (struct sockaddr *)(keymsgdata.dst));
	fprintf(fp, " ");
	fprintf(fp, "%s ", algorithm);
	printkeyiv(fp, keymsgdata.key, keymsgdata.keylen);
	fprintf(fp, " ");
	printkeyiv(fp, keymsgdata.iv, keymsgdata.ivlen);
	fprintf(fp, "\n");
      } else 
	printkeymsg((struct key_msghdr *)&key_message, &keymsgdata);
      goto readmesg2;
    }
    return (0);
  }
  return (-1);
}

/*----------------------------------------------------------------------
  main:

----------------------------------------------------------------------*/
int main(argc, argv)
     int argc;
     char *argv[];
{
  int i, j;
  u_long rcvsize;

  if (getuid()) {
    fprintf(stderr, "This program is intended for the superuser only.\n");
    exit(1);
  }

  if (!(keysock = socket(PF_KEY, SOCK_RAW, 0))) {
    perror("socket");
    return -1;
  }

  for (rcvsize = MAXRCVBUFSIZE; rcvsize; rcvsize -= 1024) {
    if (setsockopt(keysock, SOL_SOCKET, SO_RCVBUF, &rcvsize, 
		   sizeof(rcvsize)) <= 0)
      break;
  }

  mypid = getpid();
  if (mypid < 0) {
    perror("getpid");
    return -1;
  }

  if (argc > 1) {
    /*
     * Attempt to do a single command, based on command line arguments.
     */
    if (strcasecmp(argv[1], "add") == 0)
      {
        fprintf(stderr,
	        "Cannot add keys from the command line.  RTFM for why.\n");
	exit(1);
      }
    if (i = docmd(argc - 1, &(argv[1]))) {
      if (i > 0) {
	for (j = 0; keycmds[j].name; j++)
	  if (!strcasecmp(keycmds[j].name, argv[1]))
	    break;

	if (keycmds[j].name) {
	  fprintf(stderr, "usage: %s%s%s\n", keycmds[j].name,
		  keycmds[j].usage ? " " : "",
		  keycmds[j].usage ? keycmds[j].usage : "");
	  exit(1);
	}
      }
      usage(argv[0]);
    }
    return 0;
  }

  {
    char buffer[1024];
    char *iargv[KEYCMD_ARG_MAX], *head;
    int iargc;

    while(1) {
      printf("key> ");
      if (!(fgets(buffer, sizeof(buffer), stdin)))
	return -1;
      buffer[sizeof(buffer)-1] = 0;
      /*
       * get command line, and parse into an argc/argv form.
       */
      if ((i = parsecmdline(buffer, iargv, &iargc)) < 0)
	exit(1);
      if (i > 0)
	continue;
      errno = 0;
      /*
       * given argc/argv, process argument as if it came from the command
       * line.
       */
      if (i = docmd(iargc, iargv)) {
	if (i > 0) {
	  for (j = 0; keycmds[j].name; j++)
	    if (!strcasecmp(keycmds[j].name, iargv[0]))
	      break;
	  
	  if (keycmds[j].name) {
	    fprintf(stderr, "usage: %s%s%s\n", keycmds[j].name,
		    keycmds[j].usage ? " " : "",
		    keycmds[j].usage ? keycmds[j].usage : "");
	  } else
	    i = -1;
	}
	if (i < 0) {
	  if (errno)
	    perror("System error");
	  else
	    fprintf(stderr, "Unrecognized command; ");
	  fprintf(stderr, "Type 'help' if you need help\n");
	};
      };
    };
  };
}

/* EOF */
