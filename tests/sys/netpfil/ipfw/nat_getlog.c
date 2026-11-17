/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Maxim Konovalov <maxim@FreeBSD.org>
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

/*
 * Tests for the IP_FW_NAT44_XGETLOG sockopt handled by ipfw_nat(4).
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>

#include <alias.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	NAT_NAME	"17"
#define	NAT_CREATED	"nat_created"

#define	NAT_HDRLEN	(sizeof(ipfw_obj_header) + sizeof(struct nat44_cfg_nat))
#define	NAT_LOGLEN	(NAT_HDRLEN + LIBALIAS_BUF_SIZE)

static int
nat_socket(void)
{
	int s;

	s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	ATF_REQUIRE_MSG(s >= 0, "socket: %s", strerror(errno));
	return (s);
}

static ipfw_obj_header *
nat_request(uint16_t opcode, size_t len)
{
	ipfw_obj_header *oh;
	struct nat44_cfg_nat *ucfg;

	oh = calloc(1, len);
	ATF_REQUIRE(oh != NULL);
	oh->opheader.opcode = opcode;
	oh->opheader.version = IP_FW3_OPVER;
	oh->ntlv.head.length = sizeof(oh->ntlv);
	strlcpy(oh->ntlv.name, NAT_NAME, sizeof(oh->ntlv.name));
	if (len >= NAT_HDRLEN) {
		ucfg = (struct nat44_cfg_nat *)(oh + 1);
		strlcpy(ucfg->name, NAT_NAME, sizeof(ucfg->name));
	}
	return (oh);
}

/*
 * Creates a nat instance with logging enabled, which allocates the
 * libalias log buffer returned by IP_FW_NAT44_XGETLOG.
 */
static void
nat_create(int s)
{
	ipfw_obj_header *oh;
	struct nat44_cfg_nat *ucfg;
	socklen_t optlen;

	/* Do not clobber an instance that does not belong to us. */
	oh = nat_request(IP_FW_NAT44_XGETCONFIG, NAT_HDRLEN);
	optlen = NAT_HDRLEN;
	if (getsockopt(s, IPPROTO_IP, IP_FW3, oh, &optlen) == 0 ||
	    errno != ESRCH)
		atf_tc_skip("nat %s already exists", NAT_NAME);
	free(oh);

	oh = nat_request(IP_FW_NAT44_XCONFIG, NAT_HDRLEN);
	ucfg = (struct nat44_cfg_nat *)(oh + 1);
	ucfg->ip.s_addr = htonl(INADDR_LOOPBACK);
	ucfg->mode = PKT_ALIAS_LOG;
	ATF_REQUIRE_MSG(setsockopt(s, IPPROTO_IP, IP_FW3, oh, NAT_HDRLEN) == 0,
	    "IP_FW_NAT44_XCONFIG: %s", strerror(errno));
	free(oh);
	atf_utils_create_file(NAT_CREATED, "%s", "");
}

static void
nat_destroy(void)
{
	ipfw_obj_header *oh;
	int s;

	if (access(NAT_CREATED, F_OK) != 0)
		return;
	s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (s < 0)
		return;
	oh = nat_request(IP_FW_NAT44_DESTROY, sizeof(*oh));
	(void)setsockopt(s, IPPROTO_IP, IP_FW3, oh, sizeof(*oh));
	free(oh);
}

/*
 * Issues IP_FW_NAT44_XGETLOG with a buffer of len bytes.  Returns 0 or
 * the errno value, and the reply length and the size reported by the
 * kernel in the nat44_cfg_nat header.
 */
static int
nat_getlog(int s, size_t len, size_t *replylen, uint32_t *size)
{
	ipfw_obj_header *oh;
	socklen_t optlen;
	int error;

	oh = nat_request(IP_FW_NAT44_XGETLOG, len);
	optlen = len;
	error = 0;
	if (getsockopt(s, IPPROTO_IP, IP_FW3, oh, &optlen) != 0)
		error = errno;
	*replylen = optlen;
	*size = ((struct nat44_cfg_nat *)(oh + 1))->size;
	free(oh);
	return (error);
}

ATF_TC_WITH_CLEANUP(getlog_short_buffer);
ATF_TC_HEAD(getlog_short_buffer, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A buffer too short for the log gets ENOMEM and the needed size");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "ipfw_nat");
}
ATF_TC_BODY(getlog_short_buffer, tc)
{
	size_t len, replylen;
	uint32_t size;
	int error, s;

	s = nat_socket();
	nat_create(s);

	/*
	 * A buffer holding the request header but not the whole log used
	 * to pass the length check, and some of these lengths left too
	 * little sockopt buffer for the log, which was then copied to a
	 * NULL pointer.
	 */
	for (len = NAT_HDRLEN; len < NAT_LOGLEN; len++) {
		error = nat_getlog(s, len, &replylen, &size);
		ATF_CHECK_EQ_MSG(ENOMEM, error, "len %zu: error %d", len,
		    error);
		ATF_CHECK_EQ_MSG(NAT_LOGLEN, size, "len %zu: size %u", len,
		    size);
	}

	error = nat_getlog(s, NAT_LOGLEN, &replylen, &size);
	ATF_REQUIRE_EQ_MSG(0, error, "len %zu: error %d", NAT_LOGLEN, error);
	ATF_REQUIRE_EQ(NAT_LOGLEN, replylen);
}
ATF_TC_CLEANUP(getlog_short_buffer, tc)
{
	nat_destroy();
}

ATF_TC_WITH_CLEANUP(getlog_retry);
ATF_TC_HEAD(getlog_retry, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A retry with the size reported along with ENOMEM gets the log");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "ipfw_nat");
}
ATF_TC_BODY(getlog_retry, tc)
{
	size_t replylen;
	uint32_t size;
	int error, s;

	s = nat_socket();
	nat_create(s);

	error = nat_getlog(s, NAT_HDRLEN, &replylen, &size);
	ATF_REQUIRE_EQ(ENOMEM, error);
	ATF_REQUIRE(size >= NAT_HDRLEN);

	error = nat_getlog(s, size, &replylen, &size);
	ATF_REQUIRE_EQ_MSG(0, error, "retry with size %u: error %d", size,
	    error);
}
ATF_TC_CLEANUP(getlog_retry, tc)
{
	nat_destroy();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, getlog_short_buffer);
	ATF_TP_ADD_TC(tp, getlog_retry);

	return (atf_no_error());
}
