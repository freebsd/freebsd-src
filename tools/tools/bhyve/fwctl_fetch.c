/*-
 * Copyright (c) 2023 John Baldwin <jhb@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Fetch the value of fwctl nodes from a guest.
 *
 * Usage: fwctl_fetch <node>
 */

#include <sys/param.h>
#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <machine/cpufunc.h>

#define	OP_GET		3
#define	OP_GET_LEN	4

/* I/O ports */
#define	FWCTL_OUT	0x510
#define	FWCTL_IN	0x511

static void
reset_fwctl(void)
{
	char buf[4];

	outw(FWCTL_OUT, 0);
	for (u_int i = 0; i < 4; i++)
		buf[i] = inb(FWCTL_IN);
	if (memcmp(buf, "BHYV", 4) != 0)
		errx(1, "Signature mismatch: %.4s", buf);
}

static void
send_node_name(const char *name)
{
	uint32_t value;
	size_t len;

	len = strlen(name) + 1;
	while (len > 4) {
		memcpy(&value, name, 4);
		outl(FWCTL_OUT, value);
		name += 4;
		len -= 4;
	}

	if (len > 0) {
		value = 0;
		memcpy(&value, name, len);
		outl(FWCTL_OUT, value);
	}
}

static void
fwctl_op(uint32_t op, uint32_t id, const char *name, void *buf, size_t len)
{
	char *cp;
	uint32_t value, rsplen;

	/* Length */
	outl(FWCTL_OUT, 12 + strlen(name) + 1);

	/* Operation */
	outl(FWCTL_OUT, op);

	/* Transaction ID */
	outl(FWCTL_OUT, id);

	send_node_name(name);

	/* Length */
	rsplen = inl(FWCTL_IN);

	/* If there is an error, the response will have no payload. */
	if (rsplen < 4 * sizeof(value))
		errx(1, "Invalid response length (%u): %u", id, rsplen);

	/* Operation */
	value = inl(FWCTL_IN);
	if (value != op)
		errx(1, "Invalid response type (%u): %u", id, value);

	/* Transaction ID */
	value = inl(FWCTL_IN);
	if (value != id)
		errx(1, "Invalid response ID (%u): %u", id, value);

	/* Error */
	value = inl(FWCTL_IN);
	if (value != 0)
		errx(1, "Error from op %u (%u): %u", op, id, value);

	/* If there wasn't an error, require payload length to match */
	if (rsplen != 4 * sizeof(value) + len)
		errx(1, "Response payload length mismatch (%u): %zu vs %zu", id,
		    rsplen - 4 * sizeof(value), len);

	cp = buf;
	while (len > 0) {
		value = inl(FWCTL_IN);
		memcpy(cp, &value, 4);
		cp += 4;
		len -= 4;
	}
}

int
main(int ac, char **av)
{
	char *p;
	size_t len, buflen, len2;

	if (ac != 2)
		errx(1, "Need node name");

	if (open("/dev/io", O_RDWR) == -1)
		err(1, "Failed to open /dev/io");

	reset_fwctl();

	fwctl_op(OP_GET_LEN, 1, av[1], &len, sizeof(len));
	if (len == 0)
		errx(1, "Node has length of 0");

	/* Buffer includes embedded length followed by value. */
	buflen = sizeof(size_t) + roundup2(len, 4);
	p = malloc(buflen);
	fwctl_op(OP_GET, 2, av[1], p, buflen);
	memcpy(&len2, p, sizeof(len2));
	if (len2 != len)
		errx(1, "Length mismatch: %zu vs %zu", len, len2);
	hexdump(p + sizeof(len2), len, NULL, 0);

	return (0);
}
