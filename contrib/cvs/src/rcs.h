/* $CVSid: @(#)rcs.h 1.18 94/09/23 $	 */

/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * RCS source control definitions needed by rcs.c and friends
 */

#define	RCS		"rcs"
#define	RCS_CI		"ci"
#define	RCS_CO		"co"
#define	RCS_RLOG	"rlog"
#define	RCS_DIFF	"rcsdiff"
#define	RCS_RCSMERGE	"rcsmerge"
#define	RCS_MERGE_PAT	"^>>>>>>> "	/* runs "grep" with this pattern */
#define	RCSEXT		",v"
#define RCSPAT		"*,v"
#define	RCSHEAD		"head"
#define	RCSBRANCH	"branch"
#define	RCSSYMBOLS	"symbols"
#define	RCSDATE		"date"
#define	RCSDESC		"desc"
#define RCSEXPAND	"expand"

/* Used by the version of death support which resulted from old
   versions of CVS (e.g. 1.5 if you define DEATH_SUPPORT and not
   DEATH_STATE).  Only a hacked up RCS (used by those old versions of
   CVS) will put this into RCS files.  Considered obsolete.  */
#define RCSDEAD		"dead"

#define	DATEFORM	"%02d.%02d.%02d.%02d.%02d.%02d"
#define	SDATEFORM	"%d.%d.%d.%d.%d.%d"

/*
 * Opaque structure definitions used by RCS specific lookup routines
 */
#define VALID	0x1			/* flags field contains valid data */
#define	INATTIC	0x2			/* RCS file is located in the Attic */
#define PARTIAL 0x4			/* RCS file not completly parsed */

struct rcsnode
{
    int refcount;
    int flags;
    char *path;
    char *head;
    char *branch;
    char *symbols_data;
    char *expand;
    List *symbols;
    List *versions;
};

typedef struct rcsnode RCSNode;

struct rcsversnode
{
    char *version;
    char *date;
    char *author;
    char *next;
    int dead;
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
RCSNode *RCS_parse PROTO((const char *file, const char *repos));
RCSNode *RCS_parsercsfile PROTO((char *rcsfile));
char *RCS_check_kflag PROTO((const char *arg));
char *RCS_getdate PROTO((RCSNode * rcs, char *date, int force_tag_match));
char *RCS_gettag PROTO((RCSNode * rcs, char *symtag, int force_tag_match,
			int return_both));
char *RCS_getversion PROTO((RCSNode * rcs, char *tag, char *date,
		      int force_tag_match, int return_both));
char *RCS_magicrev PROTO((RCSNode *rcs, char *rev));
int RCS_isbranch PROTO((RCSNode *rcs, const char *rev));
int RCS_nodeisbranch PROTO((RCSNode *rcs, const char *tag));
char *RCS_whatbranch PROTO((RCSNode *rcs, const char *tag));
char *RCS_head PROTO((RCSNode * rcs));
int RCS_datecmp PROTO((char *date1, char *date2));
time_t RCS_getrevtime PROTO((RCSNode * rcs, char *rev, char *date, int fudge));
List *RCS_symbols PROTO((RCSNode *rcs));
void RCS_check_tag PROTO((const char *tag));
void freercsnode PROTO((RCSNode ** rnodep));
char *RCS_getbranch PROTO((RCSNode * rcs, char *tag, int force_tag_match));

int RCS_isdead PROTO((RCSNode *, const char *));
char *RCS_getexpand PROTO ((RCSNode *));
