.\" manual page [] for ppp 0.94 beta2 + alpha
.\" $Id: ppp.8,v 1.11 1995/09/24 18:15:14 nate Exp $
.Dd 20 September 1995
.Os FreeBSD
.Dt PPP 8
.Sh NAME
.Nm ppp
.Nd
Point to Point Protocol (aka iijppp)
.Sh SYNOPSIS
.Nm
.Op Fl auto \*(Ba Fl direct Fl dedicated
.Sh DESCRIPTION
This is a user process
.Em PPP
software package.  Normally,
.Em PPP
is implemented as a part of the kernel (e.g. as managed by pppd) and it's
thus somewhat hard to debug and/or modify its behavior.  However, in this
implementation
.Em PPP
is done as a user process with the help of the
tunnel device driver (tun).

.Sh Major Features

.Bl -diag
.It Provides interactive user interface.
Using its command mode, the user can
easily enter commands to establish the connection with the remote end, check
the status of connection and close the connection.  All functions can
also be optionally password protected for security.

.It Supports both manual and automatic dialing. 
Interactive mode has a
.Dq term
command which enables you to talk to your modem directly.  When your
modem is connected to the remote peer and it starts to talk
.Em PPP
, the
.Em PPP
software detects it and switches to packet
mode automatically. Once you have determined the proper sequence for connecting
with the remote host, you can write a chat script to define the necessary
dialing and login procedure for later convenience.

.It Supports on-demand dialup capability.
By using auto mode,
.Nm
will act as a daemon and wait for a packet to be sent over the
.Em PPP
link.  When this happens, the daemon automatically dials and establishes the
connection.

.It Supports server-side PPP connections.
Can act as server which accepts incoming
.Em PPP
connections. 
 
.It Supports PAP and CHAP authentication.


.It Supports Proxy Arp.
When
.Em PPP
is set up as server, you can also configure it to do proxy arp for your
connection.

.It Supports packet filtering.
User can define four kinds of filters:
.Em ifilter
for incoming packets, 
.Em ofilter
for outgoing packets,
.Em dfilter
to define a dialing trigger packet and
.Em afilter
for keeping a connection alive with the trigger packet.

.It Tunnel driver supports bpf.
The user can use
.Xr tcpdump 1
to check the packet flow over the
.Em PPP
link.

.It Supports PPP over TCP capability. 


.It Supports IETF draft Predictor-1 compression.  
.Nm
supports not only VJ-compression but also Predictor-1 compression.
Normally, a modem has built-in compression (e.g. v42.bis) and the system
may receive higher data rates from it as a result of such compression. 
While this is generally a good thing in most other situations, this
higher speed data imposes a penalty on the system by increasing the
number of serial interrupts the system has to process in talking to the
modem and also increases latency.  Unlike VJ-compression, Predictor-1
compression pre-compresses
.Em all
data flowing through the link, thus reducing overhead to a minimum.

.It Runs under BSDI-1.1 and FreeBSD.

.El


Patches for NeXTSTEP 3.2 are also available on the net.

.Sh GETTING STARTED

When you first run
.Nm
you may need to deal with some initial configuration details.  First,
your kernel should include a tunnel device (the default in FreeBSD 2.0.5
and later). If it doesn't, you'll need to rebuild your kernel with the
following line in your kernel configuration file:

.Dl pseudo-device   tun             1

You should set the numeric field to the maximum number of 
.Em PPP
connections you wish to support.

Second, check your
.Pa /dev
directory for the tunnel device entry
.Pa /dev/tun0.
If it doesn't exist, you can create it by running "MAKEDEV tun0"

.Sh MANUAL DIALING

% 
.Nm
User Process PPP written by Toshiharu OHNO.

* If you set your hostname and password in
.Pa /etc/ppp/ppp.secret,
you can't do
anything except run the quit and help commands *

ppp on "your hostname"> help
  passwd  : Password for security
  quit    : Quit the PPP program    
  help    : Display this message

ppp on tama> pass <password>

* "on" will change to "ON" if you specify the correct password. *

ppp ON tama>

* You can specify the device name and speed for your modem using the
following commands: *

ppp ON tama> set line /dev/cuaa0

ppp ON tama> set speed 38400

ppp ON tama> set parity even

ppp ON tama> show modem

* Modem related parameters are shown in here *

ppp ON tama>

* Use term command to talk with your modem *

ppp ON tama> term
 at
 OK
 atdt123456
 CONNECT

 login: ppp
 Password:

* PPP started in remote side.  When the peer start to talk PPP, the
program will detect it automatically and return to command mode. *

ppp ON tama>

.Nm PPP
ON tama>

* NOW, you are connected!  Note that
.Sq PPP
in the prompt has changed to capital letters to indicate this. *

PPP ON tama> show lcp

* You'll see LCP status *

PPP ON tama> show ipcp

* You'll see IPCP status.  At this point, your machine has a host route
to the peer. If you want to add a default route entry, then enter the
following command. *

PPP ON tama> add 0 0 HISADDR

* The string
.Sq HISADDR
represents the IP address of connected peer. *

PPP ON tama>

* Use network applications (i.e. ping, telnet, ftp) in other windows *

PPP ON tama> show log

* Gives you some logging messages *

PPP ON tama> close

* The connection is closed and modem will be disconnected. *

ppp ON tama> quit

%

.Sh AUTOMATIC DIALING

To use automatic dialing, you must prepare some Dial and Login chat scripts.
See the example definitions in
.Pa /etc/ppp/ppp.conf.sample
(the format of ppp.conf is pretty simple).

.Bl -bullet -compact
.It
Each line contains one command, label or comment.
.It
A line starting with a
.Sq #
character is treated as a comment line.
.It
A label name has to start in the first column and should be followed by
a colon (:).
.It
A command line must contain a space or tab in the first column.
.El

Once ppp.conf is ready, specify the destination label name when you
invoke
.Nm ppp .
Commands associated with the destination label are then
executed. Note that the commands associated with the
.Dq default
label are ALWAYS executed.

Once the connection is made, you'll find that the
.Nm ppp
portion of the prompt has changed to
.Nm PPP .

   % ppp pm2
   ...
   ppp ON tama> dial
   dial OK!
   login OK!
   PPP ON tama>

If the
.Pa /etc/ppp/ppp.linkup
file is available, its contents are executed
when the
.Em PPP
connection is established.  See the provided example which adds a
default route.  The string HISADDR represents the IP address of the
remote peer.

.Sh DIAL ON DEMAND

To play with demand dialing, you must use the
.Fl auto
option.  You must also specify the destination label in
.Pa /etc/ppp/ppp.conf
to use.  It should contain the
.Dq ifaddr
command to define the remote peer's IP address. (refer to
.Pa /etc/ppp/ppp.conf.sample )

   % ppp -auto pm2demand
   ...
   %

When
.Fl auto
is specified,
.Nm
runs as a daemon but you can still configure or examine its
configuration by using the diagnostic port as follows:


  % telnet localhost 3000
    Trying 127.0.0.1...
    Connected to localhost.spec.co.jp.
    Escape character is '^]'.
    User Process PPP. Written by Toshiharu OHNO.
    Working as auto mode. 
    PPP on tama> show ipcp
    what ?
    PPP on tama> pass xxxx
    PPP ON tama> show ipcp
    IPCP [OPEND]
      his side: xxxx
      ....

.Pp
Each
.Nm
daemon has an associated port number which is computed as "3000 +
tunnel_device_number". If 3000 is not good base number, edit defs.h in
the ppp sources (
.Pa /usr/src/usr.sbin/ppp )
and recompile it.

When an outgoing packet is detected,
.Nm
will perform the dialing action (chat script) and try to connect
with the peer.

If the connect fails, the default behaviour is to wait 30 seconds
and then attempt to connect when another outgoing packet is detected.
This behaviour can be changed with
.Bd -literal -offset indent
set redial seconds|random [dial_attempts]
.Ed
.Pp
Seconds is the number of seconds to wait before attempting
to connect again. If the argument is
.Sq random ,
the delay period is a random value between 0 and 30 seconds.
.Sq dial_attempts
is the number of times to try to connect for each outgoing packet
that is received. The previous value is unchanged if this parameter
is omitted.
.Bd -literal -offset indent
set redial 10 4
.Ed
.Pp
will attempt to connect 4 times for each outgoing packet that is
detected with a 10 second delay between each attempt.

Modifying the dial delay is very useful when running
.Nm
in demand
dial mode on both ends of the link. If each end has the same timeout,
both ends wind up calling each other at the same time if the link
drops and both ends have packets queued.

 To terminate the program, type

  PPP ON tama> close
  ppp ON tama> quit all

.Pp
A simple
.Dq quit
command will terminate the telnet connection but not the program itself.
You must use
.Dq quit all
to terminate the program as well.

.Sh PACKET FILTERING

This implementation supports packet filtering. There are three kinds of
filters: ifilter, ofilter and dfilter.  Here are the basics:

.Bl -bullet -compact
.It
A filter definition has the following syntax:

set filter-name rule-no action [src_addr/src_width] [dst_addr/dst_width]
[proto [src [lt|eq|gt] port ]] [dst [lt|eq|gt] port] [estab]
.Bl -enum
.It
.Sq filter-name
should be one of ifilter, ofilter, or dfilter.
.It
There are two actions: 
.Sq permit
and
.Sq deny .
If a given packet 
matches the rule, the associated action is taken immediately.
.It
.Sq src_width
and
.Sq dst_width
work like a netmask to represent an address range.
.It
.Sq proto
must be one of icmp, udp or tcp.
.It
.Sq port number
can be specified by number and service name from
.Pa /etc/services .

.El

.It
Each filter can hold up to 20 rules, starting from rule 0.

The entire rule set is not effective until rule 0 is defined.

.It
If no rule is matched to a packet, that packet will be discarded
(blocked).

.It
Use
.Dq set filter-name -1
to flush all rules.

.El

See
.Pa /etc/ppp/ppp.conf.filter.example .


.Sh RECEIVING INCOMING PPP CONNECTIONS

To handle an incoming
.Em PPP
connection request, follow these steps:

.Bl -enum
.It 
Make sure the modem and (optionally)
.Pa /etc/rc.serial
is configured correctly.
.Bl -bullet -compact
.It
Use Hardware Handshake (CTS/RTS) for flow control.
.It
Modem should be set to NO echo back (ATE0) and NO results string (ATQ1).
.El

.It
Edit
.Pa /etc/ttys
to enable a getty on the port where the modem is attached.

For example:

.Dl ttyd1  "/usr/libexec/getty std.38400" dialup on secure

Don't forget to send a HUP signal to the init process to start the getty.

.Dl # kill -HUP 1

.It
Prepare an account for the incoming user.
.Bd -literal
ppp:xxxx:66:66:PPP Login User:/home/ppp:/usr/local/bin/ppplogin
.Ed

.It
Create a 
.Pa /usr/local/bin/ppplogin
file with the following contents:
.Bd -literal -offset indent
#!/bin/sh
/usr/sbin/ppp -direct
.Ed

(You can specify a label name for further control.)

.El

.Pp
Direct mode (
.Fl direct )
lets
.Nm
work with stdin and stdout.  You can also telnet to port 3000 to get
command mode control in the same manner as client-side
.Nm .

.Sh SETTING IDLE, LINE QUALITY REQUEST, RETRY TIMER

To check/set idletimer, use the
.Dq show timeout
and
.Dq set timeout [lqrtimer [retrytimer]]
commands.

 Ex:
.Dl ppp ON tama> set timeout 600

The timeout period is measured in seconds, the  default values for which
are timeout = 180 or 3 min, lqrtimer = 30sec and retrytimer = 3sec. 
To disable the idle timer function,
use the command
.Dq set timeout 0 .

In
.Fl auto
mode, an idle timeout causes the
.Em PPP
session to be
closed, though the
.Nm
program itself remains running.  Another trigger packet will cause it to
attempt to reestablish the link.

.Sh Predictor-1 compression

This version supports CCP and Predictor type 1 compression based on
the current IETF-draft specs. As a default behavior,
.Nm
will attempt to use (or be willing to accept) this capability when the
peer agrees (or requests it).

To disable CCP/predictor functionality completely, use the
.Dq disable pred1
and
.Dq deny pred1
commands.

.Sh Controlling IP address

.Nm
uses IPCP to negotiate IP addresses. Each side of the connection
specifies the IP address that it's willing to use, and if the requested
IP address is acceptable then
.Nm
returns ACK to the requester.  Otherwise, 
.Nm
returns NAK to suggest that the peer use a different IP address. When
both sides of the connection agree to accept the received request (and
send ACK), IPCP is set to the open state and a network level connection
is established.

To control this IPCP behavior, this implementation has the
.Dq set ifaddr
command for defining the local and remote IP address:

.Dl ifaddr src_addr dst_addr

Where,
.Sq src_addr
is the IP address that the local side is willing to use and
.Sq dst_addr
is the IP address which the remote side should use.

Ex:
.Dl ifaddr 192.244.177.38 192.244.177.2

The above specification means:
.Bl -bullet -compact
.It
I strongly want to use 192.244.177.38 as my IP address, and I'll
disagree if the peer suggests that I use another address.

.It
I strongly insist that peer use 192.244.177.2 as own side address and
don't permit it to use any IP address but 192.244.177.2.  When peer
request another IP address, I always suggest that it use 192.244.177.2.

.It
This is all fine when each side has a pre-determined IP address, however
it is often the case that one side is acting as a server which controls
all IP addresses and the other side should obey the direction from it. 
.El

In order to allow more flexible behavior, `ifaddr' command allows the
user to specify IP address more loosely:

.Dl ifaddr 192.244.177.38/24 192.244.177.2/20

A number followed by a slash (/) represent the number of bits significant in
the IP address.  The above example signifies that:

.Bl -bullet -compact
.It
I'd like to use 192.244.177.38 as my address if it is possible, but I'll
also accept any IP address between 192.244.177.0 and 192.244.177.255.

.It 
I'd like to make him use 192.244.177.2 as his own address, but I'll also
permit him to use any IP address between 192.244.176.0 and
192.244.191.255.

.It
As you may have already noticed, 192.244.177.2 is equivalent to saying
192.244.177.2/32.

.It
As an exception, 0 is equivalent to 0.0.0.0/0, meaning that I have no
preferred IP address and will obey the remote peer's selection.

.It
192.244.177.2/0 means that I'll accept/permit any IP address but I'll
try to insist that 192.244.177.2 be used first.
.El

.Sh Connecting with your service provider

.Bl -enum
.It
Describe provider's phone number in DialScript: Use the
.Dq set dial
or
.Dq set phone
commands.
.It
Describe login procedure in LoginScript: Use the
.Dq set login
command.
.It
Use
.Dq set ifaddr
command to define the IP address.
.Bl -bullet
.It
If you know what IP address provider uses, then use it as the remote address.
.It
If provider has assigned a particular IP address to you, then use it as
your address.
.It
If provider assigns your address dynamically, use 0 as your address.
.It
If you have no idea which IP addresses to use, then try
.Dq set ifaddr 0 0 .
.El
.It
If provider requests that you use PAP/CHAP authentication methods, add
the next lines to your
.Pa ppp.conf
file:
.Bd -literal -offset indent
enable pap (or enable chap)
disable chap (or disable pap)
set authname MyName
set authkey MyPassword
.Ed
.El

Please refer to
.Pa /etc/ppp/ppp.conf.iij
for some real examples.

.Sh Logging facility

.Nm
is able to generate the following log info into
.Pa /var/log/ppp.log :

.Bl -column SMMMMMM -offset indent -compat
.It Li Phase	Phase transition log output
.It Li Chat	Generate Chat script trace log
.It Li LQM	Generate LQR report
.It Li LCP	Generate LCP/IPCP packet trace
.It Li TCP/IP	Dump TCP/IP packet
.It Li HDLC	Dump HDLC packet in hex
.It Li Async	Dump async level packet in hex
.El

The
.Dq set debug
command allows you to set logging output level, of which
multiple levels can be specified.  The default is equivalent to
.Dq set debug phase lcp .

.Sh MORE DETAILS

.Bl -bullet -compact
.It
Please read the Japanese doc for complete explanation. It may not be
useful for non-japanese readers,  but examples in the document may help
you to guess.

.It
Please read example configuration files.

.It
Use
.Dq help ,
.Dq show ?
and
.Dq set ?
commands.

.It
NetBSD and BSDI-1.0 were supported in previous releases but are no
longer supported in this release.  Please contact the author if you need
old driver code.
.El

.Sh FILES
.Nm
refers to three files: ppp.conf, ppp.linkup and ppp.secret.
These files are placed in
.Pa /etc/ppp ,
but the user can create his own files under his $HOME directory as
.ppp.conf,.ppp.linkup and .ppp.secret.
.Nm
will always try to consult the user's personal setup first.

.Bl -tag -width flag
.It $HOME/ppp/.ppp.[conf|linkup|secret]
User dependant configuration files.

.It /etc/ppp/ppp.conf
System default configuration file.

.It /etc/ppp/ppp.secret
An authorization file for each system.

.It /etc/ppp/ppp.linkup
A file to check when
.Nm
establishes a network level connection.

.It /var/log/ppp.log
Logging and debugging information file.

.It /var/spool/lock/Lck..* 
tty port locking file.

.It /var/run/PPP.system
Holds the pid for ppp -auto system.

.It /etc/services
Get port number if port number is using service name.
.El

.Sh HISTORY
This program was submitted in FreeBSD-2.0.5 Atsushi Murai (amurai@spec.co.jp).

.Sh AUTHORS
Toshiharu OHNO (tony-o@iij.ad.jp)
