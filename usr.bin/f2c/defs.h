/****************************************************************
Copyright 1990 - 1996 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

#include "sysdep.h"

#include "ftypes.h"
#include "defines.h"
#include "machdefs.h"

#define MAXDIM 20
#define MAXINCLUDES 10
#define MAXLITERALS 200		/* Max number of constants in the literal
				   pool */
#define MAXCTL 20
#define MAXHASH 401
#define MAXSTNO 801
#define MAXEXT 200
#define MAXEQUIV 150
#define MAXLABLIST 258		/* Max number of labels in an alternate
				   return CALL or computed GOTO */
#define MAXCONTIN 99		/* Max continuation lines */

/* These are the primary pointer types used in the compiler */

typedef union Expression *expptr, *tagptr;
typedef struct Chain *chainp;
typedef struct Addrblock *Addrp;
typedef struct Constblock *Constp;
typedef struct Exprblock *Exprp;
typedef struct Nameblock *Namep;

extern FILEP infile;
extern FILEP diagfile;
extern FILEP textfile;
extern FILEP asmfile;
extern FILEP c_file;		/* output file for all functions; extern
				   declarations will have to be prepended */
extern FILEP pass1_file;	/* Temp file to hold the function bodies
				   read on pass 1 */
extern FILEP expr_file;		/* Debugging file */
extern FILEP initfile;		/* Intermediate data file pointer */
extern FILEP blkdfile;		/* BLOCK DATA file */

extern int current_ftn_file;
extern int maxcontin;

extern char *blkdfname, *initfname, *sortfname;
extern long headoffset;		/* Since the header block requires data we
				   don't know about until AFTER each
				   function has been processed, we keep a
				   pointer to the current (dummy) header
				   block (at the top of the assembly file)
				   here */

extern char main_alias[];	/* name given to PROGRAM psuedo-op */
extern char *token;
extern int maxtoklen, toklen;
extern long err_lineno, lineno;
extern char *infname;
extern int needkwd;
extern struct Labelblock *thislabel;

/* Used to allow runtime expansion of internal tables.  In particular,
   these values can exceed their associated constants */

extern int maxctl;
extern int maxequiv;
extern int maxstno;
extern int maxhash;
extern int maxext;

extern flag nowarnflag;
extern flag ftn66flag;		/* Generate warnings when weird f77
				   features are used (undeclared dummy
				   procedure, non-char initialized with
				   string, 1-dim subscript in EQUIV) */
extern flag no66flag;		/* Generate an error when a generic
				   function (f77 feature) is used */
extern flag noextflag;		/* Generate an error when an extension to
				   Fortran 77 is used (hex/oct/bin
				   constants, automatic, static, double
				   complex types) */
extern flag zflag;		/* enable double complex intrinsics */
extern flag shiftcase;
extern flag undeftype;
extern flag shortsubs;		/* Use short subscripts on arrays? */
extern flag onetripflag;	/* if true, always execute DO loop body */
extern flag checksubs;
extern flag debugflag;
extern int nerr;
extern int nwarn;

extern int parstate;
extern flag headerdone;		/* True iff the current procedure's header
				   data has been written */
extern int blklevel;
extern flag saveall;
extern flag substars;		/* True iff some formal parameter is an
				   asterisk */
extern int impltype[ ];
extern ftnint implleng[ ];
extern int implstg[ ];

extern int tycomplex, tyint, tyioint, tyreal;
extern int tylog, tylogical;	/* TY____ of the implementation of   logical.
				   This will be LONG unless '-2' is given
				   on the command line */
extern int type_choice[];
extern char *typename[];

extern int typesize[];	/* size (in bytes) of an object of each
				   type.  Indexed by TY___ macros */
extern int typealign[];
extern int proctype;	/* Type of return value in this procedure */
extern char * procname;	/* External name of the procedure, or last ENTRY name */
extern int rtvlabel[ ];	/* Return value labels, indexed by TY___ macros */
extern Addrp retslot;
extern Addrp xretslot[];
extern int cxslot;	/* Complex return argument slot (frame pointer offset)*/
extern int chslot;	/* Character return argument slot (fp offset) */
extern int chlgslot;	/* Argument slot for length of character buffer */
extern int procclass;	/* Class of the current procedure:  either CLPROC,
			   CLMAIN, CLBLOCK or CLUNKNOWN */
extern ftnint procleng;	/* Length of function return value (e.g. char
			   string length).  If this is -1, then the length is
			   not known at compile time */
extern int nentry;	/* Number of entry points (other than the original
			   function call) into this procedure */
extern flag multitype;	/* YES iff there is more than one return value
			   possible */
extern int blklevel;
extern long lastiolabno;
extern long lastlabno;
extern int lastvarno;
extern int lastargslot;	/* integer offset pointing to the next free
			   location for an argument to the current routine */
extern int argloc;
extern int autonum[];		/* for numbering
				   automatic variables, e.g. temporaries */
extern int retlabel;
extern int ret0label;
extern int dorange;		/* Number of the label which terminates
				   the innermost DO loop */
extern int regnum[ ];		/* Numbers of DO indicies named in
				   regnamep   (below) */
extern Namep regnamep[ ];	/* List of DO indicies in registers */
extern int maxregvar;		/* number of elts in   regnamep   */
extern int highregvar;		/* keeps track of the highest register
				   number used by DO index allocator */
extern int nregvar;		/* count of DO indicies in registers */

extern chainp templist[];
extern int maxdim;
extern chainp earlylabs;
extern chainp holdtemps;
extern struct Entrypoint *entries;
extern struct Rplblock *rpllist;
extern struct Chain *curdtp;
extern ftnint curdtelt;
extern chainp allargs;		/* union of args in entries */
extern int nallargs;		/* total number of args */
extern int nallchargs;		/* total number of character args */
extern flag toomanyinit;	/* True iff too many initializers in a
				   DATA statement */

extern flag inioctl;
extern int iostmt;
extern Addrp ioblkp;
extern int nioctl;
extern int nequiv;
extern int eqvstart;	/* offset to eqv number to guarantee uniqueness
			   and prevent <something> from going negative */
extern int nintnames;

/* Chain of tagged blocks */

struct Chain
	{
	chainp nextp;
	char * datap;		/* Tagged block */
	};

extern chainp chains;

/* Recall that   field   is intended to hold four-bit characters */

/* This structure exists only to defeat the type checking */

struct Headblock
	{
	field tag;
	field vtype;
	field vclass;
	field vstg;
	expptr vleng;		/* Expression for length of char string -
				   this may be a constant, or an argument
				   generated by mkarg() */
	} ;

/* Control construct info (for do loops, else, etc) */

struct Ctlframe
	{
	unsigned ctltype:8;
	unsigned dostepsign:8;	/* 0 - variable, 1 - pos, 2 - neg */
	unsigned dowhile:1;
	int ctlabels[4];	/* Control labels, defined below */
	int dolabel;		/* label marking end of this DO loop */
	Namep donamep;		/* DO index variable */
	expptr doinit;		/* for use with -onetrip */
	expptr domax;		/* constant or temp variable holding MAX
				   loop value; or expr of while(expr) */
	expptr dostep;		/* expression */
	Namep loopname;
	};
#define endlabel ctlabels[0]
#define elselabel ctlabels[1]
#define dobodylabel ctlabels[1]
#define doposlabel ctlabels[2]
#define doneglabel ctlabels[3]
extern struct Ctlframe *ctls;		/* Keeps info on DO and BLOCK IF
					   structures - this is the stack
					   bottom */
extern struct Ctlframe *ctlstack;	/* Pointer to current nesting
					   level */
extern struct Ctlframe *lastctl;	/* Point to end of
					   dynamically-allocated array */

typedef struct {
	int type;
	chainp cp;
	} Atype;

typedef struct {
	int defined, dnargs, nargs, changes;
	Atype atypes[1];
	} Argtypes;

/* External Symbols */

struct Extsym
	{
	char *fextname;		/* Fortran version of external name */
	char *cextname;		/* C version of external name */
	field extstg;		/* STG -- should be COMMON, UNKNOWN or EXT
				   */
	unsigned extype:4;	/* for transmitting type to output routines */
	unsigned used_here:1;	/* Boolean - true on the second pass
				   through a function if the block has
				   been referenced */
	unsigned exused:1;	/* Has been used (for help with error msgs
				   about externals typed differently in
				   different modules) */
	unsigned exproto:1;	/* type specified in a .P file */
	unsigned extinit:1;	/* Procedure has been defined,
				   or COMMON has DATA */
	unsigned extseen:1;	/* True if previously referenced */
	chainp extp;		/* List of identifiers in the common
				   block for this function, stored as
				   Namep (hash table pointers) */
	chainp allextp;		/* List of lists of identifiers; we keep one
				   list for each layout of this common block */
	int curno;		/* current number for this common block,
				   used for constructing appending _nnn
				   to the common block name */
	int maxno;		/* highest curno value for this common block */
	ftnint extleng;
	ftnint maxleng;
	Argtypes *arginfo;
	};
typedef struct Extsym Extsym;

extern Extsym *extsymtab;	/* External symbol table */
extern Extsym *nextext;
extern Extsym *lastext;
extern int complex_seen, dcomplex_seen;

/* Statement labels */

struct Labelblock
	{
	int labelno;		/* Internal label */
	unsigned blklevel:8;	/* level of nesting, for branch-in-loop
				   checking */
	unsigned labused:1;
	unsigned fmtlabused:1;
	unsigned labinacc:1;	/* inaccessible? (i.e. has its scope
				   vanished) */
	unsigned labdefined:1;	/* YES or NO */
	unsigned labtype:2;	/* LAB{FORMAT,EXEC,etc} */
	ftnint stateno;		/* Original label */
	char *fmtstring;	/* format string */
	};

extern struct Labelblock *labeltab;	/* Label table - keeps track of
					   all labels, including undefined */
extern struct Labelblock *labtabend;
extern struct Labelblock *highlabtab;

/* Entry point list */

struct Entrypoint
	{
	struct Entrypoint *entnextp;
	Extsym *entryname;	/* Name of this ENTRY */
	chainp arglist;
	int typelabel;			/* Label for function exit; this
					   will return the proper type of
					   object */
	Namep enamep;			/* External name */
	};

/* Primitive block, or Primary block.  This is a general template returned
   by the parser, which will be interpreted in context.  It is a template
   for an identifier (variable name, function name), parenthesized
   arguments (array subscripts, function parameters) and substring
   specifications. */

struct Primblock
	{
	field tag;
	field vtype;
	unsigned parenused:1;		/* distinguish (a) from a */
	Namep namep;			/* Pointer to structure Nameblock */
	struct Listblock *argsp;
	expptr fcharp;			/* first-char-index-pointer (in
					   substring) */
	expptr lcharp;			/* last-char-index-pointer (in
					   substring) */
	};


struct Hashentry
	{
	int hashval;
	Namep varp;
	};
extern struct Hashentry *hashtab;	/* Hash table */
extern struct Hashentry *lasthash;

struct Intrpacked	/* bits for intrinsic function description */
	{
	unsigned f1:4;
	unsigned f2:4;
	unsigned f3:7;
	unsigned f4:1;
	};

struct Nameblock
	{
	field tag;
	field vtype;
	field vclass;
	field vstg;
	expptr vleng;		/* length of character string, if applicable */
	char *fvarname;		/* name in the Fortran source */
	char *cvarname;		/* name in the resulting C */
	chainp vlastdim;	/* datap points to new_vars entry for the */
				/* system variable, if any, storing the final */
				/* dimension; we zero the datap if this */
				/* variable is needed */
	unsigned vprocclass:3;	/* P____ macros - selects the   varxptr
				   field below */
	unsigned vdovar:1;	/* "is it a DO variable?" for register
				   and multi-level loop	checking */
	unsigned vdcldone:1;	/* "do I think I'm done?" - set when the
				   context is sufficient to determine its
				   status */
	unsigned vadjdim:1;	/* "adjustable dimension?" - needed for
				   information about copies */
	unsigned vsave:1;
	unsigned vimpldovar:1;	/* used to prevent erroneous error messages
				   for variables used only in DATA stmt
				   implicit DOs */
	unsigned vis_assigned:1;/* True if this variable has had some
				   label ASSIGNED to it; hence
				   varxptr.assigned_values is valid */
	unsigned vimplstg:1;	/* True if storage type is assigned implicitly;
				   this allows a COMMON variable to participate
				   in a DIMENSION before the COMMON declaration.
				   */
	unsigned vcommequiv:1;	/* True if EQUIVALENCEd onto STGCOMMON */
	unsigned vfmt_asg:1;	/* True if char *var_fmt needed */
	unsigned vpassed:1;	/* True if passed as a character-variable arg */
	unsigned vknownarg:1;	/* True if seen in a previous entry point */
	unsigned visused:1;	/* True if variable is referenced -- so we */
				/* can omit variables that only appear in DATA */
	unsigned vnamelist:1;	/* Appears in a NAMELIST */
	unsigned vimpltype:1;	/* True if implicitly typed and not
				   invoked as a function or subroutine
				   (so we can consistently type procedures
				   declared external and passed as args
				   but never invoked).
				   */
	unsigned vtypewarned:1;	/* so we complain just once about
				   changed types of external procedures */
	unsigned vinftype:1;	/* so we can restore implicit type to a
				   procedure if it is invoked as a function
				   after being given a different type by -it */
	unsigned vinfproc:1;	/* True if -it infers this to be a procedure */
	unsigned vcalled:1;	/* has been invoked */
	unsigned vdimfinish:1;	/* need to invoke dim_finish() */
	unsigned vrefused:1;	/* Need to #define name_ref (for -s) */
	unsigned vsubscrused:1;	/* Need to #define name_subscr (for -2) */
	unsigned veqvadjust:1;	/* voffset has been adjusted for equivalence */

/* The   vardesc   union below is used to store the number of an intrinsic
   function (when vstg == STGINTR and vprocclass == PINTRINSIC), or to
   store the index of this external symbol in   extsymtab   (when vstg ==
   STGEXT and vprocclass == PEXTERNAL) */

	union	{
		int varno;		/* Return variable for a function.
					   This is used when a function is
					   assigned a return value.  Also
					   used to point to the COMMON
					   block, when this is a field of
					   that block.  Also points to
					   EQUIV block when STGEQUIV */
		struct Intrpacked intrdesc;	/* bits for intrinsic function*/
		} vardesc;
	struct Dimblock *vdim;	/* points to the dimensions if they exist */
	ftnint voffset;		/* offset in a storage block (the variable
				   name will be "v.%d", voffset in a
				   common blck on the vax).  Also holds
				   pointers for automatic variables.  When
				   STGEQUIV, this is -(offset from array
				   base) */
	union	{
		chainp namelist;	/* points to names in the NAMELIST,
					   if this is a NAMELIST name */
		chainp vstfdesc;	/* points to (formals, expr) pair */
		chainp assigned_values;	/* list of integers, each being a
					   statement label assigned to
					   this variable in the current function */
		} varxptr;
	int argno;		/* for multiple entries */
	Argtypes *arginfo;
	};


/* PARAMETER statements */

struct Paramblock
	{
	field tag;
	field vtype;
	field vclass;
	field vstg;
	expptr vleng;
	char *fvarname;
	char *cvarname;
	expptr paramval;
	} ;


/* Expression block */

struct Exprblock
	{
	field tag;
	field vtype;
	field vclass;
	field vstg;
	expptr vleng;		/* in the case of a character expression, this
				   value is inherited from the children */
	unsigned opcode;
	expptr leftp;
	expptr rightp;
	int typefixed;
	};


union Constant
	{
	struct {
		char *ccp0;
		ftnint blanks;
		} ccp1;
	ftnint ci;		/* Constant longeger */
	double cd[2];
	char *cds[2];
	};
#define ccp ccp1.ccp0

struct Constblock
	{
	field tag;
	field vtype;
	field vclass;
	field vstg;		/* vstg = 1 when using Const.cds */
	expptr vleng;
	union Constant Const;
	};


struct Listblock
	{
	field tag;
	field vtype;
	chainp listp;
	};



/* Address block - this is the FINAL form of identifiers before being
   sent to pass 2.  We'll want to add the original identifier here so that it can
   be preserved in the translation.

   An example identifier is q.7.  The "q" refers to the storage class
   (field vstg), the 7 to the variable number (int memno). */

struct Addrblock
	{
	field tag;
	field vtype;
	field vclass;
	field vstg;
	expptr vleng;
	/* put union...user here so the beginning of an Addrblock
	 * is the same as a Constblock.
	 */
	union {
	    Namep name;		/* contains a pointer into the hash table */
	    char ident[IDENT_LEN + 1];	/* C string form of identifier */
	    char *Charp;
	    union Constant Const;	/* Constant value */
	    struct {
		double dfill[2];
		field vstg1;
		} kludge;	/* so we can distinguish string vs binary
				 * floating-point constants */
	} user;
	long memno;		/* when vstg == STGCONST, this is the
				   numeric part of the assembler label
				   where the constant value is stored */
	expptr memoffset;	/* used in subscript computations, usually */
	unsigned istemp:1;	/* used in stack management of temporary
				   variables */
	unsigned isarray:1;	/* used to show that memoffset is
				   meaningful, even if zero */
	unsigned ntempelt:10;	/* for representing temporary arrays, as
				   in concatenation */
	unsigned dbl_builtin:1;	/* builtin to be declared double */
	unsigned charleng:1;	/* so saveargtypes can get i/o calls right */
	unsigned cmplx_sub:1;	/* used in complex arithmetic under -s */
	unsigned skip_offset:1;	/* used in complex arithmetic under -s */
	unsigned parenused:1;	/* distinguish (a) from a */
	ftnint varleng;		/* holds a copy of a constant length which
				   is stored in the   vleng   field (e.g.
				   a double is 8 bytes) */
	int uname_tag;		/* Tag describing which of the unions()
				   below to use */
	char *Field;		/* field name when dereferencing a struct */
}; /* struct Addrblock */


/* Errorbock - placeholder for errors, to allow the compilation to
   continue */

struct Errorblock
	{
	field tag;
	field vtype;
	};


/* Implicit DO block, especially related to DATA statements.  This block
   keeps track of the compiler's location in the implicit DO while it's
   running.  In particular, the   isactive and isbusy   flags tell where
   it is */

struct Impldoblock
	{
	field tag;
	unsigned isactive:1;
	unsigned isbusy:1;
	Namep varnp;
	Constp varvp;
	chainp impdospec;
	expptr implb;
	expptr impub;
	expptr impstep;
	ftnint impdiff;
	ftnint implim;
	struct Chain *datalist;
	};


/* Each of these components has a first field called   tag.   This union
   exists just for allocation simplicity */

union Expression
	{
	field tag;
	struct Addrblock addrblock;
	struct Constblock constblock;
	struct Errorblock errorblock;
	struct Exprblock exprblock;
	struct Headblock headblock;
	struct Impldoblock impldoblock;
	struct Listblock listblock;
	struct Nameblock nameblock;
	struct Paramblock paramblock;
	struct Primblock primblock;
	} ;



struct Dimblock
	{
	int ndim;
	expptr nelt;		/* This is NULL if the array is unbounded */
	expptr baseoffset;	/* a constant or local variable holding
				   the offset in this procedure */
	expptr basexpr;		/* expression for comuting the offset, if
				   it's not constant.  If this is
				   non-null, the register named in
				   baseoffset will get initialized to this
				   value in the procedure's prolog */
	struct
		{
		expptr dimsize;	/* constant or register holding the size
				   of this dimension */
		expptr dimexpr;	/* as above in basexpr, this is an
				   expression for computing a variable
				   dimension */
		} dims[1];	/* Dimblocks are allocated with enough
				   space for this to become dims[ndim] */
	};


/* Statement function identifier stack - this holds the name and value of
   the parameters in a statement function invocation.  For example,

	f(x,y,z)=x+y+z
		.
		.
	y = f(1,2,3)

   generates a stack of depth 3, with <x 1>, <y 2>, <z 3> AT THE INVOCATION, NOT
   at the definition */

struct Rplblock	/* name replacement block */
	{
	struct Rplblock *rplnextp;
	Namep rplnp;		/* Name of the formal parameter */
	expptr rplvp;		/* Value of the actual parameter */
	expptr rplxp;		/* Initialization of temporary variable,
				   if required; else null */
	int rpltag;		/* Tag on the value of the actual param */
	};



/* Equivalence block */

struct Equivblock
	{
	struct Eqvchain *equivs;	/* List (Eqvchain) of primblocks
					   holding variable identifiers */
	flag eqvinit;
	long eqvtop;
	long eqvbottom;
	int eqvtype;
	} ;
#define eqvleng eqvtop

extern struct Equivblock *eqvclass;


struct Eqvchain
	{
	struct Eqvchain *eqvnextp;
	union
		{
		struct Primblock *eqvlhs;
		Namep eqvname;
		} eqvitem;
	long eqvoffset;
	} ;



/* For allocation purposes only, and to keep lint quiet.  In particular,
   don't count on the tag being able to tell you which structure is used */


/* There is a tradition in Fortran that the compiler not generate the same
   bit pattern more than is necessary.  This structure is used to do just
   that; if two integer constants have the same bit pattern, just generate
   it once.  This could be expanded to optimize without regard to type, by
   removing the type check in   putconst()   */

struct Literal
	{
	short littype;
	short lituse;		/* usage count */
	long litnum;			/* numeric part of the assembler
					   label for this constant value */
	union	{
		ftnint litival;
		double litdval[2];
		ftnint litival2[2];	/* length, nblanks for strings */
		} litval;
	char *cds[2];
	};

extern struct Literal *litpool;
extern int maxliterals, nliterals;
extern char Letters[];
#define letter(x) Letters[x]

struct Dims { expptr lb, ub; };

extern int forcedouble;		/* force real functions to double */
extern int doin_setbound;	/* special handling for array bounds */
extern int Ansi;
extern char hextoi_tab[];
#define hextoi(x) hextoi_tab[(x) & 0xff]
extern char *casttypes[], *ftn_types[], *protorettypes[], *usedcasts[];
extern int Castargs, infertypes;
extern FILE *protofile;
extern char binread[], binwrite[], textread[], textwrite[];
extern char *ei_first, *ei_last, *ei_next;
extern char *wh_first, *wh_last, *wh_next;
extern char *halign, *outbuf, *outbtail;
extern flag keepsubs;
#ifdef TYQUAD
extern flag use_tyquad;
#endif
extern int n_keywords;
extern char *c_keywords[];

#ifdef KR_headers
#define Argdcl(x) ()
#define Void /* void */
#else
#define Argdcl(x) x
#define Void void
#endif

char*	Alloc Argdcl((int));
char*	Argtype Argdcl((int, char*));
void	Fatal Argdcl((char*));
struct	Impldoblock* mkiodo Argdcl((chainp, chainp));
tagptr	Inline Argdcl((int, int, chainp));
struct	Labelblock* execlab Argdcl((long));
struct	Labelblock* mklabel Argdcl((long));
struct	Listblock* mklist Argdcl((chainp));
void	Un_link_all Argdcl((int));
void	add_extern_to_list Argdcl((Addrp, chainp*));
int	addressable Argdcl((tagptr));
tagptr	addrof Argdcl((tagptr));
char*	addunder Argdcl((char*));
Addrp	autovar Argdcl((int, int, tagptr, char*));
void	backup Argdcl((char*, char*));
void	bad_atypes Argdcl((Argtypes*, char*, int, int, int, char*, char*));
int	badchleng Argdcl((tagptr));
void	badop Argdcl((char*, int));
void	badstg Argdcl((char*, int));
void	badtag Argdcl((char*, int));
void	badthing Argdcl((char*, char*, int));
void	badtype Argdcl((char*, int));
Addrp	builtin Argdcl((int, char*, int));
char*	c_name Argdcl((char*, int));
tagptr	call0 Argdcl((int, char*));
tagptr	call1 Argdcl((int, char*, tagptr));
tagptr	call2 Argdcl((int, char*, tagptr, tagptr));
tagptr	call3 Argdcl((int, char*, tagptr, tagptr, tagptr));
tagptr	call4 Argdcl((int, char*, tagptr, tagptr, tagptr, tagptr));
tagptr	callk Argdcl((int, char*, chainp));
void	cast_args Argdcl((int, chainp));
char*	cds Argdcl((char*, char*));
void	changedtype Argdcl((Namep));
ptr	ckalloc Argdcl((int));
int	cktype Argdcl((int, int, int));
void	clf Argdcl((FILEP*, char*, int));
int	cmpstr Argdcl((char*, char*, long, long));
char*	c_type_decl Argdcl((int, int));
Extsym*	comblock Argdcl((char*));
char*	comm_union_name Argdcl((int));
void	consconv Argdcl((int, Constp, Constp));
void	consnegop Argdcl((Constp));
int	conssgn Argdcl((tagptr));
char*	convic Argdcl((long));
void	copy_data Argdcl((chainp));
char*	copyn Argdcl((int, char*));
char*	copys Argdcl((char*));
tagptr	cpblock Argdcl((int, char*));
tagptr	cpexpr Argdcl((tagptr));
void	cpn Argdcl((int, char*, char*));
char*	cpstring Argdcl((char*));
void	dataline Argdcl((char*, long, int));
char*	dataname Argdcl((int, long));
void	dataval Argdcl((tagptr, tagptr));
void	dclerr Argdcl((char*, Namep));
void	def_commons Argdcl((FILEP));
void	def_start Argdcl((FILEP, char*, char*, char*));
void	deregister Argdcl((Namep));
void	do_uninit_equivs Argdcl((FILEP, ptr));
void	doequiv(Void);
int	dofork(Void);
void	doinclude Argdcl((char*));
void	doio Argdcl((chainp));
void	done Argdcl((int));
void	donmlist(Void);
int	dsort Argdcl((char*, char*));
char*	dtos Argdcl((double));
void	elif_out Argdcl((FILEP, tagptr));
void	end_else_out Argdcl((FILEP));
void	enddcl(Void);
void	enddo Argdcl((int));
void	endio(Void);
void	endioctl(Void);
void	endproc(Void);
void	entrypt Argdcl((int, int, long, Extsym*, chainp));
int	eqn Argdcl((int, char*, char*));
char*	equiv_name Argdcl((int, char*));
void	err Argdcl((char*));
void	err66 Argdcl((char*));
void	errext Argdcl((char*));
void	erri Argdcl((char*, int));
void	errl Argdcl((char*, long));
tagptr	errnode(Void);
void	errstr Argdcl((char*, char*));
void	exarif Argdcl((tagptr, struct Labelblock*, struct Labelblock*, struct Labelblock*));
void	exasgoto Argdcl((Namep));
void	exassign Argdcl((Namep, struct Labelblock*));
void	excall Argdcl((Namep, struct Listblock*, int, struct Labelblock**));
void	exdo Argdcl((int, Namep, chainp));
void	execerr Argdcl((char*, char*));
void	exelif Argdcl((tagptr));
void	exelse(Void);
void	exenddo Argdcl((Namep));
void	exendif(Void);
void	exequals Argdcl((struct Primblock*, tagptr));
void	exgoto Argdcl((struct Labelblock*));
void	exif Argdcl((tagptr));
void	exreturn Argdcl((tagptr));
void	exstop Argdcl((int, tagptr));
void	extern_out Argdcl((FILEP, Extsym*));
void	fatali Argdcl((char*, int));
void	fatalstr Argdcl((char*, char*));
void	ffilecopy Argdcl((FILEP, FILEP));
void	fileinit(Void);
int	fixargs Argdcl((int, struct Listblock*));
tagptr	fixexpr Argdcl((Exprp));
tagptr	fixtype Argdcl((tagptr));
char*	flconst Argdcl((char*, char*));
void	flline(Void);
void	fmt_init(Void);
void	fmtname Argdcl((Namep, Addrp));
int	fmtstmt Argdcl((struct Labelblock*));
tagptr	fold Argdcl((tagptr));
void	frchain Argdcl((chainp*));
void	frdata Argdcl((chainp));
void	freetemps(Void);
void	freqchain Argdcl((struct Equivblock*));
void	frexchain Argdcl((chainp*));
void	frexpr Argdcl((tagptr));
void	frrpl(Void);
void	frtemp Argdcl((Addrp));
char*	gmem Argdcl((int, int));
void	hashclear(Void);
chainp	hookup Argdcl((chainp, chainp));
expptr	imagpart Argdcl((Addrp));
void	impldcl Argdcl((Namep));
int	in_vector Argdcl((char*, char**, int));
void	incomm Argdcl((Extsym*, Namep));
void	inferdcl Argdcl((Namep, int));
int	inilex Argdcl((char*));
void	initkey(Void);
int	inregister Argdcl((Namep));
long	int commlen Argdcl((chainp));
long	int convci Argdcl((int, char*));
long	int iarrlen Argdcl((Namep));
long	int lencat Argdcl((expptr));
long	int lmax Argdcl((long, long));
long	int lmin Argdcl((long, long));
long	int wr_char_len Argdcl((FILEP, struct Dimblock*, int, int));
Addrp	intraddr Argdcl((Namep));
tagptr	intrcall Argdcl((Namep, struct Listblock*, int));
int	intrfunct Argdcl((char*));
void	ioclause Argdcl((int, expptr));
int	iocname(Void);
int	is_negatable Argdcl((Constp));
int	isaddr Argdcl((tagptr));
int	isnegative_const Argdcl((Constp));
int	isstatic Argdcl((tagptr));
chainp	length_comp Argdcl((struct Entrypoint*, int));
int	lengtype Argdcl((int, long));
char*	lexline Argdcl((ptr));
void	list_arg_types Argdcl((FILEP, struct Entrypoint*, chainp, int, char*));
void	list_decls Argdcl((FILEP));
void	list_init_data Argdcl((FILE **, char *, FILE *));
void	listargs Argdcl((FILEP, struct Entrypoint*, int, chainp));
char*	lit_name Argdcl((struct Literal*));
int	log_2 Argdcl((long));
char*	lower_string Argdcl((char*, char*));
int	main Argdcl((int, char**));
expptr	make_int_expr Argdcl((expptr));
void	make_param Argdcl((struct Paramblock*, tagptr));
void	many Argdcl((char*, char, int));
void	margin_printf Argdcl((FILEP, char*, ...));
int	maxtype Argdcl((int, int));
char*	mem Argdcl((int, int));
void	mem_init(Void);
char*	memname Argdcl((int, long));
Addrp	memversion Argdcl((Namep));
tagptr	mkaddcon Argdcl((long));
Addrp	mkaddr Argdcl((Namep));
Addrp	mkarg Argdcl((int, int));
tagptr	mkbitcon Argdcl((int, int, char*));
chainp	mkchain Argdcl((char*, chainp));
Constp	mkconst Argdcl((int));
tagptr	mkconv Argdcl((int, tagptr));
tagptr	mkcxcon Argdcl((tagptr, tagptr));
tagptr	mkexpr Argdcl((int, tagptr, tagptr));
Extsym*	mkext Argdcl((char*, char*));
Extsym*	mkext1 Argdcl((char*, char*));
Addrp	mkfield Argdcl((Addrp, char*, int));
tagptr	mkfunct Argdcl((tagptr));
tagptr	mkintcon Argdcl((long));
tagptr	mklhs Argdcl((struct Primblock*, int));
tagptr	mklogcon Argdcl((int));
Namep	mkname Argdcl((char*));
Addrp	mkplace Argdcl((Namep));
tagptr	mkprim Argdcl((Namep, struct Listblock*, chainp));
tagptr	mkrealcon Argdcl((int, char*));
Addrp	mkscalar Argdcl((Namep));
void	mkstfunct Argdcl((struct Primblock*, tagptr));
tagptr	mkstrcon Argdcl((int, char*));
Addrp	mktmp Argdcl((int, tagptr));
Addrp	mktmp0 Argdcl((int, tagptr));
Addrp	mktmpn Argdcl((int, int, tagptr));
void	namelist Argdcl((Namep));
int	ncat Argdcl((expptr));
void	negate_const Argdcl((Constp));
void	new_endif(Void);
Extsym*	newentry Argdcl((Namep, int));
long	newlabel(Void);
void	newproc(Void);
Addrp	nextdata Argdcl((long*));
void	nice_printf Argdcl((FILEP, char*, ...));
void	not_both Argdcl((char*));
void	np_init(Void);
int	oneof_stg Argdcl((Namep, int, int));
int	op_assign Argdcl((int));
tagptr	opconv Argdcl((tagptr, int));
FILEP	opf Argdcl((char*, char*));
void	out_addr Argdcl((FILEP, Addrp));
void	out_asgoto Argdcl((FILEP, tagptr));
void	out_call Argdcl((FILEP, int, int, tagptr, tagptr, tagptr));
void	out_const Argdcl((FILEP, Constp));
void	out_else Argdcl((FILEP));
void	out_for Argdcl((FILEP, tagptr, tagptr, tagptr));
void	out_init(Void);
void	outbuf_adjust(Void);
void	p1_label Argdcl((long));
void	prcona Argdcl((FILEP, long));
void	prconi Argdcl((FILEP, long));
void	prconr Argdcl((FILEP, Constp, int));
void	procinit(Void);
void	procode Argdcl((FILEP));
void	prolog Argdcl((FILEP, chainp));
void	protowrite Argdcl((FILEP, int, char*, struct Entrypoint*, chainp));
expptr	prune_left_conv Argdcl((expptr));
int	put_one_arg Argdcl((int, char*, char**, char*, char*));
expptr	putassign Argdcl((expptr, expptr));
Addrp	putchop Argdcl((tagptr));
void	putcmgo Argdcl((tagptr, int, struct Labelblock**));
Addrp	putconst Argdcl((Constp));
tagptr	putcxop Argdcl((tagptr));
void	puteq Argdcl((expptr, expptr));
void	putexpr Argdcl((expptr));
void	puthead Argdcl((char*, int));
void	putif Argdcl((tagptr, int));
void	putout Argdcl((tagptr));
expptr	putsteq Argdcl((Addrp, Addrp));
void	putwhile Argdcl((tagptr));
tagptr	putx Argdcl((tagptr));
void	r8fix(Void);
int	rdlong Argdcl((FILEP, long*));
int	rdname Argdcl((FILEP, ptr, char*));
void	read_Pfiles Argdcl((char**));
Addrp	realpart Argdcl((Addrp));
chainp	revchain Argdcl((chainp));
int	same_expr Argdcl((tagptr, tagptr));
int	same_ident Argdcl((tagptr, tagptr));
void	save_argtypes Argdcl((chainp, Argtypes**, Argtypes**, int, char*, int, int, int, int));
void	saveargtypes Argdcl((Exprp));
void	set_externs(Void);
void	set_tmp_names(Void);
void	setbound Argdcl((Namep, int, struct Dims*));
void	setdata Argdcl((Addrp, Constp, long));
void	setext Argdcl((Namep));
void	setfmt Argdcl((struct Labelblock*));
void	setimpl Argdcl((int, long, int, int));
void	setintr Argdcl((Namep));
void	settype Argdcl((Namep, int, long));
void	sigcatch Argdcl((int));
void	sserr Argdcl((Namep));
void	start_formatting(Void);
void	startioctl(Void);
void	startproc Argdcl((Extsym*, int));
void	startrw(Void);
char*	string_num Argdcl((char*, long));
int	struct_eq Argdcl((chainp, chainp));
tagptr	subcheck Argdcl((Namep, tagptr));
tagptr	suboffset Argdcl((struct Primblock*));
int	type_fixup Argdcl((Argtypes*, Atype*, int));
void	unamstring Argdcl((Addrp, char*));
void	unclassifiable(Void);
void	vardcl Argdcl((Namep));
void	warn Argdcl((char*));
void	warn1 Argdcl((char*, char*));
void	warni Argdcl((char*, int));
void	wr_abbrevs Argdcl((FILEP, int, chainp));
char*	wr_ardecls Argdcl((FILE*, struct Dimblock*, long));
void	wr_array_init Argdcl((FILEP, int, chainp));
void	wr_common_decls Argdcl((FILEP));
void	wr_equiv_init Argdcl((FILEP, int, chainp*, int));
void	wr_globals Argdcl((FILEP));
void	wr_nv_ident_help Argdcl((FILEP, Addrp));
void	wr_struct Argdcl((FILEP, chainp));
void	wronginf Argdcl((Namep));
void	yyerror Argdcl((char*));
int	yylex(Void);
int	yyparse(Void);

#ifdef USE_DTOA
#define atof(x) strtod(x,0)
void	g_fmt Argdcl((char*, double));
#endif
