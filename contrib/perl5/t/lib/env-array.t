#!./perl

$| = 1;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

if ($^O eq 'VMS') {
    print "1..11\n";
    foreach (1..11) { print "ok $_ # skipped for VMS\n"; }
    exit 0;
}

use Env  qw(@FOO);
use vars qw(@BAR);

sub array_equal
{
    my ($a, $b) = @_;
    return 0 unless scalar(@$a) == scalar(@$b);
    for my $i (0..scalar(@$a) - 1) {
	return 0 unless $a->[$i] eq $b->[$i];
    }
    return 1;
}

sub test
{
    my ($desc, $code) = @_;

    &$code;

    print "# $desc...\n";
    print "#    FOO = (", join(", ", @FOO), ")\n";
    print "#    BAR = (", join(", ", @BAR), ")\n";

    if (defined $check) { print "not " unless &$check; }
    else { print "not " unless array_equal(\@FOO, \@BAR); }

    print "ok ", ++$i, "\n";
}

print "1..11\n";

test "Assignment", sub {
    @FOO = qw(a B c);
    @BAR = qw(a B c);
};

test "Storing", sub {
    $FOO[1] = 'b';
    $BAR[1] = 'b';
};

test "Truncation", sub {
    $#FOO = 0;
    $#BAR = 0;
};

test "Push", sub {
    push @FOO, 'b', 'c';
    push @BAR, 'b', 'c';
};

test "Pop", sub {
    pop @FOO;
    pop @BAR;
};

test "Shift", sub {
    shift @FOO;
    shift @BAR;
};

test "Push", sub {
    push @FOO, 'c';
    push @BAR, 'c';
};

test "Unshift", sub {
    unshift @FOO, 'a';
    unshift @BAR, 'a';
};

test "Reverse", sub {
    @FOO = reverse @FOO;
    @BAR = reverse @BAR;
};

test "Sort", sub {
    @FOO = sort @FOO;
    @BAR = sort @BAR;
};

test "Splice", sub {
    splice @FOO, 1, 1, 'B';
    splice @BAR, 1, 1, 'B';
};
