#!./perl -T

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') { 
	@INC = qw(: ::lib ::macos:lib); 
    } else { 
	@INC = '.'; 
	push @INC, '../lib'; 
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bFile\/Glob\b/i) {
        print "1..0\n";
        exit 0;
    }
    print "1..2\n";
}
END {
    print "not ok 1\n" unless $loaded;
}
use File::Glob;
$loaded = 1;
print "ok 1\n";

# all filenames should be tainted
@a = File::Glob::bsd_glob("*");
eval { $a = join("",@a), kill 0; 1 };
unless ($@ =~ /Insecure dependency/) {
    print "not ";
}
print "ok 2\n";
