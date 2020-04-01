#include <iostream>
#include <string>

extern "C" {
#include "asn1.h"
#include "snmp.h"
#include "snmpclient.h"
};

#include "catch.hpp"

using namespace std::string_literals;

static inline int
try_parse(struct snmp_client *sc, const char *str)
{
	const int r = snmp_parse_server(sc, str);
	if (false && r != 0)
		std::cout << "snmp_parse_server: " << sc->error << "\n";
	return r;

}

TEST_CASE("snmp_parse_server: empty string", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	REQUIRE(try_parse(&sc, "") == 0);
	REQUIRE(sc.trans == SNMP_TRANS_UDP);
	REQUIRE(sc.chost == ""s);
	REQUIRE(sc.cport == "snmp"s);
	REQUIRE(sc.read_community == "public"s);
	REQUIRE(sc.write_community == "private"s);
}

TEST_CASE("snmp_parse_server: hostname only", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("simple name without special characters") {
		const auto str = "somehost"s;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == str);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("complex host name without special characters") {
		const auto str = "some.host.domain"s;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == str);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("complex host name with special characters") {
		const auto str = "some-mul.host-32.domain."s;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == str);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("relative path name") {
		const auto str = "foo/bar"s;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_LOC_DGRAM);
		REQUIRE(sc.chost == str);
		REQUIRE(sc.cport == ""s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("absolute path name") {
		const auto str = "/foo/bar"s;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_LOC_DGRAM);
		REQUIRE(sc.chost == str);
		REQUIRE(sc.cport == ""s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
}

TEST_CASE("snmp_parse_server: ipv6 address only", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("in6_addr_any") {
		const auto host = "::"s;
		const auto str = "[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("localhost") {
		const auto host = "::1"s;
		const auto str = "[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("link local address") {
		const auto host = "fc00:0:12::1"s;
		const auto str = "[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("illegal address (bad character)") {
		const auto host = "fc00:0:1x::1"s;
		const auto str = "[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == host + ": Name does not resolve");
	}
	SECTION("illegal address (two double colons)") {
		const auto host = "fc00::0:12::1"s;
		const auto str = "[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == host + ": Name does not resolve");
	}
	SECTION("illegal address (two many colons)") {
		const auto host = "1:2:3:4:5:6:7:8:9"s;
		const auto str = "[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == host + ": Name does not resolve");
	}
	SECTION("ipv6 address and junk") {
		const auto host = "::"s;
		const auto str = "[" + host + "]" + "xxx";
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == "junk at end of server specification 'xxx'"s);
	}
}

TEST_CASE("snmp_parse_server: hostname and port", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("simple name and numeric port") {
		const auto host = "somehost"s;
		const auto port = "10007"s;
		const auto str = host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("simple name and string port") {
		const auto host = "somehost"s;
		const auto port = "telnet"s;
		const auto str = host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("name with embedded colon and numeric port") {
		const auto host = "somehost:foo"s;
		const auto port = "10007"s;
		const auto str = host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("FQDN with embedded colon and numeric port") {
		const auto host = "bla.blub:foo.baz."s;
		const auto port = "10007"s;
		const auto str = host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("simple name and empty port") {
		const auto host = "somehost"s;
		const auto port = ""s;
		const auto str = host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == "empty port name"s);
	}
}

TEST_CASE("snmp_parse_server: ipv6 and port", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("ANY address and numeric port") {
		const auto host = "::"s;
		const auto port = "10007"s;
		const auto str = "[" + host + "]:" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("localhost address and string port") {
		const auto host = "::1"s;
		const auto port = "snmp"s;
		const auto str = "[" + host + "]:" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("some address name and empty port") {
		const auto host = "fc00:00:01::2:3"s;
		const auto port = ""s;
		const auto str = "[" + host + "]:" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == "empty port name"s);
	}
}

TEST_CASE("snmp_parse_server: IPv4 address only", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("single octet address") {
		const auto host = "127"s;
		const auto str = host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("two octet address") {
		const auto host = "127.1"s;
		const auto str = host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("three octet address") {
		const auto host = "127.23.1"s;
		const auto str = host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("four octet address") {
		const auto host = "127.18.23.1"s;
		const auto str = host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("four octet octal address") {
		const auto host = "0300.077.0377.01"s;
		const auto str = host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("four octet hex address") {
		const auto host = "x80.x12.xff.x1"s;
		const auto str = host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
}

TEST_CASE("snmp_parse_server: transport and hostname", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("udp and host") {
		const auto trans = "udp"s;
		const auto host = "somehost"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("udp and ipv4 address") {
		const auto trans = "udp"s;
		const auto host = "240.0.1.2"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("udp6 and host") {
		const auto trans = "udp6"s;
		const auto host = "somehost"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("udp6 and ipv6 address") {
		const auto trans = "udp6"s;
		const auto host = "fec0:0:2::17"s;
		const auto str = trans + "::[" + host + "]";
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("dgram and host") {
		const auto trans = "dgram"s;
		const auto host = "somehost"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_LOC_DGRAM);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == ""s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("stream and host") {
		const auto trans = "stream"s;
		const auto host = "somehost"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_LOC_STREAM);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == ""s);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("unknown transport and host") {
		const auto trans = "foo"s;
		const auto host = "somehost"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == "unknown transport specifier '" + trans + "'");
	}
	SECTION("empty transport and host") {
		const auto trans = ""s;
		const auto host = "somehost"s;
		const auto str = trans + "::" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == -1);
		REQUIRE(sc.error == "empty transport specifier"s);
	}
}

TEST_CASE("snmp_parse_server: transport, host and port", "[snmp_parse_server]") {
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("udp, host and port") {
		const auto trans = "udp"s;
		const auto host = "somehost"s;
		const auto port = "ssh"s;
		const auto str = trans + "::" + host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("udp, host with colon and port") {
		const auto trans = "udp"s;
		const auto host = "somehost:foo"s;
		const auto port = "ssh"s;
		const auto str = trans + "::" + host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("udp and port") {
		const auto trans = "udp"s;
		const auto host = ""s;
		const auto port = "ssh"s;
		const auto str = trans + "::" + host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
	SECTION("udp6, ipv6 and port") {
		const auto trans = "udp6"s;
		const auto host = "::1:2"s;
		const auto port = "ssh"s;
		const auto str = trans + "::[" + host + "]:" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == "public"s);
		REQUIRE(sc.write_community == "private"s);
	}
}

TEST_CASE("snmp_parse_server: community and host", "[snmp_parse_server]")
{
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("community and host") {
		const auto comm = "public"s;
		const auto host = "server.com"s;
		const auto str = comm + "@" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == comm);
		REQUIRE(sc.write_community == comm);
	}
	SECTION("community with @ and host") {
		const auto comm = "public@bla"s;
		const auto host = "server.com"s;
		const auto str = comm + "@" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == comm);
		REQUIRE(sc.write_community == comm);
	}
	SECTION("empty community and host") {
		const auto comm = ""s;
		const auto host = "server.com"s;
		const auto str = comm + "@" + host;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == "snmp"s);
		REQUIRE(sc.read_community == comm);
		REQUIRE(sc.write_community == comm);
	}
}

TEST_CASE("snmp_parse_server: transport, community, host and port", "[snmp_parse_server]")
{
	struct snmp_client sc;
	snmp_client_init(&sc);

	SECTION("transport, community, host and numeric port") {
		const auto trans = "udp6"s;
		const auto comm = "public"s;
		const auto host = "server.com"s;
		const auto port = "65000"s;
		const auto str = trans + "::" + comm + "@" + host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == comm);
		REQUIRE(sc.write_community == comm);
	}
	SECTION("transport, community, ipv4 and symbolic port") {
		const auto trans = "udp6"s;
		const auto comm = "public"s;
		const auto host = "127.1"s;
		const auto port = "ftp"s;
		const auto str = trans + "::" + comm + "@" + host + ":" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == comm);
		REQUIRE(sc.write_community == comm);
	}
	SECTION("transport, community, ipv6 and symbolic port") {
		const auto trans = "udp"s;
		const auto comm = "public"s;
		const auto host = "fe80::1:2"s;
		const auto port = "ftp"s;
		const auto str = trans + "::" + comm + "@[" + host + "]:" + port;
		REQUIRE(try_parse(&sc, str.c_str()) == 0);
		REQUIRE(sc.trans == SNMP_TRANS_UDP6);
		REQUIRE(sc.chost == host);
		REQUIRE(sc.cport == port);
		REQUIRE(sc.read_community == comm);
		REQUIRE(sc.write_community == comm);
	}
}
