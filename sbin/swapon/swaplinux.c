/*-
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fcntl.h>
#include <err.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * Definitions and structure taken from
 * https://github.com/util-linux/util-linux/blob/master/include/swapheader.h
 */

#define SWAP_VERSION 1
#define SWAP_UUID_LENGTH 16
#define SWAP_LABEL_LENGTH 16
#define SWAP_SIGNATURE "SWAPSPACE2"
#define SWAP_SIGNATURE_SZ (sizeof(SWAP_SIGNATURE) - 1)

struct swap_header_v1_2 {
	char	      bootbits[1024];    /* Space for disklabel etc. */
	uint32_t      version;
	uint32_t      last_page;
	uint32_t      nr_badpages;
	unsigned char uuid[SWAP_UUID_LENGTH];
	char	      volume_name[SWAP_LABEL_LENGTH];
	uint32_t      padding[117];
	uint32_t      badpages[1];
};

typedef union {
	struct swap_header_v1_2 header;
	struct {
		uint8_t	reserved[4096 - SWAP_SIGNATURE_SZ];
		char	signature[SWAP_SIGNATURE_SZ];
	} tail;
} swhdr_t;

#define sw_version	header.version
#define sw_volume_name	header.volume_name
#define sw_signature	tail.signature

int
is_linux_swap(const char *name)
{
	uint8_t buf[4096];
	swhdr_t *hdr = (swhdr_t *) buf;
	int fd;

	fd = open(name, O_RDONLY);
	if (fd == -1)
		return (-1);

	if (read(fd, buf, 4096) != 4096) {
		close(fd);
		return (-1);
	}
	close(fd);

	return (hdr->sw_version == SWAP_VERSION &&
	    !memcmp(hdr->sw_signature, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ));
}
