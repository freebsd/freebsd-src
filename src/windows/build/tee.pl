# Usage 'tee filename'
# Make sure that when using this as a perl pipe you
# print a EOF char!
#  (This may be a bug in perl 4 for NT)
#
# Use it like:
#	open(PIPE, "|$^X tee.pl foo.log") || die "Can't pipe";
#	open(STDOUT, ">&PIPE") || die "Can't dup pipe to stdout";
#	open(STDERR, ">&PIPE") || die "Can't dup pipe to stderr";

use IO::File;

#$SIG{'INT'} = \&handler;
#$SIG{'QUIT'} = \&handler;

$SIG{'INT'} = 'IGNORE';
$SIG{'QUIT'} = \&handler;

my $fh = new IO::File;

my $arg = shift;
my $file;
my $access = ">";

while ($arg) {
    if ($arg =~ /-a/) {
	$access = ">>";
    } elsif ($arg =~ /-i/) {
	$SIG{'INT'} = 'IGNORE';
	$SIG{'QUIT'} = 'IGNORE';
    } else {
	$file = $arg;
	last;
    }
    $arg = shift;
}

STDOUT->autoflush(1);

if ($file) {
    $fh->open($access.$file) || die "Could not open $file\n";
    $fh->autoflush(1);
}

while (<>) {
    $_ = &logtime.$_;
    print $_;
    print $fh $_ if $file;
}

sub logtime {
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
    $mon = $mon + 1;
    $year %= 100;
    sprintf ("[%02d/%02d/%02d %02d:%02d:%02d] ",
	     $year, $mon, $mday,
	     $hour, $min, $sec);
}

sub handler {
    my $sig = shift;
    my $bailmsg = &logtime."Bailing out due to SIG$sig!\n";
    my $warnmsg = <<EOH;
*********************************
* FUTURE BUILDS MAY FAIL UNLESS *
* BUILD DIRECTORIES ARE CLEANED *
*********************************
EOH
    print $bailmsg, $warnmsg;
    print $fh $bailmsg, $warnmsg;
    print "Closing log...";
    undef $fh if $fh;
    print "closed!\n";
    exit(2);
}

END {
    undef $fh if $fh;
}
