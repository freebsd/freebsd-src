/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The game adventure was originally written in Fortran by Will Crowther
 * and Don Woods.  It was later translated to C and enhanced by Jim
 * Gillogly.  This code is derived from software contributed to Berkeley
 * by Jim Gillogly at The Rand Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)hdr.h	8.1 (Berkeley) 5/31/93
 */

/*   ADVENTURE -- Jim Gillogly, Jul 1977
 * This program is a re-write of ADVENT, written in FORTRAN mostly by
 * Don Woods of SAIL.  In most places it is as nearly identical to the
 * original as possible given the language and word-size differences.
 * A few places, such as the message arrays and travel arrays were changed
 * to reflect the smaller core size and word size.  The labels of the
 * original are reflected in this version, so that the comments of the
 * fortran are still applicable here.
 *
 * The data file distributed with the fortran source is assumed to be called
 * "glorkz" in the directory where the program is first run.
 *
 * $FreeBSD$
 */

/* hdr.h: included by c advent files */
#include <sys/types.h>
#include <signal.h>

int datfd;                              /* message file descriptor      */
volatile sig_atomic_t delhit;
int yea;
extern char data_file[];                /* Virtual data file            */

#define TAB     011
#define LF      012
#define FLUSHLINE do { int flushline_ch; while ((flushline_ch = getchar()) != EOF && flushline_ch != '\n'); } while (0)
#define FLUSHLF   while (next()!=LF)

int loc,newloc,oldloc,oldlc2,wzdark,gaveup,kq,k,k2;
char *wd1,*wd2;                         /* the complete words           */
int verb,obj,spk;
extern int blklin;
int saved,savet,mxscor,latncy;

#define SHORT 50                        /* How short is a demo game?    */

#define MAXSTR  20                      /* max length of user's words   */

#define HTSIZE  512                     /* max number of vocab words    */
struct hashtab                          /* hash table for vocabulary    */
{       int val;                        /* word type &index (ktab)      */
	char *atab;                     /* pointer to actual string     */
} voc[HTSIZE];

#define SEED 1815622                    /* "Encryption" seed            */

struct text
#ifdef OLDSTUFF
{       int seekadr;                    /* DATFILE must be < 2**16      */
#endif /* !OLDSTUFF */
{       char *seekadr;                  /* Msg start in virtual disk    */
	int txtlen;                     /* length of msg starting here  */
};

#define RTXSIZ  205
struct text rtext[RTXSIZ];              /* random text messages         */

#define MAGSIZ  35
struct text mtext[MAGSIZ];              /* magic messages               */

int clsses;
#define CLSMAX  12
struct text ctext[CLSMAX];              /* classes of adventurer        */
int cval[CLSMAX];

struct text ptext[101];                 /* object descriptions          */

#define LOCSIZ  141                     /* number of locations          */
struct text ltext[LOCSIZ];              /* long loc description         */
struct text stext[LOCSIZ];              /* short loc descriptions       */

struct travlist                         /* direcs & conditions of travel*/
{       struct travlist *next;          /* ptr to next list entry       */
	int conditions;                 /* m in writeup (newloc / 1000) */
	int tloc;                       /* n in writeup (newloc % 1000) */
	int tverb;                      /* the verb that takes you there*/
} *travel[LOCSIZ],*tkk;                 /* travel is closer to keys(...)*/

int atloc[LOCSIZ];

int  plac[101];                         /* initial object placement     */
int  fixd[101],fixed[101];              /* location fixed?              */

int actspk[35];                         /* rtext msg for verb <n>       */

int cond[LOCSIZ];                       /* various condition bits       */

extern int setbit[16];                  /* bit defn masks 1,2,4,...     */

int hntmax;
int hints[20][5];                       /* info on hints                */
int hinted[20],hintlc[20];

int place[101], prop[101],linkx[201];
int abb[LOCSIZ];

int maxtrs,tally,tally2;                /* treasure values              */

#define FALSE   0
#define TRUE    1

int keys,lamp,grate,cage,rod,rod2,steps,/* mnemonics                    */
	bird,door,pillow,snake,fissur,tablet,clam,oyster,magzin,
	dwarf,knife,food,bottle,water,oil,plant,plant2,axe,mirror,dragon,
	chasm,troll,troll2,bear,messag,vend,batter,
	nugget,coins,chest,eggs,tridnt,vase,emrald,pyram,pearl,rug,chain,
	spices,
	back,look,cave,null,entrnc,dprssn,
	enter, stream, pour,
	say,lock,throw,find,invent;

int chloc,chloc2,dseen[7],dloc[7],      /* dwarf stuff                  */
	odloc[7],dflag,daltlc;

int tk[21],stick,dtotal,attack;
int turns,lmwarn,iwest,knfloc,detail,   /* various flags & counters     */
	abbnum,maxdie,numdie,holdng,dkill,foobar,bonus,clock1,clock2,
	closng,panic,closed,scorng;

int demo,limit;

int at (int objj);
int bug (int n);
void carry (int, int);
void caveclose (void);
void checkhints (void);
void ciao (void);
void closing (void);
u_long crc (const char *ptr, size_t nr);
void crc_start (void);
int dark (void);
void datime (int *d, int *t);
char *decr (const unsigned char, const unsigned char, const unsigned char,
	    const unsigned char, const unsigned char);
void die (int entry);
void done (int entry);
void drop (int object,int where);
void dstroy (int);
int fdwarf (void);
int forced (int locc);
void getin (char **wrd1, char **wrd2);
int here (int objj);
void init (void);
void juggle (int);
int liq (void);
int liqloc (int locc);
int march (void);
void move (int, int);
void mspeak (int);
int pct (int n);
void poof (void);
void pspeak (int m, int skip);
int put (int, int, int);
int ran (int range);
void rdata (void);
int restore (const char *infile);
void rspeak (int);
int save (const char *);
int score (void);
void speak (const struct text *);
int Start (void);
void startup (void);
int toting (int objj);
void trapdel (int sig);
int trdrop (void);
int trfeed (void);
int trfill (void);
int trkill (void);
int tropen (void);
int trsay (void);
int trtake (void);
int trtoss (void);
int vocab (const char *,int,int);
int yes (int x, int y, int z);
int yesm (int x, int y, int z);

/* We need to get a little tricky to avoid strings */
#define DECR(a,b,c,d,e) decr(a+'+',b+'-',c+'#',d+'&',e+'%')

gid_t	egid;
