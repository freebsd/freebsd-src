use Thread 'async';
use Config;
use Tie::Hash;

sub start_here {
    my $i;
    print "In start_here with args: @_\n";
    for ($i = 1; $i <= 5; $i++) {
	print "start_here: $i\n";
	sleep 1;
    }
}

async {
    tie my(%h), 'Tie::StdHash';
    %h = %Config;
    print "running on $h{archname}\n";
};

print "Starting new thread now\n";
$t = new Thread \&start_here, qw(foo bar baz);
print "Started thread $t\n";
for ($count = 1; $count <= 5; $count++) {
    print "main: $count\n";
    sleep 1;
}
