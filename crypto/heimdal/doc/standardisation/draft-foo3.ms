.\" even if this file is called .ms, it's using the me macros.
.\" to format try something like `nroff -me'
.\" level 2 heading
.de HH
.$p "\\$2" "" "\\$1"
.$0 "\\$2"
..
.\" make sure footnotes produce the right thing with nroff
.ie t \
\{\
.ds { \v'-0.4m'\x'\\n(0x=0*-0.2m'\s-3
.ds } \s0\v'0.4m'
.\}
.el \
\{\
.ds { [
.ds } ]
.\}
.ds * \\*{\\n($f\\*}\k*
.\" page footer
.fo 'Westerlund, Danielsson''[Page %]'
.\" date
.ds RH \*(mo, 19\n(yr
.\" left margin
.nr lm 6
.\" heading indent per level
.nr si 3n
.\" footnote indent
.nr fi 0
.\" paragraph indent
.nr po 0
.\" don't hyphenate
.hy 0
.\" left adjustment
.ad l
.\" indent 0
.in 0
.\" line length 16cm and page length 25cm (~10 inches)
.ll 16c
.pl 25c
.ta \n(.luR
.nf
Network Working Group	Assar Westerlund
<draft-ietf-cat-krb5-firewalls.txt>	SICS
Internet-Draft	Johan Danielsson
\*(RH	PDC, KTH
Expire in six months	
.fi

.\" page header, has to be set here so it won't appear on page 1
.he 'Internet Draft'Kerberos vs firewalls'\*(RH'
.ce
.b "Kerberos vs firewalls"

.HH 1 "Status of this Memo"
.lp
This document is an Internet-Draft.  Internet-Drafts are working
documents of the Internet Engineering Task Force (IETF), its areas,
and its working groups.  Note that other groups may also distribute
working documents as Internet-Drafts.
.lp
Internet-Drafts are draft documents valid for a maximum of six months
and may be updated, replaced, or obsoleted by other documents at any
time.  It is inappropriate to use Internet- Drafts as reference
material or to cite them other than as \*(lqwork in progress.\*(rq
.lp
To view the entire list of current Internet-Drafts, please check the
\*(lq1id-abstracts.txt\*(rq listing contained in the Internet-Drafts
Shadow Directories on ftp.is.co.za (Africa), ftp.nordu.net (Europe),
munnari.oz.au (Pacific Rim), ds.internic.net (US East Coast), or
ftp.isi.edu (US West Coast).
.lp
Distribution of this memo is unlimited.  Please send comments to the
<cat-ietf@mit.edu> mailing list.
.HH 1 "Abstract"
.lp
Kerberos and firewalls both deal with security, but doesn't get along
very well. This memo discusses ways to use Kerberos in a firewalled
environment.
.HH 1 "Introduction"
.lp
Kerberos[RFC1510]
.(d
[RFC1510]
Kohl, J. and Neuman, C., \*(lqThe Kerberos Network Authentication
Service (V5)\*(rq, RFC 1510, September 1993.
.)d
is a protocol for authenticating parties communicating over insecure
networks.  Firewalling is a technique for achieving an illusion of
security by putting restrictions on what kinds of packets and how
these are sent between the internal (so called \*(lqsecure\*(rq)
network and the global (or \*(lqinsecure\*(rq) Internet.  The problems
with firewalls are many, but to name a few:
.np
Firewalls usually doesn't allow people to use UDP. The reason for this
is that UDP is (by firewall advocates) considered insecure. This
belief is probably based on the fact that many \*(lqinsecure\*(rq
protocols (like NFS) use UDP. UDP packets are also considered easy to
fake.
.np
Firewalls usually doesn't allow people to connect to arbitrary ports,
such as the ports used when talking to the KDC.
.np
In many non-computer organisations, the computer staff isn't what
you'd call \*(lqwizards\*(rq; a typical case is an academic
institution, where someone is taking care of the computers part time,
and is doing research the rest of the time. Adding a complex device
like a firewall to an environment like this, often leads to poorly run
systems that is more a hindrance for the legitimate users than to
possible crackers.
.lp
The easiest way to deal with firewalls is to ignore them, however in
some cases this just isn't possible. You might have users that are
stuck behind a firewall, but also has to access your system, or you
might find yourself behind a firewall, for instance when out
travelling.
.lp
To make it possible for people to use Kerberos from behind a firewall,
there are several things to consider.
.(q
.i
Add things to do when stuck behind a firewall, like talking about the
problem with local staff, making them open some port in the firewall,
using some other port, or proxy.
.r
.)q
.HH 1 "Firewalls"
.lp
A firewall is usually placed between the \*(lqinside\*(rq and the
\*(lqoutside\*(rq networks, and is supposed to protect the inside from the
evils on the outside.  There are different kinds of firewalls.  The
main differences are in the way they forward (or doesn't) packets.
.ip \(bu
The most straight forward type is the one that just imposes
restrictions on incoming packets. Such a firewall could be described
as a router that filters packets that match some criteria.
.ip \(bu
They may also \*(lqhide\*(rq some or all addresses on the inside of the
firewall, replacing the addresses in the outgoing packets with the
address of the firewall (aka network address translation, or NAT). NAT
can also be used without any packet filtering, for instance when you
have more than one host sharing a single address (e.g with a dialed-in
PPP connection).
.ip
There are also firewalls that does NAT both on the inside and the
outside (a server on the inside will see this as a connection from the
firewall).
.ip \(bu
A third type is the proxy type firewall, that parses the contents of
the packets, basically acting as a server to the client, and as a
client to the server (man-in-the-middle). If Kerberos is to be used
with this kind of firewall, a protocol module that handles KDC
requests has to be written\**.
.(f
\**Instead of writing a new module for Kerberos, it can be possible to
hitch a ride on some other protocol, that's already beeing handled by
the proxy.
.)f
.lp
The last type of firewall might also cause extra trouble when used
with kerberised versions of protocols that the proxy understands, in
addition to the ones mentioned below. This is the case with the FTP
Security Extensions [RFC2228],
.(d
[RFC2228]
Horowitz, M. and Lunt, S., \*(lqFTP Security Extensions\*(rq, RFC2228,
October 1997.
.)d
that adds a new set of commands to the FTP protocol [RFC959],
.(d
[RFC959] Postel, J. and Reynolds, J., \*(lqFile Transfer Protocol
(FTP)\*(rq, RFC 969, October 1985
.)d
for integrity, confidentiality, and privacy protecting commands, and
data. When transferring data, the FTP protocol uses a separate data
channel, and an FTP proxy will have to look out for commands that
start a data transfer. If all commands are encrypted, this is
impossible. A protocol that doesn't suffer from this is the Telnet
Authentication Option [RFC1416]
.(d
[RFC1416]
Borman, D., \*(lqTelnet Authentication Option\*(rq, RFC 1416, February
1993.
.)d
that does all
authentication and encryption in-bound.
.HH 1 "Scenarios"
.lp
Here the different scenarios we have considered are described, the
problems they introduce and the proposed ways of solving them.
Combinations of these can also occur.
.HH 2 "Client behind firewall"
.lp
This is the most typical and common scenario.  First of all the client
needs some way of communicating with the KDC.  This can be done with
whatever means and is usually much simpler when the KDC is able to
communicate over TCP.
.lp
Apart from that, the client needs to be sure that the ticket it will
acquire from the KDC can be used to authenticate to a server outside
its firewall.  For this, it needs to add the address(es) of potential
firewalls between itself and the KDC/server, to the list of its own
addresses when requesting the ticket.  We are not aware of any
protocol for determining this set of addresses, thus this will have to
be manually configured in the client.
.lp
The client could also request a ticket with no addresses. This is not
a recommended way to solve this problem. The address was put into the
ticket to make it harder to use a stolen ticket.  A ticket without
addresses will therefore be less \*(lqsecure.\*(rq RFC1510 also says that
the KDC may refuse to issue, and the server may refuse to accept an
address-less ticket.
.lp
With the ticket in possession, communication with the kerberised
server will not need to be any different from communicating between a
non-kerberised client and server.
.HH 2 "Kerberised server behind firewall"
.lp
The kerberised server does not talk to the KDC at all, so nothing
beyond normal firewall-traversal techniques for reaching the server
itself needs to be applied.
.lp
If the firewall rewrites the clients address, the server will have to
use some other (possibly firewall specific) protocol to retrieve the
original address. If this is not possible, the address field will have
to be ignored. This has the same effect as if there were no addresses
in the ticket (see the discussion above).
.HH 2 "KDC behind firewall"
.lp
The KDC is in this respect basically just like any other server.
.\" .uh "Specification"
.HH 1 "Security considerations"
.lp
Since the whole network behind a NAT-type firewall looks like one
computer from the outside, any security added by the addresses in the
ticket will be lost.
.HH 1 "References"
.lp
.pd
.HH 1 "Authors' Addresses"
.lp
.nf
Assar Westerlund
Swedish Institute of Computer Science
Box 1263
S-164 29 KISTA
.sp
Phone: +46-8-7521526
Fax: +46-8-7517230
EMail: assar@sics.se
.sp 2
Johan Danielsson
Center for Parallel Computers
KTH
S-100 44 STOCKHOLM
.sp
Phone: +46-8-7906356
Fax: +46-8-247784
EMail: joda@pdc.kth.se
.fi