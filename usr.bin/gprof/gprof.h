/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)gprof.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/gmon.h>

#include <stdio.h>
#include <stdlib.h>

    /*
     *	offset (in bytes) of the code from the entry address of a routine.
     *	(see asgnsamples for use and explanation.)
     */
#define OFFSET_OF_CODE	0

enum opermodes { dummy };
typedef enum opermodes	operandenum;

    /*
     * booleans
     */
typedef int	bool;
#define	FALSE	0
#define	TRUE	1

    /*
     *	Historical scale factor in profil(2)'s algorithm for converting
     *	pc addresses to bucket numbers.  This now just complicates the
     *	scaling and makes bucket:pc densities of more than 1/2 useless.
     */
#define	HISTORICAL_SCALE_2	2

#ifndef EXTERN
#define	EXTERN	extern
#endif

    /*
     *	ticks per second
     */
EXTERN long	hz;

EXTERN size_t	histcounter_size;
EXTERN int	histcounter_type;

EXTERN char	*a_outname;
#define	A_OUTNAME		"a.out"

EXTERN char	*gmonname;
#define	GMONSUM			"gmon.sum"

    /*
     *	a constructed arc,
     *	    with pointers to the namelist entry of the parent and the child,
     *	    a count of how many times this arc was traversed,
     *	    and pointers to the next parent of this child and
     *		the next child of this parent.
     */
struct arcstruct {
    struct nl		*arc_parentp;	/* pointer to parent's nl entry */
    struct nl		*arc_childp;	/* pointer to child's nl entry */
    long		arc_count;	/* num calls from parent to child */
    double		arc_time;	/* time inherited along arc */
    double		arc_childtime;	/* childtime inherited along arc */
    struct arcstruct	*arc_parentlist; /* parents-of-this-child list */
    struct arcstruct	*arc_childlist;	/* children-of-this-parent list */
    struct arcstruct	*arc_next;	/* list of arcs on cycle */
    unsigned short	arc_cyclecnt;	/* num cycles involved in */
    unsigned short	arc_flags;	/* see below */
};
typedef struct arcstruct	arctype;

    /*
     * arc flags
     */
#define	DEADARC	0x01	/* time should not propagate across the arc */
#define	ONLIST	0x02	/* arc is on list of arcs in cycles */

    /*
     * The symbol table;
     * for each external in the specified file we gather
     * its address, the number of calls and compute its share of CPU time.
     */
struct nl {
    const char		*name;		/* the name */
    unsigned long	value;		/* the pc entry point */
    unsigned long	svalue;		/* entry point aligned to histograms */
    double		time;		/* ticks in this routine */
    double		childtime;	/* cumulative ticks in children */
    long		ncall;		/* how many times called */
    long		npropcall;	/* times called by live arcs */
    long		selfcalls;	/* how many calls to self */
    double		propfraction;	/* what % of time propagates */
    double		propself;	/* how much self time propagates */
    double		propchild;	/* how much child time propagates */
    short		printflag;	/* should this be printed? */
    short		flags;		/* see below */
    int			index;		/* index in the graph list */
    int			toporder;	/* graph call chain top-sort order */
    int			cycleno;	/* internal number of cycle on */
    int			parentcnt;	/* number of live parent arcs */
    struct nl		*cyclehead;	/* pointer to head of cycle */
    struct nl		*cnext;		/* pointer to next member of cycle */
    arctype		*parents;	/* list of caller arcs */
    arctype		*children;	/* list of callee arcs */
};
typedef struct nl	nltype;

EXTERN nltype	*nl;			/* the whole namelist */
EXTERN nltype	*npe;			/* the virtual end of the namelist */
EXTERN int	nname;			/* the number of function names */

#define	HASCYCLEXIT	0x08	/* node has arc exiting from cycle */
#define	CYCLEHEAD	0x10	/* node marked as head of a cycle */
#define	VISITED		0x20	/* node visited during a cycle */

    /*
     * The cycle list.
     * for each subcycle within an identified cycle, we gather
     * its size and the list of included arcs.
     */
struct cl {
    int		size;		/* length of cycle */
    struct cl	*next;		/* next member of list */
    arctype	*list[1];	/* list of arcs in cycle */
    /* actually longer */
};
typedef struct cl cltype;

EXTERN arctype	*archead;	/* the head of arcs in current cycle list */
EXTERN cltype	*cyclehead;	/* the head of the list */
EXTERN int	cyclecnt;	/* the number of cycles found */
#define	CYCLEMAX	100	/* maximum cycles before cutting one of them */

    /*
     *	flag which marks a nl entry as topologically ``busy''
     *	flag which marks a nl entry as topologically ``not_numbered''
     */
#define	DFN_BUSY	-1
#define	DFN_NAN		0

    /*
     *	namelist entries for cycle headers.
     *	the number of discovered cycles.
     */
EXTERN nltype	*cyclenl;		/* cycle header namelist */
EXTERN int	ncycle;			/* number of cycles discovered */

    /*
     * The header on the gmon.out file.
     * gmon.out consists of a struct phdr (defined in gmon.h)
     * and then an array of ncnt samples representing the
     * discretized program counter values.
     *
     *	Backward compatible old style header
     */
struct ophdr {
    u_short	*lpc;
    u_short	*hpc;
    int		ncnt;
};

EXTERN int	debug;

    /*
     * Each discretized pc sample has
     * a count of the number of samples in its range
     */
EXTERN double	*samples;

EXTERN unsigned long	s_lowpc;	/* lowpc from the profile file */
EXTERN unsigned long	s_highpc;	/* highpc from the profile file */
/* range profiled, in historical units  */
EXTERN unsigned long	lowpc, highpc;
EXTERN unsigned sampbytes;		/* number of bytes of samples */
EXTERN int	nsamples;		/* number of samples */
/* accumulated time thus far for putprofline */
EXTERN double	actime;
EXTERN double	totime;			/* total time for all routines */
EXTERN double	printtime;		/* total of time being printed */
EXTERN double	scale;			/* scale factor converting samples to pc
				   values: each sample covers scale bytes */
EXTERN unsigned char	*textspace;	/* text space of a.out in core */
/* with -C, minimum cycle size to ignore */
EXTERN int	cyclethreshold;

    /*
     *	option flags, from a to z.
     */
EXTERN bool	aflag;			/* suppress static functions */
EXTERN bool	bflag;			/* blurbs, too */
EXTERN bool	Cflag;			/* find cut-set to eliminate cycles */
EXTERN bool	dflag;			/* debugging options */
EXTERN bool	eflag;			/* specific functions excluded */
EXTERN bool	Eflag;			/* functions excluded with time */
EXTERN bool	fflag;			/* specific functions requested */
EXTERN bool	Fflag;			/* functions requested with time */
EXTERN bool	kflag;			/* arcs to be deleted */
EXTERN bool	Kflag;			/* use the running kernel for symbols */
EXTERN bool	sflag;			/* sum multiple gmon.out files */
EXTERN bool	uflag;			/* suppress symbols hidden from C */
EXTERN bool	zflag;			/* zero time/called functions, too */

    /*
     *	structure for various string lists
     */
struct stringlist {
    struct stringlist	*next;
    char		*string;
};
extern struct stringlist	*elist;
extern struct stringlist	*Elist;
extern struct stringlist	*flist;
extern struct stringlist	*Flist;
extern struct stringlist	*kfromlist;
extern struct stringlist	*ktolist;

    /*
     *	function declarations
     */
void		addarc(nltype *, nltype *, long);
bool		addcycle(arctype **, arctype **);
void		addlist(struct stringlist *, char *);
void		alignentries(void);
int		arccmp(arctype *, arctype *);
arctype		*arclookup(nltype *, nltype *);
void		asgnsamples(void);
void		compresslist(void);
bool		cycleanalyze(void);
void		cyclelink(void);
void		cycletime(void);
bool		descend(nltype *, arctype **, arctype **);
void		dfn(nltype *);
bool		dfn_busy(nltype *);
void		dfn_findcycle(nltype *);
void		dfn_init(void);
bool		dfn_numbered(nltype *);
void		dfn_post_visit(nltype *);
void		dfn_pre_visit(nltype *);
void		dfn_self_cycle(nltype *);
nltype		**doarcs(void);
void		doflags(void);
void		dotime(void);
void		dumpsum(const char *);
int		elf_getnfile(const char *, char ***);
void		flatprofheader(void);
void		flatprofline(nltype *);
void		getpfile(char *);
void		gprofheader(void);
void		gprofline(register nltype *);
int		hertz(void);
void		inheritflags(nltype *);
int		kernel_getnfile(const char *, char ***);
/*
		main();
*/
unsigned long	max(unsigned long, unsigned long);
int		membercmp(nltype *, nltype *);
unsigned long	min(unsigned long, unsigned long);
nltype		*nllookup(unsigned long);
bool		onlist(struct stringlist *, const char *);
FILE		*openpfile(char *);
void		printblurb(const char *);
void		printchildren(nltype *);
void		printcycle(nltype *);
void		printgprof(nltype **);
void		printindex(void);
void		printmembers(nltype *);
void		printname(nltype *);
void		printparents(nltype *);
void		printprof(void);
void		printsubcycle(cltype *);
void		readsamples(FILE *);
void		sortchildren(nltype *);
void		sortmembers(nltype *);
void		sortparents(nltype *);
void		tally(struct rawarc *);
void		timepropagate(nltype *);
int		totalcmp(const void *, const void *);

#define	LESSTHAN	-1
#define	EQUALTO		0
#define	GREATERTHAN	1

#define	DFNDEBUG	1
#define	CYCLEDEBUG	2
#define	ARCDEBUG	4
#define	TALLYDEBUG	8
#define	TIMEDEBUG	16
#define	SAMPLEDEBUG	32
#define	CALLDEBUG	128
#define	LOOKUPDEBUG	256
#define	PROPDEBUG	512
#define	BREAKCYCLE	1024
#define	SUBCYCLELIST	2048
#define	ANYDEBUG	4096
