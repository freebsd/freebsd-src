/*
 * Copyright (c) 2015-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PT_SECTION_WINDOWS_H
#define PT_SECTION_WINDOWS_H

#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>

struct pt_section;


/* Fstat-based file status. */
struct pt_sec_windows_status {
	/* The file status. */
	struct _stat stat;
};

/* FileView-based section mapping information. */
struct pt_sec_windows_mapping {
	/* The file descriptor. */
	int fd;

	/* The FileMapping handle. */
	HANDLE mh;

	/* The mmap base address. */
	uint8_t *base;

	/* The begin and end of the mapped memory. */
	const uint8_t *begin, *end;
};


/* Map a section.
 *
 * The caller has already opened the file for reading.
 *
 * On success, sets @section's mapping, unmap, and read pointers.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_invalid if @section can't be mapped.
 */
extern int pt_sec_windows_map(struct pt_section *section, int fd);

/* Unmap a section.
 *
 * On success, clears @section's mapping, unmap, and read pointers.
 *
 * This function should not be called directly; call @section->unmap() instead.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_internal if @section has not been mapped.
 */
extern int pt_sec_windows_unmap(struct pt_section *section);

/* Read memory from an mmaped section.
 *
 * Reads at most @size bytes from @section at @offset into @buffer.
 *
 * This function should not be called directly; call @section->read() instead.
 *
 * Returns the number of bytes read on success, a negative error code otherwise.
 * Returns -pte_invalid if @section or @buffer are NULL.
 * Returns -pte_nomap if @offset is beyond the end of the section.
 */
extern int pt_sec_windows_read(const struct pt_section *section,
			       uint8_t *buffer, uint16_t size,
			       uint64_t offset);

/* Compute the memory size of a section.
 *
 * On success, provides the amount of memory used for mapping @section in bytes
 * in @size.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section or @size is NULL.
 * Returns -pte_internal if @section has not been mapped.
 */
extern int pt_sec_windows_memsize(const struct pt_section *section,
				  uint64_t *size);

#endif /* PT_SECTION_WINDOWS_H */
