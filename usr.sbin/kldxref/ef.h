/* $FreeBSD$ */

#ifndef _EF_H_
#define _EF_H_

#define	EFT_KLD		1
#define	EFT_KERNEL	2

typedef struct elf_file {
	char*		ef_name;
	Elf_Phdr *	ef_ph;
	int		ef_fd;
	int		ef_type;
	Elf_Ehdr	ef_hdr;
	void*		ef_fpage;		/* First block of the file */
	int		ef_fplen;		/* length of first block */
	Elf_Dyn*	ef_dyn;			/* Symbol table etc. */
	Elf_Hashelt	ef_nbuckets;
	Elf_Hashelt	ef_nchains;
	Elf_Hashelt*	ef_buckets;
	Elf_Hashelt*	ef_chains;
	Elf_Hashelt*	ef_hashtab;
	Elf_Off		ef_stroff;
	caddr_t		ef_strtab;
	int		ef_strsz;
	Elf_Off		ef_symoff;
	Elf_Sym*	ef_symtab;
	int		ef_nsegs;
	Elf_Phdr *	ef_segs[2];
	int		ef_verbose;
} *elf_file_t;

__BEGIN_DECLS
int ef_open(const char *, elf_file_t, int);
int ef_close(elf_file_t ef);
int ef_read(elf_file_t ef, Elf_Off offset, size_t len, void* dest);
int ef_read_entry(elf_file_t ef, Elf_Off offset, size_t len, void **ptr);
int ef_seg_read(elf_file_t ef, Elf_Off offset, size_t len, void *dest);
int ef_seg_read_entry(elf_file_t ef, Elf_Off offset, size_t len, void**ptr);
int ef_lookup_symbol(elf_file_t ef, const char* name, Elf_Sym** sym);
__END_DECLS

#endif /* _EF_H_*/
