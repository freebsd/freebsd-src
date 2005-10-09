/*-
 * Copyright (c) 2005, Joseph Koshy
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Transform a hwpmc(4) log into human readable form and into gprof(1)
 * compatible profiles.
 *
 * Each executable object encountered in the log gets one 'gmon.out'
 * profile per PMC.  We currently track:
 * 	- program executables
 *	- shared libraries loaded by the runtime loader
 *	- the runtime loader itself
 *	- the kernel.
 * We do not track shared objects mapped in by dlopen() yet (this
 * needs additional support from hwpmc(4)).
 *
 * 'gmon.out' profiles generated for a given sampling PMC are
 * aggregates of all the samples for that particular executable
 * object.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/gmon.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/mman.h>
#include <sys/pmc.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
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
 * A simple implementation of interned strings.  Each interned string
 * is assigned a unique address, so that subsequent string compares
 * can be done by a simple pointer comparision instead of with
 * strcmp().
 */
struct pmcstat_string {
	LIST_ENTRY(pmcstat_string)	ps_next;	/* hash link */
	int		ps_len;
	int		ps_hash;
	const char	*ps_string;
};

static LIST_HEAD(,pmcstat_string)	pmcstat_string_hash[PMCSTAT_NHASH];

/*
 * 'pmcstat_pmcrecord' is a mapping from PMC ids to human-readable
 * names.
 */

struct pmcstat_pmcrecord {
	LIST_ENTRY(pmcstat_pmcrecord)	pr_next;
	pmc_id_t	pr_pmcid;
	const char 	*pr_pmcname;
};

static LIST_HEAD(,pmcstat_pmcrecord)	pmcstat_pmcs =
	LIST_HEAD_INITIALIZER(&pmcstat_pmcs);


/*
 * struct pmcstat_gmonfile tracks a given 'gmon.out' file.  These
 * files are mmap()'ed in as needed.
 */

struct pmcstat_gmonfile {
	LIST_ENTRY(pmcstat_gmonfile)	pgf_next; /* list of entries */
	pmc_id_t	pgf_pmcid;	/* id of the associated pmc */
	size_t		pgf_nbuckets;	/* #buckets in this gmon.out */
	const char	*pgf_name;	/* pathname of gmon.out file */
	size_t		pgf_ndatabytes;	/* number of bytes mapped */
	void		*pgf_gmondata;	/* pointer to mmap'ed data */
};

static TAILQ_HEAD(,pmcstat_gmonfile)	pmcstat_gmonfiles =
	TAILQ_HEAD_INITIALIZER(pmcstat_gmonfiles);

/*
 * A 'pmcstat_image' structure describes an executable program on
 * disk.  'pi_internedpath' is a cookie representing the pathname of
 * the executable.  'pi_start' and 'pi_end' are the least and greatest
 * virtual addresses for the text segments in the executable.
 * 'pi_gmonlist' contains a linked list of gmon.out files associated
 * with this image.
 */

enum pmcstat_image_type {
	PMCSTAT_IMAGE_UNKNOWN = 0,
	PMCSTAT_IMAGE_ELF,
	PMCSTAT_IMAGE_AOUT
};

struct pmcstat_image {
	LIST_ENTRY(pmcstat_image) pi_next;	/* hash link */
	TAILQ_ENTRY(pmcstat_image) pi_lru;	/* LRU list */
	const char	*pi_internedpath;	/* cookie */
	const char	*pi_samplename;		/* sample path name */

	enum pmcstat_image_type pi_type;	/* executable type */
	uintfptr_t	pi_start;		/* start address (inclusive) */
	uintfptr_t	pi_end;			/* end address (exclusive) */
	uintfptr_t	pi_entry;		/* entry address */
	int		pi_isdynamic;		/* whether a dynamic object */
	const char	*pi_dynlinkerpath;	/* path in .interp section */

	LIST_HEAD(,pmcstat_gmonfile) pi_gmlist;
};

static LIST_HEAD(,pmcstat_image)	pmcstat_image_hash[PMCSTAT_NHASH];
static TAILQ_HEAD(,pmcstat_image)	pmcstat_image_lru =
	TAILQ_HEAD_INITIALIZER(pmcstat_image_lru);

struct pmcstat_pcmap {
	TAILQ_ENTRY(pmcstat_pcmap) ppm_next;
	uintfptr_t	ppm_lowpc;
	uintfptr_t	ppm_highpc;
	struct pmcstat_image *ppm_image;
};

/*
 * A 'pmcstat_process' structure tracks processes.
 */

struct pmcstat_process {
	LIST_ENTRY(pmcstat_process) pp_next;	/* hash-next */
	pid_t			pp_pid;		/* associated pid */
	int			pp_isactive;	/* whether active */
	uintfptr_t		pp_entryaddr;	/* entry address */
	TAILQ_HEAD(,pmcstat_pcmap) pp_map;	/* address range map */
};

static LIST_HEAD(,pmcstat_process) pmcstat_process_hash[PMCSTAT_NHASH];

static struct pmcstat_process *pmcstat_kernproc; /* kernel 'process' */

/*
 * Prototypes
 */

static void	pmcstat_gmon_create_file(struct pmcstat_gmonfile *_pgf,
    struct pmcstat_image *_image);
static const char *pmcstat_gmon_create_name(const char *_sd,
    struct pmcstat_image *_img, pmc_id_t _pmcid);
static void	pmcstat_gmon_map_file(struct pmcstat_gmonfile *_pgf);
static void	pmcstat_gmon_unmap_file(struct pmcstat_gmonfile *_pgf);

static struct pmcstat_image *pmcstat_image_from_path(const char *_path);
static enum pmcstat_image_type pmcstat_image_get_type(const char *_p);
static void pmcstat_image_get_elf_params(struct pmcstat_image *_image);
static void	pmcstat_image_increment_bucket(struct pmcstat_pcmap *_pcm,
    uintfptr_t _pc, pmc_id_t _pmcid, struct pmcstat_args *_a);
static void	pmcstat_image_link(struct pmcstat_process *_pp,
    struct pmcstat_image *_i, uintfptr_t _lpc, uintfptr_t _hpc);

static void	pmcstat_pmcid_add(pmc_id_t _pmcid, const char *_name,
    struct pmcstat_args *_a);
static const char *pmcstat_pmcid_to_name(pmc_id_t _pmcid);

static void	pmcstat_process_add_elf_image(struct pmcstat_process *_pp,
    const char *_path, uintfptr_t _entryaddr);
static void	pmcstat_process_exec(struct pmcstat_process *_pp,
    const char *_path, uintfptr_t _entryaddr);
static struct pmcstat_process *pmcstat_process_lookup(pid_t _pid, int _allocate);
static struct pmcstat_pcmap *pmcstat_process_find_map(
    struct pmcstat_process *_p, uintfptr_t _pc);

static int	pmcstat_string_compute_hash(const char *_string);
static const char *pmcstat_string_intern(const char *_s);
static struct pmcstat_string *pmcstat_string_lookup(const char *_s);


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
	char buffer[DEFAULT_BUFFER_SIZE];

	if ((fd = open(pgf->pgf_name, O_RDWR|O_NOFOLLOW|O_CREAT,
		 S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
		err(EX_OSERR, "ERROR: Cannot open \"%s\"", pgf->pgf_name);

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

	(void) close(fd);

	return;

 error:
	err(EX_OSERR, "ERROR: Cannot write \"%s\"", pgf->pgf_name);
}

const char *
pmcstat_gmon_create_name(const char *samplesdir, struct pmcstat_image *image,
    pmc_id_t pmcid)
{
	const char *pmcname;
	char fullpath[PATH_MAX];

	pmcname = pmcstat_pmcid_to_name(pmcid);

	(void) snprintf(fullpath, sizeof(fullpath),
	    "%s/%s/%s", samplesdir, pmcname, image->pi_samplename);

	return pmcstat_string_intern(fullpath);
}


static void
pmcstat_gmon_map_file(struct pmcstat_gmonfile *pgf)
{
	int fd;

	/* the gmon.out file must already exist */
	if ((fd = open(pgf->pgf_name, O_RDWR | O_NOFOLLOW, 0)) < 0)
		err(EX_OSERR, "ERROR: cannot open \"%s\"",
		    pgf->pgf_name);

	pgf->pgf_gmondata = mmap(NULL, pgf->pgf_ndatabytes,
	    PROT_READ|PROT_WRITE, MAP_NOSYNC|MAP_SHARED, fd, 0);

	if (pgf->pgf_gmondata == MAP_FAILED)
		/* XXX unmap a few files and try again? */
		err(EX_OSERR, "ERROR: cannot map \"%s\"", pgf->pgf_name);

	(void) close(fd);
}

/*
 * Unmap the data mapped from a gmon.out file.
 */

static void
pmcstat_gmon_unmap_file(struct pmcstat_gmonfile *pgf)
{
	(void) msync(pgf->pgf_gmondata, pgf->pgf_ndatabytes,
	    MS_SYNC);
	(void) munmap(pgf->pgf_gmondata, pgf->pgf_ndatabytes);
	pgf->pgf_gmondata = NULL;
}

static void
pmcstat_image_get_elf_params(struct pmcstat_image *image)
{
	int fd, i;
	struct stat st;
	void *mapbase;
	uintfptr_t minva, maxva;
	const Elf_Ehdr *h;
	const Elf_Phdr *ph;
	const Elf_Shdr *sh;
	const char *path;

	assert(image->pi_type == PMCSTAT_IMAGE_UNKNOWN);

	minva = ~(uintfptr_t) 0;
	maxva = (uintfptr_t) 0;
	path = image->pi_internedpath;

	if ((fd = open(path, O_RDONLY, 0)) < 0)
		err(EX_OSERR, "ERROR: Cannot open \"%s\"", path);

	if (fstat(fd, &st) < 0)
		err(EX_OSERR, "ERROR: Cannot stat \"%s\"", path);

	if ((mapbase = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) ==
	    MAP_FAILED)
		err(EX_OSERR, "ERROR: Cannot mmap \"%s\"", path);

	(void) close(fd);

	h = (const Elf_Ehdr *) mapbase;
	if (!IS_ELF(*h))
		err(EX_SOFTWARE, "ERROR: \"%s\" not an ELF file", path);

	sh = (const Elf_Shdr *)((uintptr_t) mapbase + h->e_shoff);

	if (h->e_type == ET_EXEC || h->e_type == ET_DYN) {
		/*
		 * Some kind of executable object: find the min,max va
		 * for its executable sections.
		 */
		for (i = 0; i < h->e_shnum; i++)
			if (sh[i].sh_flags & SHF_EXECINSTR) { /* code */
				minva = min(minva, sh[i].sh_addr);
				maxva = max(maxva, sh[i].sh_addr +
				    sh[i].sh_size);
			}
	} else
		err(EX_DATAERR, "ERROR: Unknown file type for \"%s\"",
		    image->pi_internedpath);

	image->pi_type = PMCSTAT_IMAGE_ELF;
	image->pi_start = minva;
	image->pi_entry = h->e_entry;
	image->pi_end = maxva;
	image->pi_isdynamic = 0;
	image->pi_dynlinkerpath = NULL;


	if (h->e_type == ET_EXEC) {
		ph = (const Elf_Phdr *)((uintptr_t) mapbase + h->e_phoff);
		for (i = 0; i < h->e_phnum; i++) {
			switch (ph[i].p_type) {
			case PT_DYNAMIC:
				image->pi_isdynamic = 1;
				break;
			case PT_INTERP:
				image->pi_dynlinkerpath =
				    pmcstat_string_intern((char *) mapbase +
							      ph[i].p_offset);
				break;
			}
		}
	}

	if (munmap(mapbase, st.st_size) < 0)
		err(EX_OSERR, "ERROR: Cannot unmap \"%s\"", path);

}

/*
 * Locate an image descriptor given an interned path, adding a fresh
 * descriptor to the cache if necessary.  This function also finds a
 * suitable name for this image's sample file.
 */

static struct pmcstat_image *
pmcstat_image_from_path(const char *internedpath)
{
	int count, hash, nlen;
	struct pmcstat_image *pi;
	char *sn;
	char name[NAME_MAX];

	hash = pmcstat_string_compute_hash(internedpath);

	/* Look for an existing entry. */
	LIST_FOREACH(pi, &pmcstat_image_hash[hash], pi_next)
	    if (pi->pi_internedpath == internedpath) {
		    /* move descriptor to the head of the lru list */
		    TAILQ_REMOVE(&pmcstat_image_lru, pi, pi_lru);
		    TAILQ_INSERT_HEAD(&pmcstat_image_lru, pi, pi_lru);
		    return pi;
	    }

	/*
	 * Allocate a new entry and place at the head of the hash and
	 * LRU lists.
	 */
	pi = malloc(sizeof(*pi));
	if (pi == NULL)
		return NULL;

	pi->pi_type = PMCSTAT_IMAGE_UNKNOWN;
	pi->pi_internedpath = internedpath;
	pi->pi_start = ~0;
	pi->pi_entry = ~0;
	pi->pi_end = 0;

	/*
	 * Look for a suitable name for the sample files associated
	 * with this image: if `basename(path)`+".gmon" is available,
	 * we use that, otherwise we try iterating through
	 * `basename(path)`+ "~" + NNN + ".gmon" till we get a free
	 * entry.
	 */
	if ((sn = basename(internedpath)) == NULL)
		err(EX_OSERR, "ERROR: Cannot process \"%s\"", internedpath);

	nlen = strlen(sn);
	nlen = min(nlen, (int) sizeof(name) - 6);	/* ".gmon\0" */

	snprintf(name, sizeof(name), "%.*s.gmon", nlen, sn);

	if (pmcstat_string_lookup(name) == NULL)
		pi->pi_samplename = pmcstat_string_intern(name);
	else {
		nlen = strlen(sn);
		nlen = min(nlen, (int) sizeof(name)-10); /* "~ddd.gmon\0" */
		count = 0;
		do {
			count++;
			snprintf(name, sizeof(name), "%.*s~%3.3d",
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

	return pi;
}

/*
 * Given an open file, determine its file type.
 */

static enum pmcstat_image_type
pmcstat_image_get_type(const char *path)
{
	int fd;
	Elf_Ehdr eh;
	struct exec ex;
	ssize_t nbytes;
	char buffer[DEFAULT_BUFFER_SIZE];

	if ((fd = open(path, O_RDONLY)) < 0)
		err(EX_OSERR, "ERROR: Cannot open \"%s\"", path);

	nbytes = max(sizeof(eh), sizeof(ex));
	if ((nbytes = pread(fd, buffer, nbytes, 0)) < 0)
		err(EX_OSERR, "ERROR: Cannot read \"%s\"", path);

	(void) close(fd);

	/* check if its an ELF file */
	if ((unsigned) nbytes >= sizeof(Elf_Ehdr)) {
		bcopy(buffer, &eh, sizeof(eh));
		if (IS_ELF(eh))
			return PMCSTAT_IMAGE_ELF;
	}

	/* Look for an A.OUT header */
	if ((unsigned) nbytes >= sizeof(struct exec)) {
		bcopy(buffer, &ex, sizeof(ex));
		if (!N_BADMAG(ex))
			return PMCSTAT_IMAGE_AOUT;
	}

	return PMCSTAT_IMAGE_UNKNOWN;
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

	/*
	 * Find the gmon file corresponding to 'pmcid', creating it if
	 * needed.
	 */

	image = map->ppm_image;

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

		pmcstat_gmon_create_file(pgf, image);

		LIST_INSERT_HEAD(&image->pi_gmlist, pgf, pgf_next);
	}

	/*
	 * Map the gmon file in if needed.  It may have been mapped
	 * out under memory pressure.
	 */
	if (pgf->pgf_gmondata == NULL)
		pmcstat_gmon_map_file(pgf);

	bucket = (pc - map->ppm_lowpc) / FUNCTION_ALIGNMENT;

	assert(bucket < pgf->pgf_nbuckets);

	hc = (HISTCOUNTER *) ((uintptr_t) pgf->pgf_gmondata +
	    sizeof(struct gmonhdr));

	/* saturating add */
	if (hc[bucket] < 0xFFFF)
		hc[bucket]++;

}

/*
 * Record the fact that PC values from 'lowpc' to 'highpc' come from
 * image 'image'.
 */

static void
pmcstat_image_link(struct pmcstat_process *pp, struct pmcstat_image *image,
    uintfptr_t lowpc, uintfptr_t highpc)
{
	struct pmcstat_pcmap *pcm, *pcmnew;

	if ((pcmnew = malloc(sizeof(*pcmnew))) == NULL)
		err(EX_OSERR, "ERROR: ");

	pcmnew->ppm_lowpc  = lowpc;
	pcmnew->ppm_highpc = highpc;
	pcmnew->ppm_image  = image;

	TAILQ_FOREACH(pcm, &pp->pp_map, ppm_next)
	    if (pcm->ppm_lowpc < lowpc)
		    break;

	if (pcm == NULL)
		TAILQ_INSERT_TAIL(&pp->pp_map, pcmnew, ppm_next);
	else
		TAILQ_INSERT_BEFORE(pcm, pcmnew, ppm_next);
}

/*
 * Add a {pmcid,name} mapping.
 */

static void
pmcstat_pmcid_add(pmc_id_t pmcid, const char *name, struct pmcstat_args *a)
{
	struct pmcstat_pmcrecord *pr;
	struct stat st;
	char fullpath[PATH_MAX];

	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next)
	    if (pr->pr_pmcid == pmcid) {
		    pr->pr_pmcname = name;
		    return;
	    }

	if ((pr = malloc(sizeof(*pr))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pmc record");

	pr->pr_pmcid = pmcid;
	pr->pr_pmcname = name;
	LIST_INSERT_HEAD(&pmcstat_pmcs, pr, pr_next);

	(void) snprintf(fullpath, sizeof(fullpath), "%s/%s", a->pa_samplesdir,
	    name);

	/* If the path name exists, it should be a directory */
	if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode))
		return;

	if (mkdir(fullpath, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) < 0)
		err(EX_OSERR, "ERROR: Cannot create directory \"%s\"",
		    fullpath);
}

/*
 * Given a pmcid in use, find its human-readable name, or a 
 */

static const char *
pmcstat_pmcid_to_name(pmc_id_t pmcid)
{
	struct pmcstat_pmcrecord *pr;
	char fullpath[PATH_MAX];

	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next)
	    if (pr->pr_pmcid == pmcid)
		    return pr->pr_pmcname;

	/* create a default name and add this entry */
	if ((pr = malloc(sizeof(*pr))) == NULL)
		err(EX_OSERR, "ERROR: ");
	pr->pr_pmcid = pmcid;

	(void) snprintf(fullpath, sizeof(fullpath), "%X", (unsigned int) pmcid);
	pr->pr_pmcname = pmcstat_string_intern(fullpath);

	LIST_INSERT_HEAD(&pmcstat_pmcs, pr, pr_next);

	return pr->pr_pmcname;
}

/*
 * Associate an ELF image with a process.  Argument 'path' names the
 * executable while 'fd' is an already open descriptor to it.
 */

static void
pmcstat_process_add_elf_image(struct pmcstat_process *pp, const char *path,
    uintfptr_t entryaddr)
{
	size_t linelen;
	FILE *rf;
	char *line;
	uintmax_t libstart;
	struct pmcstat_image *image, *rtldimage;
	char libpath[PATH_MAX];
	char command[PATH_MAX + sizeof(PMCSTAT_LDD_COMMAND) + 1];

	/* Look up path in the cache. */
	if ((image = pmcstat_image_from_path(path)) == NULL)
		return;

	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_get_elf_params(image);

	/* Create a map entry for the base executable. */
	pmcstat_image_link(pp, image, image->pi_start, image->pi_end);

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
		rtldimage = pmcstat_image_from_path(image->pi_dynlinkerpath);
		if (rtldimage == NULL)
			err(EX_OSERR, "ERROR: Cannot find image for "
			    "\"%s\"", image->pi_dynlinkerpath);
		if (rtldimage->pi_type == PMCSTAT_IMAGE_UNKNOWN)
			pmcstat_image_get_elf_params(rtldimage);

		libstart = entryaddr - rtldimage->pi_entry;
		pmcstat_image_link(pp, rtldimage, libstart,
		    libstart + rtldimage->pi_end - rtldimage->pi_start);

		/* Process all other objects loaded by this executable. */
		(void) snprintf(command, sizeof(command), "%s %s",
		    PMCSTAT_LDD_COMMAND, path);

		if ((rf = popen(command, "r")) == NULL)
			err(EX_OSERR, "ERROR: Cannot create pipe");

		(void) fgetln(rf, &linelen);

		while (!feof(rf) && !ferror(rf)) {

			if ((line = fgetln(rf, &linelen)) == NULL)
				continue;
			line[linelen-1] = '\0';

			if (sscanf(line, "%s %jx",
				libpath, &libstart) != 2)
				continue;

			image = pmcstat_image_from_path(
				pmcstat_string_intern(libpath));
			if (image == NULL)
				err(EX_OSERR, "ERROR: Cannot process "
				    "\"%s\"", libpath);

			if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
				pmcstat_image_get_elf_params(image);

			pmcstat_image_link(pp, image, libstart + image->pi_start,
			    libstart + image->pi_end);
		}

		(void) pclose(rf);

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
		    if (allocate && !pp->pp_isactive) {
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
		    return pp;
	    }

	if (!allocate)
		return NULL;

	if ((pp = malloc(sizeof(*pp))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pid descriptor");

	pp->pp_pid = pid;
	pp->pp_isactive = 1;

	TAILQ_INIT(&pp->pp_map);

	LIST_INSERT_HEAD(&pmcstat_process_hash[hash], pp, pp_next);
	return pp;
}

/*
 * Associate an image and a process.
 */

static void
pmcstat_process_exec(struct pmcstat_process *pp, const char *path,
    uintfptr_t entryaddr)
{
	enum pmcstat_image_type filetype;
	struct pmcstat_image *image;

	if ((image = pmcstat_image_from_path(path)) == NULL)
		return;

	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		filetype = pmcstat_image_get_type(path);
	else
		filetype = image->pi_type;

	switch (filetype) {
	case PMCSTAT_IMAGE_ELF:
		pmcstat_process_add_elf_image(pp, path, entryaddr);
		break;

	case PMCSTAT_IMAGE_AOUT:
		break;

	default:
		err(EX_SOFTWARE, "ERROR: Unsupported executable type for "
		    "\"%s\"", path);
	}
}


/*
 * Find the map entry associated with process 'p' at PC value 'pc'.
 */

static struct pmcstat_pcmap *
pmcstat_process_find_map(struct pmcstat_process *p, uintfptr_t pc)
{
	struct pmcstat_pcmap *ppm;

	TAILQ_FOREACH(ppm, &p->pp_map, ppm_next)
	    if (pc >= ppm->ppm_lowpc && pc < ppm->ppm_highpc)
		    return ppm;

	return NULL;
}


/*
 * Compute a 'hash' value for a string.
 */

static int
pmcstat_string_compute_hash(const char *s)
{
	int hash;

	for (hash = 0; *s; s++)
		hash ^= *s;

	return hash & PMCSTAT_HASH_MASK;
}

/*
 * Intern a copy of string 's', and return a pointer to it.
 */

static const char *
pmcstat_string_intern(const char *s)
{
	struct pmcstat_string *ps;
	int hash, len;

	hash = pmcstat_string_compute_hash(s);
	len  = strlen(s);

	if ((ps = pmcstat_string_lookup(s)) != NULL)
		return ps->ps_string;

	if ((ps = malloc(sizeof(*ps))) == NULL)
		err(EX_OSERR, "ERROR: Could not intern string");
	ps->ps_len = len;
	ps->ps_hash = hash;
	ps->ps_string = strdup(s);
	LIST_INSERT_HEAD(&pmcstat_string_hash[hash], ps, ps_next);
	return ps->ps_string;
}

static struct pmcstat_string *
pmcstat_string_lookup(const char *s)
{
	struct pmcstat_string *ps;
	int hash, len;

	hash = pmcstat_string_compute_hash(s);
	len = strlen(s);

	LIST_FOREACH(ps, &pmcstat_string_hash[hash], ps_next)
	    if (ps->ps_len == len && ps->ps_hash == hash &&
		strcmp(ps->ps_string, s) == 0)
		    return ps;
	return NULL;
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
	return a->pa_flags & FLAG_HAS_PIPE ? PMCSTAT_EXITING :
	    PMCSTAT_FINISHED;
}


int
pmcstat_convert_log(struct pmcstat_args *a)
{
	uintfptr_t pc;
	struct pmcstat_process *pp, *ppnew;
	struct pmcstat_pcmap *ppm, *ppmtmp;
	struct pmclog_ev ev;
	const char *image_path;

	while (pmclog_read(a->pa_logparser, &ev) == 0) {
		assert(ev.pl_state == PMCLOG_OK);

		switch (ev.pl_type) {
		case PMCLOG_TYPE_MAPPINGCHANGE:
			/*
			 * Introduce an address range mapping for a
			 * process.
			 */
			break;

		case PMCLOG_TYPE_PCSAMPLE:

			/*
			 * We bring in the gmon file for the image
			 * currently associated with the PMC & pid
			 * pair and increment the appropriate entry
			 * bin inside this.
			 */
			pc = ev.pl_u.pl_s.pl_pc;
			pp = pmcstat_process_lookup(ev.pl_u.pl_s.pl_pid, 1);
			if ((ppm = pmcstat_process_find_map(pp, pc)) == NULL &&
			    (ppm = pmcstat_process_find_map(pmcstat_kernproc,
				pc)) == NULL)
				break; /* unknown process,offset pair */

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
			pp = pmcstat_process_lookup(ev.pl_u.pl_x.pl_pid, 1);

			/* delete the current process map */
			TAILQ_FOREACH_SAFE(ppm, &pp->pp_map, ppm_next, ppmtmp) {
				TAILQ_REMOVE(&pp->pp_map, ppm, ppm_next);
				free(ppm);
			}

			/* locate the descriptor for the new 'base' image */
			image_path = pmcstat_string_intern(
				ev.pl_u.pl_x.pl_pathname);

			/* link to the new image */
			pmcstat_process_exec(pp, image_path,
			    ev.pl_u.pl_x.pl_entryaddr);
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
			pp->pp_isactive = 0;	/* make a zombie */
			break;

		case PMCLOG_TYPE_SYSEXIT:
			pp = pmcstat_process_lookup(ev.pl_u.pl_se.pl_pid, 0);
			if (pp == NULL)
				break;
			pp->pp_isactive = 0;	/* make a zombie */
			break;

		case PMCLOG_TYPE_PROCFORK:

			/*
			 * If we had been tracking 'oldpid', then clone
			 * its pid descriptor.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_f.pl_oldpid, 0);
			if (pp == NULL)
				break;

			ppnew =
			    pmcstat_process_lookup(ev.pl_u.pl_f.pl_newpid, 1);

			/* copy the old process' address maps */
			TAILQ_FOREACH(ppm, &pp->pp_map, ppm_next)
			    pmcstat_image_link(ppnew, ppm->ppm_image,
				ppm->ppm_lowpc, ppm->ppm_highpc);
			break;

		default:	/* other types of entries are not relevant */
			break;
		}
	}

	if (ev.pl_state == PMCLOG_EOF)
		return PMCSTAT_FINISHED;
	else if (ev.pl_state == PMCLOG_REQUIRE_DATA)
		return PMCSTAT_RUNNING;

	err(EX_DATAERR, "ERROR: event parsing failed (record %jd, "
	    "offset 0x%jx)", (uintmax_t) ev.pl_count + 1, ev.pl_offset);
}


/*
 * Open a log file, for reading or writing.
 *
 * The function returns the fd of a successfully opened log or -1 in
 * case of failure.
 */

int
pmcstat_open(const char *path, int mode)
{
	int fd;

	/*
	 * If 'path' is "-" then open one of stdin or stdout depending
	 * on the value of 'mode'.  Otherwise, treat 'path' as a file
	 * name and open that.
	 */
	if (path[0] == '-' && path[1] == '\0')
		fd = (mode == PMCSTAT_OPEN_FOR_READ) ? 0 : 1;
	else
		fd = open(path, mode == PMCSTAT_OPEN_FOR_READ ?
		    O_RDONLY : (O_WRONLY|O_CREAT|O_TRUNC),
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

	return fd;
}

/*
 * Print log entries as text.
 */

int
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
			break;
		case PMCLOG_TYPE_MAPPINGCHANGE:
			PMCSTAT_PRINT_ENTRY(a,"mapping","%s %d %p %p \"%s\"",
			    ev.pl_u.pl_m.pl_type == PMCLOG_MAPPING_INSERT ?
			    	"insert" : "delete",
			    ev.pl_u.pl_m.pl_pid,
			    (void *) ev.pl_u.pl_m.pl_start,
			    (void *) ev.pl_u.pl_m.pl_end,
			    ev.pl_u.pl_m.pl_pathname);
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
			fprintf(a->pa_printfile, "unknown %d",
			    ev.pl_type);
		}
	}

	if (ev.pl_state == PMCLOG_EOF)
		return PMCSTAT_FINISHED;
	else if (ev.pl_state ==  PMCLOG_REQUIRE_DATA)
		return PMCSTAT_RUNNING;

	err(EX_DATAERR, "ERROR: event parsing failed "
	    "(record %jd, offset 0x%jx)",
	    (uintmax_t) ev.pl_count + 1, ev.pl_offset);
	/*NOTREACHED*/
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
		return pmcstat_print_log(a);
	else
		/* convert the log to gprof compatible profiles */
		return pmcstat_convert_log(a);
}

void
pmcstat_initialize_logging(struct pmcstat_args *a)
{
	int i;
	const char *kernpath;
	struct pmcstat_image *img;

	/* use a convenient format for 'ldd' output */
	if (setenv("LD_TRACE_LOADED_OBJECTS_FMT1","%p %x\n",1) != 0)
		goto error;

	/* Initialize hash tables */
	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_INIT(&pmcstat_image_hash[i]);
		LIST_INIT(&pmcstat_process_hash[i]);
		LIST_INIT(&pmcstat_string_hash[i]);
	}

	/* create a fake 'process' entry for the kernel with pid == -1 */
	if ((pmcstat_kernproc = pmcstat_process_lookup((pid_t) -1, 1)) == NULL)
		goto error;

	if ((kernpath = pmcstat_string_intern(a->pa_kernel)) == NULL)
		goto error;

	img = pmcstat_image_from_path(kernpath);

	pmcstat_image_get_elf_params(img);
	pmcstat_image_link(pmcstat_kernproc, img, img->pi_start, img->pi_end);

	return;

 error:
	err(EX_OSERR, "ERROR: Cannot initialize logging");
}

void
pmcstat_shutdown_logging(void)
{
	int i;
	struct pmcstat_gmonfile *pgf, *pgftmp;
	struct pmcstat_image *pi, *pitmp;
	struct pmcstat_process *pp, *pptmp;
	struct pmcstat_string *ps, *pstmp;

	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_FOREACH_SAFE(pi, &pmcstat_image_hash[i], pi_next, pitmp) {
			/* flush gmon.out data to disk */
			LIST_FOREACH_SAFE(pgf, &pi->pi_gmlist, pgf_next,
			    pgftmp) {
			    pmcstat_gmon_unmap_file(pgf);
			    LIST_REMOVE(pgf, pgf_next);
			    free(pgf);
			}

			LIST_REMOVE(pi, pi_next);
			free(pi);
		}
		LIST_FOREACH_SAFE(pp, &pmcstat_process_hash[i], pp_next,
		    pptmp) {
			LIST_REMOVE(pp, pp_next);
			free(pp);
		}
		LIST_FOREACH_SAFE(ps, &pmcstat_string_hash[i], ps_next,
		    pstmp) {
			LIST_REMOVE(ps, ps_next);
			free(ps);
		}
	}
}
