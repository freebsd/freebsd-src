/****************************************************************
Copyright 1990, 1991, 1992, 1993 by AT&T Bell Laboratories, Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "sysdep.h"

#include "ftypes.h"
#include "defines.h"
#include "machdefs.h"

#define MAXDIM 20
#define MAXINCLUDES 10
#define MAXLITERALS 200		/* Max number of constants in the literal
				   pool */
#define MAXTOKENLEN 502		/* length of longest token */
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

extern FILEP opf();
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
extern long int headoffset;	/* Since the header block requires data we
				   don't know about until AFTER each
				   function has been processed, we keep a
				   pointer to the current (dummy) header
				   block (at the top of the assembly file)
				   here */

extern char main_alias[];	/* name given to PROGRAM psuedo-op */
extern char token [ ];
extern int toklen;
extern long lineno;
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
extern int lastlabno;
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
	unsigned blklevel:8;	/* level of nesting , for branch-in-loop
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
	unsigned f1:3;
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
	};


union Constant
	{
	struct {
		char *ccp0;
		ftnint blanks;
		} ccp1;
	ftnint ci;		/* Constant long integer */
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
	long int eqvtop;
	long int eqvbottom;
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
	long int eqvoffset;
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
	short litnum;			/* numeric part of the assembler
					   label for this constant value */
	int lituse;		/* usage count */
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


/* popular functions with non integer return values */


int *ckalloc();
char *varstr(), *nounder(), *addunder();
char *copyn(), *copys();
chainp hookup(), mkchain(), revchain();
ftnint convci();
char *convic();
char *setdoto();
double convcd();
Namep mkname();
struct Labelblock *mklabel(), *execlab();
Extsym *mkext(), *newentry();
expptr addrof(), call1(), call2(), call3(), call4();
Addrp builtin(), mktmp(), mktmp0(), mktmpn(), autovar();
Addrp mkplace(), mkaddr(), putconst(), memversion();
expptr mkprim(), mklhs(), mkexpr(), mkconv(), mkfunct(), fixexpr(), fixtype();
expptr errnode(), mkaddcon(), mkintcon(), putcxop();
tagptr cpexpr();
ftnint lmin(), lmax(), iarrlen();
char *dbconst(), *flconst();

void puteq (), putex1 ();
expptr putx (), putsteq (), putassign ();

extern int forcedouble;		/* force real functions to double */
extern int doin_setbound;	/* special handling for array bounds */
extern int Ansi;
extern char *cds(), *cpstring(), *dtos(), *string_num();
extern char *c_type_decl();
extern char hextoi_tab[];
#define hextoi(x) hextoi_tab[(x) & 0xff]
extern char *casttypes[], *ftn_types[], *protorettypes[], *usedcasts[];
extern int Castargs, infertypes;
extern FILE *protofile;
extern void exit(), inferdcl(), protowrite(), save_argtypes();
extern char binread[], binwrite[], textread[], textwrite[];
extern char *ei_first, *ei_last, *ei_next;
extern char *wh_first, *wh_last, *wh_next;
extern void putwhile();
extern char *halign;
extern flag keepsubs;
#ifdef TYQUAD
extern flag use_tyquad;
#endif
extern int n_keywords, n_st_fields;
extern char *c_keywords[], *st_fields[];
