
BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use Config;

BEGIN {
    if(-d "lib" && -f "TEST") {
        if ($Config{'extensions'} !~ /\bIO\b/ && $^O ne 'VMS') {
	    print "1..0\n";
	    exit 0;
        }
    }
}

use IO::Handle;

print "1..6\n";
my $i = 1;
foreach (qw(SEEK_SET SEEK_CUR SEEK_END     _IOFBF    _IOLBF    _IONBF)) {
    my $d1 = defined(&{"IO::Handle::" . $_}) ? 1 : 0;
    my $v1 = $d1 ? &{"IO::Handle::" . $_}() : undef;
    my $v2 = IO::Handle::constant($_);
    my $d2 = defined($v2);

    print "not "
	if($d1 != $d2 || ($d1 && ($v1 != $v2)));
    print "ok ",$i++,"\n";
}
