#!./perl

#
# grep() and map() tests
#

print "1..3\n";

$test = 1;

sub ok {
    my ($got,$expect) = @_;
    print "# expected [$expect], got [$got]\nnot " if $got ne $expect;
    print "ok $test\n";
}

{
   my @lol = ([qw(a b c)], [], [qw(1 2 3)]);
   my @mapped = map  {scalar @$_} @lol;
   ok "@mapped", "3 0 3";
   $test++;

   my @grepped = grep {scalar @$_} @lol;
   ok "@grepped", "$lol[0] $lol[2]";
   $test++;

   @grepped = grep { $_ } @mapped;
   ok "@grepped", "3 3";
   $test++;
}

