#!perl.exe

# Author: Chris Grant
# Copyright 1999, Codetalker Communications, Inc.
#
# This script takes a firewall log and breaks it into several 
# different files. Each file is named based on the service that
# runs on the port that was recognized in log line. After
# this script has run, you should end up with several files.
# Of course you will have the original log file and then files
# such as web.log, telnet.log, pop3.log, imap.log, backorifice.log,
# netbus.log, and unknown.log.
#
# The number of entries in unknown.log should be minimal. The
# mappings of the port numbers and file names are stored in the bottom
# of this file in the data section. Simply look at the ports being hit,
# find out what these ports do, and add them to the data section.
#
# You may be wondering why I haven't simply parsed RFC1700 to come up
# with a list of port numbers and files. The reason is that I don't
# believe reading firewall logs should be all that automated. You 
# should be familiar with what probes are hitting your system. By
# manually adding entries to the data section this ensures that I 
# have at least educated myself about what this protocol is, what 
# the potential exposure is, and why you might be seeing this traffic. 

%icmp = ();
%udp = ();
%tcp = ();
%openfiles = ();
$TIDBITSFILE = "unknown.log";

# Read the ports data from the end of this file and build the three hashes
while (<DATA>) {
    chomp;                                # trim the newline
    s/#.*//;                              # no comments
    s/^\s+//;                             # no leading white
    s/\s+$//;                             # no trailing white
    next unless length;                   # anything left?
    $_ = lc;                              # switch to lowercase
    ($proto, $identifier, $filename) = m/(\S+)\s+(\S+)\s+(\S+)/;
    SWITCH: {
	if ($proto =~ m/^icmp$/) { $icmp{$identifier} = $filename; last SWITCH; };
	if ($proto =~ m/^udp$/) { $udp{$identifier} = $filename; last SWITCH; };
        if ($proto =~ m/^tcp$/) { $tcp{$identifier} = $filename; last SWITCH; };
	die "An unknown protocol listed in the proto defs\n$_\n";
    }
}

$filename = shift;
unless (defined($filename)) { die "Usage: logfilter.pl <log file>\n"; }
open(LOGFILE, $filename) || die "Could not open the firewall log file.\n";
$openfiles{$filename} = "LOGFILE";

$linenum = 0;
while($line = <LOGFILE>) {

    chomp($line);
    $linenum++;

    # determine the protocol - send to unknown.log if not found
    SWITCH: {

	($line =~ m /\sicmp\s/) && do { 

	    #
	    # ICMP Protocol 
	    #
	    # Extract the icmp packet information specifying the type.
	    # 
	    # Note: Must check for ICMP first because this may be an ICMP reply
	    #       to a TCP or UDP connection (eg Port Unreachable).
	    
	    ($icmptype) = $line =~ m/icmp (\d+)\/\d+/;

	    $filename = $TIDBITSFILE;
	    $filename = $icmp{$icmptype} if (defined($icmp{$icmptype}));

	    last SWITCH; 
	  };

	($line =~ m /\stcp\s/) && do { 

	    # 
	    # TCP Protocol
	    #
	    # extract the source and destination ports and compare them to 
	    # known ports in the tcp hash. For the first match, place this
	    # line in the file specified by the tcp hash. Ignore one of the
	    # port matches if both ports happen to be known services.

	    ($sport, $dport) = $line =~ m/\d+\.\d+\.\d+\.\d+,(\d+) -> \d+\.\d+\.\d+\.\d+,(\d+)/;
	    #print "$line\n" unless (defined($sport) && defined($dport));

	    $filename = $TIDBITSFILE;
	    $filename = $tcp{$sport} if (defined($tcp{$sport}));
	    $filename = $tcp{$dport} if (defined($tcp{$dport}));

	    last SWITCH; 
	  };

	($line =~ m /\sudp\s/) && do { 

	    #
	    # UDP Protocol - same procedure as with TCP, different hash
	    # 

	    ($sport, $dport) = $line =~ m/\d+\.\d+\.\d+\.\d+,(\d+) -> \d+\.\d+\.\d+\.\d+,(\d+)/;

	    $filename = $TIDBITSFILE;
	    $filename = $udp{$sport} if (defined($udp{$sport}));
	    $filename = $udp{$dport} if (defined($udp{$dport}));

	    last SWITCH; 
	  };

	#
	# The default case is that the protocol was unknown
	#
	$filename = $TIDBITSFILE;
    }

    #
    # write the line to the appropriate file as determined above
    #
    # check for filename in the openfiles hash. if it exists then write
    # to the given handle. otherwise open a handle to the file and add
    # it to the hash of open files.
    
    if (defined($openfiles{$filename})) {
	$handle = $openfiles{$filename};
    } else {
	$handle = "HANDLE" . keys %openfiles;
	open ($handle, ">>".$filename) || die "Couldn't open|create the file $filename";
	$openfiles{$filename} = $handle;
    }
    print $handle "#$linenum\t $line\n";

}

# close all open file handles

foreach $key (keys %openfiles) {
    close($openfiles{$key});
}

close(LOGFILE);

__DATA__
icmp    3         destunreach.log
icmp    8         ping.log
icmp    9         router.log
icmp    10        router.log
icmp    11        ttl.log
tcp     23        telnet.log
tcp     25        smtp.log
udp     25        smtp.log
udp     53        dns.log
tcp     80        http.log
tcp     110       pop3.log
tcp     111       rpc.log
udp     111       rpc.log
tcp     137       netbios.log
udp     137       netbios.log
tcp     143       imap.log
udp     161       snmp.log
udp     370       backweb.log
udp     371       backweb.log
tcp     443       https.log
udp     443       https.log
udp     512       syslog.log
tcp     635       nfs.log           # NFS mount services
udp     635       nfs.log           # NFS mount services
tcp     1080      socks.log
udp     1080      socks.log
tcp     6112      games.log         # Battle net
tcp     6667      irc.log
tcp     7070      realaudio.log
tcp     8080      http.log
tcp     12345     netbus.log
udp     31337     backorifice.log