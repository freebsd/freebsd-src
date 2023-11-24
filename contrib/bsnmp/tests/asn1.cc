/*
 * Copyright (c) 2020
 *	Hartmut Brandt <harti@freebsd.org>
 *	All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * :se ts=4
 */

#include "constbuf.h"

extern "C" {
#include "asn1.h"
}

#include "catch.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>

using namespace test::literals;

template<typename T>
static std::enable_if_t<!std::is_integral_v<T>, asn_buf>
mk_asn_buf(const T &b)
{
	asn_buf abuf;

	abuf.asn_cptr = b.data();
	abuf.asn_len = b.size();

	return abuf;
}

static asn_buf
mk_asn_buf(asn_len_t len)
{
	asn_buf abuf;

	abuf.asn_ptr = new u_char[len];
	abuf.asn_len = len;

	return abuf;
}

static std::string g_errstr;

static void
save_g_errstr(const struct asn_buf *b, const char *fmt, ...)
{
	va_list ap;

	char sbuf[20000];
	va_start(ap, fmt);
	vsprintf(sbuf, fmt, ap);
	va_end(ap);

	if (b != NULL) {
		strcat(sbuf, " at");
		for (u_int i = 0; b->asn_len > i; i++)
			sprintf(sbuf + strlen(sbuf), " %02x", b->asn_cptr[i]);
	}
	strcat(sbuf, "\n");

	g_errstr = sbuf;
}

/**
 * Encapsulate an ASN.1 parse buffer and the parse header fields.
 * Constructing parses the header.
 */
struct Asn_value
{
	/** parse buffer */
	struct asn_buf buf;

	/** error from header parsing */
	asn_err err;

	/** ASN.1 tag byte */
	uint8_t type;

	/** value length */
	asn_len_t alen;

	/**
	 * Construct a parse buffer and parse the header.
	 *
	 * \tparam Tbuf		input buffer type
	 *
	 * \param ibuf		input buffer
	 */
	template<typename Tbuf>
	explicit
	Asn_value(const Tbuf &ibuf)
	  : buf {mk_asn_buf(ibuf)}, err {asn_get_header(&buf, &type, &alen)}
	{
	}
};

/**
 * Parse the ASN.1 header and check the error code. If the error is not
 * ASN_ERR_OK then check the error string.
 *
 * \tparam Tbuf		input buffer type
 *
 * \param buf		input buffer
 * \param err		expected error code (default ASN_ERR_OK)
 * \param errstr	expected error string (default empty)
 *
 * \return the parse buffer
 */
template<typename Tbuf>
static auto
check_header(const Tbuf &buf, asn_err err = ASN_ERR_OK,
  std::string_view errstr = {})
{
	g_errstr.clear();
	auto r = Asn_value(buf);
	REQUIRE(r.err == err);
	if (r.err != ASN_ERR_OK)
		REQUIRE(g_errstr == errstr);
	else
		REQUIRE(g_errstr == "");
	return r;
}

/**
 * Parse the ASN.1 header and expect it not to fail. The check the tag.
 *
 * \tparam Tbuf		input buffer type
 *
 * \param buf		input buffer
 * \param tag		expected type tag
 *
 * \return the parse buffer
 */
template<typename Tbuf>
static auto
check_header(const Tbuf &buf, uint8_t type)
{
	auto r = check_header(buf);
	REQUIRE(r.type == type);
	return r;
}

/**
 * Parse the ASN.1 header and expect it not to fail. The check the tag and
 * the length.
 *
 * \tparam Tbuf		input buffer type
 *
 * \param buf		input buffer
 * \param tag		expected type tag
 * \param alen		expected value length
 *
 * \return the parse buffer
 */
template<typename Tbuf>
static auto
check_header(const Tbuf &buf, uint8_t type, asn_len_t alen)
{
	auto r = check_header(buf);
	REQUIRE(r.type == type);
	REQUIRE(r.alen == alen);
	return r;
}

template<typename Tbuf>
static void
check_buf(const asn_buf &s, const Tbuf &exp, bool print = false)
{
	if (print) {
			for (auto c : exp)
				std::printf(":%02x", c);
			std::printf("\n");

			for (size_t i = 0; i < size(exp); i++)
				std::printf(":%02x", s.asn_ptr[i]);
			std::printf("\n");
	}
	REQUIRE(std::equal(begin(exp), end(exp), s.asn_ptr));
}

TEST_CASE("ASN.1 header parsing", "[asn1][parse]")
{
	asn_error = save_g_errstr;

	SECTION("empty buffer") {
		check_header(std::vector<u_char>{}, ASN_ERR_EOBUF,
			"no identifier for header at\n");
	}
	SECTION("tag too large") {
		check_header("x1f:06:01:7f"_cbuf, ASN_ERR_FAILED,
			"tags > 0x1e not supported (0x1f) at 1f 06 01 7f\n");
	}
	SECTION("no length field") {
		check_header("x46"_cbuf, ASN_ERR_EOBUF, "no length field at\n");
	}
	SECTION("indefinite length") {
		check_header("x46:80:02:04:06"_cbuf, ASN_ERR_FAILED,
			"indefinite length not supported at 02 04 06\n");
	}
	SECTION("long length") {
		check_header("x46:83:00:00:02:7f:12"_cbuf, ASN_ERR_FAILED,
			"long length too long (3) at 00 00 02 7f 12\n");
	}
	SECTION("truncated length field") {
		check_header("x46:82:00"_cbuf, ASN_ERR_EOBUF,
			"long length truncated at 00\n");
	}
	SECTION("correct long length") {
		check_header("x04:81:00"_cbuf, ASN_TYPE_OCTETSTRING, 0); 
#ifndef BOGUS_CVE_2019_5610_FIX
		check_header("x04:81:04:00"_cbuf, ASN_TYPE_OCTETSTRING, 4); 
		check_header("x04:81:ff:00"_cbuf, ASN_TYPE_OCTETSTRING, 255); 
#endif
		check_header("x04:82:00:00"_cbuf, ASN_TYPE_OCTETSTRING, 0); 
#ifndef BOGUS_CVE_2019_5610_FIX
		check_header("x04:82:00:80"_cbuf, ASN_TYPE_OCTETSTRING, 128); 
		check_header("x04:82:01:80"_cbuf, ASN_TYPE_OCTETSTRING, 384); 
		check_header("x04:82:ff:ff"_cbuf, ASN_TYPE_OCTETSTRING, 65535); 
#endif
	}
	SECTION("short length") {
		check_header("x04:00:00"_cbuf, ASN_TYPE_OCTETSTRING, 0); 
		check_header("x04:01:00"_cbuf, ASN_TYPE_OCTETSTRING, 1); 
#ifndef BOGUS_CVE_2019_5610_FIX
		check_header("x04:40:00"_cbuf, ASN_TYPE_OCTETSTRING, 64); 
		check_header("x04:7f:00"_cbuf, ASN_TYPE_OCTETSTRING, 127); 
#endif
	}
}

TEST_CASE("ASN.1 header building", "[asn1][build]")
{
	asn_error = save_g_errstr;

	const auto conv_err = [] (asn_len_t alen, asn_len_t vlen, uint8_t type,
	  asn_err err, std::string_view errstr) {
		auto b = mk_asn_buf(alen);
		g_errstr.clear();
		REQUIRE(asn_put_header(&b, type, vlen) == err);
		REQUIRE(g_errstr == errstr);
	};

	const auto conv = [] (asn_len_t alen, asn_len_t vlen, uint8_t type,
	  const auto &cbuf) {
		auto b = mk_asn_buf(alen);
		auto t = b;
		REQUIRE(asn_put_header(&b, type, vlen) == ASN_ERR_OK);
		REQUIRE(b.asn_len == (size_t)0);
		check_buf(t, cbuf);
	};

	SECTION("no space for tag") {
		conv_err(0, 0, ASN_TYPE_OCTETSTRING, ASN_ERR_EOBUF, "");
	}
	SECTION("no space for length") {
		conv_err(1, 0, ASN_TYPE_OCTETSTRING, ASN_ERR_EOBUF, "");
		conv_err(2, 128, ASN_TYPE_OCTETSTRING, ASN_ERR_EOBUF, "");
	}
	SECTION("bad tag") {
		conv_err(2, 0, 0x1f, ASN_ERR_FAILED,
		  "types > 0x1e not supported (0x1f)\n");
		conv_err(2, 0, 0xff, ASN_ERR_FAILED,
		  "types > 0x1e not supported (0x1f)\n");
	}
	SECTION("ok") {
		conv(2, 0, ASN_TYPE_OCTETSTRING, "x04:00"_cbuf);
	}
}

TEST_CASE("Counter64 parsing", "[asn1][parse]")
{
	asn_error = save_g_errstr;

	/**
	 * Sucessfully parse a COUNTER64 value.
	 *
	 * \param buf	buffer to parse
	 * \param xval	expected value
	 */
	const auto conv = [] (const auto &buf, uint64_t xval) {
		auto r = check_header(buf, ASN_APP_COUNTER64 | ASN_CLASS_APPLICATION);

		uint64_t val;
		REQUIRE(asn_get_counter64_raw(&r.buf, r.alen, &val) == ASN_ERR_OK);
		REQUIRE(val == xval);
	};

	/**
	 * Parse COUNTER64 with error.
	 *
	 * \param buf	buffer to parse
	 * \param err	expected error from value parser
	 * \param errstr expected error string
	 */	
	const auto conv_err = [] (const auto &buf, asn_err err,
	    std::string_view errstr) {
		auto r = check_header(buf, ASN_APP_COUNTER64 | ASN_CLASS_APPLICATION);

		g_errstr.clear();
		uint64_t val;
		REQUIRE(asn_get_counter64_raw(&r.buf, r.alen, &val) == err);
		REQUIRE(g_errstr == errstr);
	};

	SECTION("correct encoding") {

		conv("x46:01:00"_cbuf,										   0x0ULL);
		conv("x46:01:01"_cbuf,										   0x1ULL);
		conv("x46:01:7f"_cbuf,										  0x7fULL);

		conv("x46:02:00:80"_cbuf,									  0x80ULL);
		conv("x46:02:00:ff"_cbuf,									  0xffULL);
		conv("x46:02:7f:ff"_cbuf,									0x7fffULL);

		conv("x46:03:00:80:00"_cbuf,		    					0x8000ULL);
		conv("x46:03:00:ff:ff"_cbuf,		    					0xffffULL);
		conv("x46:03:7f:ff:ff"_cbuf,							  0x7fffffULL);

		conv("x46:04:00:80:00:00"_cbuf,							  0x800000ULL);
		conv("x46:04:00:ff:ff:ff"_cbuf,							  0xffffffULL);
		conv("x46:04:7f:ff:ff:ff"_cbuf,							0x7fffffffULL);

		conv("x46:05:00:80:00:00:00"_cbuf,						0x80000000ULL);
		conv("x46:05:00:ff:ff:ff:ff"_cbuf,					    0xffffffffULL);
		conv("x46:05:7f:ff:ff:ff:ff"_cbuf,					  0x7fffffffffULL);

		conv("x46:06:00:80:00:00:00:00"_cbuf,				  0x8000000000ULL);
		conv("x46:06:00:ff:ff:ff:ff:ff"_cbuf,				  0xffffffffffULL);
		conv("x46:06:7f:ff:ff:ff:ff:ff"_cbuf,				0x7fffffffffffULL);

		conv("x46:07:00:80:00:00:00:00:00"_cbuf,		    0x800000000000ULL);
		conv("x46:07:00:ff:ff:ff:ff:ff:ff"_cbuf,		    0xffffffffffffULL);
		conv("x46:07:7f:ff:ff:ff:ff:ff:ff"_cbuf,		  0x7fffffffffffffULL);

		conv("x46:08:00:80:00:00:00:00:00:00"_cbuf,		  0x80000000000000ULL);
		conv("x46:08:00:ff:ff:ff:ff:ff:ff:ff"_cbuf,		  0xffffffffffffffULL);
		conv("x46:08:7f:ff:ff:ff:ff:ff:ff:ff"_cbuf,		0x7fffffffffffffffULL);

		conv("x46:09:00:80:00:00:00:00:00:00:00"_cbuf,	0x8000000000000000ULL);
		conv("x46:09:00:ff:ff:ff:ff:ff:ff:ff:ff"_cbuf,	0xffffffffffffffffULL);
	}

	SECTION("zero length") {
		conv_err("x46:00"_cbuf, ASN_ERR_BADLEN,
			"zero-length integer at\n");
	}

	SECTION("non minimal encoding") {
		conv_err("x46:02:00:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 00\n");
		conv_err("x46:02:00:7f"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 7f\n");
		conv_err("x46:03:00:00:80"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 00 80\n");
		conv_err("x46:04:00:00:80:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 00 80 00\n");
		conv_err("x46:0a:00:00:00:00:00:00:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
			"non-minimal unsigned at 00 00 00 00 00 00 00 00 00 00\n");
		conv_err("x46:0a:00:01:00:00:00:00:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
			"non-minimal unsigned at 00 01 00 00 00 00 00 00 00 00\n");
	}

	SECTION("out of range") {
		conv_err("x46:09:01:00:00:00:00:00:00:00:00"_cbuf, ASN_ERR_RANGE,
			"unsigned too large or negative at 01 00 00 00 00 00 00 00 00\n");
		conv_err("x46:0a:01:00:00:00:00:00:00:00:00:00"_cbuf, ASN_ERR_RANGE,
			"unsigned too large or negative at 01 00 00 00 00 00 00 00 00 00\n");
		conv_err("x46:01:80"_cbuf, ASN_ERR_RANGE,
			"unsigned too large or negative at 80\n");
		conv_err("x46:02:80:00"_cbuf, ASN_ERR_RANGE,
			"unsigned too large or negative at 80 00\n");
		conv_err("x46:03:80:00:00"_cbuf, ASN_ERR_RANGE,
			"unsigned too large or negative at 80 00 00\n");
	}

#ifndef	BOGUS_CVE_2019_5610_FIX
	SECTION("truncated value") {
		conv_err("x46:02:00"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 00\n");
		conv_err("x46:09:00:80:00:00:00"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 00 80 00 00 00\n");
		conv_err("x46:09:00:ff:ff:ff:ff:ff:ff:ff"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 00 ff ff ff ff ff ff ff\n");
	}
#endif
}

TEST_CASE("Counter64 building", "[asn1][build]")
{
	asn_error = save_g_errstr;

	const auto conv = [] (asn_len_t alen, uint64_t val, const auto &buf) {
		auto b = mk_asn_buf(alen);
		auto s = b;
		REQUIRE(asn_put_counter64(&b, val) == ASN_ERR_OK);
		REQUIRE(b.asn_len == (size_t)0);
		check_buf(s, buf);
	};

	const auto conv_err = [] (asn_len_t alen, uint64_t val, asn_err err,
	  std::string_view errstr) {
		auto b = mk_asn_buf(alen);
		g_errstr.clear();
		REQUIRE(asn_put_counter64(&b, val) == err);
		REQUIRE(g_errstr == errstr);
	};

	conv(3,  0x0, "x46:01:00"_cbuf);
	conv(3,  0x1, "x46:01:01"_cbuf);
	conv(3, 0x7f, "x46:01:7f"_cbuf);

	conv(4,   0x80, "x46:02:00:80"_cbuf);
	conv(4,   0xff, "x46:02:00:ff"_cbuf);
	conv(4, 0x7fff, "x46:02:7f:ff"_cbuf);

	conv(5,   0x8000, "x46:03:00:80:00"_cbuf);
	conv(5,   0xffff, "x46:03:00:ff:ff"_cbuf);
	conv(5, 0x7fffff, "x46:03:7f:ff:ff"_cbuf);

	conv(6,   0x800000, "x46:04:00:80:00:00"_cbuf);
	conv(6,   0xffffff, "x46:04:00:ff:ff:ff"_cbuf);
	conv(6, 0x7fffffff, "x46:04:7f:ff:ff:ff"_cbuf);

	conv(7,   0x80000000, "x46:05:00:80:00:00:00"_cbuf);
	conv(7,   0xffffffff, "x46:05:00:ff:ff:ff:ff"_cbuf);
	conv(7, 0x7fffffffff, "x46:05:7f:ff:ff:ff:ff"_cbuf);

	conv(8,   0x8000000000, "x46:06:00:80:00:00:00:00"_cbuf);
	conv(8,   0xffffffffff, "x46:06:00:ff:ff:ff:ff:ff"_cbuf);
	conv(8, 0x7fffffffffff, "x46:06:7f:ff:ff:ff:ff:ff"_cbuf);

	conv(9,   0x800000000000, "x46:07:00:80:00:00:00:00:00"_cbuf);
	conv(9,   0xffffffffffff, "x46:07:00:ff:ff:ff:ff:ff:ff"_cbuf);
	conv(9, 0x7fffffffffffff, "x46:07:7f:ff:ff:ff:ff:ff:ff"_cbuf);

	conv(10,   0x80000000000000, "x46:08:00:80:00:00:00:00:00:00"_cbuf);
	conv(10,   0xffffffffffffff, "x46:08:00:ff:ff:ff:ff:ff:ff:ff"_cbuf);
	conv(10, 0x7fffffffffffffff, "x46:08:7f:ff:ff:ff:ff:ff:ff:ff"_cbuf);

	conv(11,   0x8000000000000000, "x46:09:00:80:00:00:00:00:00:00:00"_cbuf);
	conv(11,   0xffffffffffffffff, "x46:09:00:ff:ff:ff:ff:ff:ff:ff:ff"_cbuf);

	SECTION("empty buffer") {
		conv_err(0, 0, ASN_ERR_EOBUF, "");
	}
	SECTION("buffer too short for length field") {
		conv_err(1, 0, ASN_ERR_EOBUF, "");
	}
	SECTION("buffer too short") {
		conv_err(2, 0, ASN_ERR_EOBUF, "");
		conv_err(3, 0x80, ASN_ERR_EOBUF, "");
		conv_err(4, 0x8000, ASN_ERR_EOBUF, "");
		conv_err(5, 0x800000, ASN_ERR_EOBUF, "");
		conv_err(6, 0x80000000, ASN_ERR_EOBUF, "");
		conv_err(7, 0x8000000000, ASN_ERR_EOBUF, "");
		conv_err(8, 0x800000000000, ASN_ERR_EOBUF, "");
		conv_err(9, 0x80000000000000, ASN_ERR_EOBUF, "");
		conv_err(10, 0x8000000000000000, ASN_ERR_EOBUF, "");
	}
}

TEST_CASE("Unsigned32 parsing", "[asn1][parse]")
{
	asn_error = save_g_errstr;

	/**
	 * Sucessfully parse a COUNTER value.
	 *
	 * \param buf	buffer to parse
	 * \param xval	expected value
	 */
	const auto conv = [] (const auto &buf, uint32_t xval) {
		auto r = check_header(buf, ASN_APP_COUNTER | ASN_CLASS_APPLICATION);

		uint32_t val;
		REQUIRE(asn_get_uint32_raw(&r.buf, r.alen, &val) == ASN_ERR_OK);
		REQUIRE(val == xval);
	};

	/**
	 * Parse COUNTER with error.
	 *
	 * \param buf	buffer to parse
	 * \param err	expected error from value parser
	 * \param errstr expected error string
	 */	
	const auto conv_err = [] (const auto &buf, asn_err err,
	    std::string_view errstr) {
		auto r = check_header(buf, ASN_APP_COUNTER | ASN_CLASS_APPLICATION);

		g_errstr.clear();
		uint32_t val;
		REQUIRE(asn_get_uint32_raw(&r.buf, r.alen, &val) == err);
		REQUIRE(g_errstr == errstr);
	};

	SECTION("correct encoding") {
		conv("x41:01:00"_cbuf,			   0x0U);
		conv("x41:01:01"_cbuf,			   0x1U);
		conv("x41:01:7f"_cbuf,			   0x7fU);

		conv("x41:02:00:80"_cbuf,		   0x80U);
		conv("x41:02:00:ff"_cbuf,		   0xffU);
		conv("x41:02:7f:ff"_cbuf,		   0x7fffU);

		conv("x41:03:00:80:00"_cbuf,	   0x8000U);
		conv("x41:03:00:ff:ff"_cbuf,	   0xffffU);
		conv("x41:03:7f:ff:ff"_cbuf,	   0x7fffffU);

		conv("x41:04:00:80:00:00"_cbuf,	   0x800000U);
		conv("x41:04:00:ff:ff:ff"_cbuf,	   0xffffffU);
		conv("x41:04:7f:ff:ff:ff"_cbuf,	   0x7fffffffU);

		conv("x41:05:00:80:00:00:00"_cbuf, 0x80000000U);
		conv("x41:05:00:ff:ff:ff:ff"_cbuf, 0xffffffffU);
	}
	SECTION("zero length") {

		conv_err("x41:00"_cbuf, ASN_ERR_BADLEN,
			"zero-length integer at\n");
	}

	SECTION("non minimal encoding") {
		conv_err("x41:02:00:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 00\n");
		conv_err("x41:02:00:7f"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 7f\n");
		conv_err("x41:03:00:00:80"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 00 80\n");
		conv_err("x41:04:00:00:80:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal unsigned at 00 00 80 00\n");
		conv_err("x41:06:00:00:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
			"non-minimal unsigned at 00 00 00 00 00 00\n");
		conv_err("x41:06:00:01:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
			"non-minimal unsigned at 00 01 00 00 00 00\n");
	}

	SECTION("out of range") {
		conv_err("x41:05:01:00:00:00:00"_cbuf,
			ASN_ERR_RANGE, "uint32 too large 4294967296 at\n");
		conv_err("x41:06:01:00:00:00:00:00"_cbuf,
			ASN_ERR_RANGE, "uint32 too large 1099511627776 at\n");
		conv_err("x41:01:80"_cbuf,
			ASN_ERR_RANGE, "unsigned too large or negative at 80\n");
		conv_err("x41:02:80:00"_cbuf,
			ASN_ERR_RANGE, "unsigned too large or negative at 80 00\n");
		conv_err("x41:03:80:00:00"_cbuf,
			ASN_ERR_RANGE, "unsigned too large or negative at 80 00 00\n");
	}

#ifndef	BOGUS_CVE_2019_5610_FIX
	SECTION("truncated value") {
		conv_err("x41:01"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at\n");
		conv_err("x41:02:01"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 01\n");
		conv_err("x41:05:00:80:"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 00 80\n");
		conv_err("x41:05:00:ff:ff:ff"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 00 ff ff ff\n");
	}
#endif
}

TEST_CASE("Unsigned32 building", "[asn1][build]")
{
	asn_error = save_g_errstr;

	const auto conv = [] (asn_len_t alen, uint32_t val, const auto &buf) {
		auto b = mk_asn_buf(alen);
		auto s = b;
		REQUIRE(asn_put_uint32(&b, ASN_APP_COUNTER, val) == ASN_ERR_OK);
		REQUIRE(b.asn_len == (size_t)0);
		check_buf(s, buf);
	};

	const auto conv_err = [] (asn_len_t alen, uint32_t val, asn_err err,
	  std::string_view errstr) {
		auto b = mk_asn_buf(alen);
		g_errstr.clear();
		REQUIRE(asn_put_uint32(&b, ASN_APP_COUNTER, val) == err);
		REQUIRE(g_errstr == errstr);
	};

	conv(3,  0x0, "x41:01:00"_cbuf);
	conv(3,  0x1, "x41:01:01"_cbuf);
	conv(3, 0x7f, "x41:01:7f"_cbuf);

	conv(4,   0x80, "x41:02:00:80"_cbuf);
	conv(4,   0xff, "x41:02:00:ff"_cbuf);
	conv(4, 0x7fff, "x41:02:7f:ff"_cbuf);

	conv(5,   0x8000, "x41:03:00:80:00"_cbuf);
	conv(5,   0xffff, "x41:03:00:ff:ff"_cbuf);
	conv(5, 0x7fffff, "x41:03:7f:ff:ff"_cbuf);

	conv(6,   0x800000, "x41:04:00:80:00:00"_cbuf);
	conv(6,   0xffffff, "x41:04:00:ff:ff:ff"_cbuf);
	conv(6, 0x7fffffff, "x41:04:7f:ff:ff:ff"_cbuf);

	conv(7,   0x80000000, "x41:05:00:80:00:00:00"_cbuf);
	conv(7,   0xffffffff, "x41:05:00:ff:ff:ff:ff"_cbuf);

	SECTION("empty buffer") {
		conv_err(0, 0, ASN_ERR_EOBUF, "");
	}
	SECTION("buffer too short for length field") {
		conv_err(1, 0, ASN_ERR_EOBUF, "");
	}
	SECTION("buffer too short") {
		conv_err(2, 0, ASN_ERR_EOBUF, "");
		conv_err(3, 0x80, ASN_ERR_EOBUF, "");
		conv_err(4, 0x8000, ASN_ERR_EOBUF, "");
		conv_err(5, 0x800000, ASN_ERR_EOBUF, "");
		conv_err(6, 0x80000000, ASN_ERR_EOBUF, "");
	}
}

TEST_CASE("Integer parsing", "[asn1][parse]")
{
	asn_error = save_g_errstr;

	/**
	 * Sucessfully parse a INTEGER value.
	 *
	 * \param buf	buffer to parse
	 * \param xval	expected value
	 */
	const auto conv = [] (const auto &buf, int32_t xval) {
		auto r = check_header(buf, ASN_TYPE_INTEGER);

		int32_t val;
		REQUIRE(asn_get_integer_raw(&r.buf, r.alen, &val) == ASN_ERR_OK);
		REQUIRE(val == xval);
	};

	/**
	 * Parse INTEGER with error.
	 *
	 * \param buf	buffer to parse
	 * \param err	expected error from value parser
	 * \param errstr expected error string
	 */	
	const auto conv_err = [] (const auto &buf, asn_err err,
	    std::string_view errstr) {
		auto r = check_header(buf, ASN_TYPE_INTEGER);

		g_errstr.clear();
		int32_t val;
		REQUIRE(asn_get_integer_raw(&r.buf, r.alen, &val) == err);
		REQUIRE(g_errstr == errstr);
	};

	SECTION("correct encoding") {
		conv("x02:01:00"_cbuf,					   0x0);
		conv("x02:01:01"_cbuf,					   0x1);
		conv("x02:01:7f"_cbuf,					  0x7f);
		conv("x02:01:ff"_cbuf,					  -0x1);
		conv("x02:01:80"_cbuf,					 -0x80);

		conv("x02:02:00:80"_cbuf,				  0x80);
		conv("x02:02:00:ff"_cbuf,				  0xff);
		conv("x02:02:7f:ff"_cbuf,				0x7fff);
		conv("x02:02:ff:7f"_cbuf,				 -0x81);
		conv("x02:02:ff:01"_cbuf,				 -0xff);
		conv("x02:02:ff:00"_cbuf,				-0x100);
		conv("x02:02:80:00"_cbuf,			   -0x8000);

		conv("x02:03:00:80:00"_cbuf,			 0x8000);
		conv("x02:03:00:ff:ff"_cbuf,			 0xffff);
		conv("x02:03:7f:ff:ff"_cbuf,		   0x7fffff);
		conv("x02:03:ff:7f:ff"_cbuf,			-0x8001);
		conv("x02:03:ff:00:01"_cbuf,			-0xffff);
		conv("x02:03:ff:00:00"_cbuf,		   -0x10000);
		conv("x02:03:80:00:00"_cbuf,		  -0x800000);

		conv("x02:04:00:80:00:00"_cbuf,		   0x800000);
		conv("x02:04:00:ff:ff:ff"_cbuf,		   0xffffff);
		conv("x02:04:7f:ff:ff:ff"_cbuf,		 0x7fffffff);
		conv("x02:04:ff:7f:ff:ff"_cbuf,		  -0x800001);
		conv("x02:04:ff:00:00:01"_cbuf,		  -0xffffff);
		conv("x02:04:ff:00:00:00"_cbuf,		 -0x1000000);
		conv("x02:04:80:00:00:00"_cbuf,		-0x80000000);
	}

	SECTION("zero length") {
		conv_err("x02:00"_cbuf, ASN_ERR_BADLEN,
			"zero-length integer at\n");
	}
	SECTION("too long") {
		conv_err("x02:05:01:02:03:04:05"_cbuf, ASN_ERR_BADLEN,
			"integer too long at\n");
	}

	SECTION("non minimal encoding") {
		conv_err("x02:02:00:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at 00 00\n");
		conv_err("x02:02:00:7f"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at 00 7f\n");
		conv_err("x02:03:00:00:80"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at 00 00 80\n");
		conv_err("x02:04:00:00:80:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at 00 00 80 00\n");
		conv_err("x02:06:00:00:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
			"non-minimal integer at 00 00 00 00 00 00\n");
		conv_err("x02:06:00:01:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
			"non-minimal integer at 00 01 00 00 00 00\n");
		conv_err("x02:02:ff:80"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff 80\n");
		conv_err("x02:02:ff:ff"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff ff\n");
		conv_err("x02:03:ff:80:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff 80 00\n");
		conv_err("x02:03:ff:ff:ff"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff ff ff\n");
		conv_err("x02:04:ff:80:00:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff 80 00 00\n");
		conv_err("x02:04:ff:ff:ff:ff"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff ff ff ff\n");
		conv_err("x02:06:ff:80:00:00:00:00"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff 80 00 00 00 00\n");
		conv_err("x02:06:ff:ff:ff:ff:ff:ff"_cbuf, ASN_ERR_BADLEN,
		    "non-minimal integer at ff ff ff ff ff ff\n");
	}

#ifndef	BOGUS_CVE_2019_5610_FIX
	SECTION("truncated value") {
		conv_err("x02:01"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at\n");
		conv_err("x02:02:ff"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at ff\n");
		conv_err("x02:05:ff:00:03:01"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at ff 00 03 01\n");
		conv_err("x02:04:7f:ff:"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 7f ff\n");
		conv_err("x02:04:80:00:00"_cbuf, ASN_ERR_EOBUF,
			"truncated integer at 80 00 00\n");
	}
#endif
}

TEST_CASE("Integer32 building", "[asn1][build]")
{
	asn_error = save_g_errstr;

	const auto conv = [] (asn_len_t alen, int32_t val, const auto &buf) {
		auto b = mk_asn_buf(alen);
		auto s = b;
		REQUIRE(asn_put_integer(&b, val) == ASN_ERR_OK);
		REQUIRE(b.asn_len == (size_t)0);
		check_buf(s, buf);
	};

	const auto conv_err = [] (asn_len_t alen, int32_t val, asn_err err,
	  std::string_view errstr) {
		auto b = mk_asn_buf(alen);
		g_errstr.clear();
		REQUIRE(asn_put_integer(&b, val) == err);
		REQUIRE(g_errstr == errstr);
	};

	conv(3,			 0x0, "x02:01:00"_cbuf);
	conv(3,			 0x1, "x02:01:01"_cbuf);
	conv(3,			0x7f, "x02:01:7f"_cbuf);
	conv(3,			-0x1, "x02:01:ff"_cbuf);
	conv(3,		   -0x80, "x02:01:80"_cbuf);

	conv(4,			0x80, "x02:02:00:80"_cbuf);
	conv(4,			0xff, "x02:02:00:ff"_cbuf);
	conv(4,		  0x7fff, "x02:02:7f:ff"_cbuf);
	conv(4,		   -0x81, "x02:02:ff:7f"_cbuf);
	conv(4,		   -0xff, "x02:02:ff:01"_cbuf);
	conv(4,		  -0x100, "x02:02:ff:00"_cbuf);
	conv(4,		 -0x8000, "x02:02:80:00"_cbuf);

	conv(5,		 0x8000, "x02:03:00:80:00"_cbuf);
	conv(5,		 0xffff, "x02:03:00:ff:ff"_cbuf);
	conv(5,	   0x7fffff, "x02:03:7f:ff:ff"_cbuf);
	conv(5,		-0x8001, "x02:03:ff:7f:ff"_cbuf);
	conv(5,		-0xffff, "x02:03:ff:00:01"_cbuf);
	conv(5,	   -0x10000, "x02:03:ff:00:00"_cbuf);
	conv(5,	  -0x800000, "x02:03:80:00:00"_cbuf);

	conv(6,	   0x800000, "x02:04:00:80:00:00"_cbuf);
	conv(6,	   0xffffff, "x02:04:00:ff:ff:ff"_cbuf);
	conv(6,	 0x7fffffff, "x02:04:7f:ff:ff:ff"_cbuf);
	conv(6,	  -0x800001, "x02:04:ff:7f:ff:ff"_cbuf);
	conv(6,	  -0xffffff, "x02:04:ff:00:00:01"_cbuf);
	conv(6,	 -0x1000000, "x02:04:ff:00:00:00"_cbuf);
	conv(6,	-0x80000000, "x02:04:80:00:00:00"_cbuf);

	SECTION("empty buffer") {
		conv_err(0, 0, ASN_ERR_EOBUF, "");
	}
	SECTION("buffer too short for length field") {
		conv_err(1, 0, ASN_ERR_EOBUF, "");
	}
	SECTION("buffer too short") {
		conv_err(2, 0, ASN_ERR_EOBUF, "");
		conv_err(3, 0xff, ASN_ERR_EOBUF, "");
		conv_err(4, 0xffff, ASN_ERR_EOBUF, "");
		conv_err(5, 0xffffff, ASN_ERR_EOBUF, "");
		conv_err(5, 0x7fffffff, ASN_ERR_EOBUF, "");
		conv_err(2, -0x80, ASN_ERR_EOBUF, "");
		conv_err(3, -0x8000, ASN_ERR_EOBUF, "");
		conv_err(4, -0x800000, ASN_ERR_EOBUF, "");
		conv_err(5, -0x80000000, ASN_ERR_EOBUF, "");
	}
}

TEST_CASE("Oid parsing", "[asn1][parse]")
{
	asn_error = save_g_errstr;

	/**
	 * Sucessfully parse a INTEGER value.
	 *
	 * \param buf	buffer to parse
	 * \param xval	expected value
	 */
	const auto conv = [] (const auto &buf, const asn_oid &xval) {
		auto r = check_header(buf, ASN_TYPE_OBJID);

		struct asn_oid val;
		REQUIRE(asn_get_objid_raw(&r.buf, r.alen, &val) == ASN_ERR_OK);
		REQUIRE(asn_compare_oid(&val, &xval) == 0);
	};

	/**
	 * Parse INTEGER with error.
	 *
	 * \param buf	buffer to parse
	 * \param err	expected error from value parser
	 * \param errstr expected error string
	 */	
	const auto conv_err = [] (const auto &buf, asn_err err,
	    std::string_view errstr) {
		auto r = check_header(buf, ASN_TYPE_OBJID);

		g_errstr.clear();
		struct asn_oid val;
		REQUIRE(asn_get_objid_raw(&r.buf, r.alen, &val) == err);
		REQUIRE(g_errstr == errstr);
	};

	conv("x06:01:00"_cbuf, asn_oid {2, {0, 0}});
	conv("x06:01:28"_cbuf, asn_oid {2, {1, 0}});
	conv("x06:01:50"_cbuf, asn_oid {2, {2, 0}});

	conv("x06:01:27"_cbuf, asn_oid {2, {0, 39}});
	conv("x06:01:4f"_cbuf, asn_oid {2, {1, 39}});
	conv("x06:01:7f"_cbuf, asn_oid {2, {2, 47}});

	conv("x06:02:81:00"_cbuf, asn_oid {2, {2, 48}});
	conv("x06:02:ff:7f"_cbuf, asn_oid {2, {2, 16303}});
	conv("x06:03:ff:ff:7f"_cbuf, asn_oid {2, {2, 2097071}});
	conv("x06:04:ff:ff:ff:7f"_cbuf, asn_oid {2, {2, 268435375}});
	conv("x06:05:8f:ff:ff:ff:7f"_cbuf, asn_oid {2, {2, 4294967215}});

	/* maximum OID */
	conv("x06:82:02:7b:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f"_cbuf, asn_oid {128, {
		2, 4294967215, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
	}});

	SECTION("truncated OID") {
#ifndef BOGUS_CVE_2019_5610_FIX
		conv_err("x06:02:01"_cbuf, ASN_ERR_EOBUF,
			"truncated OBJID at 01\n");
#endif
		conv_err("x06:01:8f"_cbuf, ASN_ERR_EOBUF,
			"unterminated subid at\n");
		conv_err("x06:04:07:7f:82:8e"_cbuf, ASN_ERR_EOBUF,
			"unterminated subid at\n");
	}
	SECTION("short OID") {
		conv_err("x06:00"_cbuf, ASN_ERR_BADLEN,
			"short OBJID at\n");
	}
	SECTION("too long") {
		conv_err("x06:81:80:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c:7c"_cbuf, ASN_ERR_BADLEN, "OID too long (128) at 7c\n");
	}
	SECTION("subid too large") {
		conv_err("x06:06:20:90:82:83:84:75"_cbuf, ASN_ERR_RANGE,
			"OID subid too larger at 75\n");
	}
}

TEST_CASE("Objid building", "[asn1][build]")
{
	asn_error = save_g_errstr;

	const auto conv = [] (asn_len_t alen, const asn_oid &val, const auto &buf) {
		auto b = mk_asn_buf(alen);
		auto s = b;
		REQUIRE(asn_put_objid(&b, &val) == ASN_ERR_OK);
		REQUIRE(b.asn_len == (size_t)0);
		check_buf(s, buf);
	};

	const auto conv_err = [] (asn_len_t alen, const asn_oid &val, asn_err err,
	  std::string_view errstr) {
		auto b = mk_asn_buf(alen);
		g_errstr.clear();
		REQUIRE(asn_put_objid(&b, &val) == err);
		REQUIRE(g_errstr == errstr);
	};

	conv(3, asn_oid {2, {0, 0}}, "x06:01:00"_cbuf);
	conv(3, asn_oid {2, {1, 0}}, "x06:01:28"_cbuf);
	conv(3, asn_oid {2, {2, 0}}, "x06:01:50"_cbuf);

	conv(3, asn_oid {2, {0, 39}}, "x06:01:27"_cbuf);
	conv(3, asn_oid {2, {1, 39}}, "x06:01:4f"_cbuf);
	conv(3, asn_oid {2, {2, 47}}, "x06:01:7f"_cbuf);

	conv(4, asn_oid {2, {2, 48}}, "x06:02:81:00"_cbuf);
	conv(4, asn_oid {2, {2, 16303}}, "x06:02:ff:7f"_cbuf);
	conv(5, asn_oid {2, {2, 2097071}}, "x06:03:ff:ff:7f"_cbuf);
	conv(6, asn_oid {2, {2, 268435375}}, "x06:04:ff:ff:ff:7f"_cbuf);
	conv(7, asn_oid {2, {2, 4294967215}}, "x06:05:8f:ff:ff:ff:7f"_cbuf);

	SECTION("sub-id too large") {
		conv_err(3, asn_oid {2, {3, 0}}, ASN_ERR_RANGE,
			"oid out of range (3,0)\n");
		conv_err(3, asn_oid {2, {0, 40}}, ASN_ERR_RANGE,
			"oid out of range (0,40)\n");
		conv_err(3, asn_oid {2, {1, 40}}, ASN_ERR_RANGE,
			"oid out of range (1,40)\n");
		conv_err(3, asn_oid {2, {2, 4294967216}}, ASN_ERR_RANGE,
			"oid out of range (2,4294967216)\n");
	}
	SECTION("oid too long") {
		conv_err(200, asn_oid {129, {}}, ASN_ERR_RANGE,
			"oid too long 129\n");
	}
	SECTION("oid too short") {
		conv_err(3, asn_oid {0, {}}, ASN_ERR_RANGE,
			"short oid\n");
		conv_err(3, asn_oid {1, {0}}, ASN_ERR_RANGE,
			"short oid\n");
		conv_err(3, asn_oid {1, {3}}, ASN_ERR_RANGE,
			"oid[0] too large (3)\n");
	}

	/* maximum OID */
	conv(5 * (128 - 1) + 4, asn_oid {128, {
		2, 4294967215, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
		4294967295, 4294967295, 4294967295, 4294967295, 
	}}, "x06:82:02:7b:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f:8f:ff:ff:ff:7f"_cbuf);
}

/* loop tests */
