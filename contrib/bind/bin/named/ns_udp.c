#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_udp.c,v 8.9 2000/04/21 06:54:13 vixie Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
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

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <resolv.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

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
		ns_debug(ns_log_default, 1, "ns_udp: nlist (%s,%s) failed",
			 KSYMS, UDPSUM);
		return;
	}

	ns_debug(ns_log_default, 1, "ns_udp: %s %d %lu (%ld)",
		 nl[0].n_name, nl[0].n_type, nl[0].n_value, nl[0].n_value);

	if (!nl[0].n_type)
		return;

	if ((fd = open(KMEM, O_RDWR, 0)) < 0) {
		ns_debug(ns_log_default, 1, "ns_udp: open %s failed: %s", KMEM,
			 strerror(errno));
		return;
	}

	offset = nl[0].n_value;
#ifdef KMAP
	offset &= ((~0UL)>>1);
#endif

	res = lseek(fd, offset, SEEK_SET);
	if (res != offset) {
		ns_debug(ns_log_default, 1, "ns_udp: lseek %lu failed %lu: %s",
			 offset, res, strerror(errno));
		goto cleanup;
	}

	if (read(fd, &sum, sizeof(sum)) != sizeof(sum)) {
		ns_debug(ns_log_default, 1, "ns_udp: read failed: %s",
			 strerror(errno));
		goto cleanup;
	}

	ns_debug(ns_log_default, 1, "ns_udp: %d", sum);
	if (sum == 0) {
#ifdef FIX_UDP_SUM
		sum = 1;
		lseek(fd, offset, SEEK_SET);
		if (res != offset) {
			ns_debug(ns_log_default, 1,
				 "ns_udp: lseek %lu failed %lu: %s",
				 offset, res, strerror(errno));
			goto cleanup;
		}
		if (write(fd, &sum, sizeof(sum)) != sizeof(sum)) {
			ns_debug(ns_log_default, 1, "ns_udp: write failed: %s",
				 strerror(errno));
			goto cleanup;
		}
		ns_warning(ns_log_default, "ns_udp: check sums turned on");
#else
		ns_panic(ns_log_default, 0,
			 "ns_udp: checksums NOT turned on, exiting");
#endif
	}
cleanup:
	close(fd);
#endif
}
