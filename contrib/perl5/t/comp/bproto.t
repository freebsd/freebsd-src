#!./perl
#
# check if builtins behave as prototyped
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..10\n";

my $i = 1;

sub foo {}
my $bar = "bar";

sub test_too_many {
    eval $_[0];
    print "not " unless $@ =~ /^Too many arguments/;
    printf "ok %d\n",$i++;
}

sub test_no_error {
    eval $_[0];
    print "not " if $@;
    printf "ok %d\n",$i++;
}

test_too_many($_) for split /\n/,
q[	defined(&foo, $bar);
	undef(&foo, $bar);
	uc($bar,$bar);
];

test_no_error($_) for split /\n/,
q[	scalar(&foo,$bar);
	defined &foo, &foo, &foo;
	undef &foo, $bar;
	uc $bar,$bar;
	grep(not($bar), $bar);
	grep(not($bar, $bar), $bar);
	grep((not $bar, $bar, $bar), $bar);
];
