#!./perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

use Getopt::Long;

print "1..18\n";

@ARGV = qw(-Foo -baR --foo bar);
Getopt::Long::Configure ("no_ignore_case");
%lnk = ();
print "ok 1\n" if GetOptions (\%lnk, "foo", "Foo=s");
print ((defined $lnk{foo})   ? "" : "not ", "ok 2\n");
print (($lnk{foo} == 1)      ? "" : "not ", "ok 3\n");
print ((defined $lnk{Foo})   ? "" : "not ", "ok 4\n");
print (($lnk{Foo} eq "-baR") ? "" : "not ", "ok 5\n");
print ((@ARGV == 1)          ? "" : "not ", "ok 6\n");
print (($ARGV[0] eq "bar")   ? "" : "not ", "ok 7\n");
print (!(exists $lnk{baR})   ? "" : "not ", "ok 8\n");

@ARGV = qw(-Foo -baR --foo bar);
Getopt::Long::Configure ("default","no_ignore_case");
%lnk = ();
my $foo;
print "ok 9\n" if GetOptions (\%lnk, "foo" => \$foo, "Foo=s");
print ((defined $foo)        ? "" : "not ", "ok 10\n");
print (($foo == 1)           ? "" : "not ", "ok 11\n");
print ((defined $lnk{Foo})   ? "" : "not ", "ok 12\n");
print (($lnk{Foo} eq "-baR") ? "" : "not ", "ok 13\n");
print ((@ARGV == 1)          ? "" : "not ", "ok 14\n");
print (($ARGV[0] eq "bar")   ? "" : "not ", "ok 15\n");
print (!(exists $lnk{foo})   ? "" : "not ", "ok 16\n");
print (!(exists $lnk{baR})   ? "" : "not ", "ok 17\n");
print (!(exists $lnk{bar})   ? "" : "not ", "ok 18\n");
