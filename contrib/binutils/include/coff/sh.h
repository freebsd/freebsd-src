/*** coff information for Hitachi SH */

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



#define	SH_ARCH_MAGIC_BIG	0x0500
#define	SH_ARCH_MAGIC_LITTLE	0x0550  /* Little endian SH */


#define SHBADMAG(x) \
 (((x).f_magic!=SH_ARCH_MAGIC_BIG) && \
  ((x).f_magic!=SH_ARCH_MAGIC_LITTLE))

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
	char l_lnno[4];	/* line number		*/
};

#define GET_LINENO_LNNO(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *) (ext->l_lnno));
#define PUT_LINENO_LNNO(abfd,val, ext) bfd_h_put_32(abfd,val,  (bfd_byte *) (ext->l_lnno));

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
  char r_offset[4];
  char r_type[2];
  char r_stuff[2];
};


#define RELOC struct external_reloc
#define RELSZ 16

/* SH relocation types.  Not all of these are actually used.  */

#define R_SH_UNUSED	0		/* only used internally */
#define R_SH_PCREL8 	3		/*  8 bit pcrel 	*/
#define R_SH_PCREL16 	4		/* 16 bit pcrel 	*/
#define R_SH_HIGH8  	5		/* high 8 bits of 24 bit address */
#define R_SH_LOW16 	7		/* low 16 bits of 24 bit immediate */
#define R_SH_IMM24	6		/* 24 bit immediate */
#define R_SH_PCDISP8BY4	9  		/* PC rel 8 bits *4 +ve */
#define R_SH_PCDISP8BY2	10  		/* PC rel 8 bits *2 +ve */
#define R_SH_PCDISP8    11  		/* 8 bit branch */
#define R_SH_PCDISP     12  		/* 12 bit branch */
#define R_SH_IMM32      14    		/* 32 bit immediate */
#define R_SH_IMM8   	16		/* 8 bit immediate */
#define R_SH_IMM8BY2    17		/* 8 bit immediate *2 */
#define R_SH_IMM8BY4    18		/* 8 bit immediate *4 */
#define R_SH_IMM4   	19		/* 4 bit immediate */
#define R_SH_IMM4BY2    20		/* 4 bit immediate *2 */
#define R_SH_IMM4BY4    21		/* 4 bit immediate *4 */
#define R_SH_PCRELIMM8BY2   22		/* PC rel 8 bits *2 unsigned */
#define R_SH_PCRELIMM8BY4   23		/* PC rel 8 bits *4 unsigned */
#define R_SH_IMM16      24    		/* 16 bit immediate */

/* The switch table reloc types are used for relaxing.  They are
   generated for expressions such as
     .word L1 - L2
   The r_offset field holds the difference between the reloc address
   and L2.  */
#define R_SH_SWITCH8	33		/* 8 bit switch table entry */
#define R_SH_SWITCH16	25		/* 16 bit switch table entry */
#define R_SH_SWITCH32	26		/* 32 bit switch table entry */

/* The USES reloc type is used for relaxing.  The compiler will
   generate .uses pseudo-ops when it finds a function call which it
   can relax.  The r_offset field of the USES reloc holds the PC
   relative offset to the instruction which loads the register used in
   the function call.  */
#define R_SH_USES	27		/* .uses pseudo-op */

/* The COUNT reloc type is used for relaxing.  The assembler will
   generate COUNT relocs for addresses referred to by the register
   loads associated with USES relocs.  The r_offset field of the COUNT
   reloc holds the number of times the address is referenced in the
   object file.  */
#define R_SH_COUNT	28		/* Count of constant pool uses */

/* The ALIGN reloc type is used for relaxing.  The r_offset field is
   the power of two to which subsequent portions of the object file
   must be aligned.  */
#define R_SH_ALIGN	29		/* .align pseudo-op */

/* The CODE and DATA reloc types are used for aligning load and store
   instructions.  The assembler will generate a CODE reloc before a
   block of instructions.  It will generate a DATA reloc before data.
   A section should be processed assuming it contains data, unless a
   CODE reloc is seen.  The only relevant pieces of information in the
   CODE and DATA relocs are the section and the address.  The symbol
   and offset are meaningless.  */
#define R_SH_CODE	30		/* start of code */
#define R_SH_DATA	31		/* start of data */

/* The LABEL reloc type is used for aligning load and store
   instructions.  The assembler will generate a LABEL reloc for each
   label within a block of instructions.  This permits the linker to
   avoid swapping instructions which are the targets of branches.  */
#define R_SH_LABEL	32		/* label */

/* NB: R_SH_SWITCH8 is 33 */
