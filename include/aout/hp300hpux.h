/* Special version of <a.out.h> for use under hp-ux.
   Copyright 1988, 1993, 1995, 2001 Free Software Foundation, Inc. */

struct hp300hpux_exec_bytes
{
  unsigned char e_info[4];   /* a_machtype/a_magic */
  unsigned char e_spare1[4];
  unsigned char e_spare2[4];
  unsigned char e_text[4];   /* length of text, in bytes */
  unsigned char e_data[4];   /* length of data, in bytes */
  unsigned char e_bss[4];    /* length of uninitialized data area , in bytes */
  unsigned char e_trsize[4]; /* length of relocation info for text, in bytes*/
  unsigned char e_drsize[4]; /* length of relocation info for data, in bytes*/
  unsigned char e_passize[4];/* HP = pascal interface size */
  unsigned char e_syms[4];   /* HP = symbol table size */
  unsigned char e_spare5[4]; /* HP = debug name table size */
  unsigned char e_entry[4];  /* start address */
  unsigned char e_spare6[4]; /* HP = source line table size */
  unsigned char e_supsize[4];/* HP = value table size */
  unsigned char e_drelocs[4];
  unsigned char e_extension[4]; /* file offset of extension */
};
#define	EXEC_BYTES_SIZE	64

struct hp300hpux_nlist_bytes
  {
    unsigned char e_value[4];
    unsigned char e_type[1];
    unsigned char e_length[1];	/* length of ascii symbol name */
    unsigned char e_almod[2];	/* alignment mod */
    unsigned char e_shlib[2];   /* info about dynamic linking */
  };
#define EXTERNAL_NLIST_SIZE 10

struct hp300hpux_reloc
  {
    unsigned char r_address[4];/* offset of of data to relocate */
    unsigned char r_index[2];  /* symbol table index of symbol         */
    unsigned char r_type[1];   /* relocation type                      */
    unsigned char r_length[1]; /* length of item to reloc              */
  };

struct hp300hpux_header_extension
{
    unsigned char e_syms[4];
    unsigned char unique_headers[12*4];
    unsigned char e_header[2];   /* type of header */
    unsigned char e_version[2];  /* version        */
    unsigned char e_size[4];     /* bytes following*/
    unsigned char e_extension[4];/* file offset of next extension */
};
#define EXTERNAL_EXTENSION_HEADER_SIZE (16*4)

/* hpux separates object files (0x106) and impure executables (0x107)  */
/* but the bfd code does not distinguish between them. Since we want to*/
/* read hpux .o files, we add an special define and use it below in    */
/* offset and address calculations.                                    */

#define HPUX_DOT_O_MAGIC 0x106
#define OMAGIC 0x107       /* object file or impure executable.  */
#define NMAGIC 0x108       /* Code indicating pure executable.   */
#define ZMAGIC 0x10B       /* demand-paged executable.           */

#define N_HEADER_IN_TEXT(x) 0

#if 0 /* libaout.h only uses the lower 8 bits */
#define HP98x6_ID 0x20A
#define HP9000S200_ID 0x20C
#endif
#define HP98x6_ID 0x0A
#define HP9000S200_ID 0x0C

#define N_BADMAG(x) ((_N_BADMAG (x)) || (_N_BADMACH (x)))

#define N_DATADDR(x) \
  ((N_MAGIC (x) == OMAGIC || N_MAGIC (x) == HPUX_DOT_O_MAGIC)		\
   ? (N_TXTADDR (x) + N_TXTSIZE (x))					\
   : (N_SEGSIZE (x) + ((N_TXTADDR (x) + N_TXTSIZE (x) - 1)		\
		       & ~ (bfd_vma) (N_SEGSIZE (x) - 1))))

#define _N_BADMACH(x) \
  (((N_MACHTYPE (x)) != HP9000S200_ID) && ((N_MACHTYPE (x)) != HP98x6_ID))

#define _N_BADMAG(x)	  (N_MAGIC(x) != HPUX_DOT_O_MAGIC \
                        && N_MAGIC(x) != OMAGIC		\
			&& N_MAGIC(x) != NMAGIC		\
  			&& N_MAGIC(x) != ZMAGIC )

#undef _N_HDROFF
#define _N_HDROFF(x) (SEGMENT_SIZE - (sizeof (struct exec)))

#undef N_DATOFF
#undef N_PASOFF
#undef N_SYMOFF
#undef N_SUPOFF
#undef N_TRELOFF
#undef N_DRELOFF
#undef N_STROFF

#define N_DATOFF(x)	( N_TXTOFF(x) + N_TXTSIZE(x) )
#define N_PASOFF(x)     ( N_DATOFF(x) + (x).a_data)
#define N_SYMOFF(x)	( N_PASOFF(x)   /* + (x).a_passize*/ )
#define N_SUPOFF(x)     ( N_SYMOFF(x) + (x).a_syms )
#define N_TRELOFF(x)	( N_SUPOFF(x)    /* + 0 (x).a_supsize*/ )
#define N_DRELOFF(x)	( N_TRELOFF(x) + (x).a_trsize )
#define N_EXTHOFF(x)    ( N_DRELOFF(x)   /*  + 0 (x).a_drsize */)
#define N_STROFF(x)	( 0 /* no string table */ )

/* use these when the file has gnu symbol tables */
#define N_GNU_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#define N_GNU_DRELOFF(x) (N_GNU_TRELOFF(x) + (x).a_trsize)
#define N_GNU_SYMOFF(x)  (N_GNU_DRELOFF(x) + (x).a_drsize)

#define TARGET_PAGE_SIZE 0x1000
#define SEGMENT_SIZE 0x1000
#define TEXT_START_ADDR 0

#undef N_SHARED_LIB
#define N_SHARED_LIB(x)  ( 0 /* no shared libraries */ )
