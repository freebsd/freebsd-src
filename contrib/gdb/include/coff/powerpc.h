/* Basic coff information for the PowerPC
 *
 * Based on coff/rs6000.h, coff/i386.h and others.
 *
 * Initial release: Kim Knuttila (krk@cygnus.com)
 */

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

#define	FILHDR	struct external_filehdr
#define	FILHSZ	sizeof(FILHDR)

/* Bits for f_flags:
 *	F_RELFLG	relocation info stripped from file
 *	F_EXEC		file is executable (no unresolved external references)
 *	F_LNNO		line numbers stripped from file
 *	F_LSYMS		local symbols stripped from file
 *	F_AR32WR	file has byte ordering of an AR32WR machine (e.g. vax)
 */

#define F_RELFLG	(0x0001)
#define F_EXEC		(0x0002)
#define F_LNNO		(0x0004)
#define F_LSYMS		(0x0008)

/* extra NT defines */
#define PPCMAGIC       0760         /* peeked on aa PowerPC Windows NT box */
#define DOSMAGIC       0x5a4d       /* from arm.h, i386.h */
#define NT_SIGNATURE   0x00004550   /* from arm.h, i386.h */

/* from winnt.h */
#define IMAGE_NT_OPTIONAL_HDR_MAGIC        0x10b

#define PPCBADMAG(x) ((x).f_magic != PPCMAGIC) 

/********************** AOUT "OPTIONAL HEADER" **********************/

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
}
AOUTHDR;

#define AOUTSZ (sizeof(AOUTHDR))


/********************** SECTION HEADER **********************/

struct external_scnhdr {
  char		s_name[8];	/* section name			    */
  char		s_paddr[4];	/* physical address, aliased s_nlib */
  char		s_vaddr[4];	/* virtual address	            */
  char		s_size[4];	/* section size			    */
  char		s_scnptr[4];	/* file ptr to raw data for section */
  char		s_relptr[4];	/* file ptr to relocation	    */
  char		s_lnnoptr[4];	/* file ptr to line numbers	    */
  char		s_nreloc[2];	/* number of relocation entries	    */
  char		s_nlnno[2];	/* number of line number entries    */
  char		s_flags[4];	/* flags			    */
};

#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	sizeof(SCNHDR)

/*
 * names of "special" sections
 */
#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"
#define _COMMENT ".comment"
#define _LIB ".lib"

/********************** LINE NUMBERS **********************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */
struct external_lineno {
  union {
    char l_symndx[4];	/* function name symbol index, iff l_lnno == 0 */
    char l_paddr[4];	/* (physical) address of line number	       */
  } l_addr;
  char l_lnno[2];	/* line number		                       */
};

#define	LINENO	struct external_lineno
#define	LINESZ	6

/********************** SYMBOLS **********************/

#define E_SYMNMLEN	8     /* # characters in a symbol name       */

/* Allow the file name length to be overridden in the including file   */
#ifndef E_FILNMLEN
#define E_FILNMLEN	14
#endif

#define E_DIMNUM	4     /* # array dimensions in auxiliary entry */

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
#define	SYMESZ	18	

#define N_BTMASK	(0xf)
#define N_TMASK		(0x30)
#define N_BTSHFT	(4)
#define N_TSHIFT	(2)
  
union external_auxent {
  struct {
    char x_tagndx[4];	           /* str, un, or enum tag indx       */
    union {
      struct {
	char  x_lnno[2];           /* declaration line number         */
	char  x_size[2];           /* str/union/array size            */
      } x_lnsz;
      char x_fsize[4];	           /* size of function                */
    } x_misc;
    union {
      struct {		           /* if ISFCN, tag, or .bb           */
	char x_lnnoptr[4];         /* ptr to fcn line #               */
	char x_endndx[4];	   /* entry ndx past block end        */
      } x_fcn;
      struct {		           /* if ISARY, up to 4 dimen.        */
	char x_dimen[E_DIMNUM][2];
      } x_ary;
    } x_fcnary;
    char x_tvndx[2];		   /* tv index                        */
  } x_sym;
  
  union {
    char x_fname[E_FILNMLEN];
    struct {
      char x_zeroes[4];
      char x_offset[4];
    } x_n;
  } x_file;
  
  struct {
    char x_scnlen[4];	           /* section length                  */
    char x_nreloc[2];	           /* # relocation entries            */
    char x_nlinno[2];	           /* # line numbers                  */
  } x_scn;
};

#define	AUXENT	union external_auxent
#define	AUXESZ	18

#define _ETEXT	"etext"

/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
};

#define RELOC struct external_reloc
#define RELSZ 10

