/*** coff information for Apollo M68K */

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


/* Motorola 68000/68008/68010/68020 */
#define	MC68MAGIC	0520
#define MC68KWRMAGIC	0520	/* writeable text segments */
#define	MC68TVMAGIC	0521
#define MC68KROMAGIC	0521	/* readonly shareable text segments */
#define MC68KPGMAGIC	0522	/* demand paged text segments */
#define	M68MAGIC	0210
#define	M68TVMAGIC	0211

/* Apollo 68000-based machines have a different magic number. This comes
 * from /usr/include/apollo/filehdr.h
 */
#define APOLLOM68KMAGIC 0627

#define OMAGIC M68MAGIC
#define M68KBADMAG(x) (((x).f_magic!=MC68MAGIC) && ((x).f_magic!=MC68KWRMAGIC) && ((x).f_magic!=MC68TVMAGIC) && \
  ((x).f_magic!=MC68KROMAGIC) && ((x).f_magic!=MC68KPGMAGIC) && ((x).f_magic!=M68MAGIC) && ((x).f_magic!=M68TVMAGIC)  && \
  ((x).f_magic!=APOLLOM68KMAGIC) )


#define	FILHDR	struct external_filehdr
#define	FILHSZ	sizeof(FILHDR)


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
  char	o_sri[4];		/* Apollo specific - .sri data pointer */
  char  o_inlib[4];		/* Apollo specific - .inlib data pointer */
  char	vid[8];			/* Apollo specific - 64 bit version ID */
}
AOUTHDR;

#define	APOLLO_COFF_VERSION_NUMBER 1 /* the value of the aouthdr magic */
#define	AOUTHDRSZ (sizeof(AOUTHDR))
#define AOUTSZ (sizeof(AOUTHDR))



/********************** SECTION HEADER **********************/

struct external_scnhdr {
	/* Apollo allow for larger section names by  allowing it to be in
	 * the string table.
	 */
	char		s_name[8];
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

/* If s_zeores is all zeroes, s_offset gives the real location of the name
 * in the string table.
 */

#define	s_zeroes section_name.s_name
#define	s_offset (section_name.s_name+4)

/*
 * names of "special" sections
 */
#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"
#define _TV	".tv"
#define _INIT	".init"
#define _FINI	".fini"
#define	_LINES	".lines"
#define	_BLOCKS	".blocks"
#define _SRI    ".sri"                  /* Static Resource Information (systype,
 et al.) */
#define _MIR    ".mir"                  /* Module Information Records  */
#define _APTV   ".aptv"                 /* Apollo-style transfer vectors. */
#define _INLIB  ".inlib"                /* Shared Library information */
#define _RWDI   ".rwdi"         /* Read/write data initialization directives for
 compressed sections */
#define _UNWIND ".unwind"               /* Stack unwind information */

#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	sizeof(SCNHDR)


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


#define	LINENO	struct external_lineno
#define	LINESZ	sizeof(LINENO) 


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


struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
#ifdef M68K_COFF_OFFSET
  char r_offset[4];
#endif

};


#define RELOC struct external_reloc

#define RELSZ sizeof(struct external_reloc)

/* Apollo specific STYP flags */

#define STYP_RELOCATED_NOT_LOADED 0x00010000	/* Section is relocated normally during linking, but need
                                            	   not be loaded during program execution */
#define STYP_DEBUG              0x00020000	/* debug section */
#define STYP_OVERLAY		0x00040000	/* Section is overlayed */
#define STYP_INSTRUCTION    	0x00200000	/* Section contains executable code */

#define STYP_ZERO		0x00800000	/* Section is initialized to zero */
#define STYP_INSTALLED		0x02000000	/* Section should be installable in KGT */
#define STYP_LOOK_INSTALLED	0x04000000	/* Look for section in KGT */
#define STYP_SECALIGN1		0x08000000	/* Specially aligned section */
#define STYP_SECALIGN2		0x10000000	/*      "       "       "    */
#define STYP_COMPRESSED		0x20000000	/* No section data per se (s_scnptr = 0), but there are
						   initialization directives for it in .rwdi section
						   (used in conjunction with STYP_BSS) */
