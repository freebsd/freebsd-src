#!./perl

#
# test glob() in File::DosGlob
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..10\n";

# override it in main::
use File::DosGlob 'glob';

# test if $_ takes as the default
$_ = "lib/a*.t";
my @r = glob;
print "not " if $_ ne 'lib/a*.t';
print "ok 1\n";
# we should have at least abbrev.t, anydbm.t, autoloader.t
print "# |@r|\nnot " if @r < 3;
print "ok 2\n";

# check if <*/*> works
@r = <*/a*.t>;
# atleast {argv,abbrev,anydbm,autoloader,append,arith,array,assignwarn,auto}.t
print "not " if @r < 9;
print "ok 3\n";
my $r = scalar @r;

# check if scalar context works
@r = ();
while (defined($_ = <*/a*.t>)) {
    print "# $_\n";
    push @r, $_;
}
print "not " if @r != $r;
print "ok 4\n";

# check if list context works
@r = ();
for (<*/a*.t>) {
    print "# $_\n";
    push @r, $_;
}
print "not " if @r != $r;
print "ok 5\n";

# test if implicit assign to $_ in while() works
@r = ();
while (<*/a*.t>) {
    print "# $_\n";
    push @r, $_;
}
print "not " if @r != $r;
print "ok 6\n";

# test if explicit glob() gets assign magic too
my @s = ();
while (glob '*/a*.t') {
    print "# $_\n";
    push @s, $_;
}
print "not " if "@r" ne "@s";
print "ok 7\n";

# how about in a different package, like?
package Foo;
use File::DosGlob 'glob';
@s = ();
while (glob '*/a*.t') {
    print "# $_\n";
    push @s, $_;
}
print "not " if "@r" ne "@s";
print "ok 8\n";

# test if different glob ops maintain independent contexts
@s = ();
while (<*/a*.t>) {
    my $i = 0;
    print "# $_ <";
    push @s, $_;
    while (<*/b*.t>) {
        print " $_";
	$i++;
    }
    print " >\n";
}
print "not " if "@r" ne "@s";
print "ok 9\n";

# how about a global override, hm?
eval <<'EOT';
use File::DosGlob 'GLOBAL_glob';
package Bar;
@s = ();
while (<*/a*.t>) {
    my $i = 0;
    print "# $_ <";
    push @s, $_;
    while (glob '*/b*.t') {
        print " $_";
	$i++;
    }
    print " >\n";
}
print "not " if "@r" ne "@s";
print "ok 10\n";
EOT
