.pl 10.0i
.po 0
.ll 7.2i
.lt 7.2i
.nr LL 7.2i
.nr LT 7.2i
.ds LF Westerlund, Danielsson
.ds RF [Page %]
.ds CF
.ds LH Internet Draft
.ds RH November, 1997
.ds CH Kerberos over TCP
.hy 0
.ad l
.in 0
.ta \n(.luR
.nf
Network Working Group	Assar Westerlund
<draft-ietf-cat-krb5-tcp.txt>	SICS
Internet-Draft	Johan Danielsson
November, 1997	PDC, KTH
Expire in six months
.fi

.ce
Kerberos over TCP

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
This document specifies how the communication should be done between a
client and a KDC using Kerberos [RFC1510] with TCP as the transport
protocol.

.ti 0
Specification

This draft specifies an extension to section 8.2.1 of RFC1510. 

A Kerberos server MAY accept requests on TCP port 88 (decimal).

The data sent from the client to the KDC should consist of 4 bytes
containing the length, in network byte order, of the Kerberos request,
followed by the request (AS-REQ or TGS-REQ) itself.  The reply from
the KDC should consist of the length of the reply packet (4 bytes,
network byte order) followed by the packet itself (AS-REP, TGS-REP, or
KRB-ERROR).

.nf
C->S: Open connection to TCP port 88 at the server
C->S: length of request
C->S: AS-REQ or TGS-REQ
S->C: length of reply
S->C: AS-REP, TGS-REP, or KRB-ERROR
.fi

.ti 0
Discussion

Even though the preferred way of sending kerberos packets is over UDP
there are several occasions when it's more practical to use TCP.

Mainly, it's usually much less cumbersome to get TCP through firewalls
than UDP.

In theory, there's no reason for having explicit length fields, that
information is already encoded in the ASN1 encoding of the Kerberos
packets.  But having explicit lengths makes it unnecessary to have to
decode the ASN.1 encoding just to know how much data has to be read.

Another way of signaling the end of the request of the reply would be
to do a half-close after the request and a full-close after the reply.
This does not work well with all kinds of firewalls.

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

.ti 0
Authors' Addresses

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

Johan Danielsson
.br
PDC, KTH
.br
S-100 44  STOCKHOLM
.br
Sweden

Phone: +46-8-7907885
.br
Fax:   +46-8-247784
.br
EMail: joda@pdc.kth.se
