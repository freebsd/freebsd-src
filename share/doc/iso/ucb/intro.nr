.NC "Introduction"
.sh 1 "Introduction"
.pp
This document describes the usage of the ISO
transport and network layers written for the ACIS Operating System,
the IBM ACIS port of Berkeley 4.3 Unix\**
.(f
\** Unix is a registered trademark of AT&T.
.)f
for the IBM RT PC,
hereafter called AOS, as modified by UC Berkeley.
This document describes work in progress and is an extremely
hasty job of editing an earlier document written by colleagues
at the university of Wisconsin.
It is to be regarded as an emergency manual prepared for testers
at NIST and should not be redistributed further.
As such, there are philosophical
statements that Berkeley fundamentally disagrees with, which
we do not presently have the time to rip out.
Collectively, this work is called the Wisconsin ARGO kernel. 
The ARGO kernel supports the 
the connection-oriented ISO transport service (COTS), the
ISO connectionless network service (CLNS)
and a
connection-oriented network service (CONS).
The COTS is provided by the ISO transport protocol TP,
ISO 8073 Revised.
The CLNS is provided by the connectionless network protocol,
ISO 8473.
The CONS is provided by the X.25 protocols.
The ARGO implementation of the CONS is not a complete
ISO CONS, but contains enough of the CONS to support
the COTS and the CLNS (in the latter case, the CONS can be
viewed as a subnetwork service).
.pp
The purposes of this document are
.ip "1) "
to describe the transport service and the software interface
between the user and provider of this service,
.ip "2) "
to describe the network service and the software interface it
provides.
.pp
It is assumed that the reader is familiar with the \fBC\fR
programming language,
with Unix conventions, and with the ISO specifications listed in Appendix A.
.sh 1 "Organization"
.pp
This document is composed of several chapters.
Chapter One contains this introduction. Chapter Two presents a
definition of terms and phrases used throughout the document.
Chapter Three describes the transport service interface, which is
the interface between the transport protocol implementation software and the transport user software.
Chapter Four describes the network service interface, and the interface
above and below the network layer.
Chapter Five explains the format of an OSI address.
Chapter Six describes the
the architecture of the interprocess communication support in the 
kernel, which to a large degree mandates
the design of a protocol implementation for a 4.3 Unix kernel.
.\" Appendix A is a list of the applicable ISO standards.
.\" The manual pages relevant to the transport and network layers
.\" are included as Appendix B.
.pp
Several conventions are followed in this document.
All procedure names and system call names are followed
by a pair of parentheses, for example,
.i read ().
References to manual pages consist of the name of the
manual page, followed by the section in which
the man page is found:
.i read (2).
