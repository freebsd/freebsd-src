/*** coff information for 80960.  Origins: Intel corp, natch. */

/* NOTE: Tagentries (cf TAGBITS) are no longer used by the 960 */

/********************** FILE HEADER **********************/

struct external_filehdr {
	char f_magic[2];	/* magic number			*/
	char f_nscns[2];	/* number of sections		*/
	char f_timdat[4];	/* time & date stamp		*/
	char f_symptr[4];	/* file pointer to symtab	*/
	char f_nsyms[4];	/* number of symtab entries	*/
	char f_opthdr[2];	/* sizeof(optional hdr)		*/
	char f_flags[2];	/* flags			*/
};

#define OMAGIC      (0407)	/* old impure format. data immediately
                                   follows text. both sections are rw. */
#define NMAGIC      (0410)	/* split i&d, read-only text */

/*
*	Intel 80960 (I960) processor flags.
*	F_I960TYPE == mask for processor type field. 
*/

#define	F_I960TYPE	(0xf000)
#define	F_I960CORE	(0x1000)
#define	F_I960KB	(0x2000)
#define	F_I960SB	(0x2000)
#define	F_I960MC	(0x3000)
#define	F_I960XA	(0x4000)
#define	F_I960CA	(0x5000)
#define	F_I960KA	(0x6000)
#define	F_I960SA	(0x6000)
#define F_I960JX	(0x7000)
#define F_I960HX	(0x8000)


/** i80960 Magic Numbers
*/

#define I960ROMAGIC	(0x160)	/* read-only text segments */
#define I960RWMAGIC	(0x161)	/* read-write text segments */

#define I960BADMAG(x) (((x).f_magic!=I960ROMAGIC) && ((x).f_magic!=I960RWMAGIC))

#define	FILHDR	struct external_filehdr
#define	FILHSZ	20

/********************** AOUT "OPTIONAL HEADER" **********************/

typedef struct {
	unsigned long	phys_addr;
	unsigned long	bitarray;
} TAGBITS;



typedef struct 
{
  char 	magic[2];		/* type of file				*/
  char	vstamp[2];		/* version stamp			*/
  char	tsize[4];		/* text size in bytes, padded to FW bdry*/
  char	dsize[4];		/* initialized data "  "		*/
  char	bsize[4];		/* uninitialized data "   "		*/
  char	entry[4];		/* entry pt.				*/
  char 	text_start[4];		/* base of text used for this file */
  char 	data_start[4];		/* base of data used for this file */
  char	tagentries[4];		/* number of tag entries to follow */
}
AOUTHDR;

/* return a pointer to the tag bits array */

#define TAGPTR(aout) ((TAGBITS *) (&(aout.tagentries)+1))

/* compute size of a header */

/*#define AOUTSZ(aout) (sizeof(AOUTHDR)+(aout.tagentries*sizeof(TAGBITS)))*/
#define AOUTSZ (sizeof(AOUTHDR))



/********************** SECTION HEADER **********************/


struct external_scnhdr {
	char		s_name[8];	/* section name			*/
	char		s_paddr[4];	/* physical address, aliased s_nlib */
	char		s_vaddr[4];	/* virtual address		*/
	char		s_size[4];	/* section size			*/
	char		s_scnptr[4];	/* file ptr to raw data for section */
	char		s_relptr[4];	/* file ptr to relocation	*/
	char		s_lnnoptr[4];	/* file ptr to line numbers	*/
	char		s_nreloc[2];	/* number of relocation entries	*/
	char		s_nlnno[2];	/* number of line number entries*/
	char		s_flags[4];	/* flags			*/
	char 		s_align[4];	/* section alignment		*/
};


#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	sizeof(SCNHDR)

/*
 * names of "special" sections
 */
#define _TEXT   ".text"
#define _DATA   ".data"
#define _BSS    ".bss"

/********************** LINE NUMBERS **********************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */
struct external_lineno {
	union {
		char l_symndx[4];	/* function name symbol index, iff l_lnno == 0*/
		char l_paddr[4];	/* (physical) address of line number	*/
	} l_addr;
	char l_lnno[2];		/* line number		*/
	char padding[2];	/* force alignment	*/
};


#define	LINENO	struct external_lineno
#define	LINESZ	8


/********************** SYMBOLS **********************/

#define E_SYMNMLEN	8	/* # characters in a symbol name	*/
#define E_FILNMLEN	14	/* # characters in a file name		*/
#define E_DIMNUM	4	/* # array dimensions in auxiliary entry */

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
  char e_flags[2];
  char e_type[4];
  char e_sclass[1];
  char e_numaux[1];
  char pad2[2];
};




#define N_BTMASK	(0x1f)
#define N_TMASK		(0x60)
#define N_BTSHFT	(5)
#define N_TSHIFT	(2)
  
union external_auxent {
	struct {
		char x_tagndx[4];	/* str, un, or enum tag indx */
		union {
			struct {
			    char  x_lnno[2]; /* declaration line number */
			    char  x_size[2]; /* str/union/array size */
			} x_lnsz;
			char x_fsize[4];	/* size of function */
		} x_misc;
		union {
			struct {		/* if ISFCN, tag, or .bb */
			    char x_lnnoptr[4];	/* ptr to fcn line # */
			    char x_endndx[4];	/* entry ndx past block end */
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
		char x_scnlen[4];			/* section length */
		char x_nreloc[2];	/* # relocation entries */
		char x_nlinno[2];	/* # line numbers */
	} x_scn;

        struct {
		char x_tvfill[4];	/* tv fill value */
		char x_tvlen[2];	/* length of .tv */
		char x_tvran[2][2];	/* tv range */
	} x_tv;		/* info about .tv section (in auxent of symbol .tv)) */

	/******************************************
	 *  I960-specific *2nd* aux. entry formats
	 ******************************************/
	struct {
	  /* This is a very old typo that keeps getting propagated. */
#define x_stdindx x_stindx
		char x_stindx[4];	/* sys. table entry */
	} x_sc;	/* system call entry */

	struct {
		char x_balntry[4]; /* BAL entry point */
	} x_bal; /* BAL-callable function */

        struct {
		char x_timestamp[4];	        /* time stamp */
		char 	x_idstring[20];	        /* producer identity string */
	} x_ident;	                        /* Producer ident info */

};



#define	SYMENT	struct external_syment
#define	SYMESZ	sizeof(SYMENT)			/* FIXME - calc by hand */
#define	AUXENT	union external_auxent
#define	AUXESZ	sizeof(AUXENT)			/* FIXME - calc by hand */

#	define _ETEXT	"_etext"

/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
  char pad[2];
};


/* Relevent values for r_type and i960.  Would someone please document them */


#define RELOC struct external_reloc
#define RELSZ 12

