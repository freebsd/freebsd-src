/*
 * dumptrees - utility for libfdt testing
 *
 * (C) Copyright David Gibson <dwg@au1.ibm.com>, IBM Corporation.  2006.
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *                                                                   USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include <libfdt.h>

#include "testdata.h"

static struct {
	void *blob;
	const char *filename;
} trees[] = {
#define TREE(name)	{ &name, #name ".dtb" }
	TREE(test_tree1),
	TREE(bad_node_char), TREE(bad_node_format), TREE(bad_prop_char),
	TREE(ovf_size_strings),
};

#define NUM_TREES	(sizeof(trees) / sizeof(trees[0]))

int main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < NUM_TREES; i++) {
		void *blob = trees[i].blob;
		const char *filename = trees[i].filename;
		int size;
		int fd;
		int ret;

		size = fdt_totalsize(blob);

		printf("Tree \"%s\", %d bytes\n", filename, size);

		fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (fd < 0)
			perror("open()");

		ret = write(fd, blob, size);
		if (ret != size)
			perror("write()");

		close(fd);
	}
	exit(0);
}
