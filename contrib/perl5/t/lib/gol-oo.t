#!./perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

use Getopt::Long;
die("Getopt::Long version 2.24 required--this is only version ".
    $Getopt::Long::VERSION)
  unless $Getopt::Long::VERSION >= 2.24;
print "1..9\n";

@ARGV = qw(-Foo -baR --foo bar);
my $p = new Getopt::Long::Parser (config => ["no_ignore_case"]);
undef $opt_baR;
undef $opt_bar;
print "ok 1\n" if $p->getoptions ("foo", "Foo=s");
print ((defined $opt_foo)   ? "" : "not ", "ok 2\n");
print (($opt_foo == 1)      ? "" : "not ", "ok 3\n");
print ((defined $opt_Foo)   ? "" : "not ", "ok 4\n");
print (($opt_Foo eq "-baR") ? "" : "not ", "ok 5\n");
print ((@ARGV == 1)         ? "" : "not ", "ok 6\n");
print (($ARGV[0] eq "bar")  ? "" : "not ", "ok 7\n");
print (!(defined $opt_baR)  ? "" : "not ", "ok 8\n");
print (!(defined $opt_bar)  ? "" : "not ", "ok 9\n");
