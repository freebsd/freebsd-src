/*** coff information for Windows CE with MIPS VR4111 */

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



#define MIPS_ARCH_MAGIC_WINCE	0x0166  /* Windows CE - little endian */
#define MIPS_PE_MAGIC		0x010b

#define MIPSBADMAG(x) \
 ((x).f_magic!=MIPS_ARCH_MAGIC_WINCE)

#define	FILHDR	struct external_filehdr
#define	FILHSZ	20


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


#define AOUTHDRSZ 28
#define AOUTSZ 28




/* define some NT default values */
/*  #define NT_IMAGE_BASE        0x400000 moved to internal.h */
#define NT_SECTION_ALIGNMENT 0x1000
#define NT_FILE_ALIGNMENT    0x200
#define NT_DEF_RESERVE       0x100000
#define NT_DEF_COMMIT        0x1000

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
};

/*
 * names of "special" sections
 */
#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"


#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	40


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
	char l_lnno[2];	/* line number		*/
};

#define GET_LINENO_LNNO(abfd, ext) bfd_h_get_16(abfd, (bfd_byte *) (ext->l_lnno));
#define PUT_LINENO_LNNO(abfd,val, ext) bfd_h_put_16(abfd,val,  (bfd_byte *) (ext->l_lnno));

#define	LINENO	struct external_lineno
#define	LINESZ	6


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
  char e_type[2];
  char e_sclass[1];
  char e_numaux[1];
};



#define N_BTMASK	(017)
#define N_TMASK		(060)
#define N_BTSHFT	(4)
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
		char x_checksum[4];	/* section COMDAT checksum */
		char x_associated[2];	/* COMDAT associated section index */
		char x_comdat[1];	/* COMDAT selection number */
	} x_scn;

        struct {
		char x_tvfill[4];	/* tv fill value */
		char x_tvlen[2];	/* length of .tv */
		char x_tvran[2][2];	/* tv range */
	} x_tv;		/* info about .tv section (in auxent of symbol .tv)) */


};

#define	SYMENT	struct external_syment
#define	SYMESZ	18	
#define	AUXENT	union external_auxent
#define	AUXESZ	18



/********************** RELOCATION DIRECTIVES **********************/

/* The external reloc has an offset field, because some of the reloc
   types on the h8 don't have room in the instruction for the entire
   offset - eg the strange jump and high page addressing modes */

struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
};


#define RELOC struct external_reloc
#define RELSZ 10

/* MIPS PE relocation types. */

#define	MIPS_R_ABSOLUTE	0 /* ignored */
#define	MIPS_R_REFHALF	1
#define	MIPS_R_REFWORD	2
#define	MIPS_R_JMPADDR	3
#define	MIPS_R_REFHI	4 /* PAIR follows */
#define	MIPS_R_REFLO	5
#define	MIPS_R_GPREL	6
#define	MIPS_R_LITERAL	7 /* same as GPREL */
#define	MIPS_R_SECTION	10
#define	MIPS_R_SECREL	11
#define	MIPS_R_SECRELLO	12
#define	MIPS_R_SECRELHI	13 /* PAIR follows */
#define	MIPS_R_RVA	34 /* 0x22 */
#define	MIPS_R_PAIR	37 /* 0x25 - symndx is really a signed 16-bit addend */
