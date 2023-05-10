#/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Konstantin Belousov <kib@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Test scenario for Intel userspace protection keys feature on Skylake Xeons

grep -qw PKU /var/run/dmesg.boot || exit 0
cd /tmp
cat > /tmp/pkru_exec.c <<EOF
/* $Id: pkru_exec.c,v 1.4 2019/01/12 04:55:57 kostik Exp kostik $ */
/*
 * cc -Wall -Wextra -g -O -o pkru_fork64 pkru_fork.c
 * Run it with LD_BIND_NOW=1.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <machine/sysarch.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef TEST_COMPILE
int x86_pkru_get_perm(unsigned int keyidx, int *access, int *modify);
int x86_pkru_set_perm(unsigned int keyidx, int access, int modify);
int x86_pkru_protect_range(void *addr, unsigned long len, unsigned int keyidx,
    int flag);
int x86_pkru_unprotect_range(void *addr, unsigned long len);
uint32_t rdpkru(void);
void wrpkru(uint32_t);
#define	AMD64_PKRU_PERSIST	0x0001
#endif

extern char **environ;

#define	OPKEY	1

int
main(void)
{
	char *args[3] = {
	    "/bin/date",
	    NULL
	};
	struct rlimit rl;

	if (getrlimit(RLIMIT_STACK, &rl) != 0)
		err(1, "getrlimit RLIMIT_STACK");
	if (x86_pkru_protect_range(0, 0x800000000000 - rl.rlim_max, OPKEY,
	    AMD64_PKRU_PERSIST) != 0)
		err(1, "x86_pkru_protect_range");
	if (x86_pkru_set_perm(1, 1, 0) != 0)
		err(1, "x86_pkru_set_perm");
	execve("/bin/date", args, environ);
}
EOF
cc -Wall -Wextra -g -O -o pkru_exec64 pkru_exec.c || exit 1
cc -Wall -Wextra -g -O -o pkru_exec32 pkru_exec.c -m32 || exit 1
rm pkru_exec.c
echo "Expect: Segmentation fault (core dumped)"
LD_BIND_NOW=1 ./pkru_exec64
LD_BIND_NOW=1 ./pkru_exec32
rm -f pkru_exec64 pkru_exec32 pkru_exec64.core pkru_exec32.core

cat > /tmp/pkru_fork.c <<EOF
/* $Id: pkru_fork.c,v 1.2 2019/01/12 03:39:42 kostik Exp kostik $ */
/* cc -Wall -Wextra -g -O -o pkru_fork64 pkru_fork.c */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <machine/cpufunc.h>
#include <machine/sysarch.h>
#include <x86/fpu.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef TEST_COMPILE
int x86_pkru_get_perm(unsigned int keyidx, int *access, int *modify);
int x86_pkru_set_perm(unsigned int keyidx, int access, int modify);
int x86_pkru_protect_range(void *addr, unsigned long len, unsigned int keyidx,
    int flag);
int x86_pkru_unprotect_range(void *addr, unsigned long len);
uint32_t rdpkru(void);
void wrpkru(uint32_t);
#endif

static volatile char *mapping;

#define	OPKEY	1

int
main(void)
{
	int error;
	pid_t child;

	mapping = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);
	if (mapping == MAP_FAILED)
		err(1, "mmap");
	error = x86_pkru_protect_range((void *)mapping, getpagesize(),
	    OPKEY, 0);
	if (error != 0)
		err(1, "x86_pkru_protect_range");
	error = x86_pkru_set_perm(OPKEY, 0, 0);
	if (error != 0)
		err(1, "x86_pkru_set_perm");
	child = fork();
	if (child == -1)
		err(1, "fork");
	if (child == 0) {
		*mapping = 0;
		printf("Still alive, pkru did not worked after fork");
	}
	waitpid(child, NULL, 0);
}
EOF
cc -Wall -Wextra -g -O -o pkru_fork64 pkru_fork.c || exit 1
cc -Wall -Wextra -g -O -o pkru_fork32 -m32 pkru_fork.c || exit 1
rm pkru_fork.c
./pkru_fork64
./pkru_fork32
rm -f pkru_fork64 pkru_fork64.core pkru_fork32 pkru_fork32.core

cat > /tmp/pkru_perm.c <<EOF
/* $Id: pkru_perm.c,v 1.6 2019/01/12 04:43:20 kostik Exp kostik $ */
/* cc -Wall -Wextra -g -O -o pkru_fork64 pkru_fork.c */

#include <sys/types.h>
#include <sys/mman.h>
#include <machine/cpufunc.h>
#include <machine/sysarch.h>
#include <x86/fpu.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef TEST_COMPILE
int x86_pkru_get_perm(unsigned int keyidx, int *access, int *modify);
int x86_pkru_set_perm(unsigned int keyidx, int access, int modify);
int x86_pkru_protect_range(void *addr, unsigned long len, unsigned int keyidx,
    int flag);
int x86_pkru_unprotect_range(void *addr, unsigned long len);
uint32_t rdpkru(void);
void wrpkru(uint32_t);
#define	AMD64_PKRU_PERSIST	0x0001
#endif

static void
sighandler(int signo __unused, siginfo_t *si __unused, void *uc1 __unused)
{

	exit(0);
}

static volatile char *mapping;

#define	OPKEY	1

int
main(void)
{
	struct sigaction sa;
	char *mapping1;
	int error;

	error = x86_pkru_set_perm(OPKEY, 0, 0);
	if (error != 0)
		err(1, "x86_pkru_set_perm");
	mapping = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);
	if (mapping == MAP_FAILED)
		err(1, "mmap");
	error = x86_pkru_protect_range((void *)mapping, getpagesize(),
	    OPKEY, 0);
	if (error != 0)
		err(1, "x86_pkru_protect_range");
	error = munmap((void *)mapping, getpagesize());
	if (error != 0)
		err(1, "munmap");
	mapping1 = mmap((void *)mapping, getpagesize(), PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_EXCL, -1, 0);
	if (mapping1 == MAP_FAILED)
		err(1, "mmap 2");
	*mapping = 0;
	error = x86_pkru_protect_range((void *)mapping, getpagesize(),
	    OPKEY, AMD64_PKRU_PERSIST);
	mapping1 = mmap((void *)mapping, getpagesize(), PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	if (mapping1 == MAP_FAILED)
		err(1, "mmap 3");
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sighandler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		err(1, "sigaction");
	*mapping = 0;
	printf("Still alive, pkru persist did not worked");
	exit(1);
}
EOF
cc -Wall -Wextra -g -O -o pkru_perm64 pkru_perm.c || exit 1
cc -Wall -Wextra -g -O -o pkru_perm32 -m32 pkru_perm.c || exit 1
rm pkru_perm.c
./pkru_perm64
./pkru_perm32
rm -f pkru_perm64 pkru_perm32

cat > /tmp/pkru.c <<EOF
/* $Id: pkru.c,v 1.27 2019/01/10 12:06:31 kostik Exp $ */
/* cc -Wall -Wextra -g -O -o pkru64 pkru.c -lpthread */

#include <sys/types.h>
#include <sys/mman.h>
#include <machine/cpufunc.h>
#include <machine/sysarch.h>
#include <x86/fpu.h>
#include <err.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef TEST_COMPILE
int x86_pkru_get_perm(unsigned int keyidx, int *access, int *modify);
int x86_pkru_set_perm(unsigned int keyidx, int access, int modify);
int x86_pkru_protect_range(void *addr, unsigned long len, unsigned int keyidx,
    int flag);
int x86_pkru_unprotect_range(void *addr, unsigned long len);
uint32_t rdpkru(void);
void wrpkru(uint32_t);
#endif

static char *mut_region;
static size_t mut_region_len;
static unsigned *mut_region_keys;
static pthread_t bga_thr;
static int signal_seen;
static siginfo_t si_seen;
static ucontext_t *uc_seen;
static u_int rpku_offset;

static void
handler(int i __unused) {
	_exit(0);
}

static void
report_sig(int signo, siginfo_t *si, ucontext_t *uc)
{

	printf("signal %d %s", signo, strsignal(signo));
	printf(" si_code %d si_status %d si_addr %p", si->si_code,
	    si->si_status, si->si_addr);
	printf(" mc_err %#jx", (uintmax_t)uc->uc_mcontext.mc_err);
	if (uc->uc_mcontext.mc_xfpustate != 0 &&
	    (unsigned long)uc->uc_mcontext.mc_xfpustate_len >=
	    rpku_offset) {
		printf(" pkru 0x%08x", *(uint32_t *)(
		    uc->uc_mcontext.mc_xfpustate + rpku_offset));
	}
	printf("\n");
}

static void
sighandler(int signo, siginfo_t *si, void *u)
{
	ucontext_t *uc;
	pthread_t thr;
	size_t len;
	uint32_t *pkrup;

	uc = u;
	thr = pthread_self();
	if (thr == bga_thr) {
		printf("Fault from background access thread\n");
		report_sig(signo, si, uc);
		exit(1);
	}
	signal_seen = signo;
	si_seen = *si;

	len = sizeof(ucontext_t);
	if (uc->uc_mcontext.mc_xfpustate != 0)
		len += uc->uc_mcontext.mc_xfpustate_len;
	uc_seen = malloc(len);
	if (uc_seen == NULL)
		err(1, "malloc(%d)", (int)len);
	memcpy(uc_seen, uc, sizeof(*uc));
#if 0
printf("signal %d xpfustate %p len %ld rpkuo %u\n", signo, (void *)uc->uc_mcontext.mc_xfpustate, uc->uc_mcontext.mc_xfpustate_len, rpku_offset);
#endif
	if (uc->uc_mcontext.mc_xfpustate != 0) {
		uc_seen->uc_mcontext.mc_xfpustate = (uintptr_t)uc_seen +
		    sizeof(*uc);
		memcpy((void *)uc_seen->uc_mcontext.mc_xfpustate,
		    (void *)uc->uc_mcontext.mc_xfpustate,
		    uc->uc_mcontext.mc_xfpustate_len);

		if ((unsigned long)uc->uc_mcontext.mc_xfpustate_len >=
		    rpku_offset + sizeof(uint32_t)) {
			pkrup = (uint32_t *)(rpku_offset +
			    (char *)uc->uc_mcontext.mc_xfpustate);
#if 0
printf("signal %d *pkru %08x\n", signo, *pkrup);
#endif
			*pkrup = 0;
		}
	}
}

static void *
bg_access_thread_fn(void *arg __unused)
{
	char *c, x;

	pthread_set_name_np(pthread_self(), "bgaccess");
	for (x = 0, c = mut_region;;) {
		*c = x;
		if (++c >= mut_region + mut_region_len) {
			c = mut_region;
			x++;
		}
	}
	return (NULL);
}

static void
clear_signal_report(void)
{

	signal_seen = 0;
	free(uc_seen);
	uc_seen = NULL;
}

static void
check_signal(unsigned key, int check_access, int check_modify)
{

	if (signal_seen == 0) {
		printf("Did not get signal, key %d check_access %d "
		    "check_modify %d\n", key, check_access, check_modify);
		printf("pkru 0x%08x\n", rdpkru());
		exit(1);
	}
}

static void
check_no_signal(void)
{

	if (signal_seen != 0) {
		printf("pkru 0x%08x\n", rdpkru());
		printf("Got signal\n");
		report_sig(signal_seen, &si_seen, uc_seen);
		exit(1);
	}
}

static void
check(char *p, unsigned key, int check_access, int check_modify)
{
	int access, error, modify, orig_access, orig_modify;

	error = x86_pkru_get_perm(key, &orig_access, &orig_modify);
	if (error != 0)
		err(1, "x86_pkru_get_perm");
	access = check_access ? 0 : 1;
	modify = check_modify ? 0 : 1;
	error = x86_pkru_set_perm(key, access, modify);
	if (error != 0)
		err(1, "x86_pkru_set_perm access");
	clear_signal_report();
	if (check_modify)
		*(volatile char *)p = 1;
	else if (check_access)
		*(volatile char *)p;
	if (key == mut_region_keys[(p - mut_region) / getpagesize()])
		check_signal(key, check_access, check_modify);
	else
		check_no_signal();
	error = x86_pkru_set_perm(key, orig_access, orig_modify);
	if (error != 0)
		err(1, "x86_pkru_set_perm access restore");
	clear_signal_report();
	if (check_modify)
		*(volatile char *)p = 1;
	else if (check_access)
		*(volatile char *)p;
	check_no_signal();
}

static void
mutate_perms(void)
{
	unsigned key;
	char *p;

	for (p = mut_region;;) {
		for (key = 1; key < 0x10; key++) {
			check(p, key, 1, 0);
			check(p, key, 0, 1);
			check(p, key, 1, 1);
		}
		p += getpagesize();
		if (p >= mut_region + mut_region_len)
			p = mut_region;
	}
}

int
main(void)
{
	struct sigaction sa;
	char *p;
	unsigned i;
	u_int regs[4];
	int error;

	cpuid_count(0xd, 0x9, regs);
	rpku_offset = regs[1];
	if (rpku_offset != 0)
#if defined(__i386__)
		rpku_offset -= sizeof(union savefpu);
#else
		rpku_offset -= sizeof(struct savefpu);
#endif

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sighandler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		err(1, "sigaction SIGSEGV");
	if (sigaction(SIGBUS, &sa, NULL) == -1)
		err(1, "sigaction SIGBUS");

	mut_region_len = getpagesize() * 100;
	mut_region_keys = calloc(mut_region_len, sizeof(unsigned));
	if (mut_region_keys == NULL)
		err(1, "calloc keys");
	mut_region = mmap(NULL, mut_region_len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0);
	if (mut_region == MAP_FAILED)
		err(1, "mmap");
	for (i = 1, p = mut_region; p < mut_region + mut_region_len;
	     p += getpagesize()) {
		error = x86_pkru_protect_range(p, getpagesize(), i, 0);
		if (error != 0)
			err(1, "x86_pkru_protect_range key %d", i);
		mut_region_keys[(p - mut_region) / getpagesize()] = i;
		if (++i > 0xf)
			i = 1;
	}

	signal(SIGALRM, handler);
	alarm(5);
	error = pthread_create(&bga_thr, NULL, bg_access_thread_fn, NULL);
	if (error != 0)
		errc(1, error, "pthread create background access thread");

	mutate_perms();
}
EOF
cc -Wall -Wextra -g -O -o pkru64 pkru.c -lpthread || exit 1
cc -Wall -Wextra -g -O -o pkru32 -m32 pkru.c -lpthread || exit 1
rm pkru.c
./pkru64
./pkru32
rm -f pkru64 pkru32

exit
