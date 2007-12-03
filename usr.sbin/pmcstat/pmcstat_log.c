/*-
 * Copyright (c) 2005-2006, Joseph Koshy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Transform a hwpmc(4) log into human readable form, and into
 * gprof(1) compatible profiles.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/gmon.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/mman.h>
#include <sys/pmc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <netdb.h>
#include <pmc.h>
#include <pmclog.h>
#include <sysexits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pmcstat.h"

#define	min(A,B)		((A) < (B) ? (A) : (B))
#define	max(A,B)		((A) > (B) ? (A) : (B))

/*
 * PUBLIC INTERFACES
 *
 * pmcstat_initialize_logging()	initialize this module, called first
 * pmcstat_shutdown_logging()		orderly shutdown, called last
 * pmcstat_open_log()			open an eventlog for processing
 * pmcstat_process_log()		print/convert an event log
 * pmcstat_close_log()			finish processing an event log
 *
 * IMPLEMENTATION OF GMON OUTPUT
 *
 * We correlate each 'sample' seen in the event log back to an
 * executable object in the system. Executable objects include:
 * 	- program executables,
 *	- shared libraries loaded by the runtime loader,
 *	- dlopen()'ed objects loaded by the program,
 *	- the runtime loader itself,
 *	- the kernel and kernel modules.
 *
 * Each such executable object gets one 'gmon.out' profile, per PMC in
 * use.  Creation of 'gmon.out' profiles is done lazily.  The
 * 'gmon.out' profiles generated for a given sampling PMC are
 * aggregates of all the samples for that particular executable
 * object.
 *
 * Each process that we know about is treated as a set of regions that
 * map to executable objects.  Processes are described by
 * 'pmcstat_process' structures.  Executable objects are tracked by
 * 'pmcstat_image' structures.  The kernel and kernel modules are
 * common to all processes (they reside at the same virtual addresses
 * for all processes).  Individual processes can have their text
 * segments and shared libraries loaded at process-specific locations.
 *
 * A given executable object can be in use by multiple processes
 * (e.g., libc.so) and loaded at a different address in each.
 * pmcstat_pcmap structures track per-image mappings.
 *
 * The sample log could have samples from multiple PMCs; we
 * generate one 'gmon.out' profile per PMC.
 */

typedef const void *pmcstat_interned_string;

/*
 * 'pmcstat_pmcrecord' is a mapping from PMC ids to human-readable
 * names.
 */

struct pmcstat_pmcrecord {
	LIST_ENTRY(pmcstat_pmcrecord)	pr_next;
	pmc_id_t			pr_pmcid;
	pmcstat_interned_string	pr_pmcname;
};

static LIST_HEAD(,pmcstat_pmcrecord)	pmcstat_pmcs =
	LIST_HEAD_INITIALIZER(&pmcstat_pmcs);


/*
 * struct pmcstat_gmonfile tracks a given 'gmon.out' file.  These
 * files are mmap()'ed in as needed.
 */

struct pmcstat_gmonfile {
	LIST_ENTRY(pmcstat_gmonfile)	pgf_next; /* list of entries */
	int		pgf_overflow;	/* whether a count overflowed */
	pmc_id_t	pgf_pmcid;	/* id of the associated pmc */
	size_t		pgf_nbuckets;	/* #buckets in this gmon.out */
	unsigned int	pgf_nsamples;	/* #samples in this gmon.out */
	pmcstat_interned_string pgf_name;	/* pathname of gmon.out file */
	size_t		pgf_ndatabytes;	/* number of bytes mapped */
	void		*pgf_gmondata;	/* pointer to mmap'ed data */
};

/*
 * A 'pmcstat_image' structure describes an executable program on
 * disk.  'pi_execpath' is a cookie representing the pathname of
 * the executable.  'pi_start' and 'pi_end' are the least and greatest
 * virtual addresses for the text segments in the executable.
 * 'pi_gmonlist' contains a linked list of gmon.out files associated
 * with this image.
 */

enum pmcstat_image_type {
	PMCSTAT_IMAGE_UNKNOWN = 0,	/* never looked at the image */
	PMCSTAT_IMAGE_INDETERMINABLE,	/* can't tell what the image is */
	PMCSTAT_IMAGE_ELF32,		/* ELF 32 bit object */
	PMCSTAT_IMAGE_ELF64,		/* ELF 64 bit object */
	PMCSTAT_IMAGE_AOUT		/* AOUT object */
};

struct pmcstat_image {
	LIST_ENTRY(pmcstat_image) pi_next;	/* hash link */
	TAILQ_ENTRY(pmcstat_image) pi_lru;	/* LRU list */
	pmcstat_interned_string	pi_execpath;/* cookie */
	pmcstat_interned_string pi_samplename;  /* sample path name */

	enum pmcstat_image_type pi_type;	/* executable type */

	/*
	 * Executables have pi_start and pi_end; these are zero
	 * for shared libraries.
	 */
	uintfptr_t	pi_start;		/* start address (inclusive) */
	uintfptr_t	pi_end;			/* end address (exclusive) */
	uintfptr_t	pi_entry;		/* entry address */
	uintfptr_t	pi_vaddr;		/* virtual address where loaded */
	int		pi_isdynamic;		/* whether a dynamic
						 * object */
	int		pi_iskernelmodule;
	pmcstat_interned_string pi_dynlinkerpath; /* path in .interp */

	/*
	 * An image can be associated with one or more gmon.out files;
	 * one per PMC.
	 */
	LIST_HEAD(,pmcstat_gmonfile) pi_gmlist;
};

/*
 * All image descriptors are kept in a hash table.
 */
static LIST_HEAD(,pmcstat_image)	pmcstat_image_hash[PMCSTAT_NHASH];
static TAILQ_HEAD(,pmcstat_image)	pmcstat_image_lru =
	TAILQ_HEAD_INITIALIZER(pmcstat_image_lru);

/*
 * A 'pmcstat_pcmap' structure maps a virtual address range to an
 * underlying 'pmcstat_image' descriptor.
 */
struct pmcstat_pcmap {
	TAILQ_ENTRY(pmcstat_pcmap) ppm_next;
	uintfptr_t	ppm_lowpc;
	uintfptr_t	ppm_highpc;
	struct pmcstat_image *ppm_image;
};

/*
 * A 'pmcstat_process' structure models processes.  Each process is
 * associated with a set of pmcstat_pcmap structures that map
 * addresses inside it to executable objects.  This set is implemented
 * as a list, kept sorted in ascending order of mapped addresses.
 *
 * 'pp_pid' holds the pid of the process.  When a process exits, the
 * 'pp_isactive' field is set to zero, but the process structure is
 * not immediately reclaimed because there may still be samples in the
 * log for this process.
 */

struct pmcstat_process {
	LIST_ENTRY(pmcstat_process) pp_next;	/* hash-next */
	pid_t			pp_pid;		/* associated pid */
	int			pp_isactive;	/* whether active */
	uintfptr_t		pp_entryaddr;	/* entry address */
	TAILQ_HEAD(,pmcstat_pcmap) pp_map;	/* address range map */
};

#define	PMCSTAT_ALLOCATE		1

/*
 * All process descriptors are kept in a hash table.
 */
static LIST_HEAD(,pmcstat_process) pmcstat_process_hash[PMCSTAT_NHASH];

static struct pmcstat_process *pmcstat_kernproc; /* kernel 'process' */

/* Misc. statistics */
static struct pmcstat_stats {
	int ps_exec_aout;	/* # a.out executables seen */
	int ps_exec_elf;	/* # elf executables seen */
	int ps_exec_errors;	/* # errors processing executables */
	int ps_exec_indeterminable; /* # unknown executables seen */
	int ps_samples_total;	/* total number of samples processed */
	int ps_samples_unknown_offset;	/* #samples not in any map */
	int ps_samples_indeterminable;	/* #samples in indeterminable images */
} pmcstat_stats;

/*
 * Prototypes
 */

static void	pmcstat_gmon_create_file(struct pmcstat_gmonfile *_pgf,
    struct pmcstat_image *_image);
static pmcstat_interned_string pmcstat_gmon_create_name(const char *_sd,
    struct pmcstat_image *_img, pmc_id_t _pmcid);
static void	pmcstat_gmon_map_file(struct pmcstat_gmonfile *_pgf);
static void	pmcstat_gmon_unmap_file(struct pmcstat_gmonfile *_pgf);

static void pmcstat_image_determine_type(struct pmcstat_image *_image,
    struct pmcstat_args *_a);
static struct pmcstat_image *pmcstat_image_from_path(pmcstat_interned_string
    _path, int _iskernelmodule);
static void pmcstat_image_get_aout_params(struct pmcstat_image *_image,
    struct pmcstat_args *_a);
static void pmcstat_image_get_elf_params(struct pmcstat_image *_image,
    struct pmcstat_args *_a);
static void	pmcstat_image_increment_bucket(struct pmcstat_pcmap *_pcm,
    uintfptr_t _pc, pmc_id_t _pmcid, struct pmcstat_args *_a);
static void	pmcstat_image_link(struct pmcstat_process *_pp,
    struct pmcstat_image *_i, uintfptr_t _lpc);

static void	pmcstat_pmcid_add(pmc_id_t _pmcid,
    pmcstat_interned_string _name, struct pmcstat_args *_a);
static const char *pmcstat_pmcid_to_name(pmc_id_t _pmcid);

static void	pmcstat_process_aout_exec(struct pmcstat_process *_pp,
    struct pmcstat_image *_image, uintfptr_t _entryaddr,
    struct pmcstat_args *_a);
static void	pmcstat_process_elf_exec(struct pmcstat_process *_pp,
    struct pmcstat_image *_image, uintfptr_t _entryaddr,
    struct pmcstat_args *_a);
static void	pmcstat_process_exec(struct pmcstat_process *_pp,
    pmcstat_interned_string _path, uintfptr_t _entryaddr,
    struct pmcstat_args *_ao);
static struct pmcstat_process *pmcstat_process_lookup(pid_t _pid,
    int _allocate);
static struct pmcstat_pcmap *pmcstat_process_find_map(
    struct pmcstat_process *_p, uintfptr_t _pc);

static int	pmcstat_string_compute_hash(const char *_string);
static void pmcstat_string_initialize(void);
static pmcstat_interned_string pmcstat_string_intern(const char *_s);
static pmcstat_interned_string pmcstat_string_lookup(const char *_s);
static int	pmcstat_string_lookup_hash(pmcstat_interned_string _is);
static void pmcstat_string_shutdown(void);
static const char *pmcstat_string_unintern(pmcstat_interned_string _is);


/*
 * A simple implementation of interned strings.  Each interned string
 * is assigned a unique address, so that subsequent string compares
 * can be done by a simple pointer comparision instead of using
 * strcmp().  This speeds up hash table lookups and saves memory if
 * duplicate strings are the norm.
 */
struct pmcstat_string {
	LIST_ENTRY(pmcstat_string)	ps_next;	/* hash link */
	int		ps_len;
	int		ps_hash;
	char		*ps_string;
};

static LIST_HEAD(,pmcstat_string)	pmcstat_string_hash[PMCSTAT_NHASH];

/*
 * Compute a 'hash' value for a string.
 */

static int
pmcstat_string_compute_hash(const char *s)
{
	int hash;

	for (hash = 0; *s; s++)
		hash ^= *s;

	return (hash & PMCSTAT_HASH_MASK);
}

/*
 * Intern a copy of string 's', and return a pointer to the
 * interned structure.
 */

static pmcstat_interned_string
pmcstat_string_intern(const char *s)
{
	struct pmcstat_string *ps;
	const struct pmcstat_string *cps;
	int hash, len;

	if ((cps = pmcstat_string_lookup(s)) != NULL)
		return (cps);

	hash = pmcstat_string_compute_hash(s);
	len  = strlen(s);

	if ((ps = malloc(sizeof(*ps))) == NULL)
		err(EX_OSERR, "ERROR: Could not intern string");
	ps->ps_len = len;
	ps->ps_hash = hash;
	ps->ps_string = strdup(s);
	LIST_INSERT_HEAD(&pmcstat_string_hash[hash], ps, ps_next);
	return ((pmcstat_interned_string) ps);
}

static const char *
pmcstat_string_unintern(pmcstat_interned_string str)
{
	const char *s;

	s = ((const struct pmcstat_string *) str)->ps_string;
	return (s);
}

static pmcstat_interned_string
pmcstat_string_lookup(const char *s)
{
	struct pmcstat_string *ps;
	int hash, len;

	hash = pmcstat_string_compute_hash(s);
	len = strlen(s);

	LIST_FOREACH(ps, &pmcstat_string_hash[hash], ps_next)
	    if (ps->ps_len == len && ps->ps_hash == hash &&
		strcmp(ps->ps_string, s) == 0)
		    return (ps);
	return (NULL);
}

static int
pmcstat_string_lookup_hash(pmcstat_interned_string s)
{
	const struct pmcstat_string *ps;

	ps = (const struct pmcstat_string *) s;
	return (ps->ps_hash);
}

/*
 * Initialize the string interning facility.
 */

static void
pmcstat_string_initialize(void)
{
	int i;

	for (i = 0; i < PMCSTAT_NHASH; i++)
		LIST_INIT(&pmcstat_string_hash[i]);
}

/*
 * Destroy the string table, free'ing up space.
 */

static void
pmcstat_string_shutdown(void)
{
	int i;
	struct pmcstat_string *ps, *pstmp;

	for (i = 0; i < PMCSTAT_NHASH; i++)
		LIST_FOREACH_SAFE(ps, &pmcstat_string_hash[i], ps_next,
		    pstmp) {
			LIST_REMOVE(ps, ps_next);
			free(ps->ps_string);
			free(ps);
		}
}

/*
 * Create a gmon.out file and size it.
 */

static void
pmcstat_gmon_create_file(struct pmcstat_gmonfile *pgf,
    struct pmcstat_image *image)
{
	int fd;
	size_t count;
	struct gmonhdr gm;
	const char *pathname;
	char buffer[DEFAULT_BUFFER_SIZE];

	pathname = pmcstat_string_unintern(pgf->pgf_name);
	if ((fd = open(pathname, O_RDWR|O_NOFOLLOW|O_CREAT,
		 S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
		err(EX_OSERR, "ERROR: Cannot open \"%s\"", pathname);

	gm.lpc = image->pi_start;
	gm.hpc = image->pi_end;
	gm.ncnt = (pgf->pgf_nbuckets * sizeof(HISTCOUNTER)) +
	    sizeof(struct gmonhdr);
	gm.version = GMONVERSION;
	gm.profrate = 0;		/* use ticks */
	gm.histcounter_type = 0;	/* compatibility with moncontrol() */
	gm.spare[0] = gm.spare[1] = 0;

	/* Write out the gmon header */
	if (write(fd, &gm, sizeof(gm)) < 0)
		goto error;

	/* Zero fill the samples[] array */
	(void) memset(buffer, 0, sizeof(buffer));

	count = pgf->pgf_ndatabytes - sizeof(struct gmonhdr);
	while (count > sizeof(buffer)) {
		if (write(fd, &buffer, sizeof(buffer)) < 0)
			goto error;
		count -= sizeof(buffer);
	}

	if (write(fd, &buffer, count) < 0)
		goto error;

	/* TODO size the arc table */

	(void) close(fd);

	return;

 error:
	err(EX_OSERR, "ERROR: Cannot write \"%s\"", pathname);
}

/*
 * Determine the full pathname of a gmon.out file for a given
 * (image,pmcid) combination.  Return the interned string.
 */

pmcstat_interned_string
pmcstat_gmon_create_name(const char *samplesdir, struct pmcstat_image *image,
    pmc_id_t pmcid)
{
	const char *pmcname;
	char fullpath[PATH_MAX];

	pmcname = pmcstat_pmcid_to_name(pmcid);

	(void) snprintf(fullpath, sizeof(fullpath),
	    "%s/%s/%s", samplesdir, pmcname,
	    pmcstat_string_unintern(image->pi_samplename));

	return (pmcstat_string_intern(fullpath));
}


/*
 * Mmap in a gmon.out file for processing.
 */

static void
pmcstat_gmon_map_file(struct pmcstat_gmonfile *pgf)
{
	int fd;
	const char *pathname;

	pathname = pmcstat_string_unintern(pgf->pgf_name);

	/* the gmon.out file must already exist */
	if ((fd = open(pathname, O_RDWR | O_NOFOLLOW, 0)) < 0)
		err(EX_OSERR, "ERROR: cannot open \"%s\"", pathname);

	pgf->pgf_gmondata = mmap(NULL, pgf->pgf_ndatabytes,
	    PROT_READ|PROT_WRITE, MAP_NOSYNC|MAP_SHARED, fd, 0);

	if (pgf->pgf_gmondata == MAP_FAILED)
		err(EX_OSERR, "ERROR: cannot map \"%s\"", pathname);

	(void) close(fd);
}

/*
 * Unmap a gmon.out file after sync'ing its data to disk.
 */

static void
pmcstat_gmon_unmap_file(struct pmcstat_gmonfile *pgf)
{
	(void) msync(pgf->pgf_gmondata, pgf->pgf_ndatabytes,
	    MS_SYNC);
	(void) munmap(pgf->pgf_gmondata, pgf->pgf_ndatabytes);
	pgf->pgf_gmondata = NULL;
}

/*
 * Determine whether a given executable image is an A.OUT object, and
 * if so, fill in its parameters from the text file.
 * Sets image->pi_type.
 */

static void
pmcstat_image_get_aout_params(struct pmcstat_image *image,
    struct pmcstat_args *a)
{
	int fd;
	ssize_t nbytes;
	struct exec ex;
	const char *path;
	char buffer[PATH_MAX];

	path = pmcstat_string_unintern(image->pi_execpath);
	assert(path != NULL);

	if (image->pi_iskernelmodule)
		errx(EX_SOFTWARE, "ERROR: a.out kernel modules are "
		    "unsupported \"%s\"", path);

	(void) snprintf(buffer, sizeof(buffer), "%s%s",
	    a->pa_fsroot, path);

	if ((fd = open(buffer, O_RDONLY, 0)) < 0 ||
	    (nbytes = read(fd, &ex, sizeof(ex))) < 0) {
		warn("WARNING: Cannot determine type of \"%s\"", path);
		image->pi_type = PMCSTAT_IMAGE_INDETERMINABLE;
		if (fd != -1)
			(void) close(fd);
		return;
	}

	(void) close(fd);

	if ((unsigned) nbytes != sizeof(ex) ||
	    N_BADMAG(ex))
		return;

	image->pi_type = PMCSTAT_IMAGE_AOUT;

	/* TODO: the rest of a.out processing */

	return;
}

/*
 * Examine an ELF file to determine the size of its text segment.
 * Sets image->pi_type if anything conclusive can be determined about
 * this image.
 */

static void
pmcstat_image_get_elf_params(struct pmcstat_image *image,
    struct pmcstat_args *a)
{
	int fd, i;
	const char *path;
	void *mapbase;
	uintfptr_t minva, maxva;
	const Elf_Ehdr *h;
	const Elf_Phdr *ph;
	const Elf_Shdr *sh;
#if	defined(__amd64__)
	const Elf32_Ehdr *h32;
	const Elf32_Phdr *ph32;
	const Elf32_Shdr *sh32;
#endif
	enum pmcstat_image_type image_type;
	struct stat st;
	char buffer[PATH_MAX];

	assert(image->pi_type == PMCSTAT_IMAGE_UNKNOWN);

	minva = ~(uintfptr_t) 0;
	maxva = (uintfptr_t) 0;
	path = pmcstat_string_unintern(image->pi_execpath);

	assert(path != NULL);

	/*
	 * Look for kernel modules under FSROOT/KERNELPATH/NAME,
	 * and user mode executable objects under FSROOT/PATHNAME.
	 */
	if (image->pi_iskernelmodule)
		(void) snprintf(buffer, sizeof(buffer), "%s%s/%s",
		    a->pa_fsroot, a->pa_kernel, path);
	else
		(void) snprintf(buffer, sizeof(buffer), "%s%s",
		    a->pa_fsroot, path);

	if ((fd = open(buffer, O_RDONLY, 0)) < 0 ||
	    fstat(fd, &st) < 0 ||
	    (mapbase = mmap(0, st.st_size, PROT_READ, MAP_SHARED,
		fd, 0)) == MAP_FAILED) {
		warn("WARNING: Cannot determine type of \"%s\"", buffer);
		image->pi_type = PMCSTAT_IMAGE_INDETERMINABLE;
		if (fd != -1)
			(void) close(fd);
		return;
	}

	(void) close(fd);

	/* Punt on non-ELF objects */
	h = (const Elf_Ehdr *) mapbase;
	if (!IS_ELF(*h))
		return;

	/*
	 * We only handle executable ELF objects and kernel
	 * modules.
	 */
	if (h->e_type != ET_EXEC && h->e_type != ET_DYN &&
	    !(image->pi_iskernelmodule && h->e_type == ET_REL))
		return;

	image->pi_isdynamic = 0;
	image->pi_dynlinkerpath = NULL;
	image->pi_vaddr = 0;

#define	GET_VA(H, SH, MINVA, MAXVA) do {				\
		for (i = 0; i < (H)->e_shnum; i++)			\
			if ((SH)[i].sh_flags & SHF_EXECINSTR) {		\
				(MINVA) = min((MINVA),(SH)[i].sh_addr);	\
				(MAXVA) = max((MAXVA),(SH)[i].sh_addr +	\
				    (SH)[i].sh_size);			\
			}						\
	} while (0)


#define	GET_PHDR_INFO(H, PH, IMAGE) do {				\
		for (i = 0; i < (H)->e_phnum; i++) {			\
			switch ((PH)[i].p_type) {			\
			case PT_DYNAMIC:				\
				image->pi_isdynamic = 1;		\
				break;					\
			case PT_INTERP:					\
				image->pi_dynlinkerpath =		\
				    pmcstat_string_intern(		\
					(char *) mapbase +		\
					(PH)[i].p_offset);		\
				break;					\
			case PT_LOAD:					\
				if ((PH)[i].p_offset == 0)		\
				    image->pi_vaddr = 			\
					(PH)[i].p_vaddr;		\
				break;					\
			}						\
		}							\
	} while (0)

	switch (h->e_machine) {
	case EM_386:
	case EM_486:
#if	defined(__amd64__)
		/* a 32 bit executable */
		h32 = (const Elf32_Ehdr *) h;
		sh32 = (const Elf32_Shdr *)((uintptr_t) mapbase + h32->e_shoff);

		GET_VA(h32, sh32, minva, maxva);

		image->pi_entry = h32->e_entry;

		if (h32->e_type == ET_EXEC) {
			ph32 = (const Elf32_Phdr *)((uintptr_t) mapbase +
			    h32->e_phoff);
			GET_PHDR_INFO(h32, ph32, image);
		}
		image_type = PMCSTAT_IMAGE_ELF32;
		break;
#endif
	default:
		sh = (const Elf_Shdr *)((uintptr_t) mapbase + h->e_shoff);

		GET_VA(h, sh, minva, maxva);

		image->pi_entry = h->e_entry;

		if (h->e_type == ET_EXEC) {
			ph = (const Elf_Phdr *)((uintptr_t) mapbase +
			    h->e_phoff);
			GET_PHDR_INFO(h, ph, image);
		}
		image_type = PMCSTAT_IMAGE_ELF64;
		break;
	}

#undef	GET_PHDR_INFO
#undef	GET_VA

	image->pi_start = minva;
	image->pi_end   = maxva;
	image->pi_type  = image_type;

	if (munmap(mapbase, st.st_size) < 0)
		err(EX_OSERR, "ERROR: Cannot unmap \"%s\"", path);
	return;
}

/*
 * Given an image descriptor, determine whether it is an ELF, or AOUT.
 * If no handler claims the image, set its type to 'INDETERMINABLE'.
 */

static void
pmcstat_image_determine_type(struct pmcstat_image *image,
    struct pmcstat_args *a)
{
	assert(image->pi_type == PMCSTAT_IMAGE_UNKNOWN);

	/* Try each kind of handler in turn */
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_get_elf_params(image, a);
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_get_aout_params(image, a);

	/*
	 * Otherwise, remember that we tried to determine
	 * the object's type and had failed.
	 */
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		image->pi_type = PMCSTAT_IMAGE_INDETERMINABLE;
}

/*
 * Locate an image descriptor given an interned path, adding a fresh
 * descriptor to the cache if necessary.  This function also finds a
 * suitable name for this image's sample file.
 *
 * We defer filling in the file format specific parts of the image
 * structure till the time we actually see a sample that would fall
 * into this image.
 */

static struct pmcstat_image *
pmcstat_image_from_path(pmcstat_interned_string internedpath,
    int iskernelmodule)
{
	int count, hash, nlen;
	struct pmcstat_image *pi;
	char *sn;
	char name[NAME_MAX];

	hash = pmcstat_string_lookup_hash(internedpath);

	/* First, look for an existing entry. */
	LIST_FOREACH(pi, &pmcstat_image_hash[hash], pi_next)
	    if (pi->pi_execpath == internedpath &&
		  pi->pi_iskernelmodule == iskernelmodule) {
		    /* move descriptor to the head of the lru list */
		    TAILQ_REMOVE(&pmcstat_image_lru, pi, pi_lru);
		    TAILQ_INSERT_HEAD(&pmcstat_image_lru, pi, pi_lru);
		    return (pi);
	    }

	/*
	 * Allocate a new entry and place at the head of the hash and
	 * LRU lists.
	 */
	pi = malloc(sizeof(*pi));
	if (pi == NULL)
		return (NULL);

	pi->pi_type = PMCSTAT_IMAGE_UNKNOWN;
	pi->pi_execpath = internedpath;
	pi->pi_start = ~0;
	pi->pi_entry = ~0;
	pi->pi_end = 0;
	pi->pi_iskernelmodule = iskernelmodule;

	/*
	 * Look for a suitable name for the sample files associated
	 * with this image: if `basename(path)`+".gmon" is available,
	 * we use that, otherwise we try iterating through
	 * `basename(path)`+ "~" + NNN + ".gmon" till we get a free
	 * entry.
	 */
	if ((sn = basename(pmcstat_string_unintern(internedpath))) == NULL)
		err(EX_OSERR, "ERROR: Cannot process \"%s\"",
		    pmcstat_string_unintern(internedpath));

	nlen = strlen(sn);
	nlen = min(nlen, (int) (sizeof(name) - sizeof(".gmon")));

	snprintf(name, sizeof(name), "%.*s.gmon", nlen, sn);

	/* try use the unabridged name first */
	if (pmcstat_string_lookup(name) == NULL)
		pi->pi_samplename = pmcstat_string_intern(name);
	else {
		/*
		 * Otherwise use a prefix from the original name and
		 * upto 3 digits.
		 */
		nlen = strlen(sn);
		nlen = min(nlen, (int) (sizeof(name)-sizeof("~NNN.gmon")));
		count = 0;
		do {
			if (++count > 999)
				errx(EX_CANTCREAT, "ERROR: cannot create a gmon "
				    "file for \"%s\"", name);
			snprintf(name, sizeof(name), "%.*s~%3.3d.gmon",
			    nlen, sn, count);
			if (pmcstat_string_lookup(name) == NULL) {
				pi->pi_samplename = pmcstat_string_intern(name);
				count = 0;
			}
		} while (count > 0);
	}


	LIST_INIT(&pi->pi_gmlist);

	LIST_INSERT_HEAD(&pmcstat_image_hash[hash], pi, pi_next);
	TAILQ_INSERT_HEAD(&pmcstat_image_lru, pi, pi_lru);

	return (pi);
}

/*
 * Increment the bucket in the gmon.out file corresponding to 'pmcid'
 * and 'pc'.
 */

static void
pmcstat_image_increment_bucket(struct pmcstat_pcmap *map, uintfptr_t pc,
    pmc_id_t pmcid, struct pmcstat_args *a)
{
	struct pmcstat_image *image;
	struct pmcstat_gmonfile *pgf;
	uintfptr_t bucket;
	HISTCOUNTER *hc;

	assert(pc >= map->ppm_lowpc && pc < map->ppm_highpc);

	image = map->ppm_image;

	/*
	 * If this is the first time we are seeing a sample for
	 * this executable image, try determine its parameters.
	 */
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_determine_type(image, a);

	assert(image->pi_type != PMCSTAT_IMAGE_UNKNOWN);

	/* Ignore samples in images that we know nothing about. */
	if (image->pi_type == PMCSTAT_IMAGE_INDETERMINABLE) {
		pmcstat_stats.ps_samples_indeterminable++;
		return;
	}

	/*
	 * Find the gmon file corresponding to 'pmcid', creating it if
	 * needed.
	 */
	LIST_FOREACH(pgf, &image->pi_gmlist, pgf_next)
	    if (pgf->pgf_pmcid == pmcid)
		    break;

	/* If we don't have a gmon.out file for this PMCid, create one */
	if (pgf == NULL) {
		if ((pgf = calloc(1, sizeof(*pgf))) == NULL)
			err(EX_OSERR, "ERROR:");

		pgf->pgf_gmondata = NULL;	/* mark as unmapped */
		pgf->pgf_name = pmcstat_gmon_create_name(a->pa_samplesdir,
		    image, pmcid);
		pgf->pgf_pmcid = pmcid;
		assert(image->pi_end > image->pi_start);
		pgf->pgf_nbuckets = (image->pi_end - image->pi_start) /
		    FUNCTION_ALIGNMENT;	/* see <machine/profile.h> */
		pgf->pgf_ndatabytes = sizeof(struct gmonhdr) +
		    pgf->pgf_nbuckets * sizeof(HISTCOUNTER);
		pgf->pgf_nsamples = 0;

		pmcstat_gmon_create_file(pgf, image);

		LIST_INSERT_HEAD(&image->pi_gmlist, pgf, pgf_next);
	}

	/*
	 * Map the gmon file in if needed.  It may have been mapped
	 * out under memory pressure.
	 */
	if (pgf->pgf_gmondata == NULL)
		pmcstat_gmon_map_file(pgf);

	assert(pgf->pgf_gmondata != NULL);

	/*
	 *
	 */

	bucket = (pc - map->ppm_lowpc) / FUNCTION_ALIGNMENT;

	assert(bucket < pgf->pgf_nbuckets);

	hc = (HISTCOUNTER *) ((uintptr_t) pgf->pgf_gmondata +
	    sizeof(struct gmonhdr));

	/* saturating add */
	if (hc[bucket] < 0xFFFFU)  /* XXX tie this to sizeof(HISTCOUNTER) */
		hc[bucket]++;
	else /* mark that an overflow occurred */
		pgf->pgf_overflow = 1;

	pgf->pgf_nsamples++;
}

/*
 * Record the fact that PC values from 'start' to 'end' come from
 * image 'image'.
 */

static void
pmcstat_image_link(struct pmcstat_process *pp, struct pmcstat_image *image,
    uintfptr_t start)
{
	struct pmcstat_pcmap *pcm, *pcmnew;
	uintfptr_t offset;

	assert(image->pi_type != PMCSTAT_IMAGE_UNKNOWN &&
	    image->pi_type != PMCSTAT_IMAGE_INDETERMINABLE);

	if ((pcmnew = malloc(sizeof(*pcmnew))) == NULL)
		err(EX_OSERR, "ERROR: Cannot create a map entry");

	/*
	 * Adjust the map entry to only cover the text portion
	 * of the object.
	 */

	offset = start - image->pi_vaddr;
	pcmnew->ppm_lowpc  = image->pi_start + offset;
	pcmnew->ppm_highpc = image->pi_end + offset;
	pcmnew->ppm_image  = image;

	assert(pcmnew->ppm_lowpc < pcmnew->ppm_highpc);

	/* Overlapped mmap()'s are assumed to never occur. */
	TAILQ_FOREACH(pcm, &pp->pp_map, ppm_next)
	    if (pcm->ppm_lowpc >= pcmnew->ppm_highpc)
		    break;

	if (pcm == NULL)
		TAILQ_INSERT_TAIL(&pp->pp_map, pcmnew, ppm_next);
	else
		TAILQ_INSERT_BEFORE(pcm, pcmnew, ppm_next);
}

/*
 * Unmap images in the range [start..end) associated with process
 * 'pp'.
 */

static void
pmcstat_image_unmap(struct pmcstat_process *pp, uintfptr_t start,
    uintfptr_t end)
{
	struct pmcstat_pcmap *pcm, *pcmtmp, *pcmnew;

	assert(pp != NULL);
	assert(start < end);

	/*
	 * Cases:
	 * - we could have the range completely in the middle of an
	 *   existing pcmap; in this case we have to split the pcmap
	 *   structure into two (i.e., generate a 'hole').
	 * - we could have the range covering multiple pcmaps; these
	 *   will have to be removed.
	 * - we could have either 'start' or 'end' falling in the
	 *   middle of a pcmap; in this case shorten the entry.
	 */

	TAILQ_FOREACH_SAFE(pcm, &pp->pp_map, ppm_next, pcmtmp) {
		assert(pcm->ppm_lowpc < pcm->ppm_highpc);
		if (pcm->ppm_highpc <= start)
			continue;
		if (pcm->ppm_lowpc > end)
			return;
		if (pcm->ppm_lowpc >= start && pcm->ppm_highpc <= end) {
			/*
			 * The current pcmap is completely inside the
			 * unmapped range: remove it entirely.
			 */
			TAILQ_REMOVE(&pp->pp_map, pcm, ppm_next);
			free(pcm);
		} else if (pcm->ppm_lowpc < start && pcm->ppm_highpc > end) {
			/*
			 * Split this pcmap into two; curtail the
			 * current map to end at [start-1], and start
			 * the new one at [end].
			 */
			if ((pcmnew = malloc(sizeof(*pcmnew))) == NULL)
				err(EX_OSERR, "ERROR: Cannot split a map "
				    "entry");

			pcmnew->ppm_image = pcm->ppm_image;

			pcmnew->ppm_lowpc = end;
			pcmnew->ppm_highpc = pcm->ppm_highpc;

			pcm->ppm_highpc = start;

			TAILQ_INSERT_AFTER(&pp->pp_map, pcm, pcmnew, ppm_next);

			return;
		} else if (pcm->ppm_lowpc < start)
			pcm->ppm_lowpc = start;
		else if (pcm->ppm_highpc > end)
			pcm->ppm_highpc = end;
		else
			assert(0);
	}
}

/*
 * Add a {pmcid,name} mapping.
 */

static void
pmcstat_pmcid_add(pmc_id_t pmcid, pmcstat_interned_string ps,
    struct pmcstat_args *a)
{
	struct pmcstat_pmcrecord *pr;
	struct stat st;
	char fullpath[PATH_MAX];

	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next)
	    if (pr->pr_pmcid == pmcid) {
		    pr->pr_pmcname = ps;
		    return;
	    }

	if ((pr = malloc(sizeof(*pr))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pmc record");

	pr->pr_pmcid = pmcid;
	pr->pr_pmcname = ps;
	LIST_INSERT_HEAD(&pmcstat_pmcs, pr, pr_next);

	(void) snprintf(fullpath, sizeof(fullpath), "%s/%s", a->pa_samplesdir,
	    pmcstat_string_unintern(ps));

	/* If the path name exists, it should be a directory */
	if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode))
		return;

	if (mkdir(fullpath, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) < 0)
		err(EX_OSERR, "ERROR: Cannot create directory \"%s\"",
		    fullpath);
}

/*
 * Given a pmcid in use, find its human-readable name.
 */

static const char *
pmcstat_pmcid_to_name(pmc_id_t pmcid)
{
	struct pmcstat_pmcrecord *pr;
	char fullpath[PATH_MAX];

	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next)
	    if (pr->pr_pmcid == pmcid)
		    return (pmcstat_string_unintern(pr->pr_pmcname));

	/* create a default name and add this entry */
	if ((pr = malloc(sizeof(*pr))) == NULL)
		err(EX_OSERR, "ERROR: ");
	pr->pr_pmcid = pmcid;

	(void) snprintf(fullpath, sizeof(fullpath), "%X", (unsigned int) pmcid);
	pr->pr_pmcname = pmcstat_string_intern(fullpath);

	LIST_INSERT_HEAD(&pmcstat_pmcs, pr, pr_next);

	return (pmcstat_string_unintern(pr->pr_pmcname));
}

/*
 * Associate an AOUT image with a process.
 */

static void
pmcstat_process_aout_exec(struct pmcstat_process *pp,
    struct pmcstat_image *image, uintfptr_t entryaddr,
    struct pmcstat_args *a)
{
	(void) pp;
	(void) image;
	(void) entryaddr;
	(void) a;
	/* TODO Implement a.out handling */
}

/*
 * Associate an ELF image with a process.
 */

static void
pmcstat_process_elf_exec(struct pmcstat_process *pp,
    struct pmcstat_image *image, uintfptr_t entryaddr,
    struct pmcstat_args *a)
{
	uintmax_t libstart;
	struct pmcstat_image *rtldimage;

	assert(image->pi_type == PMCSTAT_IMAGE_ELF32 ||
	    image->pi_type == PMCSTAT_IMAGE_ELF64);

	/* Create a map entry for the base executable. */
	pmcstat_image_link(pp, image, image->pi_vaddr);

	/*
	 * For dynamically linked executables we need to:
	 * (a) find where the dynamic linker was mapped to for this
	 *     process,
	 * (b) find all the executable objects that the dynamic linker
	 *     brought in.
	 */

	if (image->pi_isdynamic) {

		/*
		 * The runtime loader gets loaded just after the maximum
		 * possible heap address.  Like so:
		 *
		 * [  TEXT DATA BSS HEAP -->*RTLD  SHLIBS   <--STACK]
		 * ^					            ^
		 * 0				   VM_MAXUSER_ADDRESS

		 *
		 * The exact address where the loader gets mapped in
		 * will vary according to the size of the executable
		 * and the limits on the size of the process'es data
		 * segment at the time of exec().  The entry address
		 * recorded at process exec time corresponds to the
		 * 'start' address inside the dynamic linker.  From
		 * this we can figure out the address where the
		 * runtime loader's file object had been mapped to.
		 */
		rtldimage = pmcstat_image_from_path(image->pi_dynlinkerpath,
		    0);
		if (rtldimage == NULL) {
			warnx("WARNING: Cannot find image for \"%s\".",
			    pmcstat_string_unintern(image->pi_dynlinkerpath));
			pmcstat_stats.ps_exec_errors++;
			return;
		}

		if (rtldimage->pi_type == PMCSTAT_IMAGE_UNKNOWN)
			pmcstat_image_get_elf_params(rtldimage, a);

		if (rtldimage->pi_type != PMCSTAT_IMAGE_ELF32 &&
		    rtldimage->pi_type != PMCSTAT_IMAGE_ELF64) {
			warnx("WARNING: rtld not an ELF object \"%s\".",
			    pmcstat_string_unintern(image->pi_dynlinkerpath));
			return;
		}

		libstart = entryaddr - rtldimage->pi_entry;
		pmcstat_image_link(pp, rtldimage, libstart);
	}
}

/*
 * Find the process descriptor corresponding to a PID.  If 'allocate'
 * is zero, we return a NULL if a pid descriptor could not be found or
 * a process descriptor process.  If 'allocate' is non-zero, then we
 * will attempt to allocate a fresh process descriptor.  Zombie
 * process descriptors are only removed if a fresh allocation for the
 * same PID is requested.
 */

static struct pmcstat_process *
pmcstat_process_lookup(pid_t pid, int allocate)
{
	uint32_t hash;
	struct pmcstat_pcmap *ppm, *ppmtmp;
	struct pmcstat_process *pp, *pptmp;

	hash = (uint32_t) pid & PMCSTAT_HASH_MASK;	/* simplicity wins */

	LIST_FOREACH_SAFE(pp, &pmcstat_process_hash[hash], pp_next, pptmp)
	    if (pp->pp_pid == pid) {
		    /* Found a descriptor, check and process zombies */
		    if (allocate && pp->pp_isactive == 0) {
			    /* remove maps */
			    TAILQ_FOREACH_SAFE(ppm, &pp->pp_map, ppm_next,
				ppmtmp) {
				    TAILQ_REMOVE(&pp->pp_map, ppm, ppm_next);
				    free(ppm);
			    }
			    /* remove process entry */
			    LIST_REMOVE(pp, pp_next);
			    free(pp);
			    break;
		    }
		    return (pp);
	    }

	if (!allocate)
		return (NULL);

	if ((pp = malloc(sizeof(*pp))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pid descriptor");

	pp->pp_pid = pid;
	pp->pp_isactive = 1;

	TAILQ_INIT(&pp->pp_map);

	LIST_INSERT_HEAD(&pmcstat_process_hash[hash], pp, pp_next);
	return (pp);
}

/*
 * Associate an image and a process.
 */

static void
pmcstat_process_exec(struct pmcstat_process *pp,
    pmcstat_interned_string path, uintfptr_t entryaddr,
    struct pmcstat_args *a)
{
	struct pmcstat_image *image;

	if ((image = pmcstat_image_from_path(path, 0)) == NULL) {
		pmcstat_stats.ps_exec_errors++;
		return;
	}

	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_determine_type(image, a);

	assert(image->pi_type != PMCSTAT_IMAGE_UNKNOWN);

	switch (image->pi_type) {
	case PMCSTAT_IMAGE_ELF32:
	case PMCSTAT_IMAGE_ELF64:
		pmcstat_stats.ps_exec_elf++;
		pmcstat_process_elf_exec(pp, image, entryaddr, a);
		break;

	case PMCSTAT_IMAGE_AOUT:
		pmcstat_stats.ps_exec_aout++;
		pmcstat_process_aout_exec(pp, image, entryaddr, a);
		break;

	case PMCSTAT_IMAGE_INDETERMINABLE:
		pmcstat_stats.ps_exec_indeterminable++;
		break;

	default:
		err(EX_SOFTWARE, "ERROR: Unsupported executable type for "
		    "\"%s\"", pmcstat_string_unintern(path));
	}
}


/*
 * Find the map entry associated with process 'p' at PC value 'pc'.
 */

static struct pmcstat_pcmap *
pmcstat_process_find_map(struct pmcstat_process *p, uintfptr_t pc)
{
	struct pmcstat_pcmap *ppm;

	TAILQ_FOREACH(ppm, &p->pp_map, ppm_next) {
		if (pc >= ppm->ppm_lowpc && pc < ppm->ppm_highpc)
			return (ppm);
		if (pc < ppm->ppm_lowpc)
			return (NULL);
	}

	return (NULL);
}



static int
pmcstat_convert_log(struct pmcstat_args *a)
{
	uintfptr_t pc;
	pid_t pid;
	struct pmcstat_image *image;
	struct pmcstat_process *pp, *ppnew;
	struct pmcstat_pcmap *ppm, *ppmtmp;
	struct pmclog_ev ev;
	pmcstat_interned_string image_path;

	while (pmclog_read(a->pa_logparser, &ev) == 0) {
		assert(ev.pl_state == PMCLOG_OK);

		switch (ev.pl_type) {
		case PMCLOG_TYPE_INITIALIZE:
			if ((ev.pl_u.pl_i.pl_version & 0xFF000000) !=
			    PMC_VERSION_MAJOR << 24 && a->pa_verbosity > 0)
				warnx("WARNING: Log version 0x%x does not "
				    "match compiled version 0x%x.",
				    ev.pl_u.pl_i.pl_version,
				    PMC_VERSION_MAJOR);
			break;
		case PMCLOG_TYPE_MAP_IN:
			/*
			 * Introduce an address range mapping for a
			 * userland process or the kernel (pid == -1).
			 *
			 * We always allocate a process descriptor so
			 * that subsequent samples seen for this
			 * address range are mapped to the current
			 * object being mapped in.
			 */
			pid = ev.pl_u.pl_mi.pl_pid;
			if (pid == -1)
				pp = pmcstat_kernproc;
			else
				pp = pmcstat_process_lookup(pid,
				    PMCSTAT_ALLOCATE);

			assert(pp != NULL);

			image_path = pmcstat_string_intern(ev.pl_u.pl_mi.
			    pl_pathname);
			image = pmcstat_image_from_path(image_path, pid == -1);
			if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
				pmcstat_image_determine_type(image, a);
			if (image->pi_type != PMCSTAT_IMAGE_INDETERMINABLE)
				pmcstat_image_link(pp, image,
				    ev.pl_u.pl_mi.pl_start);
			break;

		case PMCLOG_TYPE_MAP_OUT:
			/*
			 * Remove an address map.
			 */
			pid = ev.pl_u.pl_mo.pl_pid;
			if (pid == -1)
				pp = pmcstat_kernproc;
			else
				pp = pmcstat_process_lookup(pid, 0);

			if (pp == NULL)	/* unknown process */
				break;

			pmcstat_image_unmap(pp, ev.pl_u.pl_mo.pl_start,
			    ev.pl_u.pl_mo.pl_end);
			break;

		case PMCLOG_TYPE_PCSAMPLE:

			/*
			 * We bring in the gmon file for the image
			 * currently associated with the PMC & pid
			 * pair and increment the appropriate entry
			 * bin inside this.
			 */
			pmcstat_stats.ps_samples_total++;

			pc = ev.pl_u.pl_s.pl_pc;
			pp = pmcstat_process_lookup(ev.pl_u.pl_s.pl_pid,
			    PMCSTAT_ALLOCATE);
			if ((ppm = pmcstat_process_find_map(pp, pc)) == NULL &&
			    (ppm = pmcstat_process_find_map(pmcstat_kernproc,
				pc)) == NULL) {	/* unknown process,offset pair */
				pmcstat_stats.ps_samples_unknown_offset++;
				break;
			}

			pmcstat_image_increment_bucket(ppm, pc,
			    ev.pl_u.pl_s.pl_pmcid, a);

			break;

		case PMCLOG_TYPE_PMCALLOCATE:
			/*
			 * Record the association pmc id between this
			 * PMC and its name.
			 */
			pmcstat_pmcid_add(ev.pl_u.pl_a.pl_pmcid,
			    pmcstat_string_intern(ev.pl_u.pl_a.pl_evname), a);
			break;

		case PMCLOG_TYPE_PROCEXEC:

			/*
			 * Change the executable image associated with
			 * a process.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_x.pl_pid,
			    PMCSTAT_ALLOCATE);

			/* delete the current process map */
			TAILQ_FOREACH_SAFE(ppm, &pp->pp_map, ppm_next, ppmtmp) {
				TAILQ_REMOVE(&pp->pp_map, ppm, ppm_next);
				free(ppm);
			}

			/* associate this process  image */
			image_path = pmcstat_string_intern(
				ev.pl_u.pl_x.pl_pathname);
			assert(image_path != NULL);
			pmcstat_process_exec(pp, image_path,
			    ev.pl_u.pl_x.pl_entryaddr, a);
			break;

		case PMCLOG_TYPE_PROCEXIT:

			/*
			 * Due to the way the log is generated, the
			 * last few samples corresponding to a process
			 * may appear in the log after the process
			 * exit event is recorded.  Thus we keep the
			 * process' descriptor and associated data
			 * structures around, but mark the process as
			 * having exited.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_e.pl_pid, 0);
			if (pp == NULL)
				break;
			pp->pp_isactive = 0;	/* mark as a zombie */
			break;

		case PMCLOG_TYPE_SYSEXIT:
			pp = pmcstat_process_lookup(ev.pl_u.pl_se.pl_pid, 0);
			if (pp == NULL)
				break;
			pp->pp_isactive = 0;	/* make a zombie */
			break;

		case PMCLOG_TYPE_PROCFORK:

			/*
			 * Allocate a process descriptor for the new
			 * (child) process.
			 */
			ppnew =
			    pmcstat_process_lookup(ev.pl_u.pl_f.pl_newpid,
				PMCSTAT_ALLOCATE);

			/*
			 * If we had been tracking the parent, clone
			 * its address maps.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_f.pl_oldpid, 0);
			if (pp == NULL)
				break;
			TAILQ_FOREACH(ppm, &pp->pp_map, ppm_next)
			    pmcstat_image_link(ppnew, ppm->ppm_image,
				ppm->ppm_lowpc);
			break;

		default:	/* other types of entries are not relevant */
			break;
		}
	}

	if (ev.pl_state == PMCLOG_EOF)
		return (PMCSTAT_FINISHED);
	else if (ev.pl_state == PMCLOG_REQUIRE_DATA)
		return (PMCSTAT_RUNNING);

	err(EX_DATAERR, "ERROR: event parsing failed (record %jd, "
	    "offset 0x%jx)", (uintmax_t) ev.pl_count + 1, ev.pl_offset);
}

/*
 * Print log entries as text.
 */

static int
pmcstat_print_log(struct pmcstat_args *a)
{
	struct pmclog_ev ev;

	while (pmclog_read(a->pa_logparser, &ev) == 0) {
		assert(ev.pl_state == PMCLOG_OK);
		switch (ev.pl_type) {
		case PMCLOG_TYPE_CLOSELOG:
			PMCSTAT_PRINT_ENTRY(a,"closelog",);
			break;
		case PMCLOG_TYPE_DROPNOTIFY:
			PMCSTAT_PRINT_ENTRY(a,"drop",);
			break;
		case PMCLOG_TYPE_INITIALIZE:
			PMCSTAT_PRINT_ENTRY(a,"initlog","0x%x \"%s\"",
			    ev.pl_u.pl_i.pl_version,
			    pmc_name_of_cputype(ev.pl_u.pl_i.pl_arch));
			if ((ev.pl_u.pl_i.pl_version & 0xFF000000) !=
			    PMC_VERSION_MAJOR << 24 && a->pa_verbosity > 0)
				warnx("WARNING: Log version 0x%x != expected "
				    "version 0x%x.", ev.pl_u.pl_i.pl_version,
				    PMC_VERSION);
			break;
		case PMCLOG_TYPE_MAP_IN:
			PMCSTAT_PRINT_ENTRY(a,"map-in","%d %p \"%s\"",
			    ev.pl_u.pl_mi.pl_pid,
			    (void *) ev.pl_u.pl_mi.pl_start,
			    ev.pl_u.pl_mi.pl_pathname);
			break;
		case PMCLOG_TYPE_MAP_OUT:
			PMCSTAT_PRINT_ENTRY(a,"map-out","%d %p %p",
			    ev.pl_u.pl_mo.pl_pid,
			    (void *) ev.pl_u.pl_mo.pl_start,
			    (void *) ev.pl_u.pl_mo.pl_end);
			break;
		case PMCLOG_TYPE_PCSAMPLE:
			PMCSTAT_PRINT_ENTRY(a,"sample","0x%x %d %p %c",
			    ev.pl_u.pl_s.pl_pmcid,
			    ev.pl_u.pl_s.pl_pid,
			    (void *) ev.pl_u.pl_s.pl_pc,
			    ev.pl_u.pl_s.pl_usermode ? 'u' : 's');
			break;
		case PMCLOG_TYPE_PMCALLOCATE:
			PMCSTAT_PRINT_ENTRY(a,"allocate","0x%x \"%s\" 0x%x",
			    ev.pl_u.pl_a.pl_pmcid,
			    ev.pl_u.pl_a.pl_evname,
			    ev.pl_u.pl_a.pl_flags);
			break;
		case PMCLOG_TYPE_PMCATTACH:
			PMCSTAT_PRINT_ENTRY(a,"attach","0x%x %d \"%s\"",
			    ev.pl_u.pl_t.pl_pmcid,
			    ev.pl_u.pl_t.pl_pid,
			    ev.pl_u.pl_t.pl_pathname);
			break;
		case PMCLOG_TYPE_PMCDETACH:
			PMCSTAT_PRINT_ENTRY(a,"detach","0x%x %d",
			    ev.pl_u.pl_d.pl_pmcid,
			    ev.pl_u.pl_d.pl_pid);
			break;
		case PMCLOG_TYPE_PROCCSW:
			PMCSTAT_PRINT_ENTRY(a,"cswval","0x%x %d %jd",
			    ev.pl_u.pl_c.pl_pmcid,
			    ev.pl_u.pl_c.pl_pid,
			    ev.pl_u.pl_c.pl_value);
			break;
		case PMCLOG_TYPE_PROCEXEC:
			PMCSTAT_PRINT_ENTRY(a,"exec","0x%x %d %p \"%s\"",
			    ev.pl_u.pl_x.pl_pmcid,
			    ev.pl_u.pl_x.pl_pid,
			    (void *) ev.pl_u.pl_x.pl_entryaddr,
			    ev.pl_u.pl_x.pl_pathname);
			break;
		case PMCLOG_TYPE_PROCEXIT:
			PMCSTAT_PRINT_ENTRY(a,"exitval","0x%x %d %jd",
			    ev.pl_u.pl_e.pl_pmcid,
			    ev.pl_u.pl_e.pl_pid,
			    ev.pl_u.pl_e.pl_value);
			break;
		case PMCLOG_TYPE_PROCFORK:
			PMCSTAT_PRINT_ENTRY(a,"fork","%d %d",
			    ev.pl_u.pl_f.pl_oldpid,
			    ev.pl_u.pl_f.pl_newpid);
			break;
		case PMCLOG_TYPE_USERDATA:
			PMCSTAT_PRINT_ENTRY(a,"userdata","0x%x",
			    ev.pl_u.pl_u.pl_userdata);
			break;
		case PMCLOG_TYPE_SYSEXIT:
			PMCSTAT_PRINT_ENTRY(a,"exit","%d",
			    ev.pl_u.pl_se.pl_pid);
			break;
		default:
			fprintf(a->pa_printfile, "unknown event (type %d).\n",
			    ev.pl_type);
		}
	}

	if (ev.pl_state == PMCLOG_EOF)
		return (PMCSTAT_FINISHED);
	else if (ev.pl_state ==  PMCLOG_REQUIRE_DATA)
		return (PMCSTAT_RUNNING);

	errx(EX_DATAERR, "ERROR: event parsing failed "
	    "(record %jd, offset 0x%jx).",
	    (uintmax_t) ev.pl_count + 1, ev.pl_offset);
	/*NOTREACHED*/
}

/*
 * Public Interfaces.
 */

/*
 * Close a logfile, after first flushing all in-module queued data.
 */

int
pmcstat_close_log(struct pmcstat_args *a)
{
	if (pmc_flush_logfile() < 0 ||
	    pmc_configure_logfile(-1) < 0)
		err(EX_OSERR, "ERROR: logging failed");
	a->pa_flags &= ~(FLAG_HAS_OUTPUT_LOGFILE | FLAG_HAS_PIPE);
	return (a->pa_flags & FLAG_HAS_PIPE ? PMCSTAT_EXITING :
	    PMCSTAT_FINISHED);
}



/*
 * Open a log file, for reading or writing.
 *
 * The function returns the fd of a successfully opened log or -1 in
 * case of failure.
 */

int
pmcstat_open_log(const char *path, int mode)
{
	int error, fd;
	size_t hlen;
	const char *p, *errstr;
	struct addrinfo hints, *res, *res0;
	char hostname[MAXHOSTNAMELEN];

	errstr = NULL;
	fd = -1;

	/*
	 * If 'path' is "-" then open one of stdin or stdout depending
	 * on the value of 'mode'.
	 * 
	 * If 'path' contains a ':' and does not start with a '/' or '.',
	 * and is being opened for writing, treat it as a "host:port"
	 * specification and open a network socket.
	 *
	 * Otherwise, treat 'path' as a file name and open that.
	 */
	if (path[0] == '-' && path[1] == '\0')
		fd = (mode == PMCSTAT_OPEN_FOR_READ) ? 0 : 1;
	else if (mode == PMCSTAT_OPEN_FOR_WRITE && path[0] != '/' &&
	    path[0] != '.' && strchr(path, ':') != NULL) {

		p = strrchr(path, ':');
		hlen = p - path;
		if (p == path || hlen >= sizeof(hostname)) {
			errstr = strerror(EINVAL);
			goto done;
		}

		assert(hlen < sizeof(hostname));
		(void) strncpy(hostname, path, hlen);
		hostname[hlen] = '\0';

		(void) memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		if ((error = getaddrinfo(hostname, p+1, &hints, &res0)) != 0) {
			errstr = gai_strerror(error);
			goto done;
		}

		fd = -1;
		for (res = res0; res; res = res->ai_next) {
			if ((fd = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol)) < 0) {
				errstr = strerror(errno);
				continue;
			}
			if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
				errstr = strerror(errno);
				(void) close(fd);
				fd = -1;
				continue;
			}
			errstr = NULL;
			break;
		}
		freeaddrinfo(res0);

	} else if ((fd = open(path, mode == PMCSTAT_OPEN_FOR_READ ?
		    O_RDONLY : (O_WRONLY|O_CREAT|O_TRUNC),
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
			errstr = strerror(errno);

  done: 
	if (errstr)
		errx(EX_OSERR, "ERROR: Cannot open \"%s\" for %s: %s.", path,
		    (mode == PMCSTAT_OPEN_FOR_READ ? "reading" : "writing"),
		    errstr);

	return (fd);
}

/*
 * Process a log file in offline analysis mode.
 */

int
pmcstat_process_log(struct pmcstat_args *a)
{

	/*
	 * If gprof style profiles haven't been asked for, just print the
	 * log to the current output file.
	 */
	if (a->pa_flags & FLAG_DO_PRINT)
		return (pmcstat_print_log(a));
	else
		/* convert the log to gprof compatible profiles */
		return (pmcstat_convert_log(a));
}

/*
 * Initialize module.
 */

void
pmcstat_initialize_logging(struct pmcstat_args *a)
{
	int i;

	(void) a;

	/* use a convenient format for 'ldd' output */
	if (setenv("LD_TRACE_LOADED_OBJECTS_FMT1","%o \"%p\" %x\n",1) != 0)
		err(EX_OSERR, "ERROR: Cannot setenv");

	/* Initialize hash tables */
	pmcstat_string_initialize();
	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_INIT(&pmcstat_image_hash[i]);
		LIST_INIT(&pmcstat_process_hash[i]);
	}

	/*
	 * Create a fake 'process' entry for the kernel with pid -1.
	 * hwpmc(4) will subsequently inform us about where the kernel
	 * and any loaded kernel modules are mapped.
	 */
	if ((pmcstat_kernproc = pmcstat_process_lookup((pid_t) -1,
		 PMCSTAT_ALLOCATE)) == NULL)
		err(EX_OSERR, "ERROR: Cannot initialize logging");
}

/*
 * Shutdown module.
 */

void
pmcstat_shutdown_logging(struct pmcstat_args *a)
{
	int i;
	FILE *mf;
	struct pmcstat_gmonfile *pgf, *pgftmp;
	struct pmcstat_image *pi, *pitmp;
	struct pmcstat_process *pp, *pptmp;

	/* determine where to send the map file */
	mf = NULL;
	if (a->pa_mapfilename != NULL)
		mf = (strcmp(a->pa_mapfilename, "-") == 0) ?
		    a->pa_printfile : fopen(a->pa_mapfilename, "w");

	if (mf == NULL && a->pa_flags & FLAG_DO_GPROF &&
	    a->pa_verbosity >= 2)
		mf = a->pa_printfile;

	if (mf)
		(void) fprintf(mf, "MAP:\n");

	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_FOREACH_SAFE(pi, &pmcstat_image_hash[i], pi_next, pitmp) {

			if (mf)
				(void) fprintf(mf, " \"%s\" => \"%s\"",
				    pmcstat_string_unintern(pi->pi_execpath),
				    pmcstat_string_unintern(pi->pi_samplename));

			/* flush gmon.out data to disk */
			LIST_FOREACH_SAFE(pgf, &pi->pi_gmlist, pgf_next,
			    pgftmp) {
				pmcstat_gmon_unmap_file(pgf);
			    	LIST_REMOVE(pgf, pgf_next);
				if (mf)
					(void) fprintf(mf, " %s/%d",
					    pmcstat_pmcid_to_name(pgf->pgf_pmcid),
					    pgf->pgf_nsamples);
				if (pgf->pgf_overflow && a->pa_verbosity >= 1)
					warnx("WARNING: profile \"%s\" "
					    "overflowed.",
					    pmcstat_string_unintern(
					        pgf->pgf_name));
			    	free(pgf);
			}

			if (mf)
				(void) fprintf(mf, "\n");

			LIST_REMOVE(pi, pi_next);
			free(pi);
		}
		LIST_FOREACH_SAFE(pp, &pmcstat_process_hash[i], pp_next,
		    pptmp) {
			LIST_REMOVE(pp, pp_next);
			free(pp);
		}
	}

	pmcstat_string_shutdown();

	/*
	 * Print errors unless -q was specified.  Print all statistics
	 * if verbosity > 1.
	 */
#define	PRINT(N,V,A) do {						\
		if (pmcstat_stats.ps_##V || (A)->pa_verbosity >= 2)	\
			(void) fprintf((A)->pa_printfile, " %-40s %d\n",\
			    N, pmcstat_stats.ps_##V);			\
	} while (0)

	if (a->pa_verbosity >= 1 && a->pa_flags & FLAG_DO_GPROF) {
		(void) fprintf(a->pa_printfile, "CONVERSION STATISTICS:\n");
		PRINT("#exec/a.out", exec_aout, a);
		PRINT("#exec/elf", exec_elf, a);
		PRINT("#exec/unknown", exec_indeterminable, a);
		PRINT("#exec handling errors", exec_errors, a);
		PRINT("#samples/total", samples_total, a);
		PRINT("#samples/unclaimed", samples_unknown_offset, a);
		PRINT("#samples/unknown-object", samples_indeterminable, a);
	}

	if (mf)
		(void) fclose(mf);
}
