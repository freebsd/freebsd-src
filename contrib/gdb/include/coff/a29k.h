/* COFF spec for AMD 290*0 
   Contributed by David Wood @ New York University.
 */
 
#ifndef AMD
# define AMD
#endif

/****************************************************************/

/*
** File Header and related definitions
*/

struct external_filehdr
{
	char f_magic[2];	/* magic number		 */
	char f_nscns[2];	/* number of sections	   */
	char f_timdat[4];       /* time & date stamp	    */
	char f_symptr[4];       /* file pointer to symtab       */
	char f_nsyms[4];	/* number of symtab entries     */
	char f_opthdr[2];       /* sizeof(optional hdr)	 */
	char f_flags[2];	/* flags			*/
};

#define FILHDR  struct external_filehdr
#define FILHSZ	sizeof (FILHDR)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Magic numbers for Am29000 
**	(AT&T will assign the "real" magic number)  
*/

#define SIPFBOMAGIC     0572    /* Am29000 (Byte 0 is MSB) */
#define SIPRBOMAGIC     0573    /* Am29000 (Byte 0 is LSB) */


#define A29K_MAGIC_BIG 		SIPFBOMAGIC	
#define A29K_MAGIC_LITTLE	SIPRBOMAGIC	
#define A29KBADMAG(x) 	(((x).f_magic!=A29K_MAGIC_BIG) && \
			  ((x).f_magic!=A29K_MAGIC_LITTLE))

#define OMAGIC A29K_MAGIC_BIG
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** File header flags currently known to us.
**
** Am29000 will use the F_AR32WR and F_AR32W flags to indicate
** the byte ordering in the file.
*/

/*--------------------------------------------------------------*/

/*
** Optional (a.out) header 
*/

typedef	struct external_aouthdr 
{
  char  magic[2];	       /* type of file			 */
  char  vstamp[2];	      /* version stamp			*/
  char  tsize[4];	       /* text size in bytes, padded to FW bdry*/
  char  dsize[4];	       /* initialized data "  "		*/
  char  bsize[4];	       /* uninitialized data "   "	     */
  char  entry[4];	       /* entry pt.			    */
  char  text_start[4];	  /* base of text used for this file */
  char  data_start[4];	  /* base of data used for this file */
} AOUTHDR;

#define AOUTSZ (sizeof(AOUTHDR))
#define AOUTHDRSZ (sizeof(AOUTHDR))

/* aouthdr magic numbers */
#define NMAGIC		0410	/* separate i/d executable */
#define SHMAGIC	0406		/* NYU/Ultra3 shared data executable 
				   (writable text) */

#define _ETEXT   	"_etext"

/*--------------------------------------------------------------*/

/*
** Section header and related definitions
*/

struct external_scnhdr 
{
	char	    s_name[8];      /* section name		 */
	char	    s_paddr[4];     /* physical address, aliased s_nlib */
	char	    s_vaddr[4];     /* virtual address	      */
	char	    s_size[4];      /* section size		 */
	char	    s_scnptr[4];    /* file ptr to raw data for section */
	char	    s_relptr[4];    /* file ptr to relocation       */
	char	    s_lnnoptr[4];   /* file ptr to line numbers     */
	char	    s_nreloc[2];    /* number of relocation entries */
	char	    s_nlnno[2];     /* number of line number entries*/
	char	    s_flags[4];     /* flags			*/
};

#define	SCNHDR	struct	external_scnhdr
#define	SCNHSZ	sizeof	(SCNHDR)

/*
 * names of "special" sections
 */
#define _TEXT   ".text"
#define _DATA   ".data"
#define _BSS    ".bss"
#define _LIT	".lit"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Section types - with additional section type for global 
** registers which will be relocatable for the Am29000.
**
** In instances where it is necessary for a linker to produce an
** output file which contains text or data not based at virtual
** address 0, e.g. for a ROM, then the linker should accept
** address base information as command input and use PAD sections
** to skip over unused addresses.
*/

#define	STYP_BSSREG	0x1200	/* Global register area (like STYP_INFO) */
#define STYP_ENVIR	0x2200	/* Environment (like STYP_INFO) */
#define STYP_ABS	0x4000	/* Absolute (allocated, not reloc, loaded) */

/*--------------------------------------------------------------*/

/*
** Relocation information declaration and related definitions
*/

struct external_reloc {
  char r_vaddr[4];	/* (virtual) address of reference */
  char r_symndx[4];	/* index into symbol table */
  char r_type[2];	/* relocation type */
};

#define	RELOC		struct external_reloc
#define	RELSZ		10		/* sizeof (RELOC) */ 

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Relocation types for the Am29000 
*/

#define	R_ABS		0	/* reference is absolute */
 
#define	R_IREL		030	/* instruction relative (jmp/call) */
#define	R_IABS		031	/* instruction absolute (jmp/call) */
#define	R_ILOHALF	032	/* instruction low half  (const)  */
#define	R_IHIHALF	033	/* instruction high half (consth) part 1 */
#define	R_IHCONST	034	/* instruction high half (consth) part 2 */
				/* constant offset of R_IHIHALF relocation */
#define	R_BYTE		035	/* relocatable byte value */
#define R_HWORD		036	/* relocatable halfword value */
#define R_WORD		037	/* relocatable word value */

#define	R_IGLBLRC	040	/* instruction global register RC */
#define	R_IGLBLRA	041	/* instruction global register RA */
#define	R_IGLBLRB	042	/* instruction global register RB */
 
/*
NOTE:
All the "I" forms refer to 29000 instruction formats.  The linker is 
expected to know how the numeric information is split and/or aligned
within the instruction word(s).  R_BYTE works for instructions, too.

If the parameter to a CONSTH instruction is a relocatable type, two 
relocation records are written.  The first has an r_type of R_IHIHALF 
(33 octal) and a normal r_vaddr and r_symndx.  The second relocation 
record has an r_type of R_IHCONST (34 octal), a normal r_vaddr (which 
is redundant), and an r_symndx containing the 32-bit constant offset 
to the relocation instead of the actual symbol table index.  This 
second record is always written, even if the constant offset is zero.
The constant fields of the instruction are set to zero.
*/

/*--------------------------------------------------------------*/

/*
** Line number entry declaration and related definitions
*/

struct external_lineno 
{
   union {
	 char l_symndx[4]; /* function name symbol index, iff l_lnno == 0*/
	 char l_paddr[4];  /* (physical) address of line number    */
   } l_addr;
   char l_lnno[2]; 	/* line number	  */
};

#define	LINENO		struct external_lineno
#define	LINESZ		6		/* sizeof (LINENO) */

/*--------------------------------------------------------------*/

/*
** Symbol entry declaration and related definitions
*/

#define	E_SYMNMLEN	8	/* Number of characters in a symbol name */

struct external_syment
{
  union {
    char e_name[E_SYMNMLEN];
    struct {
      char e_zeroes[4];
      char e_offset[4];
    } e;
  } e;
  char e_value[4];
  char e_scnum[2];
  char e_type[2];
  char e_sclass[1];
  char e_numaux[1];
};

#define	SYMENT	struct external_syment
#define	SYMESZ 	sizeof(SYMENT)	

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Storage class definitions - new classes for global registers.
*/

#define C_GLBLREG	19		/* global register */
#define C_EXTREG	20		/* external global register */
#define	C_DEFREG	21		/* ext. def. of global register */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*
** Derived symbol mask/shifts.
*/

#define N_BTMASK	(0xf)
#define N_BTSHFT	(4)
#define N_TMASK		(0x30)
#define N_TSHIFT	(2)

/*--------------------------------------------------------------*/

/*
** Auxiliary symbol table entry declaration and related 
** definitions.
*/

#define E_FILNMLEN	14      /* # characters in a file name	  */
#define E_DIMNUM  	4       /* # array dimensions in auxiliary entry */

union external_auxent {
	struct {
		char x_tagndx[4];       /* str, un, or enum tag indx */
		union {
			struct {
			    char  x_lnno[2]; /* declaration line number */
			    char  x_size[2]; /* str/union/array size */
			} x_lnsz;
			char x_fsize[4];	/* size of function */
		} x_misc;
		union {
			struct {		/* if ISFCN, tag, or .bb */
			    char x_lnnoptr[4];  /* ptr to fcn line # */
			    char x_endndx[4];   /* entry ndx past block end */
			} x_fcn;
			struct {		/* if ISARY, up to 4 dimen. */
			    char x_dimen[E_DIMNUM][2];
			} x_ary;
		} x_fcnary;
		char x_tvndx[2];		/* tv index */
	} x_sym;

	union {
		char x_fname[E_FILNMLEN];
		struct {
			char x_zeroes[4];
			char x_offset[4];
		} x_n;
	} x_file;

	struct {
		char x_scnlen[4];		       /* section length */
		char x_nreloc[2];       /* # relocation entries */
		char x_nlinno[2];       /* # line numbers */
	} x_scn;

	struct {
		char x_tvfill[4];       /* tv fill value */
		char x_tvlen[2];	/* length of .tv */
		char x_tvran[2][2];     /* tv range */
	} x_tv;	 /* info about .tv section (in auxent of symbol .tv)) */
};

#define	AUXENT		union external_auxent
#define	AUXESZ		18	
