/*-
 * Copyright (C) 2010-2014 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/endian.h>

#include "stand.h"
#include "host_syscall.h"
#include "kboot.h"

struct region_desc {
	uint64_t start;
	uint64_t end;
};

/*
 * Find a good place to load the kernel, subject to the PowerPC's constraints
 *
 * This excludes ranges that are marked as reserved.
 * And 0..end of kernel
 *
 * It then tries to find the memory exposed from the DTB, which it assumes is one
 * contiguous range. It adds everything not in that list to the excluded list.
 *
 * Sort, dedup, and it finds the first region and uses that as the load_segment
 * and returns that. All addresses are offset by this amount.
 */
uint64_t
kboot_get_phys_load_segment(void)
{
	int fd;
	uint64_t entry[2];
	static uint64_t load_segment = ~(0UL);
	uint64_t val_64;
	uint32_t val_32;
	struct region_desc rsvd_reg[32];
	int rsvd_reg_cnt = 0;
	int ret, a, b;
	uint64_t start, end;

	if (load_segment == ~(0UL)) {

		/* Default load address is 0x00000000 */
		load_segment = 0UL;

		/* Read reserved regions */
		fd = host_open("/proc/device-tree/reserved-ranges", O_RDONLY, 0);
		if (fd >= 0) {
			while (host_read(fd, &entry[0], sizeof(entry)) == sizeof(entry)) {
				rsvd_reg[rsvd_reg_cnt].start = be64toh(entry[0]);
				rsvd_reg[rsvd_reg_cnt].end =
				    be64toh(entry[1]) + rsvd_reg[rsvd_reg_cnt].start - 1;
				rsvd_reg_cnt++;
			}
			host_close(fd);
		}
		/* Read where the kernel ends */
		fd = host_open("/proc/device-tree/chosen/linux,kernel-end", O_RDONLY, 0);
		if (fd >= 0) {
			ret = host_read(fd, &val_64, sizeof(val_64));

			if (ret == sizeof(uint64_t)) {
				rsvd_reg[rsvd_reg_cnt].start = 0;
				rsvd_reg[rsvd_reg_cnt].end = be64toh(val_64) - 1;
			} else {
				memcpy(&val_32, &val_64, sizeof(val_32));
				rsvd_reg[rsvd_reg_cnt].start = 0;
				rsvd_reg[rsvd_reg_cnt].end = be32toh(val_32) - 1;
			}
			rsvd_reg_cnt++;

			host_close(fd);
		}
		/* Read memory size (SOCKET0 only) */
		fd = host_open("/proc/device-tree/memory@0/reg", O_RDONLY, 0);
		if (fd < 0)
			fd = host_open("/proc/device-tree/memory/reg", O_RDONLY, 0);
		if (fd >= 0) {
			ret = host_read(fd, &entry, sizeof(entry));

			/* Memory range in start:length format */
			entry[0] = be64toh(entry[0]);
			entry[1] = be64toh(entry[1]);

			/* Reserve everything what is before start */
			if (entry[0] != 0) {
				rsvd_reg[rsvd_reg_cnt].start = 0;
				rsvd_reg[rsvd_reg_cnt].end = entry[0] - 1;
				rsvd_reg_cnt++;
			}
			/* Reserve everything what is after end */
			if (entry[1] != 0xffffffffffffffffUL) {
				rsvd_reg[rsvd_reg_cnt].start = entry[0] + entry[1];
				rsvd_reg[rsvd_reg_cnt].end = 0xffffffffffffffffUL;
				rsvd_reg_cnt++;
			}

			host_close(fd);
		}

		/* Sort entries in ascending order (bubble) */
		for (a = rsvd_reg_cnt - 1; a > 0; a--) {
			for (b = 0; b < a; b++) {
				if (rsvd_reg[b].start > rsvd_reg[b + 1].start) {
					struct region_desc tmp;
					tmp = rsvd_reg[b];
					rsvd_reg[b] = rsvd_reg[b + 1];
					rsvd_reg[b + 1] = tmp;
				}
			}
		}

		/* Join overlapping/adjacent regions */
		for (a = 0; a < rsvd_reg_cnt - 1; ) {

			if ((rsvd_reg[a + 1].start >= rsvd_reg[a].start) &&
			    ((rsvd_reg[a + 1].start - 1) <= rsvd_reg[a].end)) {
				/* We have overlapping/adjacent regions! */
				rsvd_reg[a].end =
				    MAX(rsvd_reg[a].end, rsvd_reg[a + a].end);

				for (b = a + 1; b < rsvd_reg_cnt - 1; b++)
					rsvd_reg[b] = rsvd_reg[b + 1];
				rsvd_reg_cnt--;
			} else
				a++;
		}

		/* Find the first free region */
		if (rsvd_reg_cnt > 0) {
			start = 0;
			end = rsvd_reg[0].start;
			for (a = 0; a < rsvd_reg_cnt - 1; a++) {
				if ((start >= rsvd_reg[a].start) &&
				    (start <= rsvd_reg[a].end)) {
					start = rsvd_reg[a].end + 1;
					end = rsvd_reg[a + 1].start;
				} else
					break;
			}

			if (start != end) {
				uint64_t align = 64UL*1024UL*1024UL;

				/* Align both to 64MB boundary */
				start = (start + align - 1UL) & ~(align - 1UL);
				end = ((end + 1UL) & ~(align - 1UL)) - 1UL;

				if (start < end)
					load_segment = start;
			}
		}
	}

	return (load_segment);
}

#if 0
/*
 * XXX this appears to be unused, but may have been for selecting the allowed
 * kernels ABIs. It's been unused since the first commit, which suggests an
 * error in bringing this into the tree.
 */
uint8_t
kboot_get_kernel_machine_bits(void)
{
	static uint8_t bits = 0;
	struct old_utsname utsname;
	int ret;

	if (bits == 0) {
		/* Default is 32-bit kernel */
		bits = 32;

		/* Try to get system type */
		memset(&utsname, 0, sizeof(utsname));
		ret = host_uname(&utsname);
		if (ret == 0) {
			if (strcmp(utsname.machine, "ppc64") == 0)
				bits = 64;
			else if (strcmp(utsname.machine, "ppc64le") == 0)
				bits = 64;
		}
	}

	return (bits);
}
#endif

/* Need to transition from current hacky FDT way to this code */
bool enumerate_memory_arch(void)
{
	/*
	 * For now, we dig it out of the FDT, plus we need to pass all data into
	 * the kernel via the (adjusted) FDT we find.
	 */
	setenv("usefdt", "1", 1);

	return true;
}

void
bi_loadsmap(struct preloaded_file *kfp)
{
	/* passed in via the DTB */
}
