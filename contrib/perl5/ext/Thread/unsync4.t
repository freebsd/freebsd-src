use Thread;

$| = 1;

srand($$^$^T);

sub printargs {
    my(@copyargs) = @_;
    my $thread = shift @copyargs;
    my $arg;
    my $i;
    while ($arg = shift @copyargs) {
	my $delay = int(rand(500));
	$i++;
	print "$thread arg $i is $arg\n";
	1 while $delay--;
    }
}

sub start_thread {
    my(@threadargs) = @_;
    my $thread = $threadargs[0];
    my $count = 10;
    while ($count--) {
	my(@args) = ($thread) x int(rand(10));
	print "$thread $count calling printargs @args\n";
	printargs($thread, @args);
    }
}

new Thread (\&start_thread, "A");
new Thread (\&start_thread, "B");
new Thread (\&start_thread, "C");
new Thread (\&start_thread, "D");
new Thread (\&start_thread, "E");
new Thread (\&start_thread, "F");

print "main: exiting\n";
