package Math::BigInt;
$VERSION='0.01';

use overload
'+'	=>	sub {new Math::BigInt &badd},
'-'	=>	sub {new Math::BigInt
		       $_[2]? bsub($_[1],${$_[0]}) : bsub(${$_[0]},$_[1])},
'<=>'	=>	sub {$_[2]? bcmp($_[1],${$_[0]}) : bcmp(${$_[0]},$_[1])},
'cmp'	=>	sub {$_[2]? ($_[1] cmp ${$_[0]}) : (${$_[0]} cmp $_[1])},
'*'	=>	sub {new Math::BigInt &bmul},
'/'	=>	sub {new Math::BigInt 
		       $_[2]? scalar bdiv($_[1],${$_[0]}) :
			 scalar bdiv(${$_[0]},$_[1])},
'%'	=>	sub {new Math::BigInt
		       $_[2]? bmod($_[1],${$_[0]}) : bmod(${$_[0]},$_[1])},
'**'	=>	sub {new Math::BigInt
		       $_[2]? bpow($_[1],${$_[0]}) : bpow(${$_[0]},$_[1])},
'neg'	=>	sub {new Math::BigInt &bneg},
'abs'	=>	sub {new Math::BigInt &babs},
'<<'	=>	sub {new Math::BigInt
		       $_[2]? blsft($_[1],${$_[0]}) : blsft(${$_[0]},$_[1])},
'>>'	=>	sub {new Math::BigInt
		       $_[2]? brsft($_[1],${$_[0]}) : brsft(${$_[0]},$_[1])},
'&'	=>	sub {new Math::BigInt &band},
'|'	=>	sub {new Math::BigInt &bior},
'^'	=>	sub {new Math::BigInt &bxor},
'~'	=>	sub {new Math::BigInt &bnot},

qw(
""	stringify
0+	numify)			# Order of arguments unsignificant
;

$NaNOK=1;

sub new {
  my($class) = shift;
  my($foo) = bnorm(shift);
  die "Not a number initialized to Math::BigInt" if !$NaNOK && $foo eq "NaN";
  bless \$foo, $class;
}
sub stringify { "${$_[0]}" }
sub numify { 0 + "${$_[0]}" }	# Not needed, additional overhead
				# comparing to direct compilation based on
				# stringify
sub import {
  shift;
  return unless @_;
  die "unknown import: @_" unless @_ == 1 and $_[0] eq ':constant';
  overload::constant integer => sub {Math::BigInt->new(shift)};
}

$zero = 0;

# overcome a floating point problem on certain osnames (posix-bc, os390)
BEGIN {
    my $x = 100000.0;
    my $use_mult = int($x*1e-5)*1e5 == $x ? 1 : 0;
}

# normalize string form of number.   Strip leading zeros.  Strip any
#   white space and add a sign, if missing.
# Strings that are not numbers result the value 'NaN'.

sub bnorm { #(num_str) return num_str
    local($_) = @_;
    s/\s+//g;                           # strip white space
    if (s/^([+-]?)0*(\d+)$/$1$2/) {     # test if number
	substr($_,$[,0) = '+' unless $1; # Add missing sign
	s/^-0/+0/;
	$_;
    } else {
	'NaN';
    }
}

# Convert a number from string format to internal base 100000 format.
#   Assumes normalized value as input.
sub internal { #(num_str) return int_num_array
    local($d) = @_;
    ($is,$il) = (substr($d,$[,1),length($d)-2);
    substr($d,$[,1) = '';
    ($is, reverse(unpack("a" . ($il%5+1) . ("a5" x ($il/5)), $d)));
}

# Convert a number from internal base 100000 format to string format.
#   This routine scribbles all over input array.
sub external { #(int_num_array) return num_str
    $es = shift;
    grep($_ > 9999 || ($_ = substr('0000'.$_,-5)), @_);   # zero pad
    &bnorm(join('', $es, reverse(@_)));    # reverse concat and normalize
}

# Negate input value.
sub bneg { #(num_str) return num_str
    local($_) = &bnorm(@_);
    return $_ if $_ eq '+0' or $_ eq 'NaN';
    vec($_,0,8) ^= ord('+') ^ ord('-');
    $_;
}

# Returns the absolute value of the input.
sub babs { #(num_str) return num_str
    &abs(&bnorm(@_));
}

sub abs { # post-normalized abs for internal use
    local($_) = @_;
    s/^-/+/;
    $_;
}

# Compares 2 values.  Returns one of undef, <0, =0, >0. (suitable for sort)
sub bcmp { #(num_str, num_str) return cond_code
    local($x,$y) = (&bnorm($_[$[]),&bnorm($_[$[+1]));
    if ($x eq 'NaN') {
	undef;
    } elsif ($y eq 'NaN') {
	undef;
    } else {
	&cmp($x,$y) <=> 0;
    }
}

sub cmp { # post-normalized compare for internal use
    local($cx, $cy) = @_;
    
    return 0 if ($cx eq $cy);

    local($sx, $sy) = (substr($cx, 0, 1), substr($cy, 0, 1));
    local($ld);

    if ($sx eq '+') {
      return  1 if ($sy eq '-' || $cy eq '+0');
      $ld = length($cx) - length($cy);
      return $ld if ($ld);
      return $cx cmp $cy;
    } else { # $sx eq '-'
      return -1 if ($sy eq '+');
      $ld = length($cy) - length($cx);
      return $ld if ($ld);
      return $cy cmp $cx;
    }
}

sub badd { #(num_str, num_str) return num_str
    local(*x, *y); ($x, $y) = (&bnorm($_[$[]),&bnorm($_[$[+1]));
    if ($x eq 'NaN') {
	'NaN';
    } elsif ($y eq 'NaN') {
	'NaN';
    } else {
	@x = &internal($x);             # convert to internal form
	@y = &internal($y);
	local($sx, $sy) = (shift @x, shift @y); # get signs
	if ($sx eq $sy) {
	    &external($sx, &add(*x, *y)); # if same sign add
	} else {
	    ($x, $y) = (&abs($x),&abs($y)); # make abs
	    if (&cmp($y,$x) > 0) {
		&external($sy, &sub(*y, *x));
	    } else {
		&external($sx, &sub(*x, *y));
	    }
	}
    }
}

sub bsub { #(num_str, num_str) return num_str
    &badd($_[$[],&bneg($_[$[+1]));    
}

# GCD -- Euclids algorithm Knuth Vol 2 pg 296
sub bgcd { #(num_str, num_str) return num_str
    local($x,$y) = (&bnorm($_[$[]),&bnorm($_[$[+1]));
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	($x, $y) = ($y,&bmod($x,$y)) while $y ne '+0';
	$x;
    }
}

# routine to add two base 1e5 numbers
#   stolen from Knuth Vol 2 Algorithm A pg 231
#   there are separate routines to add and sub as per Kunth pg 233
sub add { #(int_num_array, int_num_array) return int_num_array
    local(*x, *y) = @_;
    $car = 0;
    for $x (@x) {
	last unless @y || $car;
	$x -= 1e5 if $car = (($x += (@y ? shift(@y) : 0) + $car) >= 1e5) ? 1 : 0;
    }
    for $y (@y) {
	last unless $car;
	$y -= 1e5 if $car = (($y += $car) >= 1e5) ? 1 : 0;
    }
    (@x, @y, $car);
}

# subtract base 1e5 numbers -- stolen from Knuth Vol 2 pg 232, $x > $y
sub sub { #(int_num_array, int_num_array) return int_num_array
    local(*sx, *sy) = @_;
    $bar = 0;
    for $sx (@sx) {
	last unless @sy || $bar;
	$sx += 1e5 if $bar = (($sx -= (@sy ? shift(@sy) : 0) + $bar) < 0);
    }
    @sx;
}

# multiply two numbers -- stolen from Knuth Vol 2 pg 233
sub bmul { #(num_str, num_str) return num_str
    local(*x, *y); ($x, $y) = (&bnorm($_[$[]), &bnorm($_[$[+1]));
    if ($x eq 'NaN') {
	'NaN';
    } elsif ($y eq 'NaN') {
	'NaN';
    } else {
	@x = &internal($x);
	@y = &internal($y);
	&external(&mul(*x,*y));
    }
}

# multiply two numbers in internal representation
# destroys the arguments, supposes that two arguments are different
sub mul { #(*int_num_array, *int_num_array) return int_num_array
    local(*x, *y) = (shift, shift);
    local($signr) = (shift @x ne shift @y) ? '-' : '+';
    @prod = ();
    for $x (@x) {
      ($car, $cty) = (0, $[);
      for $y (@y) {
	$prod = $x * $y + ($prod[$cty] || 0) + $car;
        if ($use_mult) {
	$prod[$cty++] =
	  $prod - ($car = int($prod * 1e-5)) * 1e5;
        }
        else {
	$prod[$cty++] =
	  $prod - ($car = int($prod / 1e5)) * 1e5;
        }
      }
      $prod[$cty] += $car if $car;
      $x = shift @prod;
    }
    ($signr, @x, @prod);
}

# modulus
sub bmod { #(num_str, num_str) return num_str
    (&bdiv(@_))[$[+1];
}

sub bdiv { #(dividend: num_str, divisor: num_str) return num_str
    local (*x, *y); ($x, $y) = (&bnorm($_[$[]), &bnorm($_[$[+1]));
    return wantarray ? ('NaN','NaN') : 'NaN'
	if ($x eq 'NaN' || $y eq 'NaN' || $y eq '+0');
    return wantarray ? ('+0',$x) : '+0' if (&cmp(&abs($x),&abs($y)) < 0);
    @x = &internal($x); @y = &internal($y);
    $srem = $y[$[];
    $sr = (shift @x ne shift @y) ? '-' : '+';
    $car = $bar = $prd = 0;
    if (($dd = int(1e5/($y[$#y]+1))) != 1) {
	for $x (@x) {
	    $x = $x * $dd + $car;
            if ($use_mult) {
	    $x -= ($car = int($x * 1e-5)) * 1e5;
            }
            else {
	    $x -= ($car = int($x / 1e5)) * 1e5;
            }
	}
	push(@x, $car); $car = 0;
	for $y (@y) {
	    $y = $y * $dd + $car;
            if ($use_mult) {
	    $y -= ($car = int($y * 1e-5)) * 1e5;
            }
            else {
	    $y -= ($car = int($y / 1e5)) * 1e5;
            }
	}
    }
    else {
	push(@x, 0);
    }
    @q = (); ($v2,$v1) = @y[-2,-1];
    $v2 = 0 unless $v2;
    while ($#x > $#y) {
	($u2,$u1,$u0) = @x[-3..-1];
	$u2 = 0 unless $u2;
	$q = (($u0 == $v1) ? 99999 : int(($u0*1e5+$u1)/$v1));
	--$q while ($v2*$q > ($u0*1e5+$u1-$q*$v1)*1e5+$u2);
	if ($q) {
	    ($car, $bar) = (0,0);
	    for ($y = $[, $x = $#x-$#y+$[-1; $y <= $#y; ++$y,++$x) {
		$prd = $q * $y[$y] + $car;
                if ($use_mult) {
		$prd -= ($car = int($prd * 1e-5)) * 1e5;
                }
                else {
		$prd -= ($car = int($prd / 1e5)) * 1e5;
                }
		$x[$x] += 1e5 if ($bar = (($x[$x] -= $prd + $bar) < 0));
	    }
	    if ($x[$#x] < $car + $bar) {
		$car = 0; --$q;
		for ($y = $[, $x = $#x-$#y+$[-1; $y <= $#y; ++$y,++$x) {
		    $x[$x] -= 1e5
			if ($car = (($x[$x] += $y[$y] + $car) > 1e5));
		}
	    }   
	}
	pop(@x); unshift(@q, $q);
    }
    if (wantarray) {
	@d = ();
	if ($dd != 1) {
	    $car = 0;
	    for $x (reverse @x) {
		$prd = $car * 1e5 + $x;
		$car = $prd - ($tmp = int($prd / $dd)) * $dd;
		unshift(@d, $tmp);
	    }
	}
	else {
	    @d = @x;
	}
	(&external($sr, @q), &external($srem, @d, $zero));
    } else {
	&external($sr, @q);
    }
}

# compute power of two numbers -- stolen from Knuth Vol 2 pg 233
sub bpow { #(num_str, num_str) return num_str
    local(*x, *y); ($x, $y) = (&bnorm($_[$[]), &bnorm($_[$[+1]));
    if ($x eq 'NaN') {
	'NaN';
    } elsif ($y eq 'NaN') {
	'NaN';
    } elsif ($x eq '+1') {
	'+1';
    } elsif ($x eq '-1') {
	&bmod($x,2) ? '-1': '+1';
    } elsif ($y =~ /^-/) {
	'NaN';
    } elsif ($x eq '+0' && $y eq '+0') {
	'NaN';
    } else {
	@x = &internal($x);
	local(@pow2)=@x;
	local(@pow)=&internal("+1");
	local($y1,$res,@tmp1,@tmp2)=(1); # need tmp to send to mul
	while ($y ne '+0') {
	  ($y,$res)=&bdiv($y,2);
	  if ($res ne '+0') {@tmp=@pow2; @pow=&mul(*pow,*tmp);}
	  if ($y ne '+0') {@tmp=@pow2;@pow2=&mul(*pow2,*tmp);}
	}
	&external(@pow);
    }
}

# compute x << y, y >= 0
sub blsft { #(num_str, num_str) return num_str
    &bmul($_[$[], &bpow(2, $_[$[+1]));
}

# compute x >> y, y >= 0
sub brsft { #(num_str, num_str) return num_str
    &bdiv($_[$[], &bpow(2, $_[$[+1]));
}

# compute x & y
sub band { #(num_str, num_str) return num_str
    local($x,$y,$r,$m,$xr,$yr) = (&bnorm($_[$[]),&bnorm($_[$[+1]),0,1);
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	while ($x ne '+0' && $y ne '+0') {
	    ($x, $xr) = &bdiv($x, 0x10000);
	    ($y, $yr) = &bdiv($y, 0x10000);
	    $r = &badd(&bmul(int $xr & $yr, $m), $r);
	    $m = &bmul($m, 0x10000);
	}
	$r;
    }
}

# compute x | y
sub bior { #(num_str, num_str) return num_str
    local($x,$y,$r,$m,$xr,$yr) = (&bnorm($_[$[]),&bnorm($_[$[+1]),0,1);
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	while ($x ne '+0' || $y ne '+0') {
	    ($x, $xr) = &bdiv($x, 0x10000);
	    ($y, $yr) = &bdiv($y, 0x10000);
	    $r = &badd(&bmul(int $xr | $yr, $m), $r);
	    $m = &bmul($m, 0x10000);
	}
	$r;
    }
}

# compute x ^ y
sub bxor { #(num_str, num_str) return num_str
    local($x,$y,$r,$m,$xr,$yr) = (&bnorm($_[$[]),&bnorm($_[$[+1]),0,1);
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	while ($x ne '+0' || $y ne '+0') {
	    ($x, $xr) = &bdiv($x, 0x10000);
	    ($y, $yr) = &bdiv($y, 0x10000);
	    $r = &badd(&bmul(int $xr ^ $yr, $m), $r);
	    $m = &bmul($m, 0x10000);
	}
	$r;
    }
}

# represent ~x as twos-complement number
sub bnot { #(num_str) return num_str
    &bsub(-1,$_[$[]);
}

1;
__END__

=head1 NAME

Math::BigInt - Arbitrary size integer math package

=head1 SYNOPSIS

  use Math::BigInt;
  $i = Math::BigInt->new($string);

  $i->bneg return BINT               negation
  $i->babs return BINT               absolute value
  $i->bcmp(BINT) return CODE         compare numbers (undef,<0,=0,>0)
  $i->badd(BINT) return BINT         addition
  $i->bsub(BINT) return BINT         subtraction
  $i->bmul(BINT) return BINT         multiplication
  $i->bdiv(BINT) return (BINT,BINT)  division (quo,rem) just quo if scalar
  $i->bmod(BINT) return BINT         modulus
  $i->bgcd(BINT) return BINT         greatest common divisor
  $i->bnorm return BINT              normalization
  $i->blsft(BINT) return BINT        left shift
  $i->brsft(BINT) return (BINT,BINT) right shift (quo,rem) just quo if scalar
  $i->band(BINT) return BINT         bit-wise and
  $i->bior(BINT) return BINT         bit-wise inclusive or
  $i->bxor(BINT) return BINT         bit-wise exclusive or
  $i->bnot return BINT               bit-wise not

=head1 DESCRIPTION

All basic math operations are overloaded if you declare your big
integers as

  $i = new Math::BigInt '123 456 789 123 456 789';


=over 2

=item Canonical notation

Big integer value are strings of the form C</^[+-]\d+$/> with leading
zeros suppressed.

=item Input

Input values to these routines may be strings of the form
C</^\s*[+-]?[\d\s]+$/>.

=item Output

Output values always always in canonical form

=back

Actual math is done in an internal format consisting of an array
whose first element is the sign (/^[+-]$/) and whose remaining 
elements are base 100000 digits with the least significant digit first.
The string 'NaN' is used to represent the result when input arguments 
are not numbers, as well as the result of dividing by zero.

=head1 EXAMPLES

   '+0'                            canonical zero value
   '   -123 123 123'               canonical value '-123123123'
   '1 23 456 7890'                 canonical value '+1234567890'


=head1 Autocreating constants

After C<use Math::BigInt ':constant'> all the integer decimal constants
in the given scope are converted to C<Math::BigInt>.  This conversion
happens at compile time.

In particular

  perl -MMath::BigInt=:constant -e 'print 2**100'

print the integer value of C<2**100>.  Note that without conversion of 
constants the expression 2**100 will be calculated as floating point number.

=head1 BUGS

The current version of this module is a preliminary version of the
real thing that is currently (as of perl5.002) under development.

=head1 AUTHOR

Mark Biggar, overloaded interface by Ilya Zakharevich.

=cut
