#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_udp.c,v 8.3 1996/08/27 08:33:23 vixie Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <netdb.h>
#include <nlist.h>
#include <resolv.h>

#include "named.h"

void
ns_udp() {
#if defined(CHECK_UDP_SUM) || defined(FIX_UDP_SUM)
	struct nlist nl[2];
	int fd;
	int sum;
	u_long res, offset;

	nl[0].n_name = UDPSUM;
	nl[1].n_name = 0;

	if (nlist(KSYMS, nl)) {
		dprintf(1, (ddt, "ns_udp: nlist (%s,%s) failed\n",
				KSYMS, UDPSUM));
		return;
	}

	dprintf(1, (ddt, "ns_udp: %s %d %lu (%ld)\n",
		nl[0].n_name, nl[0].n_type, nl[0].n_value,
		nl[0].n_value));

	if (!nl[0].n_type)
		return;

	if ((fd = open(KMEM, O_RDWR, 0)) < 0) {
		dprintf(1, (ddt, "ns_udp: open %s failed\n", KMEM));
		return;
	}

	offset = nl[0].n_value;
#ifdef KMAP
	offset &= ((~0UL)>>1);
#endif

	res = lseek(fd, offset, SEEK_SET);
	if (res != offset) {
		dprintf(1, (ddt, "ns_udp: lseek %ul failed %lu %d\n",
			    offset, res, errno));
		goto cleanup;
	}

	if (read(fd, &sum, sizeof(sum)) != sizeof(sum)) {
		dprintf(1, (ddt, "ns_udp: read failed\n"));
		goto cleanup;
	}

	dprintf(1, (ddt, "ns_udp: %d\n", sum));
	if (sum == 0) {
#ifdef FIX_UDP_SUM
		sum = 1;
		lseek(fd, offset, SEEK_SET);
		if (res != offset) {
			dprintf(1, (ddt, "ns_udp: lseek %ul failed %lu %d\n",
				    offset, res, errno));
			goto cleanup;
		}
		if (write(fd, &sum, sizeof(sum)) != sizeof(sum)) {
			dprintf(1, (ddt, "ns_udp: write failed\n"));
			goto cleanup;
		}
		dprintf(1, (ddt, "ns_udp: set to 1\n"));
		syslog(LOG_WARNING, "ns_udp: check sums turned on");
#else
		dprintf(1, (ddt, "ns_udp: Exiting\n"));
		syslog(LOG_WARNING, "ns_udp: checksums NOT turned on: Exiting");
		exit(1);
#endif
	}
cleanup:
	close(fd);
#endif
}
