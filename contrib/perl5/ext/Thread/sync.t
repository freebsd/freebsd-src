use Thread;

$level = 0;

sub single_file : locked {
    my $arg = shift;
    $level++;
    print "Level $level for $arg\n";
    print "(something is wrong)\n" if $level < 0 || $level > 1;
    sleep 1;
    $level--;
    print "Back to level $level\n";
}

sub start_bar {
    my $i;
    print "start bar\n";
    for $i (1..3) {
	print "bar $i\n";
	single_file("bar $i");
	sleep 1 if rand > 0.5;
    }
    print "end bar\n";
    return 1;
}

sub start_foo {
    my $i;
    print "start foo\n";
    for $i (1..3) {
	print "foo $i\n";
	single_file("foo $i");
	sleep 1 if rand > 0.5;
    }
    print "end foo\n";
    return 1;
}

sub start_baz {
    my $i;
    print "start baz\n";
    for $i (1..3) {
	print "baz $i\n";
	single_file("baz $i");
	sleep 1 if rand > 0.5;
    }
    print "end baz\n";
    return 1;
}

$| = 1;
srand($$^$^T);

$foo = new Thread \&start_foo;
$bar = new Thread \&start_bar;
$baz = new Thread \&start_baz;
$foo->join();
$bar->join();
$baz->join();
print "main: threads finished, exiting\n";
