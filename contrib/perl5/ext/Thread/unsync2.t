use Thread;

$| = 1;

srand($$^$^T);

sub printargs {
    my $thread = shift;
    my $arg;
    my $i;
    while ($arg = shift) {
	my $delay = int(rand(500));
	$i++;
	print "$thread arg $i is $arg\n";
	1 while $delay--;
    }
}

sub start_thread {
    my $thread = shift;
    my $count = 10;
    while ($count--) {
	my(@args) = ($thread) x int(rand(10));
	print "$thread $count calling printargs @args\n";
	printargs($thread, @args);
    }
}

new Thread (\&start_thread, "A");
new Thread (\&start_thread, "B");
#new Thread (\&start_thread, "C");
#new Thread (\&start_thread, "D");
#new Thread (\&start_thread, "E");
#new Thread (\&start_thread, "F");

print "main: exiting\n";
