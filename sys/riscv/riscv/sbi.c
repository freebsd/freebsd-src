/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Mitchell Horne <mhorne@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/reboot.h>

#include <machine/md_var.h>
#include <machine/sbi.h>

/* SBI Implementation-Specific Definitions */
#define	OPENSBI_VERSION_MAJOR_OFFSET	16
#define	OPENSBI_VERSION_MINOR_MASK	0xFFFF

u_long sbi_spec_version;
u_long sbi_impl_id;
u_long sbi_impl_version;

static bool has_time_extension = false;
static bool has_ipi_extension = false;
static bool has_rfnc_extension = false;
static bool has_srst_extension = false;

static struct sbi_ret
sbi_get_spec_version(void)
{
	return (SBI_CALL0(SBI_EXT_ID_BASE, SBI_BASE_GET_SPEC_VERSION));
}

static struct sbi_ret
sbi_get_impl_id(void)
{
	return (SBI_CALL0(SBI_EXT_ID_BASE, SBI_BASE_GET_IMPL_ID));
}

static struct sbi_ret
sbi_get_impl_version(void)
{
	return (SBI_CALL0(SBI_EXT_ID_BASE, SBI_BASE_GET_IMPL_VERSION));
}

static struct sbi_ret
sbi_get_mvendorid(void)
{
	return (SBI_CALL0(SBI_EXT_ID_BASE, SBI_BASE_GET_MVENDORID));
}

static struct sbi_ret
sbi_get_marchid(void)
{
	return (SBI_CALL0(SBI_EXT_ID_BASE, SBI_BASE_GET_MARCHID));
}

static struct sbi_ret
sbi_get_mimpid(void)
{
	return (SBI_CALL0(SBI_EXT_ID_BASE, SBI_BASE_GET_MIMPID));
}

static void
sbi_shutdown_final(void *dummy __unused, int howto)
{
	if ((howto & RB_POWEROFF) != 0)
		sbi_system_reset(SBI_SRST_TYPE_SHUTDOWN, SBI_SRST_REASON_NONE);
}

void
sbi_system_reset(u_long reset_type, u_long reset_reason)
{
	/* Use the SRST extension, if available. */
	if (has_srst_extension) {
		(void)SBI_CALL2(SBI_EXT_ID_SRST, SBI_SRST_SYSTEM_RESET,
		    reset_type, reset_reason);
	}
	(void)SBI_CALL0(SBI_SHUTDOWN, 0);
}

void
sbi_print_version(void)
{
	u_int major;
	u_int minor;

	/* For legacy SBI implementations. */
	if (sbi_spec_version == 0) {
		printf("SBI: Unknown (Legacy) Implementation\n");
		printf("SBI Specification Version: 0.1\n");
		return;
	}

	switch (sbi_impl_id) {
	case (SBI_IMPL_ID_BBL):
		printf("SBI: Berkely Boot Loader %lu\n", sbi_impl_version);
		break;
	case (SBI_IMPL_ID_XVISOR):
		printf("SBI: eXtensible Versatile hypervISOR %lu\n",
		    sbi_impl_version);
		break;
	case (SBI_IMPL_ID_KVM):
		printf("SBI: Kernel-based Virtual Machine %lu\n",
		    sbi_impl_version);
		break;
	case (SBI_IMPL_ID_RUSTSBI):
		printf("SBI: RustSBI %lu\n", sbi_impl_version);
		break;
	case (SBI_IMPL_ID_DIOSIX):
		printf("SBI: Diosix %lu\n", sbi_impl_version);
		break;
	case (SBI_IMPL_ID_OPENSBI):
		major = sbi_impl_version >> OPENSBI_VERSION_MAJOR_OFFSET;
		minor = sbi_impl_version & OPENSBI_VERSION_MINOR_MASK;
		printf("SBI: OpenSBI v%u.%u\n", major, minor);
		break;
	default:
		printf("SBI: Unrecognized Implementation: %lu\n", sbi_impl_id);
		break;
	}

	major = (sbi_spec_version & SBI_SPEC_VERS_MAJOR_MASK) >>
	    SBI_SPEC_VERS_MAJOR_OFFSET;
	minor = (sbi_spec_version & SBI_SPEC_VERS_MINOR_MASK);
	printf("SBI Specification Version: %u.%u\n", major, minor);
}

void
sbi_set_timer(uint64_t val)
{
	struct sbi_ret ret __diagused;

	/* Use the TIME legacy replacement extension, if available. */
	if (has_time_extension) {
		ret = SBI_CALL1(SBI_EXT_ID_TIME, SBI_TIME_SET_TIMER, val);
		MPASS(ret.error == SBI_SUCCESS);
	} else {
		(void)SBI_CALL1(SBI_SET_TIMER, 0, val);
	}
}

void
sbi_send_ipi(const u_long *hart_mask)
{
	struct sbi_ret ret __diagused;

	/* Use the IPI legacy replacement extension, if available. */
	if (has_ipi_extension) {
		ret = SBI_CALL2(SBI_EXT_ID_IPI, SBI_IPI_SEND_IPI,
		    *hart_mask, 0);
		MPASS(ret.error == SBI_SUCCESS);
	} else {
		(void)SBI_CALL1(SBI_SEND_IPI, 0, (uint64_t)hart_mask);
	}
}

void
sbi_remote_fence_i(const u_long *hart_mask)
{
	struct sbi_ret ret __diagused;

	/* Use the RFENCE legacy replacement extension, if available. */
	if (has_rfnc_extension) {
		ret = SBI_CALL2(SBI_EXT_ID_RFNC, SBI_RFNC_REMOTE_FENCE_I,
		    *hart_mask, 0);
		MPASS(ret.error == SBI_SUCCESS);
	} else {
		(void)SBI_CALL1(SBI_REMOTE_FENCE_I, 0, (uint64_t)hart_mask);
	}
}

void
sbi_remote_sfence_vma(const u_long *hart_mask, u_long start, u_long size)
{
	struct sbi_ret ret __diagused;

	/* Use the RFENCE legacy replacement extension, if available. */
	if (has_rfnc_extension) {
		ret = SBI_CALL4(SBI_EXT_ID_RFNC, SBI_RFNC_REMOTE_SFENCE_VMA,
		    *hart_mask, 0, start, size);
		MPASS(ret.error == SBI_SUCCESS);
	} else {
		(void)SBI_CALL3(SBI_REMOTE_SFENCE_VMA, 0, (uint64_t)hart_mask,
		    start, size);
	}
}

void
sbi_remote_sfence_vma_asid(const u_long *hart_mask, u_long start, u_long size,
    u_long asid)
{
	struct sbi_ret ret __diagused;

	/* Use the RFENCE legacy replacement extension, if available. */
	if (has_rfnc_extension) {
		ret = SBI_CALL5(SBI_EXT_ID_RFNC,
		    SBI_RFNC_REMOTE_SFENCE_VMA_ASID, *hart_mask, 0, start,
		    size, asid);
		MPASS(ret.error == SBI_SUCCESS);
	} else {
		(void)SBI_CALL4(SBI_REMOTE_SFENCE_VMA_ASID, 0,
		    (uint64_t)hart_mask, start, size, asid);
	}
}

int
sbi_hsm_hart_start(u_long hart, u_long start_addr, u_long priv)
{
	struct sbi_ret ret;

	ret = SBI_CALL3(SBI_EXT_ID_HSM, SBI_HSM_HART_START, hart, start_addr,
	    priv);
	return (ret.error != 0 ? (int)ret.error : 0);
}

void
sbi_hsm_hart_stop(void)
{
	(void)SBI_CALL0(SBI_EXT_ID_HSM, SBI_HSM_HART_STOP);
}

int
sbi_hsm_hart_status(u_long hart)
{
	struct sbi_ret ret;

	ret = SBI_CALL1(SBI_EXT_ID_HSM, SBI_HSM_HART_STATUS, hart);

	return (ret.error != 0 ? (int)ret.error : (int)ret.value);
}

void
sbi_init(void)
{
	struct sbi_ret sret;

	/*
	 * Get the spec version. For legacy SBI implementations this will
	 * return an error, otherwise it is guaranteed to succeed.
	 */
	sret = sbi_get_spec_version();
	if (sret.error != 0) {
		/* We are running a legacy SBI implementation. */
		sbi_spec_version = 0;
		return;
	}

	/* Set the SBI implementation info. */
	sbi_spec_version = sret.value;
	sbi_impl_id = sbi_get_impl_id().value;
	sbi_impl_version = sbi_get_impl_version().value;

	/* Set the hardware implementation info. */
	mvendorid = sbi_get_mvendorid().value;
	marchid = sbi_get_marchid().value;
	mimpid = sbi_get_mimpid().value;

	/* Probe for legacy replacement extensions. */
	if (sbi_probe_extension(SBI_EXT_ID_TIME) != 0)
		has_time_extension = true;
	if (sbi_probe_extension(SBI_EXT_ID_IPI) != 0)
		has_ipi_extension = true;
	if (sbi_probe_extension(SBI_EXT_ID_RFNC) != 0)
		has_rfnc_extension = true;
	if (sbi_probe_extension(SBI_EXT_ID_SRST) != 0)
		has_srst_extension = true;

	/*
	 * Probe for legacy extensions. We still rely on many of them to be
	 * implemented, but this is not guaranteed by the spec.
	 */
	KASSERT(has_time_extension || sbi_probe_extension(SBI_SET_TIMER) != 0,
	    ("SBI doesn't implement sbi_set_timer()"));
	KASSERT(sbi_probe_extension(SBI_CONSOLE_PUTCHAR) != 0,
	    ("SBI doesn't implement sbi_console_putchar()"));
	KASSERT(sbi_probe_extension(SBI_CONSOLE_GETCHAR) != 0,
	    ("SBI doesn't implement sbi_console_getchar()"));
	KASSERT(has_ipi_extension || sbi_probe_extension(SBI_SEND_IPI) != 0,
	    ("SBI doesn't implement sbi_send_ipi()"));
	KASSERT(has_rfnc_extension ||
	    sbi_probe_extension(SBI_REMOTE_FENCE_I) != 0,
	    ("SBI doesn't implement sbi_remote_fence_i()"));
	KASSERT(has_rfnc_extension ||
	    sbi_probe_extension(SBI_REMOTE_SFENCE_VMA) != 0,
	    ("SBI doesn't implement sbi_remote_sfence_vma()"));
	KASSERT(has_rfnc_extension ||
	    sbi_probe_extension(SBI_REMOTE_SFENCE_VMA_ASID) != 0,
	    ("SBI doesn't implement sbi_remote_sfence_vma_asid()"));
	KASSERT(has_srst_extension || sbi_probe_extension(SBI_SHUTDOWN) != 0,
	    ("SBI doesn't implement a shutdown or reset extension"));
}

static void
sbi_late_init(void *dummy __unused)
{
	EVENTHANDLER_REGISTER(shutdown_final, sbi_shutdown_final, NULL,
	    SHUTDOWN_PRI_LAST);
}

SYSINIT(sbi, SI_SUB_KLD, SI_ORDER_ANY, sbi_late_init, NULL);
