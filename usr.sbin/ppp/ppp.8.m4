.\" manual page [] for ppp 0.94 beta2 + alpha
.\" $Id: ppp.8,v 1.1.1.1 1995/01/31 06:29:58 amurai Exp $
.\" SH section heading
.\" SS subsection heading
.\" LP paragraph
.\" IP indented paragraph
.\" TP hanging label
.TH PPP 8
.SH NAME
ppp \- Point to Point Protocol (aka iijppp)
.SH SYNOPSIS
.B ppp
[
.I -auto | -direct -dedicated
] [
.I system
]
.SH DESCRIPTION
.LP
This is user process \fIPPP\fR software package. Normally, \fIPPP\fR
is implemented as a part of kernel and hard to debug and/or modify its
behavior. However, in this implementation, \fIPPP\fR is implemented as
a user process with the help of tunnel device driver.
.LP

.SH Major Features

.TP 2
o Provide interactive user interface. Using its command mode, user can
easily enter commands to establish the connection with the peer, check
the status of connection, and close the connection.  And now, all
functions has password protected if describe your hostname/password in
secret file or exist secret file itself.

.TP 2
o Supports both of manual and automatic dialing. Interactive mode has
``term'' command which enables you to talk to your modem
directory. When your modem is connected to the peer, and it starts to
speak \fIPPP\fR, \fIPPP\fR software detects it and turns into packet
mode automatically. Once you have convinced how to connect with the
peer, you can write chat script to define necessary dialing and login
procedure for later convenience.

.TP 2
o Supports on-demand dialup capability. By using auto mode, \fIPPP\fR
program will act as a daemon and wait for the packet send to the peer. 
Once packet is found, daemon automatically dials and establish the
connection.

.TP 2
o
Can act as server which accept incoming \fIPPP\fR connection. 
                 
.TP 2
o
Supports PAP and CHAP authentification.                                     

.TP 2
o
Supports Proxy Arp.

.TP 2
o Supports packet filtering. User can define four kinds of filters;
ifilter for incoming packet, ofilter for outgoing packet, dfilter to
define dialing trigger packet and afilter to keep alive a connection
by trigger packet.

.TP 2
o Tunnel driver supports bpf. That is, user can use tcpdump to check
packet flow over the \fIPPP\fR link.

.TP 2
o
Supports \fIPPP\fR over TCP capability. 

.TP 2
o
Supports IETF draft Predictor-1 compression.

.TP 2
o Runs under BSDI-1.1 and FreeBSD-1.1. Patch for NeXTSTEP 3.2 is also
available on the net.

.SH MANUAL DIALING

   % ppp
   User Process PPP written by Toshiharu OHNO.
   -- If you write your hostname and password in ppp.secret,
      you can't do anything even quit command --
   ppp on tama> quit
   what ?
   ppp on tama> pass <password>
   -- You can specify modem and device name using following commands.
   ppp ON tama> set line /dev/cua01
   ppp ON tama> set speed 38400
   ppp ON tama> set parity even
   ppp ON tama> show modem
   -- Modem related parameters are shown in here
   ppp ON tama>
   -- Use term command to talk with your modem
   ppp ON tama> term
   at
   OK
   atdt123456
   CONNECT

   login: ppp
   Password:
   -- PPP started in remote side ---

   -- When peer start to speak PPP, the program will detect it
   -- automatically and back to command mode.
   ppp on tama>
   \fBPPP\fR>

   -- NOW, you are get connected !! Note that prompt has changed to
   -- capital letters
   PPP ON tama> show lcp

   -- You'll see LCP status --

   PPP ON tama> show ipcp

   -- You'll see IPCP status --
   -- At this point, your machine has host route to the peer.
   -- If your want to add default route entry, then enter

   PPP ON tama> add 0 0 HISADDR

   -- Here string `HISADDR' represents IP address of connected peer.

   PPP ON tama>
   -- Use applications (i.e. ping, telnet, ftp) in other windows

   PPP ON tama> show log

   -- Gives you some logging messages

   PPP ON tama> close

   -- Connection is closed, and modem will be hanged.

   ppp ON tama> quit
   %
.LP

.SH AUTOMATIC DIALING

.LP
To use automatic dialing, you must prepare Dial and Login chat script.
See example definition found in ppp.conf.sample (Format of ppp.conf is
pretty simple.)

.TP 2
o
Each line contains one command, label or comment.

.TP 2
o 
Line stating with # is treated as a comment line.

.TP 2
o
Label name has to start from first column and should be followed by colon (:).

.TP 2
o
Command line must contains space or tab at first column.

.LP
If ppp.conf is ready, specify destination label name when you invoke
ppp. Commands associated with destination label is executed when ppp
command is invoked. Note that commands associated with ``default''
label is ALWAYS executed.

Once connection is made, you'll find that prompt is changed to

 capital \fIPPP\fR>.

   % ppp pm2
   ...
   ppp ON tama> dial
   dial OK!
   login OK!
   PPP ON tama>

If ppp.linkup file is available, its contents are executed when
\fIPPP\fR link is connected.  See example which add default route.
The string HISADDR matches with IP address of connected peer.


.SH DAIL ON DEMAND

.LP
 To play with demand dialing, you must use -auto option. Also, you
must specify destination label with proper setup in ppp.conf. It must
contain ``ifaddr'' command to define peer's IP address. (refer
/etc/ppp/ppp.conf.sample)


   % ppp -auto pm2demand
   ...
   %

.LP
When -auto is specified, \fIPPP\fR program works as a daemon.  But,
you are still able to use command features to check its behavior.


  % telnet localhost 3000
  ...
  PPP on tama> show ipcp
  ....

.LP
 Each ppp has associated port number, which is computed as "3000 +
tunnel_device_number". If 3000 is not good base number, edit defs.h.
When packet toward to remote network is detected, \fIPPP\fR will take
dialing action and try to connect with the peer. If dialing is failed,
program will wait for 30 seconds. Once this hold time expired, another
trigger packet cause dialing action. Note that automatic re-dialing is
NOT implemented.


 To terminate program, use

  PPP on tama> close
  \fBppp\fR> quit all

.LP
 Simple ``quit'' command will terminates telnet connection, but \fIPPP\fR program itself is not terminated. You must use ``quit all'' to terminate the program running as daemon.
.LP

.SH PACKET FILTERING

.LP
This implementation supports packet filtering. There are three filters; ifilter, ofilter and dfilter. Here's some basics.
.LP

.TP 2
o
Filter definition has next syntax.

   set filter-name rule-no action [src_addr/src_width] [dst_addr/dst_width]
       [proto [src [lt|eq|gt] port ] [dst [lt|eq|gt] port] [estab]

   a) filter-name should be ifilter, ofilter or dfiler.
   
   b) There are two actions permit and deny. If given packet is matched
      against the rule, action is taken immediately.

   c) src_width and dst_width works like a netmask to represent address range.

   d) proto must be one of icmp, udp or tcp.

.TP 2
o
Each filter can hold upto 20 rules. Rule number starts from 0.  Entire rule set is not effective until rule 0 is defined.

.TP 2
o
If no rule is matched with a packet, that packet will be discarded (blocked).

.TP 2
o
Use ``set filer-name -1'' to flush all rules.

.LP
 See /etc/ppp/ppp.conf.filter.example
.LP

.SH RECEIVE INCOMING PPP CONNECTION

.LP
 To receive incoming \fIPPP\fR connection request, follow next steps. 
.LP

 a) Prepare bidir entry in your /etc/gettytab

	bidir.38400:\
	    :bi:ap:hf:tc=38400-baud:

 b) Edit /etc/ttys to enable getty on the port where modem is attached.

	cua00  "/usr/libexec/getty stdir.38400" dialup on

    Don't forget to send HUP signal to init process.

	# kill -HUP 1

 c) Prepare account for incoming user.

ppp:*:21:0:PPP Login User:/home/ppp:/usr/local/bin/ppplogin

 d) Create /usr/local/bin/ppplogin file with next contents.

	#!/bin/sh
	/usr/local/bin/ppp -direct

    You can specify label name for further control.

.LP
 Direct mode (-direct) lets \fIPPP\fR to work with standard in and out.  Again, you can telnet to 3000 to get command mode control.
.LP

.SH SETTING IDLE TIMER

.LP
 To check/set idletimer, use ``show timeout'' and ``set timeout'' command.
.LP

	Ex. ppp> set timeout 600

.LP
 Timeout period is measured in secs and default value is 180 or 3 min. To disable idle timer function, use ``set timeout 0''.
.LP

.LP
 In -auto mode, idle timeout cause \fIPPP\fR session closed. However, \fIPPP\fR program itself is keep running. Another trigger packet cause dialing action.
.LP

.SH Predictor-1 compression

.LP
 This version supports CCP and Predictor type 1 compression based on current IETF-draft specs. As a default behavior, \fIPPP\fR will propose to use (or willing to accept) this capability and use it if peer agrees (or requests).
.LP

.LP
 To disable CCP/predictor function completely, use ``disable pred'' and ``deny pred'' command.
.LP

.SH Controlling IP address

.LP
 \fIPPP\fR uses IPCP to negotiate IP addresses. Each side of node
informs IP address that willing to use to the peer, and if requested
IP address is acceptable, \fIPPP\fR returns ACK to
requester. Otherwise, \fIPPP\fR returns NAK to suggest the peer to use
different IP address. When both side of nodes agrees to accept the
received request (and send ACK), IPCP is reached to open state and
network level connection is established.


.LP
 To control, this IPCP behavior, this implementation has ``set
ifaddr'' to define MY and HIS IP address.


.TP3
ifaddr src_addr dst_addr

.LP
Where, src_addr is the IP address that my side is willing to use, and
dst_addr is the IP address which his side should use.
.LP

.TP3
ifaddr 192.244.177.38 192.244.177.2

For example, above specification means

.TP
o I strongly want to use 192.244.177.38 as my side. I'll disagree when
peer suggest me to use other addresses.

.TP 2
o I strongly insists peer to use 192.244.177.2 as his side address.  I
don't permit him to use any IP address but 192.244.177.2.  When peer
request other IP address, I always suggest him to use 192.244.177.2.

.LP
 This is all right, when each side has pre-determined IP address.
However, it is often the case one side is acting as a server which
controls IP address and the other side should obey the direction from
him.  In order to allow more flexible behavior, `ifaddr' command
allows user to specify IP address more loosely.


.TP 2
ifaddr 192.244.177.38/24 192.244.177.2/20

 Number followed by slash (/) represents number of bits significant in
IP address. That is, this example means

.TP 2
o I'd like to use 192.244.177.38 as my side address, if it is
possible.  But I also accept any IP address between 192.244.177.0 and
192.244.177.255.
 
.TP 2
o I'd like to make him to use 192.244.177.2 as his side address.  But
I also permit him to use any IP address between 192.244.176.0 and
192.244.191.255.

 Notes:

.TP 2
o As you may have already noticed, 192.244.177.2 is equivalent to say
192.244.177.2/32.

.TP 2
o As an exception, 0 is equivalent to 0.0.0.0/0. Means, I have no idea
about IP address and obey what he says.

.TP 2
o 192.244.177.2/0 means that I'll accept/permit any IP address but
I'll try to insist to use 192.244.177.2 at first.

.SH Connecting with service provider

.LP
  1) Describe provider's phone number in DialScript. Use ``set dial'' or
     ``set phone'' command.

  2) Describle login procedure in LoginScript. Use ``set login'' command.

.TP
3) Use ``set ifaddr'' command to define IP address.

     o If you know what IP address provider uses, then use it as his address.

     o If provider has assigned particular IP address for you, then use it
       as my address.

     o If provider assigns your address dynamically, use 0 as my address.

     o If you have no info on IP addresses, then try

	set ifaddr 0 0
.TP 2
4) If provider request you to use PAP/CHAP auth method,
add next lines into your ppp.conf.

.TP 3
.B enable pap (or enable chap)
.TP 3
.B disable chap	(or disable pap)
.TP 3
.B set authname MyName
.TP 3
.B set authkey MyPassword
.TP 3

.LP
Please refer /etc/ppp/ppp.conf.iij for some real examples.
.LP

.SH Logging facility

.LP
 \fI\fIPPP\fR\fR is able to generate following level log info as
/var/log/ppp.log


.TP
.B Phase
Phase transition log output
.TP 
.B Chat
Generate Chat script trace log
.TP 
.B LQM
Generate LQR report
.TP 
.B LCP
Generate LCP/IPCP packet trace
.TP 
.B TCP/IP
Dump TCP/IP packet
.TP 
.B HDLC
Dump HDLC packet in hex
.TP 
.B Async
Dump async level packet in hex

.LP
``set debug'' command allows you to set logging output level, and
multiple levels can be specified.  Default is equivalent to ``set
debug phase lcp''.

.SH For more details

.TP 2
o
Please read Japanese doc for complete explanation.
Well, it is not useful for non-japanese readers, 
but examples in the document may help you to guess.

.TP 2
o
Please read example configuration files.

.TP 2
o
Use ``help'', ``show ?'' and ``set ?'' command.

.TP 2
o
NetBSD and BSDI-1.0 has been supported in previous release,
but no longer supported in this release.
Please contact to author if you need old driver code.

.SH FILES
.LP
\fIPPP\fR may refers three files, ppp.conf, ppp.linkup and ppp.secret.
These files are placed in /etc/ppp, but user can create his own files
under HOME directory as .ppp.conf,.ppp.linkup and .ppp.secret.the ppp
always try to consult to user's personal setup first.

.TP
.B $HOME/ppp/.ppp.[conf|linkup|secret]
User depend configuration files.

.TP
.B /etc/ppp/ppp.conf
System default configuration file.

.TP
.B /etc/ppp/ppp.secret
A authorization file for each system.

.TP
.B /etc/ppp/ppp.linkup
A checking file when
.I ppp
establishes network level connection.

.TP
.B /var/log/ppp.log
Logging and debug information file.

.TP
.B /var/spool/lock/Lck..* 
tty port locking file.

.SH BUGS

.SH HISTORY
This programm has deliverd into core since FreeBSD-2.1 by Atsushi
Murai (amurai@spec.co.jp).

.SH AUTHORS
Toshiharu OHNO (tony-o@iij.ad.jp)
