package Math::BigFloat;

use Math::BigInt;

use Exporter;  # just for use to be happy
@ISA = (Exporter);

use overload
'+'	=>	sub {new Math::BigFloat &fadd},
'-'	=>	sub {new Math::BigFloat
		       $_[2]? fsub($_[1],${$_[0]}) : fsub(${$_[0]},$_[1])},
'<=>'	=>	sub {new Math::BigFloat
		       $_[2]? fcmp($_[1],${$_[0]}) : fcmp(${$_[0]},$_[1])},
'cmp'	=>	sub {new Math::BigFloat
		       $_[2]? ($_[1] cmp ${$_[0]}) : (${$_[0]} cmp $_[1])},
'*'	=>	sub {new Math::BigFloat &fmul},
'/'	=>	sub {new Math::BigFloat 
		       $_[2]? scalar fdiv($_[1],${$_[0]}) :
			 scalar fdiv(${$_[0]},$_[1])},
'neg'	=>	sub {new Math::BigFloat &fneg},
'abs'	=>	sub {new Math::BigFloat &fabs},

qw(
""	stringify
0+	numify)			# Order of arguments unsignificant
;

sub new {
  my ($class) = shift;
  my ($foo) = fnorm(shift);
  panic("Not a number initialized to Math::BigFloat") if $foo eq "NaN";
  bless \$foo, $class;
}
sub numify { 0 + "${$_[0]}" }	# Not needed, additional overhead
				# comparing to direct compilation based on
				# stringify
sub stringify {
    my $n = ${$_[0]};

    my $minus = ($n =~ s/^([+-])// && $1 eq '-');
    $n =~ s/E//;

    $n =~ s/([-+]\d+)$//;

    my $e = $1;
    my $ln = length($n);

    if ($e > 0) {
	$n .= "0" x $e . '.';
    } elsif (abs($e) < $ln) {
	substr($n, $ln + $e, 0) = '.';
    } else {
	$n = '.' . ("0" x (abs($e) - $ln)) . $n;
    }
    $n = "-$n" if $minus;

    # 1 while $n =~ s/(.*\d)(\d\d\d)/$1,$2/;

    return $n;
}

$div_scale = 40;

# Rounding modes one of 'even', 'odd', '+inf', '-inf', 'zero' or 'trunc'.

$rnd_mode = 'even';

sub fadd; sub fsub; sub fmul; sub fdiv;
sub fneg; sub fabs; sub fcmp;
sub fround; sub ffround;
sub fnorm; sub fsqrt;

# Convert a number to canonical string form.
#   Takes something that looks like a number and converts it to
#   the form /^[+-]\d+E[+-]\d+$/.
sub fnorm { #(string) return fnum_str
    local($_) = @_;
    s/\s+//g;                               # strip white space
    if (/^([+-]?)(\d*)(\.(\d*))?([Ee]([+-]?\d+))?$/ && "$2$4" ne '') {
	&norm(($1 ? "$1$2$4" : "+$2$4"),(($4 ne '') ? $6-length($4) : $6));
    } else {
	'NaN';
    }
}

# normalize number -- for internal use
sub norm { #(mantissa, exponent) return fnum_str
    local($_, $exp) = @_;
    if ($_ eq 'NaN') {
	'NaN';
    } else {
	s/^([+-])0+/$1/;                        # strip leading zeros
	if (length($_) == 1) {
	    '+0E+0';
	} else {
	    $exp += length($1) if (s/(0+)$//);  # strip trailing zeros
	    sprintf("%sE%+ld", $_, $exp);
	}
    }
}

# negation
sub fneg { #(fnum_str) return fnum_str
    local($_) = fnorm($_[$[]);
    vec($_,0,8) ^= ord('+') ^ ord('-') unless $_ eq '+0E+0'; # flip sign
    s/^H/N/;
    $_;
}

# absolute value
sub fabs { #(fnum_str) return fnum_str
    local($_) = fnorm($_[$[]);
    s/^-/+/;		                       # mash sign
    $_;
}

# multiplication
sub fmul { #(fnum_str, fnum_str) return fnum_str
    local($x,$y) = (fnorm($_[$[]),fnorm($_[$[+1]));
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	local($ym,$ye) = split('E',$y);
	&norm(Math::BigInt::bmul($xm,$ym),$xe+$ye);
    }
}

# addition
sub fadd { #(fnum_str, fnum_str) return fnum_str
    local($x,$y) = (fnorm($_[$[]),fnorm($_[$[+1]));
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	local($ym,$ye) = split('E',$y);
	($xm,$xe,$ym,$ye) = ($ym,$ye,$xm,$xe) if ($xe < $ye);
	&norm(Math::BigInt::badd($ym,$xm.('0' x ($xe-$ye))),$ye);
    }
}

# subtraction
sub fsub { #(fnum_str, fnum_str) return fnum_str
    fadd($_[$[],fneg($_[$[+1]));    
}

# division
#   args are dividend, divisor, scale (optional)
#   result has at most max(scale, length(dividend), length(divisor)) digits
sub fdiv #(fnum_str, fnum_str[,scale]) return fnum_str
{
    local($x,$y,$scale) = (fnorm($_[$[]),fnorm($_[$[+1]),$_[$[+2]);
    if ($x eq 'NaN' || $y eq 'NaN' || $y eq '+0E+0') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	local($ym,$ye) = split('E',$y);
	$scale = $div_scale if (!$scale);
	$scale = length($xm)-1 if (length($xm)-1 > $scale);
	$scale = length($ym)-1 if (length($ym)-1 > $scale);
	$scale = $scale + length($ym) - length($xm);
	&norm(&round(Math::BigInt::bdiv($xm.('0' x $scale),$ym),$ym),
	    $xe-$ye-$scale);
    }
}

# round int $q based on fraction $r/$base using $rnd_mode
sub round { #(int_str, int_str, int_str) return int_str
    local($q,$r,$base) = @_;
    if ($q eq 'NaN' || $r eq 'NaN') {
	'NaN';
    } elsif ($rnd_mode eq 'trunc') {
	$q;                         # just truncate
    } else {
	local($cmp) = Math::BigInt::bcmp(Math::BigInt::bmul($r,'+2'),$base);
	if ( $cmp < 0 ||
		 ($cmp == 0 &&
		  ( $rnd_mode eq 'zero'                             ||
		   ($rnd_mode eq '-inf' && (substr($q,$[,1) eq '+')) ||
		   ($rnd_mode eq '+inf' && (substr($q,$[,1) eq '-')) ||
		   ($rnd_mode eq 'even' && $q =~ /[24680]$/)        ||
		   ($rnd_mode eq 'odd'  && $q =~ /[13579]$/)        )) ) {
	    $q;                     # round down
	} else {
	    Math::BigInt::badd($q, ((substr($q,$[,1) eq '-') ? '-1' : '+1'));
				    # round up
	}
    }
}

# round the mantissa of $x to $scale digits
sub fround { #(fnum_str, scale) return fnum_str
    local($x,$scale) = (fnorm($_[$[]),$_[$[+1]);
    if ($x eq 'NaN' || $scale <= 0) {
	$x;
    } else {
	local($xm,$xe) = split('E',$x);
	if (length($xm)-1 <= $scale) {
	    $x;
	} else {
	    &norm(&round(substr($xm,$[,$scale+1),
			 "+0".substr($xm,$[+$scale+1,1),"+10"),
		  $xe+length($xm)-$scale-1);
	}
    }
}

# round $x at the 10 to the $scale digit place
sub ffround { #(fnum_str, scale) return fnum_str
    local($x,$scale) = (fnorm($_[$[]),$_[$[+1]);
    if ($x eq 'NaN') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	if ($xe >= $scale) {
	    $x;
	} else {
	    $xe = length($xm)+$xe-$scale;
	    if ($xe < 1) {
		'+0E+0';
	    } elsif ($xe == 1) {
		&norm(&round('+0',"+0".substr($xm,$[+1,1),"+10"), $scale);
	    } else {
		&norm(&round(substr($xm,$[,$xe),
		      "+0".substr($xm,$[+$xe,1),"+10"), $scale);
	    }
	}
    }
}
    
# compare 2 values returns one of undef, <0, =0, >0
#   returns undef if either or both input value are not numbers
sub fcmp #(fnum_str, fnum_str) return cond_code
{
    local($x, $y) = (fnorm($_[$[]),fnorm($_[$[+1]));
    if ($x eq "NaN" || $y eq "NaN") {
	undef;
    } else {
	ord($y) <=> ord($x)
	||
	(  local($xm,$xe,$ym,$ye) = split('E', $x."E$y"),
	     (($xe <=> $ye) * (substr($x,$[,1).'1')
             || Math::BigInt::cmp($xm,$ym))
	);
    }
}

# square root by Newtons method.
sub fsqrt { #(fnum_str[, scale]) return fnum_str
    local($x, $scale) = (fnorm($_[$[]), $_[$[+1]);
    if ($x eq 'NaN' || $x =~ /^-/) {
	'NaN';
    } elsif ($x eq '+0E+0') {
	'+0E+0';
    } else {
	local($xm, $xe) = split('E',$x);
	$scale = $div_scale if (!$scale);
	$scale = length($xm)-1 if ($scale < length($xm)-1);
	local($gs, $guess) = (1, sprintf("1E%+d", (length($xm)+$xe-1)/2));
	while ($gs < 2*$scale) {
	    $guess = fmul(fadd($guess,fdiv($x,$guess,$gs*2)),".5");
	    $gs *= 2;
	}
	new Math::BigFloat &fround($guess, $scale);
    }
}

1;
__END__

=head1 NAME

Math::BigFloat - Arbitrary length float math package

=head1 SYNOPSIS

  use Math::BigFloat;
  $f = Math::BigFloat->new($string);

  $f->fadd(NSTR) return NSTR            addition
  $f->fsub(NSTR) return NSTR            subtraction
  $f->fmul(NSTR) return NSTR            multiplication
  $f->fdiv(NSTR[,SCALE]) returns NSTR   division to SCALE places
  $f->fneg() return NSTR                negation
  $f->fabs() return NSTR                absolute value
  $f->fcmp(NSTR) return CODE            compare undef,<0,=0,>0
  $f->fround(SCALE) return NSTR         round to SCALE digits
  $f->ffround(SCALE) return NSTR        round at SCALEth place
  $f->fnorm() return (NSTR)             normalize
  $f->fsqrt([SCALE]) return NSTR        sqrt to SCALE places

=head1 DESCRIPTION

All basic math operations are overloaded if you declare your big
floats as

    $float = new Math::BigFloat "2.123123123123123123123123123123123";

=over 2

=item number format

canonical strings have the form /[+-]\d+E[+-]\d+/ .  Input values can
have imbedded whitespace.

=item Error returns 'NaN'

An input parameter was "Not a Number" or divide by zero or sqrt of
negative number.

=item Division is computed to 

C<max($div_scale,length(dividend)+length(divisor))> digits by default.
Also used for default sqrt scale.

=back

=head1 BUGS

The current version of this module is a preliminary version of the
real thing that is currently (as of perl5.002) under development.

=head1 AUTHOR

Mark Biggar

=cut
