#!./perl

#
# test the conversion operators
#
# Notations:
#
# "N p i N vs N N":  Apply op-N, then op-p, then op-i, then reporter-N
# Compare with application of op-N, then reporter-N
# Right below are descriptions of different ops and reporters.

# We do not use these subroutines any more, sub overhead makes a "switch"
# solution better:

# obviously, 0, 1 and 2, 3 are destructive.  (XXXX 64-bit? 4 destructive too)

# *0 = sub {--$_[0]};		# -
# *1 = sub {++$_[0]};		# +

# # Converters
# *2 = sub { $_[0] = $max_uv & $_[0]}; # U
# *3 = sub { use integer; $_[0] += $zero}; # I
# *4 = sub { $_[0] += $zero};	# N
# *5 = sub { $_[0] = "$_[0]" };	# P

# # Side effects
# *6 = sub { $max_uv & $_[0]};	# u
# *7 = sub { use integer; $_[0] + $zero};	# i
# *8 = sub { $_[0] + $zero};	# n
# *9 = sub { $_[0] . "" };	# p

# # Reporters
# sub a2 { sprintf "%u", $_[0] }	# U
# sub a3 { sprintf "%d", $_[0] }	# I
# sub a4 { sprintf "%g", $_[0] }	# N
# sub a5 { "$_[0]" }		# P

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict 'vars';

my $max_chain = $ENV{PERL_TEST_NUMCONVERTS} || 2;

# Bulk out if unsigned type is hopelessly wrong:
my $max_uv1 = ~0;
my $max_uv2 = sprintf "%u", $max_uv1 ** 6; # 6 is an arbitrary number here
my $big_iv = do {use integer; $max_uv1 * 16}; # 16 is an arbitrary number here

print "# max_uv1 = $max_uv1, max_uv2 = $max_uv2, big_iv = $big_iv\n";
if ($max_uv1 ne $max_uv2 or $big_iv > $max_uv1) {
  print "1..0 # skipped: unsigned perl arithmetic is not sane";
  eval { require Config; import Config };
  use vars qw(%Config);
  if ($Config{d_quad} eq 'define') {
      print " (common in 64-bit platforms)";
  }
  print "\n";
  exit 0;
}

my $st_t = 4*4;			# We try 4 initializers and 4 reporters

my $num = 0;
$num += 10**$_ - 4**$_ for 1.. $max_chain;
$num *= $st_t;
print "1..$num\n";		# In fact 15 times more subsubtests...

my $max_uv = ~0;
my $max_iv = int($max_uv/2);
my $zero = 0;

my $l_uv = length $max_uv;
my $l_iv = length $max_iv;

# Hope: the first digits are good
my $larger_than_uv = substr 97 x 100, 0, $l_uv;
my $smaller_than_iv = substr 12 x 100, 0, $l_iv;
my $yet_smaller_than_iv = substr 97 x 100, 0, ($l_iv - 1);

my @list = (1, $yet_smaller_than_iv, $smaller_than_iv, $max_iv, $max_iv + 1,
	    $max_uv, $max_uv + 1);
unshift @list, (reverse map -$_, @list), 0; # 15 elts
@list = map "$_", @list; # Normalize

# print "@list\n";


my @opnames = split //, "-+UINPuinp";

# @list = map { 2->($_), 3->($_), 4->($_), 5->($_),  } @list; # Prepare input

#print "@list\n";
#print "'@ops'\n";

my $test = 1;
my $nok;
for my $num_chain (1..$max_chain) {
  my @ops = map [split //], grep /[4-9]/,
    map { sprintf "%0${num_chain}d", $_ }  0 .. 10**$num_chain - 1;

  #@ops = ([]) unless $num_chain;
  #@ops = ([6, 4]);

  # print "'@ops'\n";
  for my $op (@ops) {
    for my $first (2..5) {
      for my $last (2..5) {
	$nok = 0;
	my @otherops = grep $_ <= 3, @$op;
	my @curops = ($op,\@otherops);

	for my $num (@list) {
	  my $inpt;
	  my @ans;

	  for my $short (0, 1) {
	    # undef $inpt;	# Forget all we had - some bugs were masked

	    $inpt = $num;	# Try to not contaminate $num...
	    $inpt = "$inpt";
	    if ($first == 2) {
	      $inpt = $max_uv & $inpt; # U 2
	    } elsif ($first == 3) {
	      use integer; $inpt += $zero; # I 3
	    } elsif ($first == 4) {
	      $inpt += $zero;	# N 4
	    } else {
	      $inpt = "$inpt";	# P 5
	    }

	    # Saves 20% of time - not with this logic:
	    #my $tmp = $inpt;
	    #my $tmp1 = $num;
	    #next if $num_chain > 1
	    #  and "$tmp" ne "$tmp1"; # Already the coercion gives problems...

	    for my $curop (@{$curops[$short]}) {
	      if ($curop < 5) {
		if ($curop < 3) {
		  if ($curop == 0) {
		    --$inpt;	# - 0
		  } elsif ($curop == 1) {
		    ++$inpt;	# + 1
		  } else {
		    $inpt = $max_uv & $inpt; # U 2
		  }
		} elsif ($curop == 3) {
		  use integer; $inpt += $zero;
		} else {
		  $inpt += $zero; # N 4
		}
	      } elsif ($curop < 8) {
		if ($curop == 5) {
		  $inpt = "$inpt"; # P 5
		} elsif ($curop == 6) {
		  $max_uv & $inpt; # u 6
		} else {
		  use integer; $inpt + $zero;
		}
	      } elsif ($curop == 8) {
		$inpt + $zero;	# n 8
	      } else {
		$inpt . "";	# p 9
	      }
	    }

	    if ($last == 2) {
	      $inpt = sprintf "%u", $inpt; # U 2
	    } elsif ($last == 3) {
	      $inpt = sprintf "%d", $inpt; # I 3
	    } elsif ($last == 4) {
	      $inpt = sprintf "%g", $inpt; # N 4
	    } else {
	      $inpt = "$inpt";	# P 5
	    }
	    push @ans, $inpt;
	  }
	  $nok++,
	    print "# '$ans[0]' ne '$ans[1]',\t$num\t=> @opnames[$first,@{$curops[0]},$last] vs @opnames[$first,@{$curops[1]},$last]\n"
	      if $ans[0] ne $ans[1];
	}
	print "not " if $nok;
	print "ok $test\n";
	#print $txt if $nok;
	$test++;
      }
    }
  }
}
