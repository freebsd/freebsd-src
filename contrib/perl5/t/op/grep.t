#!./perl

#
# grep() and map() tests
#

print "1..27\n";

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

{
   print map({$_} ("ok $test\n"));
   $test++;
   print map
            ({$_} ("ok $test\n"));
   $test++;
   print((map({a => $_}, ("ok $test\n")))[0]->{a});
   $test++;
   print((map
            ({a=>$_},
	     ("ok $test\n")))[0]->{a});
   $test++;
   print map { $_ } ("ok $test\n");
   $test++;
   print map
            { $_ } ("ok $test\n");
   $test++;
   print((map {a => $_}, ("ok $test\n"))[0]->{a});
   $test++;
   print((map
            {a=>$_},
	     ("ok $test\n"))[0]->{a});
   $test++;
   my $x = "ok \xFF\xFF\n";
   print map($_&$x,("ok $test\n"));
   $test++;
   print map
            ($_ & $x, ("ok $test\n"));
   $test++;
   print map { $_ & $x } ("ok $test\n");
   $test++;
   print map
             { $_&$x } ("ok $test\n");
   $test++;

   print grep({$_} ("ok $test\n"));
   $test++;
   print grep
            ({$_} ("ok $test\n"));
   $test++;
   print grep({a => $_}->{a}, ("ok $test\n"));
   $test++;
   print grep
	     ({a => $_}->{a},
	     ("ok $test\n"));
   $test++;
   print grep { $_ } ("ok $test\n");
   $test++;
   print grep
             { $_ } ("ok $test\n");
   $test++;
   print grep {a => $_}->{a}, ("ok $test\n");
   $test++;
   print grep
	     {a => $_}->{a},
	     ("ok $test\n");
   $test++;
   print grep($_&"X",("ok $test\n"));
   $test++;
   print grep
            ($_&"X", ("ok $test\n"));
   $test++;
   print grep { $_ & "X" } ("ok $test\n");
   $test++;
   print grep
             { $_ & "X" } ("ok $test\n");
   $test++;
}
