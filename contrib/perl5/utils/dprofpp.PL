#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
chdir(dirname($0));
($file = basename($0)) =~ s/\.PL$//;
$file =~ s/\.pl$// if ($Config{'osname'} eq 'OS2');      # "case-forgiving"
$file =~ s/\.pl$/.com/ if ($Config{'osname'} eq 'VMS');  # "case-forgiving"

my $dprof_pm = '../ext/Devel/DProf/DProf.pm';
my $VERSION = 0;
open( PM, "<$dprof_pm" ) || die "Can't open $dprof_pm: $!";
while(<PM>){
	if( /^\$Devel::DProf::VERSION\s*=\s*'([\d._]+)'/ ){
		$VERSION = $1;
		last;
	}
}
close PM;
if( $VERSION == 0 ){
	die "Did not find VERSION in $dprof_pm";
}
open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{'startperl'}
    eval 'exec perl -S \$0 "\$@"'
	if 0;

require 5.003;

my \$VERSION = '$VERSION';

!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';
=head1 NAME

dprofpp - display perl profile data

=head1 SYNOPSIS

dprofpp [B<-a>|B<-z>|B<-l>|B<-v>|B<-U>] [B<-s>|B<-r>|B<-u>] [B<-q>] [B<-F>] [B<-I|-E>] [B<-O cnt>] [B<-A>] [B<-R>] [B<-S>] [B<-g subroutine>] [profile]

dprofpp B<-T> [B<-F>] [B<-g subroutine>] [profile]

dprofpp B<-t> [B<-F>] [B<-g subroutine>] [profile]

dprofpp B<-p script> [B<-Q>] [other opts]

dprofpp B<-V> [profile]

=head1 DESCRIPTION

The I<dprofpp> command interprets profile data produced by a profiler, such
as the Devel::DProf profiler.  Dprofpp will read the file F<tmon.out> and
will display the 15 subroutines which are using the most time.  By default
the times for each subroutine are given exclusive of the times of their
child subroutines.

To profile a Perl script run the perl interpreter with the B<-d> switch.  So
to profile script F<test.pl> with Devel::DProf the following command should
be used.

	$ perl5 -d:DProf test.pl

Then run dprofpp to analyze the profile.  The output of dprofpp depends
on the flags to the program and the version of Perl you're using.

	$ dprofpp -u
	Total Elapsed Time =    1.67 Seconds
		 User Time =    0.61 Seconds
	Exclusive Times
	%Time Seconds     #Calls sec/call Name
	 52.4   0.320          2   0.1600 main::foo
	 45.9   0.280        200   0.0014 main::bar
	 0.00   0.000          1   0.0000 DynaLoader::import
	 0.00   0.000          1   0.0000 main::baz

The dprofpp tool can also run the profiler before analyzing the profile
data.  The above two commands can be executed with one dprofpp command.

	$ dprofpp -u -p test.pl

Consult L<Devel::DProf/"PROFILE FORMAT"> for a description of the raw profile.

=head1 OUTPUT

Columns are:

=over 4

=item %Time

Percentage of time spent in this routine.

=item #Calls

Number of calls to this routine.

=item sec/call

Average number of seconds per call to this routine.

=item Name

Name of routine.

=item CumulS

Time (in seconds) spent in this routine and routines called from it.

=item ExclSec

Time (in seconds) spent in this routine (not including those called
from it).

=item Csec/c

Average time (in seconds) spent in each call of this routine
(including those called from it).

=back

=head1 OPTIONS

=over 5

=item B<-a>

Sort alphabetically by subroutine names.

=item B<-A>

Count timing for autoloaded subroutine as timing for C<*::AUTOLOAD>.
Otherwise the time to autoload it is counted as time of the subroutine
itself (there is no way to separate autoload time from run time).

This is going to be irrelevant with newer Perls.  They will inform
C<Devel::DProf> I<when> the C<AUTOLOAD> switches to actual subroutine,
so a separate statistics for C<AUTOLOAD> will be collected no matter
whether this option is set.

=item B<-R>

Count anonymous subroutines defined in the same package separately.

=item B<-E>

(default)  Display all subroutine times exclusive of child subroutine times.

=item B<-F>

Force the generation of fake exit timestamps if dprofpp reports that the
profile is garbled.  This is only useful if dprofpp determines that the
profile is garbled due to missing exit timestamps.  You're on your own if
you do this.  Consult the BUGS section.

=item B<-I>

Display all subroutine times inclusive of child subroutine times.

=item B<-l>

Sort by number of calls to the subroutines.  This may help identify
candidates for inlining.

=item B<-O cnt>

Show only I<cnt> subroutines.  The default is 15.

=item B<-p script>

Tells dprofpp that it should profile the given script and then interpret its
profile data.  See B<-Q>.

=item B<-Q>

Used with B<-p> to tell dprofpp to quit after profiling the script, without
interpreting the data.

=item B<-q>

Do not display column headers.

=item B<-r>

Display elapsed real times rather than user+system times.

=item B<-s>

Display system times rather than user+system times.

=item B<-T>

Display subroutine call tree to stdout.  Subroutine statistics are
not displayed.

=item B<-t>

Display subroutine call tree to stdout.  Subroutine statistics are not
displayed.  When a function is called multiple consecutive times at the same
calling level then it is displayed once with a repeat count.

=item B<-S>

Display I<merged> subroutine call tree to stdout.  Statistics is
displayed for each branch of the tree.  

When a function is called multiple (I<not necessarily consecutive>)
times in the same branch then all these calls go into one branch of
the next level.  A repeat count is output together with combined
inclusive, exclusive and kids time.

Branches are sorted w.r.t. inclusive time.

=item B<-U>

Do not sort.  Display in the order found in the raw profile.

=item B<-u>

Display user times rather than user+system times.

=item B<-V>

Print dprofpp's version number and exit.  If a raw profile is found then its
XS_VERSION variable will be displayed, too.

=item B<-v>

Sort by average time spent in subroutines during each call.  This may help
identify candidates for inlining. 

=item B<-z>

(default) Sort by amount of user+system time used.  The first few lines
should show you which subroutines are using the most time.

=item B<-g> C<subroutine>

Ignore subroutines except C<subroutine> and whatever is called from it.

=back

=head1 ENVIRONMENT

The environment variable B<DPROFPP_OPTS> can be set to a string containing
options for dprofpp.  You might use this if you prefer B<-I> over B<-E> or
if you want B<-F> on all the time.

This was added fairly lazily, so there are some undesirable side effects.
Options on the commandline should override options in DPROFPP_OPTS--but
don't count on that in this version.

=head1 BUGS

Applications which call _exit() or exec() from within a subroutine
will leave an incomplete profile.  See the B<-F> option.

Any bugs in Devel::DProf, or any profiler generating the profile data, could
be visible here.  See L<Devel::DProf/BUGS>.

Mail bug reports and feature requests to the perl5-porters mailing list at
F<E<lt>perl5-porters@perl.orgE<gt>>.  Bug reports should include the
output of the B<-V> option.

=head1 FILES

	dprofpp		- profile processor
	tmon.out	- raw profile

=head1 SEE ALSO

L<perl>, L<Devel::DProf>, times(2)

=cut

use Getopt::Std 'getopts';
use Config '%Config';

Setup: {
	my $options = 'O:g:lzaAvuTtqrRsUFEIp:QVS';

	$Monfile = 'tmon.out';
	if( exists $ENV{DPROFPP_OPTS} ){
		my @tmpargv = @ARGV;
		@ARGV = split( ' ', $ENV{DPROFPP_OPTS} );
		getopts( $options );
		if( @ARGV ){
			# there was a filename.
			$Monfile = shift;
		}
		@ARGV = @tmpargv;
	}

	getopts( $options );
	if( @ARGV ){
		# there was a filename, it overrides any earlier name.
		$Monfile = shift;
	}

# -O cnt	Specifies maximum number of subroutines to display.
# -a		Sort by alphabetic name of subroutines.
# -z		Sort by user+system time spent in subroutines. (default)
# -l		Sort by number of calls to subroutines.
# -v		Sort by average amount of time spent in subroutines.
# -T		Show call tree.
# -t		Show call tree, compressed.
# -q		Do not print column headers.
# -u		Use user time rather than user+system time.
# -s		Use system time rather than user+system time.
# -r		Use real elapsed time rather than user+system time.
# -U		Do not sort subroutines.
# -E		Sub times are reported exclusive of child times. (default)
# -I		Sub times are reported inclusive of child times.
# -V		Print dprofpp's version.
# -p script	Specifies name of script to be profiled.
# -Q		Used with -p to indicate the dprofpp should quit after
#		profiling the script, without interpreting the data.
# -A		count autoloaded to *AUTOLOAD
# -R		count anonyms separately even if from the same package
# -g subr	count only those who are SUBR or called from SUBR
# -S		Create statistics for all the depths

	if( defined $opt_V ){
		my $fh = 'main::fh';
		print "$0 version: $VERSION\n";
		open( $fh, "<$Monfile" ) && do {
			local $XS_VERSION = 'early';
			header($fh);
			close( $fh );
			print "XS_VERSION: $XS_VERSION\n";
		};
		exit(0);
	}
	$cnt = $opt_O || 15;
	$sort = 'by_time';
	$sort = 'by_ctime' if defined $opt_I;
	$sort = 'by_calls' if defined $opt_l;
	$sort = 'by_alpha' if defined $opt_a;
	$sort = 'by_avgcpu' if defined $opt_v;
	$incl_excl = 'Exclusive';
	$incl_excl = 'Inclusive' if defined $opt_I;
	$whichtime = 'User+System';
	$whichtime = 'System' if defined $opt_s;
	$whichtime = 'Real' if defined $opt_r;
	$whichtime = 'User' if defined $opt_u;

	if( defined $opt_p ){
		my $prof = 'DProf';
		my $startperl = $Config{'startperl'};

		$startperl =~ s/^#!//; # remove shebang
		run_profiler( $opt_p, $prof, $startperl );
		$Monfile = 'tmon.out';  # because that's where it is
		exit(0) if defined $opt_Q;
	}
	elsif( defined $opt_Q ){
		die "-Q is meaningful only when used with -p\n";
	}
}

Main: {
	my $monout = $Monfile;
	my $fh = 'main::fh';
	local $names = {};
	local $times = {};   # times in hz
	local $ctimes = {};  # Cumulative times in hz
	local $calls = {};
	local $persecs = {}; # times in seconds
	local $idkeys = [];
	local $runtime; # runtime in seconds
	my @a = ();
	my $a;
	local $rrun_utime = 0;	# user time in hz
	local $rrun_stime = 0;	# system time in hz
	local $rrun_rtime = 0;	# elapsed run time in hz
	local $rrun_ustime = 0;	# user+system time in hz
	local $hz = 0;
	local $deep_times = {count => 0 , kids => {}, incl_time => 0};
	local $time_precision = 2;
	local $overhead = 0;

	open( $fh, "<$monout" ) || die "Unable to open $monout\n";

	header($fh);

	$rrun_ustime = $rrun_utime + $rrun_stime;

	$~ = 'STAT';
	if( ! $opt_q ){
		$^ = 'CSTAT_top';
	}

	parsestack( $fh, $names, $calls, $times, $ctimes, $idkeys );

	settime( \$runtime, $hz ) unless $opt_g;

	exit(0) if $opt_T || $opt_t;

	if( $opt_v ){
		percalc( $calls, ($opt_I ? $ctimes : $times), $persecs, $idkeys );
	}
	if( ! $opt_U ){
		@a = sort $sort @$idkeys;
		$a = \@a;
	}
	else {
		$a = $idkeys;
	}
	display( $runtime, $hz, $names, $calls, $times, $ctimes, $cnt, $a,
		 $deep_times);
}


# Sets $runtime to user, system, real, or user+system time.  The
# result is given in seconds.
#
sub settime {
  my( $runtime, $hz ) = @_;

  $hz ||= 1;
  
  if( $opt_r ){
    $$runtime = ($rrun_rtime - $overhead - $over_rtime * $total_marks/$over_tests/2)/$hz;
  }
  elsif( $opt_s ){
    $$runtime = ($rrun_stime - $overhead - $over_stime * $total_marks/$over_tests/2)/$hz;
  }
  elsif( $opt_u ){
    $$runtime = ($rrun_utime - $overhead - $over_utime * $total_marks/$over_tests/2)/$hz;
  }
  else{
    $$runtime = ($rrun_ustime - $overhead - ($over_utime + $over_stime) * $total_marks/$over_tests/2)/$hz;
  }
  $$runtime = 0 unless $$runtime > 0;
}

sub exclusives_in_tree {
  my( $deep_times ) = @_;
  my $kids_time = 0;
  my $kid;
  # When summing, take into account non-rounded-up kids time.
  for $kid (keys %{$deep_times->{kids}}) {
    $kids_time += $deep_times->{kids}{$kid}{incl_time};
  }
  $kids_time = 0 unless $kids_time >= 0;
  $deep_times->{excl_time} = $deep_times->{incl_time} - $kids_time;
  $deep_times->{excl_time} = 0 unless $deep_times->{excl_time} >= 0;
  for $kid (keys %{$deep_times->{kids}}) {
    exclusives_in_tree($deep_times->{kids}{$kid});
  }
  $deep_times->{incl_time} = 0 unless $deep_times->{incl_time} >= 0;
  $deep_times->{kids_time} = $kids_time;
}

sub kids_by_incl { $kids{$b}{incl_time} <=> $kids{$a}{excl_time} 
		   or $a cmp $b }

sub display_tree {
  my( $deep_times, $name, $level ) = @_;
  exclusives_in_tree($deep_times);
  
  my $kid;
  local *kids = $deep_times->{kids}; # %kids

  my $time;
  if (%kids) {
    $time = sprintf '%.*fs = (%.*f + %.*f)', 
      $time_precision, $deep_times->{incl_time}/$hz,
        $time_precision, $deep_times->{excl_time}/$hz,
          $time_precision, $deep_times->{kids_time}/$hz;
  } else {
    $time = sprintf '%.*f', $time_precision, $deep_times->{incl_time}/$hz;
  }
  print ' ' x (2*$level), "$name x $deep_times->{count}  \t${time}s\n"
    if $deep_times->{count};

  for $kid (sort kids_by_incl keys %kids) {
    display_tree( $deep_times->{kids}{$kid}, $kid, $level + 1 );
  }  
}

# Report the times in seconds.
sub display {
	my( $runtime, $hz, $names, $calls, $times, $ctimes, $cnt, 
	    $idkeys, $deep_times ) = @_;
	my( $x, $key, $s, $cs );
	#format: $ncalls, $name, $secs, $percall, $pcnt

	if ($opt_S) {
	  display_tree( $deep_times, 'toplevel', -1 )
	} else {
	  for( $x = 0; $x < @$idkeys; ++$x ){
	    $key = $idkeys->[$x];
	    $ncalls = $calls->{$key};
	    $name = $names->{$key};
	    $s = $times->{$key}/$hz;
	    $secs = sprintf("%.3f", $s );
	    $cs = $ctimes->{$key}/$hz;
	    $csecs = sprintf("%.3f", $cs );
	    $percall = sprintf("%.4f", $s/$ncalls );
	    $cpercall = sprintf("%.4f", $cs/$ncalls );
	    $pcnt = sprintf("%.2f",
			    $runtime? ((($opt_I ? $csecs : $secs) / $runtime) * 100.0): 0 );
	    write;
	    $pcnt = $secs = $ncalls = $percall = "";
	    write while( length $name );
	    last unless --$cnt;
	  }	  
	}
}

sub move_keys {
  my ($source, $dest) = @_;
  my $kid;
  
  for $kid (keys %$source) {
    if (exists $dest->{$kid}) {
      $dest->{count} += $source->{count};
      $dest->{incl_time} += $source->{incl_time};
      move_keys($source->{kids},$dest->{kids});
    } else {
      $dest->{$kid} = delete $source->{$kid};
    }
  }
}

sub add_to_tree {
  my ($curdeep_times, $name, $t) = @_;
  if ($name ne $curdeep_times->[-1]{name} and $opt_A) {
    $name = $curdeep_times->[-1]{name};
  }
  die "Shorted?!" unless @$curdeep_times >= 2;
  $curdeep_times->[-2]{kids}{$name} = { count => 0 , kids => {}, 
					incl_time => 0,
				      } 
    unless exists $curdeep_times->[-2]{kids}{$name};
  my $entry = $curdeep_times->[-2]{kids}{$name};
  # Now transfer to the new node (could not do earlier, since name can change)
  $entry->{count}++;
  $entry->{incl_time} += $t - $curdeep_times->[-1]{enter_stamp};
  # Merge the kids?
  move_keys($curdeep_times->[-1]->{kids},$entry->{kids});
  pop @$curdeep_times;
}

sub parsestack {
	my( $fh, $names, $calls, $times, $ctimes, $idkeys ) = @_;
	my( $dir, $name );
	my( $t, $syst, $realt, $usert );
	my( $x, $z, $c, $id, $pack );
	my @stack = ();
	my @tstack = ();
	my $tab = 3;
	my $in = 0;

	# remember last call depth and function name
	my $l_in = $in;
	my $l_name = '';
	my $repcnt = 0;
	my $repstr = '';
	my $dprof_t = 0;
	my $dprof_stamp;
	my %cv_hash;
	my $in_level = not defined $opt_g; # Level deep in report grouping
	my $curdeep_times = [$deep_times];

	my $over_per_call;
	if   ( $opt_u )	{	$over_per_call = $over_utime		}
	elsif( $opt_s )	{	$over_per_call = $over_stime		}
	elsif( $opt_r )	{	$over_per_call = $over_rtime		}
	else		{	$over_per_call = $over_utime + $over_stime }
	$over_per_call /= 2*$over_tests; # distribute over entry and exit

	while(<$fh>){
		next if /^#/;
		last if /^PART/;

		chop;
		if (/^&/) {
		  ($dir, $id, $pack, $name) = split;
		  if ($opt_R and ($name =~ /::(__ANON_|END)$/)) {
		    $name .= "($id)";
		  }
		  $cv_hash{$id} = "$pack\::$name";
		  next;
		}
		($dir, $usert, $syst, $realt, $name) = split;

		my $ot = $t;
		if ( $dir eq '/' ) {
		  $syst = $stack[-1][0];
		  $usert = '&';
		  $dir = '-';
		  #warn("Inserted exit for $stack[-1][0].\n")
		}
		if (defined $realt) { # '+ times nam' '- times nam' or '@ incr'
		  if   ( $opt_u )	{	$t = $usert		}
		  elsif( $opt_s )	{	$t = $syst		}
		  elsif( $opt_r )	{	$t = $realt		}
		  else			{	$t = $usert + $syst	}
		  $t += $ot, next if $dir eq '@'; # Increments there
		} else {
		  # "- id" or "- & name"
		  $name = defined $syst ? $syst : $cv_hash{$usert};
		}

		next unless $in_level or $name eq $opt_g or $dir eq '*';
		if ( $dir eq '-' or $dir eq '*' ) {
		  	my $ename = $dir eq '*' ? $stack[-1][0]  : $name;
			$overhead += $over_per_call;
		  	if ($name eq "Devel::DProf::write") {
			  $dprof_t += $t - $dprof_stamp;
			  next;
		  	} elsif (defined $opt_g and $ename eq $opt_g) {
			  $in_level--;
			}
			add_to_tree($curdeep_times, $ename,
				    $t - $dprof_t - $overhead) if $opt_S;
			exitstamp( \@stack, \@tstack, 
				   $t - $dprof_t - $overhead, 
				   $times, $ctimes, $ename, \$in, $tab, 
				   $curdeep_times );
		} 
		next unless $in_level or $name eq $opt_g;
		if( $dir eq '+' or $dir eq '*' ){
		  	if ($name eq "Devel::DProf::write") {
			  $dprof_stamp = $t;
			  next;
		  	} elsif (defined $opt_g and $name eq $opt_g) {
			  $in_level++;
		  	}
			$overhead += $over_per_call;
			if( $opt_T ){
				print ' ' x $in, "$name\n";
				$in += $tab;
			}
			elsif( $opt_t ){
				# suppress output on same function if the
				# same calling level is called.
				if ($l_in == $in and $l_name eq $name) {
					$repcnt++;
				} else {
					$repstr = ' ('.++$repcnt.'x)'
						 if $repcnt;
					print ' ' x $l_in, "$l_name$repstr\n"
						if $l_name ne '';
					$repstr = '';
					$repcnt = 0;
					$l_in = $in;
					$l_name = $name;
				}
				$in += $tab;
			}
			if( ! defined $names->{$name} ){
				$names->{$name} = $name;
				$times->{$name} = 0;
				$ctimes->{$name} = 0;
				push( @$idkeys, $name );
			}
			$calls->{$name}++;
			push @$curdeep_times, { kids => {}, 
						name => $name, 
						enter_stamp => $t - $dprof_t - $overhead,
					      } if $opt_S;
			$x = [ $name, $t - $dprof_t - $overhead ];
			push( @stack, $x );

			# my children will put their time here
			push( @tstack, 0 );
		} elsif ($dir ne '-'){
		    die "Bad profile: $_";
	        }
	}
	if( $opt_t ){
		$repstr = ' ('.++$repcnt.'x)' if $repcnt;
		print ' ' x $l_in, "$l_name$repstr\n";
	}

	if( @stack ){
		if( ! $opt_F ){
			warn "Garbled profile is missing some exit time stamps:\n";
			foreach $x (@stack) {
				print $x->[0],"\n";
			}
			die "Try rerunning dprofpp with -F.\n";
			# I don't want -F to be default behavior--yet
			#  9/18/95 dmr
		}
		else{
			warn( "Faking " . scalar( @stack ) . " exit timestamp(s).\n");
			foreach $x ( reverse @stack ){
				$name = $x->[0];
				exitstamp( \@stack, \@tstack, 
					   $t - $dprof_t - $overhead, $times, 
					   $ctimes, $name, \$in, $tab, 
					   $curdeep_times );
				add_to_tree($curdeep_times, $name,
					    $t - $dprof_t - $overhead)
				  if $opt_S;
			}
		}
	}
	if (defined $opt_g) {
	  $runtime = $ctimes->{$opt_g}/$hz;
	  $runtime = 0 unless $runtime > 0;
	}
}

sub exitstamp {
	my($stack, $tstack, $t, $times, $ctimes, $name, $in, $tab, $deep) = @_;
	my( $x, $c, $z );

	$x = pop( @$stack );
	if( ! defined $x ){
		die "Garbled profile, missing an enter time stamp";
	}
	if( $x->[0] ne $name ){
	  if ($x->[0] =~ /::AUTOLOAD$/) {
	    if ($opt_A) {
	      $name = $x->[0];
	    }
	  } elsif ( $opt_F ) {
	    warn( "Garbled profile, faking exit timestamp:\n\t$name => $x->[0].\n");
	    $name = $x->[0];
	  } else {
	    foreach $z (@stack, $x) {
	      print $z->[0],"\n";
	    }
	    die "Garbled profile, unexpected exit time stamp";
	  }
	}
	if( $opt_T || $opt_t ){
		$$in -= $tab;
	}
	# collect childtime
	$c = pop( @$tstack );
	# total time this func has been active
	$z = $t - $x->[1];
	$ctimes->{$name} += $z;
	$times->{$name} += ($z > $c)? $z - $c: 0;
	# pass my time to my parent
	if( @$tstack ){
		$c = pop( @$tstack );
		push( @$tstack, $c + $z );
	}
}


sub header {
	my $fh = shift;
	chop($_ = <$fh>);
	if( ! /^#fOrTyTwO$/ ){
		die "Not a perl profile";
	}
	while(<$fh>){
		next if /^#/;
		last if /^PART/;
		eval;
	}
	$over_tests = 1 unless $over_tests;
	$time_precision = length int ($hz - 1);	# log ;-)
}


# Report avg time-per-function in seconds
sub percalc {
	my( $calls, $times, $persecs, $idkeys ) = @_;
	my( $x, $t, $n, $key );

	for( $x = 0; $x < @$idkeys; ++$x ){
		$key = $idkeys->[$x];
		$n = $calls->{$key};
		$t = $times->{$key} / $hz;
		$persecs->{$key} = $t ? $t / $n : 0;
	}
}


# Runs the given script with the given profiler and the given perl.
sub run_profiler {
	my $script = shift;
	my $profiler = shift;
	my $startperl = shift;

	system $startperl, "-d:$profiler", $script;
	if( $? / 256 > 0 ){
		die "Failed: $startperl -d:$profiler $script: $!";
	}
}


sub by_time { $times->{$b} <=> $times->{$a} }
sub by_ctime { $ctimes->{$b} <=> $ctimes->{$a} }
sub by_calls { $calls->{$b} <=> $calls->{$a} }
sub by_alpha { $names->{$a} cmp $names->{$b} }
sub by_avgcpu { $persecs->{$b} <=> $persecs->{$a} }


format CSTAT_top =
Total Elapsed Time = @>>>>>>> Seconds
(($rrun_rtime - $overhead - $over_rtime * $total_marks/$over_tests/2) / $hz)
  @>>>>>>>>>> Time = @>>>>>>> Seconds
$whichtime, $runtime
@<<<<<<<< Times
$incl_excl
%Time ExclSec CumulS #Calls sec/call Csec/c  Name
.

format STAT =
 ^>>>   ^>>>> ^>>>>> ^>>>>>   ^>>>>> ^>>>>>  ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$pcnt, $secs, $csecs, $ncalls, $percall, $cpercall, $name
.

!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
