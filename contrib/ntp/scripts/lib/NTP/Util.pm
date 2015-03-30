package NTP::Util;
use strict;
use warnings;
use Exporter 'import';
use Carp;
use version 0.77;

our @EXPORT_OK = qw(ntp_read_vars do_dns ntp_peers ntp_sntp_line);

my $ntpq_path = 'ntpq';
my $sntp_path = 'sntp';

our $IP_AGNOSTIC;

BEGIN {
    require Socket;
    if (version->parse($Socket::VERSION) >= version->parse(1.94)) {
        Socket->import(qw(getaddrinfo getnameinfo SOCK_RAW AF_INET));
        $IP_AGNOSTIC = 1;
    }
    else {
        Socket->import(qw(inet_aton SOCK_RAW AF_INET));
    }
}

my %obsolete_vars = (
    phase          => 'offset',
    rootdispersion => 'rootdisp',
);

sub ntp_read_vars {
    my ($peer, $vars, $host) = @_;
    my $do_all   = !@$vars;
    my %out_vars = map {; $_ => undef } @$vars;

    $out_vars{status_line} = {} if $do_all;

    my $cmd = "$ntpq_path -n -c 'rv $peer ".(join ',', @$vars)."'";
    $cmd .= " $host" if defined $host;
    $cmd .= " |";

    open my $fh, $cmd or croak "Could not start ntpq: $!";

    while (<$fh>) {
        return undef if /Connection refused/;

        if (/^asso?c?id=0 status=(\S{4}) (\S+), (\S+),/gi) {
            $out_vars{status_line}{status} = $1;
            $out_vars{status_line}{leap}   = $2;
            $out_vars{status_line}{sync}   = $3;
        }

        while (/(\w+)=([^,]+),?\s/g) {
            my ($var, $val) = ($1, $2);
            $val =~ s/^"([^"]+)"$/$1/;
            $var = $obsolete_vars{$var} if exists $obsolete_vars{$var};
            if ($do_all) {
                $out_vars{$var} = $val
            }
            else {
                $out_vars{$var} = $val if exists $out_vars{$var};
            }
        }
    }

    close $fh or croak "running ntpq failed: $! (exit status $?)";
    return \%out_vars;
}

sub do_dns {
    my ($host) = @_;

    if ($IP_AGNOSTIC) {
        my ($err, $res);

        ($err, $res) = getaddrinfo($host, '', {socktype => SOCK_RAW});
        die "getaddrinfo failed: $err\n" if $err;

        ($err, $res) = getnameinfo($res->{addr}, 0);
        die "getnameinfo failed: $err\n" if $err;

        return $res;
    }
    # Too old perl, do only ipv4
    elsif ($host =~ /^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})$/) {
        return gethostbyaddr inet_aton($host), AF_INET;
    }
    else {
        return;
    }
}

sub ntp_peers {
    my ($host) = @_;

    my $cmd = "$ntpq_path -np $host |";

    open my $fh, $cmd or croak "Could not start ntpq: $!";

    <$fh> for 1 .. 2;

    my @columns = qw(remote refid st t when poll reach delay offset jitter);
    my @peers;
    while (<$fh>) {
        if (/(?:[\w\.\*-]+\s*){10}/) {
            my $col = 0;
            push @peers, { map {; $columns[ $col++ ] => $_ } split /(?<=.)\s+/ };
        }
        else {
            #TODO return error (but not needed anywhere now)
            warn "ERROR: $_";
        }
    }

    close $fh or croak "running ntpq failed: $! (exit status $?)";
    return \@peers;
}

# TODO: we don't need this but it would be nice to have all the line parsed
sub ntp_sntp_line {
    my ($host) = @_;

    my $cmd = "$sntp_path $host |";
    open my $fh, $cmd or croak "Could not start sntp: $!";

    my ($offset, $stratum);
    while (<$fh>) {
        next if !/^\d{4}-\d\d-\d\d/;
        chomp;
        my @output = split / /;

        $offset = $output[3];
        ($stratum = pop @output) =~ s/s(\d{1,2})/$1/;
    }
    close $fh or croak "running sntp failed: $! (exit status $?)";
    return ($offset, $stratum);
}
