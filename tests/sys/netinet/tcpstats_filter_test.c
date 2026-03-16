/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * ATF tests for the tcpstats filter string parser.
 */

#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atf-c.h>

#include <netinet/tcp_statsdev.h>
#include <netinet/tcp_statsdev_filter.h>

/* TCP state constants (matching FreeBSD netinet/tcp_fsm.h) */
#ifndef TCPS_CLOSED
#define	TCPS_CLOSED		0
#define	TCPS_LISTEN		1
#define	TCPS_SYN_SENT		2
#define	TCPS_SYN_RECEIVED	3
#define	TCPS_ESTABLISHED	4
#define	TCPS_CLOSE_WAIT		5
#define	TCPS_FIN_WAIT_1		6
#define	TCPS_CLOSING		7
#define	TCPS_LAST_ACK		8
#define	TCPS_FIN_WAIT_2		9
#define	TCPS_TIME_WAIT		10
#endif

/* ================================================================
 * Positive tests: empty/whitespace
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(empty_string);
ATF_TC_BODY(empty_string, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("", 0, &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK_EQ(TSF_VERSION, f.version);
	ATF_CHECK_EQ(0xFFFF, f.state_mask);
	ATF_CHECK_EQ(TSR_FIELDS_DEFAULT, f.field_mask);
}

ATF_TC_WITHOUT_HEAD(whitespace_only);
ATF_TC_BODY(whitespace_only, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("   ", 3, &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

/* ================================================================
 * Positive tests: port parsing
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(single_port);
ATF_TC_BODY(single_port, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=443",
	    strlen("local_port=443"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK_EQ(htons(443), f.local_ports[0]);
	ATF_CHECK(f.flags & TSF_LOCAL_PORT_MATCH);
}

ATF_TC_WITHOUT_HEAD(multiple_ports);
ATF_TC_BODY(multiple_ports, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=443,8443,8080",
	    strlen("local_port=443,8443,8080"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

ATF_TC_WITHOUT_HEAD(max_ports);
ATF_TC_BODY(max_ports, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=1,2,3,4,5,6,7,8",
	    strlen("local_port=1,2,3,4,5,6,7,8"), &f, errbuf,
	    sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

ATF_TC_WITHOUT_HEAD(both_port_directions);
ATF_TC_BODY(both_port_directions, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=443 remote_port=80",
	    strlen("local_port=443 remote_port=80"), &f, errbuf,
	    sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK(f.flags & TSF_LOCAL_PORT_MATCH);
	ATF_CHECK(f.flags & TSF_REMOTE_PORT_MATCH);
}

/* ================================================================
 * Positive tests: state parsing
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(exclude_listen);
ATF_TC_BODY(exclude_listen, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("exclude=listen",
	    strlen("exclude=listen"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK(!(f.state_mask & (1 << TCPS_LISTEN)));
	ATF_CHECK(f.state_mask & (1 << TCPS_ESTABLISHED));
}

ATF_TC_WITHOUT_HEAD(exclude_multiple);
ATF_TC_BODY(exclude_multiple, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("exclude=listen,timewait",
	    strlen("exclude=listen,timewait"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK(!(f.state_mask & (1 << TCPS_LISTEN)));
	ATF_CHECK(!(f.state_mask & (1 << TCPS_TIME_WAIT)));
}

ATF_TC_WITHOUT_HEAD(include_established);
ATF_TC_BODY(include_established, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("include_state=established",
	    strlen("include_state=established"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK_EQ((uint16_t)(1 << TCPS_ESTABLISHED), f.state_mask);
	ATF_CHECK(f.flags & TSF_STATE_INCLUDE_MODE);
}

ATF_TC_WITHOUT_HEAD(case_insensitive_states);
ATF_TC_BODY(case_insensitive_states, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("EXCLUDE=LISTEN",
	    strlen("EXCLUDE=LISTEN"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

/* ================================================================
 * Positive tests: IPv4 parsing
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(ipv4_exact);
ATF_TC_BODY(ipv4_exact, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=10.0.0.1",
	    strlen("local_addr=10.0.0.1"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK(f.flags & TSF_LOCAL_ADDR_MATCH);
}

ATF_TC_WITHOUT_HEAD(ipv4_cidr24);
ATF_TC_BODY(ipv4_cidr24, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	struct in_addr expected_addr, expected_mask;
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=10.0.0.0/24",
	    strlen("local_addr=10.0.0.0/24"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	expected_addr.s_addr = htonl(0x0A000000);
	expected_mask.s_addr = htonl(0xFFFFFF00);
	ATF_CHECK_EQ(expected_addr.s_addr, f.local_addr_v4.s_addr);
	ATF_CHECK_EQ(expected_mask.s_addr, f.local_mask_v4.s_addr);
}

ATF_TC_WITHOUT_HEAD(ipv4_cidr0);
ATF_TC_BODY(ipv4_cidr0, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=0.0.0.0/0",
	    strlen("local_addr=0.0.0.0/0"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

/* ================================================================
 * Positive tests: IPv6 parsing
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(ipv6_loopback);
ATF_TC_BODY(ipv6_loopback, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	struct in6_addr expected;
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=::1",
	    strlen("local_addr=::1"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	memset(&expected, 0, sizeof(expected));
	expected.s6_addr[15] = 1;
	ATF_CHECK_EQ(0, memcmp(&f.local_addr_v6, &expected,
	    sizeof(expected)));
	ATF_CHECK_EQ(128, f.local_prefix_v6);
}

ATF_TC_WITHOUT_HEAD(ipv6_compressed);
ATF_TC_BODY(ipv6_compressed, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=2001:db8::1",
	    strlen("local_addr=2001:db8::1"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

ATF_TC_WITHOUT_HEAD(ipv6_link_local_cidr);
ATF_TC_BODY(ipv6_link_local_cidr, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=fe80::/10",
	    strlen("local_addr=fe80::/10"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
}

/* ================================================================
 * Positive tests: flags, format, fields
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(ipv4_only_flag);
ATF_TC_BODY(ipv4_only_flag, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("ipv4_only",
	    strlen("ipv4_only"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK(f.flags & TSF_IPV4_ONLY);
}

ATF_TC_WITHOUT_HEAD(format_full);
ATF_TC_BODY(format_full, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("format=full",
	    strlen("format=full"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK_EQ(TSF_FORMAT_FULL, f.format);
}

ATF_TC_WITHOUT_HEAD(fields_multiple);
ATF_TC_BODY(fields_multiple, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	uint32_t expected;
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("fields=state,rtt,buffers",
	    strlen("fields=state,rtt,buffers"), &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	expected = TSR_FIELDS_STATE | TSR_FIELDS_RTT | TSR_FIELDS_BUFFERS;
	ATF_CHECK_EQ(expected, f.field_mask);
}

ATF_TC_WITHOUT_HEAD(full_combo);
ATF_TC_BODY(full_combo, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string(
	    "local_port=443 exclude=listen,timewait ipv4_only format=full",
	    strlen("local_port=443 exclude=listen,timewait ipv4_only format=full"),
	    &f, errbuf, sizeof(errbuf));
	ATF_REQUIRE_EQ(0, err);
	ATF_CHECK(f.flags & TSF_LOCAL_PORT_MATCH);
	ATF_CHECK(f.flags & TSF_IPV4_ONLY);
	ATF_CHECK(!(f.state_mask & (1 << TCPS_LISTEN)));
	ATF_CHECK(!(f.state_mask & (1 << TCPS_TIME_WAIT)));
}

/* ================================================================
 * Negative tests: structural rejections
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(non_printable_char);
ATF_TC_BODY(non_printable_char, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=443\x01",
	    strlen("local_port=443\x01"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
	ATF_CHECK(strstr(errbuf, "non-printable") != NULL);
}

ATF_TC_WITHOUT_HEAD(unknown_directive);
ATF_TC_BODY(unknown_directive, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("foobar=123",
	    strlen("foobar=123"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
	ATF_CHECK(strstr(errbuf, "unknown directive") != NULL);
}

ATF_TC_WITHOUT_HEAD(missing_value);
ATF_TC_BODY(missing_value, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port",
	    strlen("local_port"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(empty_value);
ATF_TC_BODY(empty_value, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=",
	    strlen("local_port="), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

/* ================================================================
 * Negative tests: port rejections
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(port_zero);
ATF_TC_BODY(port_zero, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=0",
	    strlen("local_port=0"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(port_overflow);
ATF_TC_BODY(port_overflow, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=65536",
	    strlen("local_port=65536"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(port_leading_zero);
ATF_TC_BODY(port_leading_zero, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=0443",
	    strlen("local_port=0443"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(port_duplicate);
ATF_TC_BODY(port_duplicate, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=443,443",
	    strlen("local_port=443,443"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(port_too_many);
ATF_TC_BODY(port_too_many, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_port=1,2,3,4,5,6,7,8,9",
	    strlen("local_port=1,2,3,4,5,6,7,8,9"), &f, errbuf,
	    sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

/* ================================================================
 * Negative tests: state rejections
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(unknown_state);
ATF_TC_BODY(unknown_state, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("exclude=foobar",
	    strlen("exclude=foobar"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(exclude_include_conflict);
ATF_TC_BODY(exclude_include_conflict, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string(
	    "exclude=listen include_state=established",
	    strlen("exclude=listen include_state=established"),
	    &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
	ATF_CHECK(strstr(errbuf, "mutually exclusive") != NULL);
}

/* ================================================================
 * Negative tests: IPv4 rejections
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(ipv4_host_bits);
ATF_TC_BODY(ipv4_host_bits, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=10.0.0.1/24",
	    strlen("local_addr=10.0.0.1/24"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
	ATF_CHECK(strstr(errbuf, "host bits") != NULL);
}

ATF_TC_WITHOUT_HEAD(ipv4_bad_octet);
ATF_TC_BODY(ipv4_bad_octet, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=999.1.2.3",
	    strlen("local_addr=999.1.2.3"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

/* ================================================================
 * Negative tests: IPv6 rejections
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(ipv6_host_bits);
ATF_TC_BODY(ipv6_host_bits, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("remote_addr=fe80::1/10",
	    strlen("remote_addr=fe80::1/10"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(ipv6_multiple_double_colon);
ATF_TC_BODY(ipv6_multiple_double_colon, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("remote_addr=2001::1::2",
	    strlen("remote_addr=2001::1::2"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
}

/* ================================================================
 * Negative tests: conflict rejections
 * ================================================================ */

ATF_TC_WITHOUT_HEAD(both_af_flags);
ATF_TC_BODY(both_af_flags, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("ipv4_only ipv6_only",
	    strlen("ipv4_only ipv6_only"), &f, errbuf, sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
	ATF_CHECK(strstr(errbuf, "mutually exclusive") != NULL);
}

ATF_TC_WITHOUT_HEAD(ipv4_addr_ipv6_only_conflict);
ATF_TC_BODY(ipv4_addr_ipv6_only_conflict, tc)
{
	struct tcpstats_filter f;
	char errbuf[TSF_ERRBUF_SIZE];
	int err;

	memset(&f, 0, sizeof(f));
	err = tsf_parse_filter_string("local_addr=10.0.0.1 ipv6_only",
	    strlen("local_addr=10.0.0.1 ipv6_only"), &f, errbuf,
	    sizeof(errbuf));
	ATF_CHECK_EQ(EINVAL, err);
	ATF_CHECK(strstr(errbuf, "conflicts") != NULL);
}

/* ================================================================
 * Test registration
 * ================================================================ */

ATF_TP_ADD_TCS(tp)
{
	/* Positive: empty/whitespace */
	ATF_TP_ADD_TC(tp, empty_string);
	ATF_TP_ADD_TC(tp, whitespace_only);

	/* Positive: ports */
	ATF_TP_ADD_TC(tp, single_port);
	ATF_TP_ADD_TC(tp, multiple_ports);
	ATF_TP_ADD_TC(tp, max_ports);
	ATF_TP_ADD_TC(tp, both_port_directions);

	/* Positive: states */
	ATF_TP_ADD_TC(tp, exclude_listen);
	ATF_TP_ADD_TC(tp, exclude_multiple);
	ATF_TP_ADD_TC(tp, include_established);
	ATF_TP_ADD_TC(tp, case_insensitive_states);

	/* Positive: IPv4 */
	ATF_TP_ADD_TC(tp, ipv4_exact);
	ATF_TP_ADD_TC(tp, ipv4_cidr24);
	ATF_TP_ADD_TC(tp, ipv4_cidr0);

	/* Positive: IPv6 */
	ATF_TP_ADD_TC(tp, ipv6_loopback);
	ATF_TP_ADD_TC(tp, ipv6_compressed);
	ATF_TP_ADD_TC(tp, ipv6_link_local_cidr);

	/* Positive: flags/format/fields */
	ATF_TP_ADD_TC(tp, ipv4_only_flag);
	ATF_TP_ADD_TC(tp, format_full);
	ATF_TP_ADD_TC(tp, fields_multiple);
	ATF_TP_ADD_TC(tp, full_combo);

	/* Negative: structural */
	ATF_TP_ADD_TC(tp, non_printable_char);
	ATF_TP_ADD_TC(tp, unknown_directive);
	ATF_TP_ADD_TC(tp, missing_value);
	ATF_TP_ADD_TC(tp, empty_value);

	/* Negative: ports */
	ATF_TP_ADD_TC(tp, port_zero);
	ATF_TP_ADD_TC(tp, port_overflow);
	ATF_TP_ADD_TC(tp, port_leading_zero);
	ATF_TP_ADD_TC(tp, port_duplicate);
	ATF_TP_ADD_TC(tp, port_too_many);

	/* Negative: states */
	ATF_TP_ADD_TC(tp, unknown_state);
	ATF_TP_ADD_TC(tp, exclude_include_conflict);

	/* Negative: IPv4 */
	ATF_TP_ADD_TC(tp, ipv4_host_bits);
	ATF_TP_ADD_TC(tp, ipv4_bad_octet);

	/* Negative: IPv6 */
	ATF_TP_ADD_TC(tp, ipv6_host_bits);
	ATF_TP_ADD_TC(tp, ipv6_multiple_double_colon);

	/* Negative: conflicts */
	ATF_TP_ADD_TC(tp, both_af_flags);
	ATF_TP_ADD_TC(tp, ipv4_addr_ipv6_only_conflict);

	return (atf_no_error());
}
