#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	if ($^O eq 'MacOS') { 
	    @INC = qw(: ::lib ::macos:lib); 
	} else { 
	    @INC = '../lib'; 
	}
    }
}

use Errno;

print "1..5\n";

print "not " unless @Errno::EXPORT_OK;
print "ok 1\n";
die unless @Errno::EXPORT_OK;

$err = $Errno::EXPORT_OK[0];
$num = &{"Errno::$err"};

print "not " unless &{"Errno::$err"} == $num;
print "ok 2\n";

$! = $num;
print "not " unless $!{$err};
print "ok 3\n";

$! = 0;
print "not " if $!{$err};
print "ok 4\n";

$s1 = join(",",sort keys(%!));
$s2 = join(",",sort @Errno::EXPORT_OK);

if($s1 ne $s2) {
    my @s1 = keys(%!);
    my @s2 = @Errno::EXPORT_OK;
    my(%s1,%s2);
    @s1{@s1} = ();
    @s2{@s2} = ();
    delete @s2{@s1};
    delete @s1{@s2};
    print "# These are only in \%!\n";
    print "# ",join(" ",map { "'$_'" } keys %s1),"\n";
    print "# These are only in \@EXPORT_OK\n";
    print "# ",join(" ",map { "'$_'" } keys %s2),"\n";
    print "not ";
}

print "ok 5\n";
