/*** coff information for 88k bcs */

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

#define MC88MAGIC  0540           /* 88k BCS executable */
#define MC88DMAGIC 0541           /* DG/UX executable   */
#define MC88OMAGIC 0555	          /* Object file        */

#define MC88BADMAG(x) (((x).f_magic!=MC88MAGIC) &&((x).f_magic!=MC88DMAGIC) && ((x).f_magic != MC88OMAGIC))

#define	FILHDR	struct external_filehdr
#define	FILHSZ	sizeof(FILHDR)


/********************** AOUT "OPTIONAL HEADER" **********************/


#define PAGEMAGIC3 0414 /* Split i&d, zero mapped */
#define PAGEMAGICBCS 0413


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


/* compute size of a header */

#define AOUTSZ (sizeof(AOUTHDR))


/********************** SECTION HEADER **********************/


struct external_scnhdr 
{
  char		s_name[8];	/* section name			*/
  char		s_paddr[4];	/* physical address, aliased s_nlib */
  char		s_vaddr[4];	/* virtual address		*/
  char		s_size[4];	/* section size			*/
  char		s_scnptr[4];	/* file ptr to raw data for section */
  char		s_relptr[4];	/* file ptr to relocation	*/
  char		s_lnnoptr[4];	/* file ptr to line numbers	*/
  char		s_nreloc[4];	/* number of relocation entries	*/
  char		s_nlnno[4];	/* number of line number entries*/
  char		s_flags[4];	/* flags			*/
};


#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	sizeof(SCNHDR)

/*
 * names of "special" sections
 */
#define _TEXT   ".text"
#define _DATA   ".data"
#define _BSS    ".bss"
#define _COMMENT ".comment"

/********************** LINE NUMBERS **********************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */
struct external_lineno{
	union {
		char l_symndx[4];	/* function name symbol index, iff l_lnno == 0*/
		char l_paddr[4];	/* (physical) address of line number	*/
	} l_addr;

	char l_lnno[4];

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
  char e_type[2];
  char e_sclass[1];
  char e_numaux[1];
  char pad2[2];
};




#define N_BTMASK	017
#define N_TMASK		060
#define N_BTSHFT	4
#define N_TSHIFT	2


/* Note that this isn't the same shape as other coffs */
union external_auxent {
  struct {
    char x_tagndx[4];		/* str, un, or enum tag indx */
    /* 4 */
    union {
      char x_fsize[4];		/* size of function */
      struct {
	char  x_lnno[4];	/* declaration line number */
	char  x_size[4];	/* str/union/array size */
      } x_lnsz;
    } x_misc;
    
    /* 12 */
    union {
      struct {			/* if ISFCN, tag, or .bb */
	char x_lnnoptr[4];	/* ptr to fcn line # */
	char x_endndx[4];		/* entry ndx past block end */
      } x_fcn;
      struct {			/* if ISARY, up to 4 dimen. */
	char x_dimen[E_DIMNUM][2];
      } x_ary;
    } x_fcnary;
    /* 20 */
    
  } x_sym;
  
  union {
    char x_fname[E_FILNMLEN];
    struct {
      char x_zeroes[4];
      char x_offset[4];
    } x_n;
  } x_file;
  
  struct {
    char x_scnlen[4];		/* section length */
    char x_nreloc[4];		/* # relocation entries */
    char x_nlinno[4];		/* # line numbers */
  } x_scn;
  
  struct {
    char x_tvfill[4];		/* tv fill value */
    char x_tvlen[2];		/* length of .tv */
    char x_tvran[2][2];		/* tv range */
  } x_tv;			/* info about .tv section (in auxent of symbol .tv)) */

};

#define GET_FCN_LNNOPTR(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *)ext->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#define GET_FCN_ENDNDX(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx)
#define PUT_FCN_LNNOPTR(abfd, in, ext)  bfd_h_put_32(abfd,  in, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#define PUT_FCN_ENDNDX(abfd, in, ext) bfd_h_put_32(abfd, in, (bfd_byte *)   ext->x_sym.x_fcnary.x_fcn.x_endndx)
#define GET_LNSZ_SIZE(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_misc.x_lnsz.x_size)
#define GET_LNSZ_LNNO(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_misc.x_lnsz.x_lnno)
#define PUT_LNSZ_LNNO(abfd, in, ext) bfd_h_put_32(abfd, in, (bfd_byte *) ext->x_sym.x_misc.x_lnsz.x_lnno)
#define PUT_LNSZ_SIZE(abfd, in, ext) bfd_h_put_32(abfd, in, (bfd_byte *) ext->x_sym.x_misc.x_lnsz.x_size)
#define GET_SCN_SCNLEN(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *) ext->x_scn.x_scnlen)
#define GET_SCN_NRELOC(abfd, ext) bfd_h_get_32(abfd, (bfd_byte *) ext->x_scn.x_nreloc)
#define GET_SCN_NLINNO(abfd, ext)  bfd_h_get_32(abfd, (bfd_byte *) ext->x_scn.x_nlinno)
#define PUT_SCN_SCNLEN(abfd,in, ext) bfd_h_put_32(abfd, in, (bfd_byte *) ext->x_scn.x_scnlen)
#define PUT_SCN_NRELOC(abfd,in, ext) bfd_h_put_32(abfd, in, (bfd_byte *)ext->x_scn.x_nreloc)
#define PUT_SCN_NLINNO(abfd,in, ext)  bfd_h_put_32(abfd,in, (bfd_byte *) ext->x_scn.x_nlinno)
#define GET_LINENO_LNNO(abfd, ext)  bfd_h_get_32(abfd, (bfd_byte *) (ext->l_lnno))
#define PUT_LINENO_LNNO(abfd,val, ext)  bfd_h_put_32(abfd,val,  (bfd_byte *) (ext->l_lnno));



#define	SYMENT	struct external_syment
#define	SYMESZ	20
#define	AUXENT	union external_auxent
#define	AUXESZ	20


/********************** RELOCATION DIRECTIVES **********************/

struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_type[2];
  char r_offset[2];
};

#define RELOC struct external_reloc
#define RELSZ  12

#define NO_TVNDX
