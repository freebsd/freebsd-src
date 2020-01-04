DLT and LINKTYPE allocation
===========================

DLT_ types live in pcap/dlt.h.  They can be requested by the community on a
First-Come First-Served basis [i.e. https://tools.ietf.org/html/rfc8126#section-4.4 ]
(Although libpcap is not at this time an IETF specification, there have been
some as yet-incomplete efforts to do this).

The Tcpdump Group prefers to link to an open specification on the new DLT_
type,  but they are available for closed, proprietary projects as well.
In that case, a stable email address suffices so that someone who finds
an unknown DLT_ type can investigate.
We prefer to give out unambiguous numbers, and we try to do it as quickly
as possible, but DLT_USERx is available while you wait.

Note that DLT_ types are, in theory, private to the capture mechanism and can
in some cases be operating system specific, and so a second set of values,
LINKTYPE_ is allocated for actually writing to pcap files.  As much as
possible going forward, the DLT_ and LINKTYPE_ value are identical, however,
this was not always the case.  See pcap-common.c.

The LINKTYPE_ values are not exported, but are in pcap-common.c only.

DEVELOPER NOTES
---------------

When allocating a new DLT_ value, a corresponding value needs to be
added to pcap-common.c.
It is not necessary to copy the comments from dlt.h to pcap-common.c.
