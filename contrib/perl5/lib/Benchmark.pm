package Benchmark;

=head1 NAME

Benchmark - benchmark running times of Perl code

=head1 SYNOPSIS

    timethis ($count, "code");

    # Use Perl code in strings...
    timethese($count, {
	'Name1' => '...code1...',
	'Name2' => '...code2...',
    });

    # ... or use subroutine references.
    timethese($count, {
	'Name1' => sub { ...code1... },
	'Name2' => sub { ...code2... },
    });

    # cmpthese can be used both ways as well
    cmpthese($count, {
	'Name1' => '...code1...',
	'Name2' => '...code2...',
    });

    cmpthese($count, {
	'Name1' => sub { ...code1... },
	'Name2' => sub { ...code2... },
    });

    # ...or in two stages
    $results = timethese($count, 
        {
	    'Name1' => sub { ...code1... },
	    'Name2' => sub { ...code2... },
        },
	'none'
    );
    cmpthese( $results ) ;

    $t = timeit($count, '...other code...')
    print "$count loops of other code took:",timestr($t),"\n";

    $t = countit($time, '...other code...')
    $count = $t->iters ;
    print "$count loops of other code took:",timestr($t),"\n";

=head1 DESCRIPTION

The Benchmark module encapsulates a number of routines to help you
figure out how long it takes to execute some code.

timethis - run a chunk of code several times

timethese - run several chunks of code several times

cmpthese - print results of timethese as a comparison chart

timeit - run a chunk of code and see how long it goes

countit - see how many times a chunk of code runs in a given time


=head2 Methods

=over 10

=item new

Returns the current time.   Example:

    use Benchmark;
    $t0 = new Benchmark;
    # ... your code here ...
    $t1 = new Benchmark;
    $td = timediff($t1, $t0);
    print "the code took:",timestr($td),"\n";

=item debug

Enables or disable debugging by setting the C<$Benchmark::Debug> flag:

    debug Benchmark 1;
    $t = timeit(10, ' 5 ** $Global ');
    debug Benchmark 0;

=item iters

Returns the number of iterations.

=back

=head2 Standard Exports

The following routines will be exported into your namespace
if you use the Benchmark module:

=over 10

=item timeit(COUNT, CODE)

Arguments: COUNT is the number of times to run the loop, and CODE is
the code to run.  CODE may be either a code reference or a string to
be eval'd; either way it will be run in the caller's package.

Returns: a Benchmark object.

=item timethis ( COUNT, CODE, [ TITLE, [ STYLE ]] )

Time COUNT iterations of CODE. CODE may be a string to eval or a
code reference; either way the CODE will run in the caller's package.
Results will be printed to STDOUT as TITLE followed by the times.
TITLE defaults to "timethis COUNT" if none is provided. STYLE
determines the format of the output, as described for timestr() below.

The COUNT can be zero or negative: this means the I<minimum number of
CPU seconds> to run.  A zero signifies the default of 3 seconds.  For
example to run at least for 10 seconds:

	timethis(-10, $code)

or to run two pieces of code tests for at least 3 seconds:

	timethese(0, { test1 => '...', test2 => '...'})

CPU seconds is, in UNIX terms, the user time plus the system time of
the process itself, as opposed to the real (wallclock) time and the
time spent by the child processes.  Less than 0.1 seconds is not
accepted (-0.01 as the count, for example, will cause a fatal runtime
exception).

Note that the CPU seconds is the B<minimum> time: CPU scheduling and
other operating system factors may complicate the attempt so that a
little bit more time is spent.  The benchmark output will, however,
also tell the number of C<$code> runs/second, which should be a more
interesting number than the actually spent seconds.

Returns a Benchmark object.

=item timethese ( COUNT, CODEHASHREF, [ STYLE ] )

The CODEHASHREF is a reference to a hash containing names as keys
and either a string to eval or a code reference for each value.
For each (KEY, VALUE) pair in the CODEHASHREF, this routine will
call

	timethis(COUNT, VALUE, KEY, STYLE)

The routines are called in string comparison order of KEY.

The COUNT can be zero or negative, see timethis().

Returns a hash of Benchmark objects, keyed by name.

=item timediff ( T1, T2 )

Returns the difference between two Benchmark times as a Benchmark
object suitable for passing to timestr().

=item timestr ( TIMEDIFF, [ STYLE, [ FORMAT ] ] )

Returns a string that formats the times in the TIMEDIFF object in
the requested STYLE. TIMEDIFF is expected to be a Benchmark object
similar to that returned by timediff().

STYLE can be any of 'all', 'none', 'noc', 'nop' or 'auto'. 'all' shows
each of the 5 times available ('wallclock' time, user time, system time,
user time of children, and system time of children). 'noc' shows all
except the two children times. 'nop' shows only wallclock and the
two children times. 'auto' (the default) will act as 'all' unless
the children times are both zero, in which case it acts as 'noc'.
'none' prevents output.

FORMAT is the L<printf(3)>-style format specifier (without the
leading '%') to use to print the times. It defaults to '5.2f'.

=back

=head2 Optional Exports

The following routines will be exported into your namespace
if you specifically ask that they be imported:

=over 10

=item clearcache ( COUNT )

Clear the cached time for COUNT rounds of the null loop.

=item clearallcache ( )

Clear all cached times.

=item cmpthese ( COUT, CODEHASHREF, [ STYLE ] )

=item cmpthese ( RESULTSHASHREF )

Optionally calls timethese(), then outputs comparison chart.  This 
chart is sorted from slowest to fastest, and shows the percent 
speed difference between each pair of tests.  Can also be passed 
the data structure that timethese() returns:

    $results = timethese( .... );
    cmpthese( $results );

Returns the data structure returned by timethese() (or passed in).

=item countit(TIME, CODE)

Arguments: TIME is the minimum length of time to run CODE for, and CODE is
the code to run.  CODE may be either a code reference or a string to
be eval'd; either way it will be run in the caller's package.

TIME is I<not> negative.  countit() will run the loop many times to
calculate the speed of CODE before running it for TIME.  The actual
time run for will usually be greater than TIME due to system clock
resolution, so it's best to look at the number of iterations divided
by the times that you are concerned with, not just the iterations.

Returns: a Benchmark object.

=item disablecache ( )

Disable caching of timings for the null loop. This will force Benchmark
to recalculate these timings for each new piece of code timed.

=item enablecache ( )

Enable caching of timings for the null loop. The time taken for COUNT
rounds of the null loop will be calculated only once for each
different COUNT used.

=item timesum ( T1, T2 )

Returns the sum of two Benchmark times as a Benchmark object suitable
for passing to timestr().

=back

=head1 NOTES

The data is stored as a list of values from the time and times
functions:

      ($real, $user, $system, $children_user, $children_system, $iters)

in seconds for the whole loop (not divided by the number of rounds).

The timing is done using time(3) and times(3).

Code is executed in the caller's package.

The time of the null loop (a loop with the same
number of rounds but empty loop body) is subtracted
from the time of the real loop.

The null loop times can be cached, the key being the
number of rounds. The caching can be controlled using
calls like these:

    clearcache($key);
    clearallcache();

    disablecache();
    enablecache();

Caching is off by default, as it can (usually slightly) decrease
accuracy and does not usually noticably affect runtimes.

=head1 EXAMPLES

For example,

   use Benchmark;$x=3;cmpthese(-5,{a=>sub{$x*$x},b=>sub{$x**2}})

outputs something like this:

   Benchmark: running a, b, each for at least 5 CPU seconds...
	    a: 10 wallclock secs ( 5.14 usr +  0.13 sys =  5.27 CPU) @ 3835055.60/s (n=20210743)
	    b:  5 wallclock secs ( 5.41 usr +  0.00 sys =  5.41 CPU) @ 1574944.92/s (n=8520452)
	  Rate    b    a
   b 1574945/s   -- -59%
   a 3835056/s 144%   --

while 

   use Benchmark;
   $x=3;
   $r=timethese(-5,{a=>sub{$x*$x},b=>sub{$x**2}},'none');
   cmpthese($r);

outputs something like this:

          Rate    b    a
   b 1559428/s   -- -62%
   a 4152037/s 166%   --


=head1 INHERITANCE

Benchmark inherits from no other class, except of course
for Exporter.

=head1 CAVEATS

Comparing eval'd strings with code references will give you
inaccurate results: a code reference will show a slightly slower
execution time than the equivalent eval'd string.

The real time timing is done using time(2) and
the granularity is therefore only one second.

Short tests may produce negative figures because perl
can appear to take longer to execute the empty loop
than a short test; try:

    timethis(100,'1');

The system time of the null loop might be slightly
more than the system time of the loop with the actual
code and therefore the difference might end up being E<lt> 0.

=head1 SEE ALSO

L<Devel::DProf> - a Perl code profiler

=head1 AUTHORS

Jarkko Hietaniemi <F<jhi@iki.fi>>, Tim Bunce <F<Tim.Bunce@ig.co.uk>>

=head1 MODIFICATION HISTORY

September 8th, 1994; by Tim Bunce.

March 28th, 1997; by Hugo van der Sanden: added support for code
references and the already documented 'debug' method; revamped
documentation.

April 04-07th, 1997: by Jarkko Hietaniemi, added the run-for-some-time
functionality.

September, 1999; by Barrie Slaymaker: math fixes and accuracy and 
efficiency tweaks.  Added cmpthese().  A result is now returned from 
timethese().  Exposed countit() (was runfor()).

=cut

# evaluate something in a clean lexical environment
sub _doeval { eval shift }

#
# put any lexicals at file scope AFTER here
#

use Carp;
use Exporter;
@ISA=(Exporter);
@EXPORT=qw(timeit timethis timethese timediff timestr);
@EXPORT_OK=qw(timesum cmpthese countit
	      clearcache clearallcache disablecache enablecache);

$VERSION = 1.00;

&init;

sub init {
    $debug = 0;
    $min_count = 4;
    $min_cpu   = 0.4;
    $defaultfmt = '5.2f';
    $defaultstyle = 'auto';
    # The cache can cause a slight loss of sys time accuracy. If a
    # user does many tests (>10) with *very* large counts (>10000)
    # or works on a very slow machine the cache may be useful.
    &disablecache;
    &clearallcache;
}

sub debug { $debug = ($_[1] != 0); }

# The cache needs two branches: 's' for strings and 'c' for code.  The
# emtpy loop is different in these two cases.
sub clearcache    { delete $cache{"$_[0]c"}; delete $cache{"$_[0]s"}; }
sub clearallcache { %cache = (); }
sub enablecache   { $cache = 1; }
sub disablecache  { $cache = 0; }

# --- Functions to process the 'time' data type

sub new { my @t = (time, times, @_ == 2 ? $_[1] : 0);
	  print "new=@t\n" if $debug;
	  bless \@t; }

sub cpu_p { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $pu+$ps         ; }
sub cpu_c { my($r,$pu,$ps,$cu,$cs) = @{$_[0]};         $cu+$cs ; }
sub cpu_a { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $pu+$ps+$cu+$cs ; }
sub real  { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $r              ; }
sub iters { $_[0]->[5] ; }

sub timediff {
    my($a, $b) = @_;
    my @r;
    for (my $i=0; $i < @$a; ++$i) {
	push(@r, $a->[$i] - $b->[$i]);
    }
    bless \@r;
}

sub timesum {
     my($a, $b) = @_;
     my @r;
     for (my $i=0; $i < @$a; ++$i) {
 	push(@r, $a->[$i] + $b->[$i]);
     }
     bless \@r;
}

sub timestr {
    my($tr, $style, $f) = @_;
    my @t = @$tr;
    warn "bad time value (@t)" unless @t==6;
    my($r, $pu, $ps, $cu, $cs, $n) = @t;
    my($pt, $ct, $tt) = ($tr->cpu_p, $tr->cpu_c, $tr->cpu_a);
    $f = $defaultfmt unless defined $f;
    # format a time in the required style, other formats may be added here
    $style ||= $defaultstyle;
    $style = ($ct>0) ? 'all' : 'noc' if $style eq 'auto';
    my $s = "@t $style"; # default for unknown style
    $s=sprintf("%2d wallclock secs (%$f usr %$f sys + %$f cusr %$f csys = %$f CPU)",
			    $r,$pu,$ps,$cu,$cs,$tt) if $style eq 'all';
    $s=sprintf("%2d wallclock secs (%$f usr + %$f sys = %$f CPU)",
			    $r,$pu,$ps,$pt) if $style eq 'noc';
    $s=sprintf("%2d wallclock secs (%$f cusr + %$f csys = %$f CPU)",
			    $r,$cu,$cs,$ct) if $style eq 'nop';
    $s .= sprintf(" @ %$f/s (n=$n)", $n / ( $pu + $ps )) if $n && $pu+$ps;
    $s;
}

sub timedebug {
    my($msg, $t) = @_;
    print STDERR "$msg",timestr($t),"\n" if $debug;
}

# --- Functions implementing low-level support for timing loops

sub runloop {
    my($n, $c) = @_;

    $n+=0; # force numeric now, so garbage won't creep into the eval
    croak "negative loopcount $n" if $n<0;
    confess "Usage: runloop(number, [string | coderef])" unless defined $c;
    my($t0, $t1, $td); # before, after, difference

    # find package of caller so we can execute code there
    my($curpack) = caller(0);
    my($i, $pack)= 0;
    while (($pack) = caller(++$i)) {
	last if $pack ne $curpack;
    }

    my ($subcode, $subref);
    if (ref $c eq 'CODE') {
	$subcode = "sub { for (1 .. $n) { local \$_; package $pack; &\$c; } }";
        $subref  = eval $subcode;
    }
    else {
	$subcode = "sub { for (1 .. $n) { local \$_; package $pack; $c;} }";
        $subref  = _doeval($subcode);
    }
    croak "runloop unable to compile '$c': $@\ncode: $subcode\n" if $@;
    print STDERR "runloop $n '$subcode'\n" if $debug;

    # Wait for the user timer to tick.  This makes the error range more like 
    # -0.01, +0.  If we don't wait, then it's more like -0.01, +0.01.  This
    # may not seem important, but it significantly reduces the chances of
    # getting a too low initial $n in the initial, 'find the minimum' loop
    # in &countit.  This, in turn, can reduce the number of calls to
    # &runloop a lot, and thus reduce additive errors.
    my $tbase = Benchmark->new(0)->[1];
    while ( ( $t0 = Benchmark->new(0) )->[1] == $tbase ) {} ;
    &$subref;
    $t1 = Benchmark->new($n);
    $td = &timediff($t1, $t0);
    timedebug("runloop:",$td);
    $td;
}


sub timeit {
    my($n, $code) = @_;
    my($wn, $wc, $wd);

    printf STDERR "timeit $n $code\n" if $debug;
    my $cache_key = $n . ( ref( $code ) ? 'c' : 's' );
    if ($cache && exists $cache{$cache_key} ) {
	$wn = $cache{$cache_key};
    } else {
	$wn = &runloop($n, ref( $code ) ? sub { undef } : '' );
	# Can't let our baseline have any iterations, or they get subtracted
	# out of the result.
	$wn->[5] = 0;
	$cache{$cache_key} = $wn;
    }

    $wc = &runloop($n, $code);

    $wd = timediff($wc, $wn);
    timedebug("timeit: ",$wc);
    timedebug("      - ",$wn);
    timedebug("      = ",$wd);

    $wd;
}


my $default_for = 3;
my $min_for     = 0.1;


sub countit {
    my ( $tmax, $code ) = @_;

    if ( not defined $tmax or $tmax == 0 ) {
	$tmax = $default_for;
    } elsif ( $tmax < 0 ) {
	$tmax = -$tmax;
    }

    die "countit($tmax, ...): timelimit cannot be less than $min_for.\n"
	if $tmax < $min_for;

    my ($n, $tc);

    # First find the minimum $n that gives a significant timing.
    for ($n = 1; ; $n *= 2 ) {
	my $td = timeit($n, $code);
	$tc = $td->[1] + $td->[2];
	last if $tc > 0.1;
    }

    my $nmin = $n;

    # Get $n high enough that we can guess the final $n with some accuracy.
    my $tpra = 0.1 * $tmax; # Target/time practice.
    while ( $tc < $tpra ) {
	# The 5% fudge is to keep us from iterating again all
	# that often (this speeds overall responsiveness when $tmax is big
	# and we guess a little low).  This does not noticably affect 
	# accuracy since we're not couting these times.
	$n = int( $tpra * 1.05 * $n / $tc ); # Linear approximation.
	my $td = timeit($n, $code);
	my $new_tc = $td->[1] + $td->[2];
        # Make sure we are making progress.
        $tc = $new_tc > 1.2 * $tc ? $new_tc : 1.2 * $tc;
    }

    # Now, do the 'for real' timing(s), repeating until we exceed
    # the max.
    my $ntot  = 0;
    my $rtot  = 0;
    my $utot  = 0.0;
    my $stot  = 0.0;
    my $cutot = 0.0;
    my $cstot = 0.0;
    my $ttot  = 0.0;

    # The 5% fudge is because $n is often a few % low even for routines
    # with stable times and avoiding extra timeit()s is nice for
    # accuracy's sake.
    $n = int( $n * ( 1.05 * $tmax / $tc ) );

    while () {
	my $td = timeit($n, $code);
	$ntot  += $n;
	$rtot  += $td->[0];
	$utot  += $td->[1];
	$stot  += $td->[2];
	$cutot += $td->[3];
	$cstot += $td->[4];
	$ttot = $utot + $stot;
	last if $ttot >= $tmax;

        $ttot = 0.01 if $ttot < 0.01;
	my $r = $tmax / $ttot - 1; # Linear approximation.
	$n = int( $r * $ntot );
	$n = $nmin if $n < $nmin;
    }

    return bless [ $rtot, $utot, $stot, $cutot, $cstot, $ntot ];
}

# --- Functions implementing high-level time-then-print utilities

sub n_to_for {
    my $n = shift;
    return $n == 0 ? $default_for : $n < 0 ? -$n : undef;
}

sub timethis{
    my($n, $code, $title, $style) = @_;
    my($t, $for, $forn);

    if ( $n > 0 ) {
	croak "non-integer loopcount $n, stopped" if int($n)<$n;
	$t = timeit($n, $code);
	$title = "timethis $n" unless defined $title;
    } else {
	$fort  = n_to_for( $n );
	$t     = countit( $fort, $code );
	$title = "timethis for $fort" unless defined $title;
	$forn  = $t->[-1];
    }
    local $| = 1;
    $style = "" unless defined $style;
    printf("%10s: ", $title) unless $style eq 'none';
    print timestr($t, $style, $defaultfmt),"\n" unless $style eq 'none';

    $n = $forn if defined $forn;

    # A conservative warning to spot very silly tests.
    # Don't assume that your benchmark is ok simply because
    # you don't get this warning!
    print "            (warning: too few iterations for a reliable count)\n"
	if     $n < $min_count
	    || ($t->real < 1 && $n < 1000)
	    || $t->cpu_a < $min_cpu;
    $t;
}

sub timethese{
    my($n, $alt, $style) = @_;
    die "usage: timethese(count, { 'Name1'=>'code1', ... }\n"
		unless ref $alt eq HASH;
    my @names = sort keys %$alt;
    $style = "" unless defined $style;
    print "Benchmark: " unless $style eq 'none';
    if ( $n > 0 ) {
	croak "non-integer loopcount $n, stopped" if int($n)<$n;
	print "timing $n iterations of" unless $style eq 'none';
    } else {
	print "running" unless $style eq 'none';
    }
    print " ", join(', ',@names) unless $style eq 'none';
    unless ( $n > 0 ) {
	my $for = n_to_for( $n );
	print ", each for at least $for CPU seconds" unless $style eq 'none';
    }
    print "...\n" unless $style eq 'none';

    # we could save the results in an array and produce a summary here
    # sum, min, max, avg etc etc
    my %results;
    foreach my $name (@names) {
        $results{$name} = timethis ($n, $alt -> {$name}, $name, $style);
    }

    return \%results;
}

sub cmpthese{
    my $results = ref $_[0] ? $_[0] : timethese( @_ );

    return $results
       if defined $_[2] && $_[2] eq 'none';

    # Flatten in to an array of arrays with the name as the first field
    my @vals = map{ [ $_, @{$results->{$_}} ] } keys %$results;

    for (@vals) {
	# The epsilon fudge here is to prevent div by 0.  Since clock
	# resolutions are much larger, it's below the noise floor.
	my $rate = $_->[6] / ( $_->[2] + $_->[3] + 0.000000000000001 );
	$_->[7] = $rate;
    }

    # Sort by rate
    @vals = sort { $a->[7] <=> $b->[7] } @vals;

    # If more than half of the rates are greater than one...
    my $display_as_rate = $vals[$#vals>>1]->[7] > 1;

    my @rows;
    my @col_widths;

    my @top_row = ( 
        '', 
	$display_as_rate ? 'Rate' : 's/iter', 
	map { $_->[0] } @vals 
    );

    push @rows, \@top_row;
    @col_widths = map { length( $_ ) } @top_row;

    # Build the data rows
    # We leave the last column in even though it never has any data.  Perhaps
    # it should go away.  Also, perhaps a style for a single column of
    # percentages might be nice.
    for my $row_val ( @vals ) {
	my @row;

        # Column 0 = test name
	push @row, $row_val->[0];
	$col_widths[0] = length( $row_val->[0] )
	    if length( $row_val->[0] ) > $col_widths[0];

        # Column 1 = performance
	my $row_rate = $row_val->[7];

	# We assume that we'll never get a 0 rate.
	my $a = $display_as_rate ? $row_rate : 1 / $row_rate;

	# Only give a few decimal places before switching to sci. notation,
	# since the results aren't usually that accurate anyway.
	my $format = 
	   $a >= 100 ? 
	       "%0.0f" : 
	   $a >= 10 ?
	       "%0.1f" :
	   $a >= 1 ?
	       "%0.2f" :
	   $a >= 0.1 ?
	       "%0.3f" :
	       "%0.2e";

	$format .= "/s"
	    if $display_as_rate;
	# Using $b here due to optimizing bug in _58 through _61
	my $b = sprintf( $format, $a );
	push @row, $b;
	$col_widths[1] = length( $b )
	    if length( $b ) > $col_widths[1];

        # Columns 2..N = performance ratios
	my $skip_rest = 0;
	for ( my $col_num = 0 ; $col_num < @vals ; ++$col_num ) {
	    my $col_val = $vals[$col_num];
	    my $out;
	    if ( $skip_rest ) {
		$out = '';
	    }
	    elsif ( $col_val->[0] eq $row_val->[0] ) {
		$out = "--";
		# $skip_rest = 1;
	    }
	    else {
		my $col_rate = $col_val->[7];
		$out = sprintf( "%.0f%%", 100*$row_rate/$col_rate - 100 );
	    }
	    push @row, $out;
	    $col_widths[$col_num+2] = length( $out )
		if length( $out ) > $col_widths[$col_num+2];

	    # A little wierdness to set the first column width properly
	    $col_widths[$col_num+2] = length( $col_val->[0] )
		if length( $col_val->[0] ) > $col_widths[$col_num+2];
	}
	push @rows, \@row;
    }

    # Equalize column widths in the chart as much as possible without
    # exceeding 80 characters.  This does not use or affect cols 0 or 1.
    my @sorted_width_refs = 
       sort { $$a <=> $$b } map { \$_ } @col_widths[2..$#col_widths];
    my $max_width = ${$sorted_width_refs[-1]};

    my $total = @col_widths - 1 ;
    for ( @col_widths ) { $total += $_ }

    STRETCHER:
    while ( $total < 80 ) {
	my $min_width = ${$sorted_width_refs[0]};
	last
	   if $min_width == $max_width;
	for ( @sorted_width_refs ) {
	    last 
		if $$_ > $min_width;
	    ++$$_;
	    ++$total;
	    last STRETCHER
		if $total >= 80;
	}
    }

    # Dump the output
    my $format = join( ' ', map { "%${_}s" } @col_widths ) . "\n";
    substr( $format, 1, 0 ) = '-';
    for ( @rows ) {
	printf $format, @$_;
    }

    return $results;
}


1;
