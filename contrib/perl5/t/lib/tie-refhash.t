#!/usr/bin/perl -w
# 
# Basic test suite for Tie::RefHash and Tie::RefHash::Nestable.
# 
# The testing is in two parts: first, run lots of tests on both a tied
# hash and an ordinary un-tied hash, and check they give the same
# answer.  Then there are tests for those cases where the tied hashes
# should behave differently to normal hashes, that is, when using
# references as keys.
# 

BEGIN {
    chdir 't' if -d 't';
    @INC = '.'; 
    push @INC, '../lib';
}    

use strict;
use Tie::RefHash;
use Data::Dumper;
my $numtests = 34;
my $currtest = 1;
print "1..$numtests\n";

my $ref = []; my $ref1 = [];

# Test standard hash functionality, by performing the same operations
# on a tied hash and on a normal hash, and checking that the results
# are the same.  This does of course assume that Perl hashes are not
# buggy :-)
# 
my @tests = standard_hash_tests();

my @ordinary_results = runtests(\@tests, undef);
foreach my $class ('Tie::RefHash', 'Tie::RefHash::Nestable') {
    my @tied_results = runtests(\@tests, $class);
    my $all_ok = 1;

    die if @ordinary_results != @tied_results;
    foreach my $i (0 .. $#ordinary_results) {
        my ($or, $ow, $oe) = @{$ordinary_results[$i]};
        my ($tr, $tw, $te) = @{$tied_results[$i]};
        
        my $ok = 1;
        local $^W = 0;
        $ok = 0 if (defined($or) != defined($tr)) or ($or ne $tr);
        $ok = 0 if (defined($ow) != defined($tw)) or ($ow ne $tw);
        $ok = 0 if (defined($oe) != defined($te)) or ($oe ne $te);
        
        if (not $ok) {
            print STDERR
              "failed for $class: $tests[$i]\n",
              "ordinary hash gave:\n",
              defined $or ? "\tresult:    $or\n" : "\tundef result\n",
              defined $ow ? "\twarning:   $ow\n" : "\tno warning\n",
              defined $oe ? "\texception: $oe\n" : "\tno exception\n",
              "tied $class hash gave:\n",
              defined $tr ? "\tresult:    $tr\n" : "\tundef result\n",
              defined $tw ? "\twarning:   $tw\n" : "\tno warning\n",
              defined $te ? "\texception: $te\n" : "\tno exception\n",
              "\n";
            $all_ok = 0;
        }
    }
    test($all_ok);
}

# Now test Tie::RefHash's special powers
my (%h, $h);
$h = eval { tie %h, 'Tie::RefHash' };
warn $@ if $@;
test(not $@);
test(ref($h) eq 'Tie::RefHash');
test(defined(tied(%h)) and tied(%h) =~ /^Tie::RefHash/);
$h{$ref} = 'cholet';
test($h{$ref} eq 'cholet');
test(exists $h{$ref});
test((keys %h) == 1);
test(ref((keys %h)[0]) eq 'ARRAY');
test((keys %h)[0] eq $ref);
test((values %h) == 1);
test((values %h)[0] eq 'cholet');
my $count = 0;
while (my ($k, $v) = each %h) {
    if ($count++ == 0) {
        test(ref($k) eq 'ARRAY');
        test($k eq $ref);
    }
}
test($count == 1);
delete $h{$ref};
test(not defined $h{$ref});
test(not exists($h{$ref}));
test((keys %h) == 0);
test((values %h) == 0);
undef $h;
untie %h;

# And now Tie::RefHash::Nestable's differences from Tie::RefHash.
$h = eval { tie %h, 'Tie::RefHash::Nestable' };
warn $@ if $@;
test(not $@);
test(ref($h) eq 'Tie::RefHash::Nestable');
test(defined(tied(%h)) and tied(%h) =~ /^Tie::RefHash::Nestable/);
$h{$ref}->{$ref1} = 'bungo';
test($h{$ref}->{$ref1} eq 'bungo');

# Test that the nested hash is also tied (for current implementation)
test(defined(tied(%{$h{$ref}}))
     and tied(%{$h{$ref}}) =~ /^Tie::RefHash::Nestable=/ );

test((keys %h) == 1);
test((keys %h)[0] eq $ref);
test((keys %{$h{$ref}}) == 1);
test((keys %{$h{$ref}})[0] eq $ref1);


die "expected to run $numtests tests, but ran ", $currtest - 1
  if $currtest - 1 != $numtests;

@tests = ();
undef $ref;
undef $ref1;

exit();


# Print 'ok X' if true, 'not ok X' if false
# Uses global $currtest.
# 
sub test {
    my $t = shift;
    print 'not ' if not $t;
    print 'ok ', $currtest++, "\n";
}


# Wrapper for Data::Dumper to 'dump' a scalar as an EXPR string. 
sub dumped {
    my $s = shift;
    my $d = Dumper($s);
    $d =~ s/^\$VAR1 =\s*//;
    $d =~ s/;$//;
    chomp $d;
    return $d;
}

# Crudely dump a hash into a canonical string representation (because
# hash keys can appear in any order, Data::Dumper may give different
# strings for the same hash).
# 
sub dumph {
    my $h = shift;
    my $r = '';
    foreach (sort keys %$h) {
        $r = dumped($_) . ' => ' . dumped($h->{$_}) . "\n";
    }
    return $r;
}

# Run the tests and give results.
# 
# Parameters: reference to list of tests to run
#             name of class to use for tied hash, or undef if not tied
# 
# Returns: list of [R, W, E] tuples, one for each test.
# R is the return value from running the test, W any warnings it gave,
# and E any exception raised with 'die'.  E and W will be tidied up a
# little to remove irrelevant details like line numbers :-)
# 
# Will also run a few of its own 'ok N' tests.
# 
sub runtests {
    my ($tests, $class) = @_;
    my @r;

    my (%h, $h);
    if (defined $class) {
        $h = eval { tie %h, $class };
        warn $@ if $@;
        test(not $@);
        test(ref($h) eq $class);
        test(defined(tied(%h)) and tied(%h) =~ /^\Q$class\E/);
    }

    foreach (@$tests) {
        my ($result, $warning, $exception);
        local $SIG{__WARN__} = sub { $warning .= $_[0] };
        $result = scalar(eval $_);
        if ($@)
         {
          die "$@:$_" unless defined $class;
          $exception = $@;
         }

        foreach ($warning, $exception) {
            next if not defined;
            s/ at .+ line \d+\.$//mg;
            s/ at .+ line \d+, at .*//mg;
            s/ at .+ line \d+, near .*//mg;
        }

        my (@warnings, %seen);
        foreach (split /\n/, $warning) {
            push @warnings, $_ unless $seen{$_}++;
        }
        $warning = join("\n", @warnings);

        push @r, [ $result, $warning, $exception ];
    }

    return @r;
}


# Things that should work just the same for an ordinary hash and a
# Tie::RefHash.
# 
# Each test is a code string to be eval'd, it should do something with
# %h and give a scalar return value.  The global $ref and $ref1 may
# also be used.
# 
# One thing we don't test is that the ordering from 'keys', 'values'
# and 'each' is the same.  You can't reasonably expect that.
# 
sub standard_hash_tests {
    my @r;

    # Library of standard tests on keys, values and each
    my $STD_TESTS = <<'END'
    join $;, sort keys %h;
    join $;, sort values %h;
    { my ($v, %tmp); $tmp{$v}++ while (defined($v = each %h)); dumph(\%tmp) }
    { my ($k, $v, %tmp); $tmp{"$k$;$v"}++ while (($k, $v) = each %h); dumph(\%tmp) }
END
  ;
    
    # Tests on the existence of the element 'foo'
    my $FOO_TESTS = <<'END'
    defined $h{foo};
    exists $h{foo};
    $h{foo};    
END
  ;

    # Test storing and deleting 'foo'
    push @r, split /\n/, <<"END"
    $STD_TESTS;
    $FOO_TESTS;
    \$h{foo} = undef;
    $STD_TESTS;
    $FOO_TESTS;
    \$h{foo} = 'hello';
    $STD_TESTS;
    $FOO_TESTS;
    delete  \$h{foo};
    $STD_TESTS;
    $FOO_TESTS;
END
  ;

    # Test storing and removing under ordinary keys
    my @things = ('boink', 0, 1, '', undef);
    foreach my $key (map { dumped($_) } @things) {
        foreach my $value ((map { dumped($_) } @things), '$ref') {
            push @r, split /\n/, <<"END"
            \$h{$key} = $value;
            $STD_TESTS;
            defined \$h{$key};
            exists \$h{$key};
            \$h{$key};
            delete \$h{$key};
            $STD_TESTS;
            defined \$h{$key};
            exists \$h{$key};
            \$h{$key};
END
  ;
        }
    }
    
    # Test hash slices
    my @slicetests;
    @slicetests = split /\n/, <<'END'
    @h{'b'} = ();
    @h{'c'} = ('d');
    @h{'e'} = ('f', 'g');
    @h{'h', 'i'} = ();
    @h{'j', 'k'} = ('l');
    @h{'m', 'n'} = ('o', 'p');
    @h{'q', 'r'} = ('s', 't', 'u');
END
  ;
    my @aaa = @slicetests;
    foreach (@slicetests) {
        push @r, $_;
        push @r, split(/\n/, $STD_TESTS);
    }

    # Test CLEAR
    push @r, '%h = ();', split(/\n/, $STD_TESTS);

    return @r;
}

