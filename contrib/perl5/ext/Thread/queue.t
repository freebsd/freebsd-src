use Thread;
use Thread::Queue;

$q = new Thread::Queue;

sub reader {
    my $tid = Thread->self->tid;
    my $i = 0;
    while (1) {
	$i++;
	print "reader (tid $tid): waiting for element $i...\n";
	my $el = $q->dequeue;
	print "reader (tid $tid): dequeued element $i: value $el\n";
	select(undef, undef, undef, rand(2));
	if ($el == -1) {
	    # end marker
	    print "reader (tid $tid) returning\n";
	    return;
	}
    }
}

my $nthreads = 3;

for (my $i = 0; $i < $nthreads; $i++) {
    Thread->new(\&reader, $i);
}

for (my $i = 1; $i <= 10; $i++) {
    my $el = int(rand(100));
    select(undef, undef, undef, rand(2));
    print "writer: enqueuing value $el\n";
    $q->enqueue($el);
}

$q->enqueue((-1) x $nthreads); # one end marker for each thread
