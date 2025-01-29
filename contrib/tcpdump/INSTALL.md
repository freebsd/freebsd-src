# tcpdump installation notes
If you have not built libpcap, and your system does not have libpcap
installed, install libpcap first.  Your system might provide a version
of libpcap that can be installed; if so, to compile tcpdump you might
need to install a "developer" version of libpcap as well as the
"run-time" version.  You can also install tcpdump.org's version of
libpcap; see [this file](README.md) for the location.

You will need a C99 compiler to build tcpdump.  The build system
will abort if your compiler is not C99 compliant.  If this happens, use
the generally available GNU C compiler (GCC) or Clang.

After libpcap has been built (either install it with `make install` or
make sure both the libpcap and tcpdump source trees are in the same
directory), run `./configure` (a shell script). `configure` will
determine your system attributes and generate an appropriate `Makefile`
from `Makefile.in`.  Now build tcpdump by running `make`.

If everything builds ok, `su` and type `make install`.  This will install
tcpdump and the manual entry.  Any user will be able to use tcpdump to
read saved captures.  Whether a user will be able to capture traffic
depends on the OS and the configuration of the system; see the
[tcpdump man page](https://www.tcpdump.org/manpages/tcpdump.1.html)
for details.  DO NOT give untrusted users the ability to
capture traffic.  If a user can capture traffic, he or she could use
utilities such as tcpdump to capture any traffic on your net, including
passwords.

Note that most systems ship tcpdump, but usually an older version.
Building tcpdump from source as explained above will usually install the
binary as `/usr/local/bin/tcpdump`.  If your system has other tcpdump
binaries, you might need to deinstall these or to set the PATH environment
variable if you need the `tcpdump` command to run the new binary
(`tcpdump --version` can be used to tell different versions apart).

If your system is not one which we have tested tcpdump on, you may have
to modify the `configure` script and `Makefile.in`. Please
[send us patches](https://www.tcpdump.org/index.html#patches)
for any modifications you need to make.

Please see [this file](README.md) for notes about tested platforms.


## Description of files
```
CHANGES		- description of differences between releases
CONTRIBUTING.md	- guidelines for contributing
CREDITS		- people that have helped tcpdump along
INSTALL.md	- this file
LICENSE		- the license under which tcpdump is distributed
Makefile.in	- compilation rules (input to the configure script)
README.md	- description of distribution
VERSION		- version of this release
aclocal.m4	- autoconf macros
addrtoname.c	- address to hostname routines
addrtoname.h	- address to hostname definitions
addrtostr.c	- address to printable string routines
addrtostr.h	- address to printable string definitions
ah.h		- IPSEC Authentication Header definitions
appletalk.h	- AppleTalk definitions
ascii_strcasecmp.c - locale-independent case-independent string comparison
		routines
atime.awk	- TCP ack awk script
atm.h		- ATM traffic type definitions
bpf_dump.c	- BPF program printing routines, in case libpcap doesn't
		  have them
chdlc.h		- Cisco HDLC definitions
cpack.c		- functions to extract packed data
cpack.h		- declarations of functions to extract packed data
config.guess	- autoconf support
config.h.in	- autoconf input
config.sub	- autoconf support
configure	- configure script (run this first)
configure.ac	- configure script source
doc/README.*	- some building documentation
ethertype.h	- Ethernet type value definitions
extract.h	- alignment definitions
gmpls.c		- GMPLS definitions
gmpls.h		- GMPLS declarations
install-sh	- BSD style install script
interface.h	- globals, prototypes and definitions
ip.h		- IP definitions
ip6.h		- IPv6 definitions
ipproto.c	- IP protocol type value-to-name table
ipproto.h	- IP protocol type value definitions
l2vpn.c		- L2VPN encapsulation value-to-name table
l2vpn.h		- L2VPN encapsulation definitions
lbl/os-*.h	- OS-dependent defines and prototypes
llc.h		- LLC definitions
machdep.c	- machine dependent routines
machdep.h	- machine dependent definitions
makemib		- mib to header script
mib.h		- mib definitions
missing/*	- replacements for missing library functions
ntp.c		- functions to handle ntp structs
ntp.h		- declarations of functions to handle ntp structs
mkdep		- construct Makefile dependency list
mpls.h		- MPLS definitions
nameser.h	- DNS definitions
netdissect.h	- definitions and declarations for tcpdump-as-library
		  (under development)
nfs.h		- Network File System V2 definitions
nfsfh.h		- Network File System file handle definitions
nlpid.c		- OSI NLPID value-to-name table
nlpid.h		- OSI NLPID definitions
ospf.h		- Open Shortest Path First definitions
packetdat.awk	- TCP chunk summary awk script
parsenfsfh.c	- Network File System file parser routines
pcap-missing.h	- declarations of functions possibly missing from libpcap
ppp.h		- Point to Point Protocol definitions
print.c		- Top-level routines for protocol printing
print-*.c	- The netdissect printers
rpc_auth.h	- definitions for ONC RPC authentication
rpc_msg.h	- definitions for ONC RPC messages
send-ack.awk	- unidirectional tcp send/ack awk script
slcompress.h	- SLIP/PPP Van Jacobson compression (RFC1144) definitions
smb.h		- SMB/CIFS definitions
smbutil.c	- SMB/CIFS utility routines
stime.awk	- TCP send awk script
tcp.h		- TCP definitions
tcpdump.1	- manual entry
tcpdump.c	- main program
timeval-operations.h - timeval operations macros
udp.h		- UDP definitions
util-print.c	- utility routines for protocol printers
```
