#!./perl

# Check for presence and correctness of .ph files; for now,
# just socket.ph and pals.
#   -- Kurt Starsinic <kstar@isinet.com>

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

# All the constants which Socket.pm tries to make available:
my @possibly_defined = qw(
    INADDR_ANY INADDR_LOOPBACK INADDR_NONE AF_802 AF_APPLETALK AF_CCITT
    AF_CHAOS AF_DATAKIT AF_DECnet AF_DLI AF_ECMA AF_GOSIP AF_HYLINK AF_IMPLINK
    AF_INET AF_LAT AF_MAX AF_NBS AF_NIT AF_NS AF_OSI AF_OSINET AF_PUP
    AF_SNA AF_UNIX AF_UNSPEC AF_X25 MSG_DONTROUTE MSG_MAXIOVLEN MSG_OOB
    MSG_PEEK PF_802 PF_APPLETALK PF_CCITT PF_CHAOS PF_DATAKIT PF_DECnet PF_DLI
    PF_ECMA PF_GOSIP PF_HYLINK PF_IMPLINK PF_INET PF_LAT PF_MAX PF_NBS PF_NIT
    PF_NS PF_OSI PF_OSINET PF_PUP PF_SNA PF_UNIX PF_UNSPEC PF_X25 SOCK_DGRAM
    SOCK_RAW SOCK_RDM SOCK_SEQPACKET SOCK_STREAM SOL_SOCKET SOMAXCONN
    SO_ACCEPTCONN SO_BROADCAST SO_DEBUG SO_DONTLINGER SO_DONTROUTE SO_ERROR
    SO_KEEPALIVE SO_LINGER SO_OOBINLINE SO_RCVBUF SO_RCVLOWAT SO_RCVTIMEO
    SO_REUSEADDR SO_SNDBUF SO_SNDLOWAT SO_SNDTIMEO SO_TYPE SO_USELOOPBACK
);


# The libraries which I'm going to require:
my @libs = qw(Socket "sys/types.ph" "sys/socket.ph" "netinet/in.ph");


# These are defined by Socket.pm even if the C header files don't define them:
my %ok_to_miss = (
    INADDR_NONE         => 1,
    INADDR_LOOPBACK     => 1,
);


my $total_tests = scalar @libs + scalar @possibly_defined;
my $i           = 0;

print "1..$total_tests\n";


foreach (@libs) {
    $i++;

    if (eval "require $_" ) {
        print "ok $i\n";
    } else {
        print "# Skipping tests; $_ may be missing\n";
        foreach ($i .. $total_tests) { print "ok $_\n" }
        exit;
    }
}


foreach (@possibly_defined) {
    $i++;

    $pm_val = eval "Socket::$_()";
    $ph_val = eval "main::$_()";

    if (defined $pm_val and !defined $ph_val) {
        if ($ok_to_miss{$_}) { print "ok $i\n" }
        else                 { print "not ok $i\n" }
        next;
    } elsif (defined $ph_val and !defined $pm_val) {
        print "not ok $i\n";
        next;
    }

    # Socket.pm converts these to network byte order, so we convert the
    # socket.ph version to match; note that these cases skip the following
    # `elsif', which is only applied to _numeric_ values, not literal
    # bitmasks.
    if ($_ eq 'INADDR_ANY'
    or  $_ eq 'INADDR_LOOPBACK'
    or  $_ eq 'INADDR_NONE') {
        $ph_val = pack("N*", $ph_val);  # htonl(3) equivalent
    }

    # Since Socket.pm and socket.ph wave their hands over macros differently,
    # they could return functionally equivalent bitmaps with different numeric
    # interpretations (due to sign extension).  The only apparent case of this
    # is SO_DONTLINGER (only on Solaris, and deprecated, at that):
    elsif ($pm_val != $ph_val) {
        $pm_val = oct(sprintf "0x%lx", $pm_val);
        $ph_val = oct(sprintf "0x%lx", $ph_val);
    }

    if ($pm_val == $ph_val) { print "ok $i\n" }
    else                    { print "not ok $i\n" }
}


