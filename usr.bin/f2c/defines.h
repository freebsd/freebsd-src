#define PDP11 4

#define BIGGEST_CHAR	0x7f		/* Assumes 32-bit arithmetic */
#define BIGGEST_SHORT	0x7fff		/* Assumes 32-bit arithmetic */
#define BIGGEST_LONG	0x7fffffff	/* Assumes 32-bit arithmetic */

#define M(x) (1<<x)	/* Mask (x) returns 2^x */

#define ALLOC(x)	(struct x *) ckalloc((int)sizeof(struct x))
#define ALLEXPR		(expptr) ckalloc((int)sizeof(union Expression) )
typedef int *ptr;
typedef char *charptr;
typedef FILE *FILEP;
typedef int flag;
typedef char field;	/* actually need only 4 bits */
typedef long int ftnint;
#define LOCAL static

#define NO 0
#define YES 1

#define CNULL (char *) 0	/* Character string null */
#define PNULL (ptr) 0
#define CHNULL (chainp) 0	/* Chain null */
#define ENULL (expptr) 0


/* BAD_MEMNO - used to distinguish between long string constants and other
   constants in the table */

#define BAD_MEMNO -32768


/* block tag values -- syntactic stuff */

#define TNAME 1
#define TCONST 2
#define TEXPR 3
#define TADDR 4
#define TPRIM 5		/* Primitive datum - should not appear in an
			   expptr variable, it should have already been
			   identified */
#define TLIST 6
#define TIMPLDO 7
#define TERROR 8


/* parser states - order is important, since there are several tests for
   state < INDATA   */

#define OUTSIDE 0
#define INSIDE 1
#define INDCL 2
#define INDATA 3
#define INEXEC 4

/* procedure classes */

#define PROCMAIN 1
#define PROCBLOCK 2
#define PROCSUBR 3
#define PROCFUNCT 4


/* storage classes -- vstg values.  BSS and INIT are used in the later
   merge pass over identifiers; and they are entered differently into the
   symbol table */

#define STGUNKNOWN 0
#define STGARG 1	/* adjustable dimensions */
#define STGAUTO 2	/* for stack references */
#define STGBSS 3	/* uninitialized storage (normal variables) */
#define STGINIT 4	/* initialized storage */
#define STGCONST 5
#define STGEXT 6	/* external storage */
#define STGINTR 7	/* intrinsic (late decision) reference.  See
			   chapter 5 of the Fortran 77 standard */
#define STGSTFUNCT 8
#define STGCOMMON 9
#define STGEQUIV 10
#define STGREG 11	/* register - the outermost DO loop index will be
			   in a register (because the compiler is one
			   pass, it can't know where the innermost loop is
			   */
#define STGLENG 12
#define STGNULL 13
#define STGMEMNO 14	/* interemediate-file pointer to constant table */

/* name classes -- vclass values, also   procclass   values */

#define CLUNKNOWN 0
#define CLPARAM 1	/* Parameter - macro definition */
#define CLVAR 2		/* variable */
#define CLENTRY 3
#define CLMAIN 4
#define CLBLOCK 5
#define CLPROC 6
#define CLNAMELIST 7	/* in data with this tag, the   vdcldone   flag should
			   be ignored (according to vardcl()) */


/* vprocclass values -- there is some overlap with the vclass values given
   above */

#define PUNKNOWN 0
#define PEXTERNAL 1
#define PINTRINSIC 2
#define PSTFUNCT 3
#define PTHISPROC 4	/* here to allow recursion - further distinction
			   is given in the CL tag (those just above).
			   This applies to the presence of the name of a
			   function used within itself.  The function name
			   means either call the function again, or assign
			   some value to the storage allocated to the
			   function's return value. */

/* control stack codes - these are part of a state machine which handles
   the nesting of blocks (i.e. what to do about the ELSE statement) */

#define CTLDO 1
#define CTLIF 2
#define CTLELSE 3
#define CTLIFX 4


/* operators for both Fortran input and C output.  They are common because
   so many are shared between the trees */

#define OPPLUS 1
#define OPMINUS 2
#define OPSTAR 3
#define OPSLASH 4
#define OPPOWER 5
#define OPNEG 6
#define OPOR 7
#define OPAND 8
#define OPEQV 9
#define OPNEQV 10
#define OPNOT 11
#define OPCONCAT 12
#define OPLT 13
#define OPEQ 14
#define OPGT 15
#define OPLE 16
#define OPNE 17
#define OPGE 18
#define OPCALL 19
#define OPCCALL 20
#define OPASSIGN 21
#define OPPLUSEQ 22
#define OPSTAREQ 23
#define OPCONV 24
#define OPLSHIFT 25
#define OPMOD 26
#define OPCOMMA 27
#define OPQUEST 28
#define OPCOLON 29
#define OPABS 30
#define OPMIN 31
#define OPMAX 32
#define OPADDR 33
#define OPCOMMA_ARG 34
#define OPBITOR 35
#define OPBITAND 36
#define OPBITXOR 37
#define OPBITNOT 38
#define OPRSHIFT 39
#define OPWHATSIN 40		/* dereferencing operator */
#define OPMINUSEQ 41		/* assignment operators */
#define OPSLASHEQ 42
#define OPMODEQ 43
#define OPLSHIFTEQ 44
#define OPRSHIFTEQ 45
#define OPBITANDEQ 46
#define OPBITXOREQ 47
#define OPBITOREQ 48
#define OPPREINC 49		/* Preincrement (++x) operator */
#define OPPREDEC 50		/* Predecrement (--x) operator */
#define OPDOT 51		/* structure field reference */
#define OPARROW 52		/* structure pointer field reference */
#define OPNEG1 53		/* simple negation under forcedouble */
#define OPDMIN 54		/* min(a,b) macro under forcedouble */
#define OPDMAX 55		/* max(a,b) macro under forcedouble */
#define OPASSIGNI 56		/* assignment for inquire stmt */
#define OPIDENTITY 57		/* for turning TADDR into TEXPR */
#define OPCHARCAST 58		/* for casting to char * (in I/O stmts) */
#define OPDABS 59		/* abs macro under forcedouble */
#define OPMIN2 60		/* min(a,b) macro */
#define OPMAX2 61		/* max(a,b) macro */
#define OPBITTEST 62		/* btest */
#define OPBITCLR 63		/* ibclr */
#define OPBITSET 64		/* ibset */
#define OPQBITCLR 65		/* ibclr, integer*8 */
#define OPQBITSET 66		/* ibset, integer*8 */
#define OPBITBITS 67		/* ibits */
#define OPBITSH 68		/* ishft */
#define OPBITSHC 69		/* ishftc */

/* label type codes -- used with the ASSIGN statement */

#define LABUNKNOWN 0
#define LABEXEC 1
#define LABFORMAT 2
#define LABOTHER 3


/* INTRINSIC function codes*/

#define INTREND 0
#define INTRCONV 1
#define INTRMIN 2
#define INTRMAX 3
#define INTRGEN 4	/* General intrinsic, e.g. cos v. dcos, zcos, ccos */
#define INTRSPEC 5
#define INTRBOOL 6
#define INTRCNST 7	/* constants, e.g. bigint(1.0) v. bigint (1d0) */
#define INTRBGEN 8	/* bit manipulation */


/* I/O statement codes - these all form Integer Constants, and are always
   reevaluated */

#define IOSTDIN ICON(5)
#define IOSTDOUT ICON(6)
#define IOSTDERR ICON(0)

#define IOSBAD (-1)
#define IOSPOSITIONAL 0
#define IOSUNIT 1
#define IOSFMT 2

#define IOINQUIRE 1
#define IOOPEN 2
#define IOCLOSE 3
#define IOREWIND 4
#define IOBACKSPACE 5
#define IOENDFILE 6
#define IOREAD 7
#define IOWRITE 8


/* User name tags -- these identify the form of the original identifier
   stored in a   struct Addrblock   structure (in the   user   field). */

#define UNAM_UNKNOWN 0		/* Not specified */
#define UNAM_NAME 1		/* Local symbol, store in the hash table */
#define UNAM_IDENT 2		/* Character string not stored elsewhere */
#define UNAM_EXTERN 3		/* External reference; check symbol table
				   using   memno   as index */
#define UNAM_CONST 4		/* Constant value */
#define UNAM_CHARP 5		/* pointer to string */
#define UNAM_REF 6		/* subscript reference with -s */


#define IDENT_LEN 31		/* Maximum length user.ident */
#define MAXNAMELEN 50		/* Maximum Fortran name length */

/* type masks - TYLOGICAL defined in   ftypes   */

#define MSKLOGICAL	M(TYLOGICAL)|M(TYLOGICAL1)|M(TYLOGICAL2)
#define MSKADDR	M(TYADDR)
#define MSKCHAR	M(TYCHAR)
#ifdef TYQUAD
#define MSKINT	M(TYINT1)|M(TYSHORT)|M(TYLONG)|M(TYQUAD)
#else
#define MSKINT	M(TYINT1)|M(TYSHORT)|M(TYLONG)
#endif
#define MSKREAL	M(TYREAL)|M(TYDREAL)	/* DREAL means Double Real */
#define MSKCOMPLEX	M(TYCOMPLEX)|M(TYDCOMPLEX)
#define MSKSTATIC (M(STGINIT)|M(STGBSS)|M(STGCOMMON)|M(STGEQUIV)|M(STGCONST))

/* miscellaneous macros */

/* ONEOF (x, y) -- x is the number of one of the OR'ed masks in y (i.e., x is
   the log of one of the OR'ed masks in y) */

#define ONEOF(x,y) (M(x) & (y))
#define ISCOMPLEX(z) ONEOF(z, MSKCOMPLEX)
#define ISREAL(z) ONEOF(z, MSKREAL)
#define ISNUMERIC(z) ONEOF(z, MSKINT|MSKREAL|MSKCOMPLEX)
#define ISICON(z) (z->tag==TCONST && ISINT(z->constblock.vtype))
#define ISLOGICAL(z) ONEOF(z, MSKLOGICAL)

/* ISCHAR assumes that   z   has some kind of structure, i.e. is not null */

#define ISCHAR(z) (z->headblock.vtype==TYCHAR)
#define ISINT(z)   ONEOF(z, MSKINT)	/*   z   is a tag, i.e. a mask number */
#define ISCONST(z) (z->tag==TCONST)
#define ISERROR(z) (z->tag==TERROR)
#define ISPLUSOP(z) (z->tag==TEXPR && z->exprblock.opcode==OPPLUS)
#define ISSTAROP(z) (z->tag==TEXPR && z->exprblock.opcode==OPSTAR)
#define ISONE(z) (ISICON(z) && z->constblock.Const.ci==1)
#define INT(z) ONEOF(z, MSKINT|MSKCHAR)	/* has INT storage in real life */
#define ICON(z) mkintcon( (ftnint)(z) )

/* NO66 -- F77 feature is being used
   NOEXT -- F77 extension is being used */

#define NO66(s)	if(no66flag) err66(s)
#define NOEXT(s)	if(noextflag) errext(s)
