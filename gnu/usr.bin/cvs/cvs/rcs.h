/* @(#)rcs.h 1.14 92/03/31	 */

/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * RCS source control definitions needed by rcs.c and friends
 */

#define	RCS		"rcs"
#define	RCS_CI		"ci"
#define	RCS_CO		"co"
#define	RCS_RLOG	"rlog"
#define	RCS_DIFF	"rcsdiff"
#define	RCS_MERGE	"merge"
#define	RCS_RCSMERGE	"rcsmerge"
#define	RCS_MERGE_PAT	"^>>>>>>> "	/* runs "grep" with this pattern */
#define	RCSEXT		",v"
#define	RCSHEAD		"head"
#define	RCSBRANCH	"branch"
#define	RCSSYMBOLS	"symbols"
#define	RCSDATE		"date"
#define	RCSDESC		"desc"
#define	DATEFORM	"%02d.%02d.%02d.%02d.%02d.%02d"
#define	SDATEFORM	"%d.%d.%d.%d.%d.%d"

/*
 * Opaque structure definitions used by RCS specific lookup routines
 */
#define VALID	0x1			/* flags field contains valid data */
#define	INATTIC	0x2			/* RCS file is located in the Attic */
struct rcsnode
{
    int refcount;
    int flags;
    char *path;
    char *head;
    char *branch;
    List *symbols;
    List *versions;
    List *dates;
};
typedef struct rcsnode RCSNode;

struct rcsversnode
{
    char *version;
    char *date;
    char *next;
    List *branches;
};
typedef struct rcsversnode RCSVers;

/*
 * CVS reserves all even-numbered branches for its own use.  "magic" branches
 * (see rcs.c) are contained as virtual revision numbers (within symbolic
 * tags only) off the RCS_MAGIC_BRANCH, which is 0.  CVS also reserves the
 * ".1" branch for vendor revisions.  So, if you do your own branching, you
 * should limit your use to odd branch numbers starting at 3.
 */
#define	RCS_MAGIC_BRANCH	0

/*
 * exported interfaces
 */
#if __STDC__
List *RCS_parsefiles (List * files, char *xrepos);
RCSNode *RCS_parse (char *file, char *repos);
RCSNode *RCS_parsercsfile (char *rcsfile);
char *RCS_check_kflag (char *arg);
char *RCS_getdate (RCSNode * rcs, char *date, int force_tag_match);
char *RCS_gettag (RCSNode * rcs, char *tag, int force_tag_match);
char *RCS_getversion (RCSNode * rcs, char *tag, char *date,
		      int force_tag_match);
char *RCS_magicrev (RCSNode *rcs, char *rev);
int RCS_isbranch (char *file, char *rev, List *srcfiles);
char *RCS_whatbranch (char *file, char *tag, List *srcfiles);
char *RCS_head (RCSNode * rcs);
int RCS_datecmp (char *date1, char *date2);
time_t RCS_getrevtime (RCSNode * rcs, char *rev, char *date, int fudge);
void RCS_check_tag (char *tag);
void freercsnode (RCSNode ** rnodep);
#else
List *RCS_parsefiles ();
RCSNode *RCS_parse ();
char *RCS_head ();
char *RCS_getversion ();
char *RCS_magicrev ();
int RCS_isbranch ();
char *RCS_whatbranch ();
char *RCS_gettag ();
char *RCS_getdate ();
char *RCS_check_kflag ();
void RCS_check_tag ();
time_t RCS_getrevtime ();
RCSNode *RCS_parsercsfile ();
int RCS_datecmp ();
void freercsnode ();
#endif				/* __STDC__ */
