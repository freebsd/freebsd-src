/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Netflix, Inc.
 *
 * This software was developed by Ali Mashtizadeh under the sponsorship from
 * Netflix, Inc.
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
 *
 */

#ifndef __HEADERS_HH__
#define __HEADERS_HH__

#define PMC_HEADER_MAGIC	0x504D434C
#define PMC_HEADER_VERSION	0x00

#define PMC_ARCH_AMD64		0x01
#define PMC_ARCH_ARM64		0x02
#define PMC_ARCH_PPC64		0x03
#define PMC_ARCH_RISCV64	0x04
#define PMC_ARCH_ARM		0x05

struct pmchdr_header
{
	uint32_t	magic;
	uint16_t	version;
	uint16_t	arch;
};

#define INFOHDR_TYPE_DONE	0x00
#define INFOHDR_TYPE_SYSINFO	0x01
#define INFOHDR_TYPE_PMCINFO	0x02
#define INFOHDR_TYPE_CPUID	0x03

struct pmchdr_infohdr
{
	uint8_t		type;
	uint8_t		reserved0;
	uint16_t	length;
	uint32_t	_reserved1;
};

struct pmchdr_sysinfo
{
	char		cpumodel[64];
	char		osrelease[64];
	char		buildid[64];
};

struct pmchdr_pmcinfo
{
	uint64_t	rate;
	char		pmc[];
};

struct pmchdr_cpuidinfo
{
	uint32_t	cpuid[];
};

#endif

