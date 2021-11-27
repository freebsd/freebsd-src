/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
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

#include "namespace.h"
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <spawn.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"
#include "libc_private.h"

extern char **environ;

struct __posix_spawnattr {
	short			sa_flags;
	pid_t			sa_pgroup;
	struct sched_param	sa_schedparam;
	int			sa_schedpolicy;
	sigset_t		sa_sigdefault;
	sigset_t		sa_sigmask;
};

struct __posix_spawn_file_actions {
	STAILQ_HEAD(, __posix_spawn_file_actions_entry) fa_list;
};

typedef struct __posix_spawn_file_actions_entry {
	STAILQ_ENTRY(__posix_spawn_file_actions_entry) fae_list;
	enum {
		FAE_OPEN,
		FAE_DUP2,
		FAE_CLOSE,
	} fae_action;

	int fae_fildes;
	union {
		struct {
			char *path;
#define fae_path	fae_data.open.path
			int oflag;
#define fae_oflag	fae_data.open.oflag
			mode_t mode;
#define fae_mode	fae_data.open.mode
		} open;
		struct {
			int newfildes;
#define fae_newfildes	fae_data.dup2.newfildes
		} dup2;
	} fae_data;
} posix_spawn_file_actions_entry_t;

/*
 * Spawn routines
 */

static int
process_spawnattr(const posix_spawnattr_t sa)
{
	struct sigaction sigact = { .sa_flags = 0, .sa_handler = SIG_DFL };
	int i;

	/*
	 * POSIX doesn't really describe in which order everything
	 * should be set. We'll just set them in the order in which they
	 * are mentioned.
	 */

	/* Set process group */
	if (sa->sa_flags & POSIX_SPAWN_SETPGROUP) {
		if (setpgid(0, sa->sa_pgroup) != 0)
			return (errno);
	}

	/* Set scheduler policy */
	if (sa->sa_flags & POSIX_SPAWN_SETSCHEDULER) {
		if (sched_setscheduler(0, sa->sa_schedpolicy,
		    &sa->sa_schedparam) != 0)
			return (errno);
	} else if (sa->sa_flags & POSIX_SPAWN_SETSCHEDPARAM) {
		if (sched_setparam(0, &sa->sa_schedparam) != 0)
			return (errno);
	}

	/* Reset user ID's */
	if (sa->sa_flags & POSIX_SPAWN_RESETIDS) {
		if (setegid(getgid()) != 0)
			return (errno);
		if (seteuid(getuid()) != 0)
			return (errno);
	}

	/*
	 * Set signal masks/defaults.
	 * Use unwrapped syscall, libthr is in undefined state after vfork().
	 */
	if (sa->sa_flags & POSIX_SPAWN_SETSIGMASK) {
		__sys_sigprocmask(SIG_SETMASK, &sa->sa_sigmask, NULL);
	}

	if (sa->sa_flags & POSIX_SPAWN_SETSIGDEF) {
		for (i = 1; i <= _SIG_MAXSIG; i++) {
			if (sigismember(&sa->sa_sigdefault, i))
				if (__sys_sigaction(i, &sigact, NULL) != 0)
					return (errno);
		}
	}

	return (0);
}

static int
process_file_actions_entry(posix_spawn_file_actions_entry_t *fae)
{
	int fd, saved_errno;

	switch (fae->fae_action) {
	case FAE_OPEN:
		/* Perform an open(), make it use the right fd */
		fd = _open(fae->fae_path, fae->fae_oflag, fae->fae_mode);
		if (fd < 0)
			return (errno);
		if (fd != fae->fae_fildes) {
			if (_dup2(fd, fae->fae_fildes) == -1) {
				saved_errno = errno;
				(void)_close(fd);
				return (saved_errno);
			}
			if (_close(fd) != 0) {
				if (errno == EBADF)
					return (EBADF);
			}
		}
		if (_fcntl(fae->fae_fildes, F_SETFD, 0) == -1)
			return (errno);
		break;
	case FAE_DUP2:
		/* Perform a dup2() */
		if (_dup2(fae->fae_fildes, fae->fae_newfildes) == -1)
			return (errno);
		if (_fcntl(fae->fae_newfildes, F_SETFD, 0) == -1)
			return (errno);
		break;
	case FAE_CLOSE:
		/* Perform a close(), do not fail if already closed */
		(void)_close(fae->fae_fildes);
		break;
	}
	return (0);
}

static int
process_file_actions(const posix_spawn_file_actions_t fa)
{
	posix_spawn_file_actions_entry_t *fae;
	int error;

	/* Replay all file descriptor modifications */
	STAILQ_FOREACH(fae, &fa->fa_list, fae_list) {
		error = process_file_actions_entry(fae);
		if (error)
			return (error);
	}
	return (0);
}

struct posix_spawn_args {
	const char *path;
	const posix_spawn_file_actions_t *fa;
	const posix_spawnattr_t *sa;
	char * const * argv;
	char * const * envp;
	int use_env_path;
	volatile int error;
};

#define	PSPAWN_STACK_ALIGNMENT	16
#define	PSPAWN_STACK_ALIGNBYTES	(PSPAWN_STACK_ALIGNMENT - 1)
#define	PSPAWN_STACK_ALIGN(sz) \
	(((sz) + PSPAWN_STACK_ALIGNBYTES) & ~PSPAWN_STACK_ALIGNBYTES)

#if defined(__i386__) || defined(__amd64__)
/*
 * Below we'll assume that _RFORK_THREAD_STACK_SIZE is appropriately aligned for
 * the posix_spawn() case where we do not end up calling _execvpe and won't ever
 * try to allocate space on the stack for argv[].
 */
#define	_RFORK_THREAD_STACK_SIZE	4096
_Static_assert((_RFORK_THREAD_STACK_SIZE % PSPAWN_STACK_ALIGNMENT) == 0,
    "Inappropriate stack size alignment");
#endif

static int
_posix_spawn_thr(void *data)
{
	struct posix_spawn_args *psa;
	char * const *envp;

	psa = data;
	if (psa->sa != NULL) {
		psa->error = process_spawnattr(*psa->sa);
		if (psa->error)
			_exit(127);
	}
	if (psa->fa != NULL) {
		psa->error = process_file_actions(*psa->fa);
		if (psa->error)
			_exit(127);
	}
	envp = psa->envp != NULL ? psa->envp : environ;
	if (psa->use_env_path)
		_execvpe(psa->path, psa->argv, envp);
	else
		_execve(psa->path, psa->argv, envp);
	psa->error = errno;

	/* This is called in such a way that it must not exit. */
	_exit(127);
}

static int
do_posix_spawn(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *fa,
    const posix_spawnattr_t *sa,
    char * const argv[], char * const envp[], int use_env_path)
{
	struct posix_spawn_args psa;
	pid_t p;
#ifdef _RFORK_THREAD_STACK_SIZE
	char *stack;
	size_t cnt, stacksz;

	stacksz = _RFORK_THREAD_STACK_SIZE;
	if (use_env_path) {
		/*
		 * We need to make sure we have enough room on the stack for the
		 * potential alloca() in execvPe if it gets kicked back an
		 * ENOEXEC from execve(2), plus the original buffer we gave
		 * ourselves; this protects us in the event that the caller
		 * intentionally or inadvertently supplies enough arguments to
		 * make us blow past the stack we've allocated from it.
		 */
		for (cnt = 0; argv[cnt] != NULL; ++cnt)
			;
		stacksz += MAX(3, cnt + 2) * sizeof(char *);
		stacksz = PSPAWN_STACK_ALIGN(stacksz);
	}

	/*
	 * aligned_alloc is not safe to use here, because we can't guarantee
	 * that aligned_alloc and free will be provided by the same
	 * implementation.  We've actively hit at least one application that
	 * will provide its own malloc/free but not aligned_alloc leading to
	 * a free by the wrong allocator.
	 */
	stack = malloc(stacksz);
	if (stack == NULL)
		return (ENOMEM);
	stacksz = (((uintptr_t)stack + stacksz) & ~PSPAWN_STACK_ALIGNBYTES) -
	    (uintptr_t)stack;
#endif
	psa.path = path;
	psa.fa = fa;
	psa.sa = sa;
	psa.argv = argv;
	psa.envp = envp;
	psa.use_env_path = use_env_path;
	psa.error = 0;

	/*
	 * Passing RFSPAWN to rfork(2) gives us effectively a vfork that drops
	 * non-ignored signal handlers.  We'll fall back to the slightly less
	 * ideal vfork(2) if we get an EINVAL from rfork -- this should only
	 * happen with newer libc on older kernel that doesn't accept
	 * RFSPAWN.
	 */
#ifdef _RFORK_THREAD_STACK_SIZE
	/*
	 * x86 stores the return address on the stack, so rfork(2) cannot work
	 * as-is because the child would clobber the return address om the
	 * parent.  Because of this, we must use rfork_thread instead while
	 * almost every other arch stores the return address in a register.
	 */
	p = rfork_thread(RFSPAWN, stack + stacksz, _posix_spawn_thr, &psa);
	free(stack);
#else
	p = rfork(RFSPAWN);
	if (p == 0)
		/* _posix_spawn_thr does not return */
		_posix_spawn_thr(&psa);
#endif
	/*
	 * The above block should leave us in a state where we've either
	 * succeeded and we're ready to process the results, or we need to
	 * fallback to vfork() if the kernel didn't like RFSPAWN.
	 */

	if (p == -1 && errno == EINVAL) {
		p = vfork();
		if (p == 0)
			/* _posix_spawn_thr does not return */
			_posix_spawn_thr(&psa);
	}
	if (p == -1)
		return (errno);
	if (psa.error != 0)
		/* Failed; ready to reap */
		_waitpid(p, NULL, WNOHANG);
	else if (pid != NULL)
		/* exec succeeded */
		*pid = p;
	return (psa.error);
}

int
posix_spawn(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *fa,
    const posix_spawnattr_t *sa,
    char * const argv[], char * const envp[])
{
	return do_posix_spawn(pid, path, fa, sa, argv, envp, 0);
}

int
posix_spawnp(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *fa,
    const posix_spawnattr_t *sa,
    char * const argv[], char * const envp[])
{
	return do_posix_spawn(pid, path, fa, sa, argv, envp, 1);
}

/*
 * File descriptor actions
 */

int
posix_spawn_file_actions_init(posix_spawn_file_actions_t *ret)
{
	posix_spawn_file_actions_t fa;

	fa = malloc(sizeof(struct __posix_spawn_file_actions));
	if (fa == NULL)
		return (-1);

	STAILQ_INIT(&fa->fa_list);
	*ret = fa;
	return (0);
}

int
posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
	posix_spawn_file_actions_entry_t *fae;

	while ((fae = STAILQ_FIRST(&(*fa)->fa_list)) != NULL) {
		/* Remove file action entry from the queue */
		STAILQ_REMOVE_HEAD(&(*fa)->fa_list, fae_list);

		/* Deallocate file action entry */
		if (fae->fae_action == FAE_OPEN)
			free(fae->fae_path);
		free(fae);
	}

	free(*fa);
	return (0);
}

int
posix_spawn_file_actions_addopen(posix_spawn_file_actions_t * __restrict fa,
    int fildes, const char * __restrict path, int oflag, mode_t mode)
{
	posix_spawn_file_actions_entry_t *fae;
	int error;

	if (fildes < 0)
		return (EBADF);

	/* Allocate object */
	fae = malloc(sizeof(posix_spawn_file_actions_entry_t));
	if (fae == NULL)
		return (errno);

	/* Set values and store in queue */
	fae->fae_action = FAE_OPEN;
	fae->fae_path = strdup(path);
	if (fae->fae_path == NULL) {
		error = errno;
		free(fae);
		return (error);
	}
	fae->fae_fildes = fildes;
	fae->fae_oflag = oflag;
	fae->fae_mode = mode;

	STAILQ_INSERT_TAIL(&(*fa)->fa_list, fae, fae_list);
	return (0);
}

int
posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa,
    int fildes, int newfildes)
{
	posix_spawn_file_actions_entry_t *fae;

	if (fildes < 0 || newfildes < 0)
		return (EBADF);

	/* Allocate object */
	fae = malloc(sizeof(posix_spawn_file_actions_entry_t));
	if (fae == NULL)
		return (errno);

	/* Set values and store in queue */
	fae->fae_action = FAE_DUP2;
	fae->fae_fildes = fildes;
	fae->fae_newfildes = newfildes;

	STAILQ_INSERT_TAIL(&(*fa)->fa_list, fae, fae_list);
	return (0);
}

int
posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa,
    int fildes)
{
	posix_spawn_file_actions_entry_t *fae;

	if (fildes < 0)
		return (EBADF);

	/* Allocate object */
	fae = malloc(sizeof(posix_spawn_file_actions_entry_t));
	if (fae == NULL)
		return (errno);

	/* Set values and store in queue */
	fae->fae_action = FAE_CLOSE;
	fae->fae_fildes = fildes;

	STAILQ_INSERT_TAIL(&(*fa)->fa_list, fae, fae_list);
	return (0);
}

/*
 * Spawn attributes
 */

int
posix_spawnattr_init(posix_spawnattr_t *ret)
{
	posix_spawnattr_t sa;

	sa = calloc(1, sizeof(struct __posix_spawnattr));
	if (sa == NULL)
		return (errno);

	/* Set defaults as specified by POSIX, cleared above */
	*ret = sa;
	return (0);
}

int
posix_spawnattr_destroy(posix_spawnattr_t *sa)
{
	free(*sa);
	return (0);
}

int
posix_spawnattr_getflags(const posix_spawnattr_t * __restrict sa,
    short * __restrict flags)
{
	*flags = (*sa)->sa_flags;
	return (0);
}

int
posix_spawnattr_getpgroup(const posix_spawnattr_t * __restrict sa,
    pid_t * __restrict pgroup)
{
	*pgroup = (*sa)->sa_pgroup;
	return (0);
}

int
posix_spawnattr_getschedparam(const posix_spawnattr_t * __restrict sa,
    struct sched_param * __restrict schedparam)
{
	*schedparam = (*sa)->sa_schedparam;
	return (0);
}

int
posix_spawnattr_getschedpolicy(const posix_spawnattr_t * __restrict sa,
    int * __restrict schedpolicy)
{
	*schedpolicy = (*sa)->sa_schedpolicy;
	return (0);
}

int
posix_spawnattr_getsigdefault(const posix_spawnattr_t * __restrict sa,
    sigset_t * __restrict sigdefault)
{
	*sigdefault = (*sa)->sa_sigdefault;
	return (0);
}

int
posix_spawnattr_getsigmask(const posix_spawnattr_t * __restrict sa,
    sigset_t * __restrict sigmask)
{
	*sigmask = (*sa)->sa_sigmask;
	return (0);
}

int
posix_spawnattr_setflags(posix_spawnattr_t *sa, short flags)
{
	(*sa)->sa_flags = flags;
	return (0);
}

int
posix_spawnattr_setpgroup(posix_spawnattr_t *sa, pid_t pgroup)
{
	(*sa)->sa_pgroup = pgroup;
	return (0);
}

int
posix_spawnattr_setschedparam(posix_spawnattr_t * __restrict sa,
    const struct sched_param * __restrict schedparam)
{
	(*sa)->sa_schedparam = *schedparam;
	return (0);
}

int
posix_spawnattr_setschedpolicy(posix_spawnattr_t *sa, int schedpolicy)
{
	(*sa)->sa_schedpolicy = schedpolicy;
	return (0);
}

int
posix_spawnattr_setsigdefault(posix_spawnattr_t * __restrict sa,
    const sigset_t * __restrict sigdefault)
{
	(*sa)->sa_sigdefault = *sigdefault;
	return (0);
}

int
posix_spawnattr_setsigmask(posix_spawnattr_t * __restrict sa,
    const sigset_t * __restrict sigmask)
{
	(*sa)->sa_sigmask = *sigmask;
	return (0);
}
