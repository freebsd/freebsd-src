#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bOpcode\b/ && $Config{'osname'} ne 'VMS') {
        print "1..0\n";
        exit 0;
    }
}

print "1..2\n";

eval <<'EOP';
	no ops 'fileno';	# equiv to "perl -M-ops=fileno"
	$a = fileno STDIN;
EOP

print $@ =~ /trapped/ ? "ok 1\n" : "not ok 1\n# $@\n";

eval <<'EOP';
	use ops ':default';	# equiv to "perl -M(as above) -Mops=:default"
	eval 1;
EOP

print $@ =~ /trapped/ ? "ok 2\n" : "not ok 2\n# $@\n";

1;
