/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/types.h>

#include <sys/endian.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/cheri.h>
#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <cheri/sandbox.h>

#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "imagebox.h"
#include "iboxpriv.h"

int ibox_verbose;

struct pthr_decode_private
{
	pthread_t	pthr;
};

struct fork_decode_private
{
	pid_t	pid;
};

static void *
pthr_decode_png(void *arg)
{
	struct ibox_decode_state *ids = arg;

	decode_png(ids, NULL, NULL);

	ids->is->times[3] = sysarch(MIPS_GET_COUNT, NULL);

	free(ids);

	pthread_exit(NULL);
}

static struct iboxstate*
pthr_png_read_start(int pfd, uint32_t width, uint32_t height, enum sbtype sb)
{
	struct iboxstate		*is = NULL;
	struct ibox_decode_state	*ids = NULL;
	struct pthr_decode_private	*pdp;

	if ((is = malloc(sizeof(struct iboxstate))) == NULL)
		goto error;
	memset(is, 0, sizeof(struct iboxstate));
	is->sb = sb;
	is->width = width;
	is->height = height;
	is->passes_remaining = UINT32_MAX;
	is->times[0] = sysarch(MIPS_GET_COUNT, NULL);

	if ((pdp = malloc(sizeof(*pdp))) == NULL)
		goto error;
	is->private = pdp;

	if ((ids = malloc(sizeof(*ids))) == NULL)
		goto error;
	memset(ids, 0, sizeof(*ids));
	ids->is = is;
	ids->fd = pfd;

	if ((ids->buffer = malloc(is->width * is->height *
	    sizeof(*ids->buffer))) == NULL)
		goto error;
	is->buffer = ids->buffer;
	
	if (pthread_create(&(pdp->pthr), NULL, pthr_decode_png, ids) != 0)
		goto error;
	goto started;

error:
	close(pfd);
	free(is);
	is = NULL;
	if (ids != NULL) {
		free(ids->buffer);
		free(ids);
	}
started:
	return is;
}

static struct iboxstate*
capsicum_png_read_start(int pfd, uint32_t width, uint32_t height,
    enum sbtype sb)
{
	int bfd, isfd, highfd;
	int nbfd, nisfd, npfd;
	struct iboxstate		*is = NULL;
	struct fork_decode_private	*fdp = NULL;
	
	bfd = isfd = -1;

	if ((isfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR))
	     == -1)
		goto error;
	if (ftruncate(isfd, sizeof(struct iboxstate)) == -1)
		goto error;
	if ((is = mmap(NULL, sizeof(*is), PROT_READ | PROT_WRITE, MAP_SHARED,
	    isfd, 0)) == MAP_FAILED)
		goto error;
	memset(is, 0, sizeof(struct iboxstate));
	is->sb = sb;
	is->width = width;
	is->height = height;
	is->passes_remaining = UINT32_MAX;
	is->times[0] = sysarch(MIPS_GET_COUNT, NULL);
		
	if ((bfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR))
	     == -1)
		goto error;
	if (ftruncate(bfd, width * height * sizeof(uint32_t)) == -1)
		goto error;
	if ((is->buffer = mmap(NULL, width * height * sizeof(uint32_t),
	    PROT_READ | PROT_WRITE, MAP_SHARED, bfd, 0)) == MAP_FAILED)
		goto error;

	if ((fdp = malloc(sizeof(struct fork_decode_private))) == NULL)
		goto error;
	is->private = fdp;

	if ((fdp->pid = fork()) == 0) {
		/*
		 * Relocate pfd, bfd, and isfd to fd's 3, 4, 5 for the
		 * worker process.  First, move them to new, higher locations
		 * to ensure none are in the range 3-5 (assumes stdin, out,
		 * err) are open..  Second, install them in the expected
		 * locations.  Third, close all higher FDs.
		 */
		highfd = pfd;
		if (bfd > highfd)
			highfd = bfd;
		if (isfd > highfd)
			highfd = isfd;
		npfd = highfd + 1;
		nbfd = highfd + 2;
		nisfd = highfd + 3;
		if (dup2(pfd, npfd) == -1)
			exit(1);
		if (dup2(bfd, nbfd) == -1)
			exit(1);
		if (dup2(isfd, nisfd) == -1)
			exit(1);
		close(pfd);
		close(bfd);
		close(isfd);
		if (dup2(npfd, 3) == -1)
			exit(1);
		if (dup2(nbfd, 4) == -1)
			exit(1);
		if (dup2(nisfd, 5) == -1)
			exit(1);
		closefrom(6);

		if (execl("/usr/libexec/readpng-capsicum-helper",
		    "readpng-capsicum-helper", NULL) == -1)
			exit(1);
	} else if (fdp->pid > 0)
		goto started;

error:
	if (is != NULL) {
		if (is->buffer != NULL)
			munmap(__DEVOLATILE(void*, is->buffer),
			    width * height * sizeof(uint32_t));
		munmap(is, sizeof(*is));
		is = NULL;
	}
	free(fdp);
started:
	close(pfd);
	if (bfd >= 0)
		close(bfd);
	if (isfd >= 0)
		close(isfd);

	return (is);
}

/*
 * XXX: rwatson reports that capabilities end up misaligned on the stack.
 */
static struct chericap c3, c4, c5;

static struct iboxstate*
cheri_png_read_start(char *pngbuffer, size_t pnglen,
    uint32_t width, uint32_t height, enum sbtype sb)
{
	static struct sandbox		*sandbox = NULL;
	struct iboxstate		*is = NULL;
        register_t			 v;

	if ((is = malloc(sizeof(struct iboxstate))) == NULL)
		goto error;
	memset(is, 0, sizeof(struct iboxstate));
	is->sb = sb;
	is->width = width;
	is->height = height;
	is->passes_remaining = UINT32_MAX;
	is->times[0] = sysarch(MIPS_GET_COUNT, NULL);

        if ((is->buffer = malloc(is->width * is->height *
            sizeof(*is->buffer))) == NULL)
                goto error;

	if (ibox_verbose)
		sb_verbose = ibox_verbose;

	if (sandbox == NULL)
		if (sandbox_setup("/usr/libexec/readpng-cheri-helper.bin",
		    4*1024*1024, &sandbox) < 0)
			goto error;

        CHERI_CINCBASE(10, 0, is->buffer);
        CHERI_CSETLEN(10, 10, is->width * is->height * sizeof(uint32_t));
        CHERI_CANDPERM(10, 10, CHERI_PERM_STORE);
        CHERI_CSC(10, 0, &c3, 0);

        CHERI_CINCBASE(10, 0, pngbuffer);
        CHERI_CSETLEN(10, 10, pnglen);
        CHERI_CANDPERM(10, 10, CHERI_PERM_LOAD);
        CHERI_CSC(10, 0, &c4, 0);

        CHERI_CINCBASE(10, 0, is->times + 1);
        CHERI_CSETLEN(10, 10, sizeof(uint32_t) * 2);
        CHERI_CANDPERM(10, 10, CHERI_PERM_STORE);
        CHERI_CSC(10, 0, &c5, 0);

        v = sandbox_invoke(sandbox, width, height, pnglen, 0,
            &c3, &c4, &c5, NULL, NULL, NULL, NULL, NULL);
	if (ibox_verbose)
		printf("%s: sandbox returned %ju\n", __func__, (uintmax_t)v);
	is->valid_rows = height;
	is->passes_remaining = 0;
	is->times[3] = sysarch(MIPS_GET_COUNT, NULL);
	return (is);
error:
	munmap(pngbuffer, pnglen);
	if (is != NULL) {
		free(__DEVOLATILE(void *, is->buffer));
		free(is);
	}
	return (NULL);
}

/*
 * Begin decoding a stream containing a PNG image.  Reads will proceed
 * in the background.  The file descriptor will be under the control of
 * the png_read code and will be closed when decoding is complete.
 */
struct iboxstate*
png_read_start(int pfd, uint32_t maxw, uint32_t maxh, enum sbtype sb)
{
	size_t pnglen;
	uint32_t header[9], width, height;
	struct stat statbuf;
	char *cheader = (char *)header;
	char *pngbuffer;
	char ihdr[] = {0x00, 0x00, 0x00, 0x0d, 'I', 'H', 'D', 'R'};

	if (read(pfd, header, sizeof(header)) != sizeof(header)) {
		close(pfd);
		return (NULL);
	}
	if (lseek(pfd, 0, SEEK_SET) != 0) {
		close(pfd);
		return (NULL);
	}

	if (png_sig_cmp(cheader, 0, 8) != 0) {
		errno = EINVAL;
		close(pfd);
		return (NULL);
	}
	if (memcmp(header + 2, ihdr, sizeof(ihdr)) != 0) {
		errno = EINVAL;
		close(pfd);
		return (NULL);
	}
	width = be32toh(*(header + 4));
	height = be32toh(*(header + 5));
	if (width > maxw || height > maxh) {
		close(pfd);
		return NULL;
	}

	switch (sb) {
	case SB_NONE:
		return pthr_png_read_start(pfd, width, height, sb);
	case SB_CAPSICUM:
		return capsicum_png_read_start(pfd, width, height, sb);
	case SB_CHERI:
		if (fstat(pfd, &statbuf) == -1) {
			close(pfd);
			return (NULL);
		}
		pnglen = statbuf.st_size;
		if ((pngbuffer = mmap(NULL, pnglen, PROT_READ,
		    0, pfd, 0)) == NULL) {
			close(pfd);
			return (NULL);
		}
		close(pfd);
		return cheri_png_read_start(pngbuffer, pnglen, width, height,
		   sb);
	default:
		close(pfd);
		return NULL;
	}
}

/*
 * Return when the png has finished decoding.
 */
int
png_read_finish(struct iboxstate *is)
{
	int error, status;
	struct pthr_decode_private *pdp;
	struct fork_decode_private	*fdp = NULL;

	switch (is->sb) {
	case SB_NONE:
		pdp = is->private;
		error = pthread_join(pdp->pthr, NULL);
		free(pdp);
		is->private = NULL;
		break;
	case SB_CAPSICUM:
		fdp = is->private;
		waitpid(fdp->pid, &status, 0);
		free(fdp);
		is->private = NULL;
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			error = 1;
		else
			error = 0;
		break;
	case SB_CHERI:
		/* sandbox runs synchronously so nothing to do */
		error = 0;
		break;
	default:
		error = 1;
	}

	return (error);
}

void
iboxstate_free(struct iboxstate *is)
{

	if (is->private != NULL)
		png_read_finish(is);
	switch (is->sb){
	case SB_NONE:
		free(__DEVOLATILE(void *, is->buffer));
		free(is);
		break;
	case SB_CAPSICUM:
		munmap(__DEVOLATILE(void *, is->buffer),
		    is->width * is->height * sizeof(uint32_t));
		munmap(is, sizeof(*is));
		break;
	case SB_CHERI:
		free(__DEVOLATILE(void *, is->buffer));
		free(is);
		break;
	default:
		break;
	}
}

static uint32_t
counter_diff(uint32_t first, uint32_t second)
{

	if (first < second)
		return (second - first);
	else
		return (second + (UINT32_MAX - first));
}

uint32_t
iboxstate_get_ttime(struct iboxstate *is) {

	return (counter_diff(is->times[0], is->times[3]));
}

uint32_t
iboxstate_get_dtime(struct iboxstate *is) {

	return (counter_diff(is->times[1], is->times[2]));
}
