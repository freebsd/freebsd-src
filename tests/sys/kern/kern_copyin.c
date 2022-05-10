/*-
 * Copyright (c) 2015, 2020 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <atf-c.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>

static int scratch_file;

static int
copyin_checker(uintptr_t uaddr, size_t len)
{
	ssize_t ret;

	ret = write(scratch_file, (const void *)uaddr, len);
	return (ret == -1 ? errno : 0);
}

#if __SIZEOF_POINTER__ == 8
/*
 * A slightly more direct path to calling copyin(), but without the ability
 * to specify a length.
 */
static int
copyin_checker2(uintptr_t uaddr)
{
	int ret;

	ret = fcntl(scratch_file, F_GETLK, (const void *)uaddr);
	return (ret == -1 ? errno : 0);
}
#endif

static int
get_vm_layout(struct kinfo_vm_layout *kvm)
{
	size_t len;
	int mib[4];

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_VM_LAYOUT;
	mib[3] = getpid();
	len = sizeof(*kvm);

	return (sysctl(mib, nitems(mib), kvm, &len, NULL, 0));
}

#define	FMAX	ULONG_MAX
#if __SIZEOF_POINTER__ == 8
/* PR 257193 */
#define	ADDR_SIGNED	0x800000c000000000
#endif

ATF_TC_WITHOUT_HEAD(kern_copyin);
ATF_TC_BODY(kern_copyin, tc)
{
	char template[] = "copyin.XXXXXX";
	struct kinfo_vm_layout kvm;
	uintptr_t maxuser;
	long page_size;
	void *addr;
	int error;

	addr = MAP_FAILED;

	error = get_vm_layout(&kvm);
	ATF_REQUIRE(error == 0);

	page_size = sysconf(_SC_PAGESIZE);
	ATF_REQUIRE(page_size != (long)-1);

	maxuser = kvm.kvm_max_user_addr;
	scratch_file = mkstemp(template);
	ATF_REQUIRE(scratch_file != -1);
	unlink(template);

	/*
	 * Since the shared page address can be randomized we need to make
	 * sure that something is mapped at the top of the user address space.
	 * Otherwise reading bytes from maxuser-X will fail rendering this test
	 * useless.
	 */
	if (kvm.kvm_shp_addr + kvm.kvm_shp_size < maxuser) {
		addr = mmap((void *)(maxuser - page_size), page_size, PROT_READ,
		    MAP_ANON | MAP_FIXED, -1, 0);
		ATF_REQUIRE(addr != MAP_FAILED);
	}

	ATF_CHECK(copyin_checker(0, 0) == 0);
	ATF_CHECK(copyin_checker(maxuser - 10, 9) == 0);
	ATF_CHECK(copyin_checker(maxuser - 10, 10) == 0);
	ATF_CHECK(copyin_checker(maxuser - 10, 11) == EFAULT);
	ATF_CHECK(copyin_checker(maxuser - 1, 1) == 0);
	ATF_CHECK(copyin_checker(maxuser, 0) == 0);
	ATF_CHECK(copyin_checker(maxuser, 1) == EFAULT);
	ATF_CHECK(copyin_checker(maxuser, 2) == EFAULT);
	ATF_CHECK(copyin_checker(maxuser + 1, 0) == 0);
	ATF_CHECK(copyin_checker(maxuser + 1, 2) == EFAULT);
	ATF_CHECK(copyin_checker(FMAX - 10, 9) == EFAULT);
	ATF_CHECK(copyin_checker(FMAX - 10, 10) == EFAULT);
	ATF_CHECK(copyin_checker(FMAX - 10, 11) == EFAULT);
#if __SIZEOF_POINTER__ == 8
	ATF_CHECK(copyin_checker(ADDR_SIGNED, 1) == EFAULT);
	ATF_CHECK(copyin_checker2(ADDR_SIGNED) == EFAULT);
#endif

	if (addr != MAP_FAILED)
		munmap(addr, PAGE_SIZE);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, kern_copyin);
	return (atf_no_error());
}
