use Thread;

$| = 1;

if (@ARGV) {
    srand($ARGV[0]);
} else {
    my $seed = $$ ^ $^T;
    print "Randomising to $seed\n";
    srand($seed);
}

sub whoami {
    my ($depth, $a, $b, $c) = @_;
    my $i;
    print "whoami ($depth): $a $b $c\n";
    sleep 1;
    whoami($depth - 1, $a, $b, $c) if $depth > 0;
}

sub start_foo {
    my $r = 3 + int(10 * rand);
    print "start_foo: r is $r\n";
    whoami($r, "start_foo", "foo1", "foo2");
    print "start_foo: finished\n";
}

sub start_bar {
    my $r = 3 + int(10 * rand);
    print "start_bar: r is $r\n";
    whoami($r, "start_bar", "bar1", "bar2");
    print "start_bar: finished\n";
}

$foo = new Thread \&start_foo;
$bar = new Thread \&start_bar;
print "main: exiting\n";
