.pl 10.0i
.po 0
.ll 7.2i
.lt 7.2i
.nr LL 7.2i
.nr LT 7.2i
.ds LF Westerlund
.ds RF [Page %]
.ds CF
.ds LH Internet Draft
.ds RH October, 1997
.ds CH Kerberos over IPv6
.hy 0
.ad l
.in 0
.ta \n(.luR
Network Working Group	Assar Westerlund
<draft-ietf-cat-krb5-ipv6.txt>	SICS
Internet-Draft	October, 1997
Expire in six months

.ce
Kerberos over IPv6

.ti 0
Status of this Memo

.in 3
This document is an Internet-Draft.  Internet-Drafts are working
documents of the Internet Engineering Task Force (IETF), its
areas, and its working groups.  Note that other groups may also
distribute working documents as Internet-Drafts.

Internet-Drafts are draft documents valid for a maximum of six
months and may be updated, replaced, or obsoleted by other
documents at any time.  It is inappropriate to use Internet-
Drafts as reference material or to cite them other than as
"work in progress."

To view the entire list of current Internet-Drafts, please check
the "1id-abstracts.txt" listing contained in the Internet-Drafts
Shadow Directories on ftp.is.co.za (Africa), ftp.nordu.net
(Europe), munnari.oz.au (Pacific Rim), ds.internic.net (US East
Coast), or ftp.isi.edu (US West Coast).

Distribution of this memo is unlimited.  Please send comments to the
<cat-ietf@mit.edu> mailing list.

.ti 0
Abstract

.in 3
This document specifies the address types and transport types
necessary for using Kerberos [RFC1510] over IPv6 [RFC1883].

.ti 0
Specification

.in 3
IPv6 addresses are 128-bit (16-octet) quantities, encoded in MSB
order.  The type of IPv6 addresses is twenty-four (24).

The following addresses (see [RFC1884]) MUST not appear in any
Kerberos packet:

the Unspecified Address
.br
the Loopback Address
.br
Link-Local addresses

IPv4-mapped IPv6 addresses MUST be represented as addresses of type 2.

Communication with the KDC over IPv6 MUST be done as in section
8.2.1 of [RFC1510].

.ti 0
Discussion

.in 3
[RFC1510] suggests using the address family constants in
<sys/socket.h> from BSD.  This cannot be done for IPv6 as these
numbers have diverged and are different on different BSD-derived
systems.  [RFC2133] does not either specify a value for AF_INET6.
Thus a value has to be decided and the implementations have to convert
between the value used in Kerberos HostAddress and the local AF_INET6.

There are a few different address types in IPv6, see [RFC1884].  Some
of these are used for quite special purposes and it makes no sense to
include them in Kerberos packets.

It is necessary to represent IPv4-mapped addresses as Internet
addresses (type 2) to be compatible with Kerberos implementations that
only support IPv4.

.ti 0
Security considerations

.in 3
This memo does not introduce any known security considerations in
addition to those mentioned in [RFC1510].

.ti 0
References

.in 3
[RFC1510] Kohl, J. and Neuman, C., "The Kerberos Network
Authentication Service (V5)", RFC 1510, September 1993.

[RFC1883] Deering, S., Hinden, R., "Internet Protocol, Version 6
(IPv6) Specification", RFC 1883, December 1995.

[RFC1884] Hinden, R., Deering, S., "IP Version 6 Addressing
Architecture", RFC 1884, December 1995.

[RFC2133] Gilligan, R., Thomson, S., Bound, J., Stevens, W., "Basic
Socket Interface Extensions for IPv6", RFC2133, April 1997.

.ti 0
Author's Address

Assar Westerlund
.br
Swedish Institute of Computer Science
.br
Box 1263
.br
S-164 29  KISTA
.br
Sweden

Phone: +46-8-7521526
.br
Fax:   +46-8-7517230
.br
EMail: assar@sics.se
