.\"	$Id$
.\"
.NC "NSAP Addresses & Routing"
.sh 1 "OSI Address Formats"
.pp
ARGO supports an ISO address family, AF_ISO, in addition to the
DoD Internet address family, AF_INET.
Addresses in the family AF_ISO 
take the form described by ISO 8348/DAD2, which is an addendum to the
OSI network service standard that describes network layer addressing.
.sh 2 "ISO 8348/DAD2 Tutorial"
.pp
.\" FIGURE
.\".so figs/osi_addr.nr
.so figs/addrfmt.nr
.CF
shows the
format of an OSI NSAP-address.
The address has two major parts: the initial domain part
(IDP) and the domain specific part (DSP). The IDP is further divided into
two parts: the authority and format identifier (AFI) and the
initial domain identifier (IDI). The AFI specifies the format of the
IDI, the authority responsible for allocating values of the IDI, and
the syntax of the DSP. The IDI specifies the network addressing domain
from which DSP values are allocated, and the authority responsible for
allocating DSP values.
.sh 2 "Supported Formats"
.pp
ARGO supports three types of ISO NSAP-addresses:
one type with AFI 37(hex) and two types with AFI 47(hex).
.sh 3 "AFI 37"
.pp
This value of the AFI defines the IDI to be an X.121 address or DTE
address. 
The DTE address is encoded in binary coded decimal.  
The DSP syntax is binary.
This form is intended to be used when communicating 
across a public data network (PDN). 
The ARGO software and documentation
refer to this type of NSAP-address as a
\*(lqtype 37.\*(rq
address.
.sh 3 "AFI 47"
.pp
The value of 47 for the AFI defines the IDI to be a 4 digit International
Code Designator (ICD) allocated according to ISO 6523. 
ARGO support two
ICD values.
.sh 4 "ICD 0004"
.pp
The ICD value of 0004 is assigned to OSINET,
an experimental OSI network overseen by 
National Institute of Science and Technology.\**
.(f
\** formerly the National Bureau of Standards
.)f
When this style of NSAP-address
is used, 
the DSP is divided into four parts: an organization identifier (2 bytes),
a subnet identifier (2 bytes), 
an SNPA-part (4-8 bytes), and
an NSAP selector (1 byte). 
The use of these fields is defined by the OSINET steering committee.
This type of address is known as an
\*(lqOSINET\*(rq
address.
.sh 4 "ICD 0006"
.pp
The ICD value of 0006 is assigned to the Department of Defense (DoD).
In ARGO, NSAP-addresses with an ICD value of 0006 
are of the format defined in RFC 986, a proposal for embedding DARPA Internet
addresses within an OSI NSAP-address. 
In this case, the DSP takes the form:
version (1 byte), DARPA Internet Address (4 bytes), upper layer protocol
identifier (1 byte).
This is called an
\*(lqrfc986\*(rq
address.
.sh 1 "Internal Representation"
.pp
Internally, an NSAP address takes the form
.(b
\fC
.TS
tab(+);
l s s s s.
struct iso_addr {
.T&
l l l l l.
+u_char+isoa_afi;+/* authority &
+++format id */
+union+{+ 
++struct addr_37+addr_37;+/* x.121 */
++struct addr_osinet+addr_osinet;+/* OSINET */
++struct addr_rfc986+addr_rfc986;+/* Internet*/
+}+isoa_u;
+u_char+isoa_len;+/* length */
}
.TE
\fR
.)b
The field \fIisoa_afi\fR contains the AFI for the address. 
The union
\fIisoa_u\fR contains the remainder of the address. 
The length of the entire address (the AFI, IDI, and DSP) is 
stored in \fIisoa_len\fR.
.sh 1 "Network Layer Routing"
.pp
Routing at the network layer is performed by the 
routing procedure \fIrtalloc()\fR as described in Chapter 5. 
\fIRtalloc()\fR was designed for used in the DoD Internet
domain.
An unfortunate
effect of this is that routing decisions are based upon either the
entire NSAP address or the network portion of the address. 
The problem is defining the network portion of an NSAP address. 
The location and extent of the 
network portion of an NSAP address depends on the
style of NSAP address. 
This decision is made by the function \fIiso_netof()\fR.
.sh 2 "Network Portion of Type 37 Addresses"
.pp
There is no network portion of an X.121 address. 
In ARGO, the network portion of a type 37 address 
is defined to be just the AFI. 
The obvious consequence of this is that all type 37 addresses will 
match all other type 37 addresses
in a network-portion comparison.
.sh 2 "Network Portion of OSINET Addresses"
.pp
The network portion of an OSINET address is the organization identifier and
the subnet identifier.
.sh 2 "Network Portion of RFC986 Addresses"
.pp
The network portion of an RFC986 address is the network portion of the
embedded DARPA Internet address. 
ARGO does not support subnetting, a method of subdividing Internet addresses.
.sh 1 "NSAP Address / Subnetwork Point of Attachment Mapping"
.pp
In order to transmit a packet on a real subnetwork, the destination
NSAP address
must be mapped to an SNPA address. 
An SNPA address is the real, "hardware" address
of a system on a network. 
This address corresponds to the 6 byte Ethernet or Token Ring
Adapter address, 
or to the DTE address, which may be up to 7 bytes
long (14 decimal digits).
.pp
A table, \fIsnpa_cache\fR is kept in the kernel which contains the 
translation between NSAP-addresses and SNPA-addresses. 
This table is used by \fIiso_snparesolve()\fR whenever a 
datagram must be dispatched.
The table is maintained by the ISO ES-IS protocol entity.
Entries can be added and deleted
by the user program \fIclnlutil(8)\fR and
by the CONS entity.
