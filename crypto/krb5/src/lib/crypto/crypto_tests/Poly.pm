# Copyright 2002 by the Massachusetts Institute of Technology.
# All Rights Reserved.
#
# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
# 
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

package Poly;

# Poly: implements some basic operations on polynomials in the field
# of integers (mod 2).
#
# The rep is an array of coefficients, highest order term first.
#
# This is rather slow at the moment.

use overload
    '+' => \&add,
    '-' => \&add,
    '*' => \&mul,
    '%' => sub {$_[2] ? mod($_[1], $_[0]) : mod($_[0], $_[1])},
    '/' => sub { $_[2] ? scalar(div($_[1], $_[0]))
		     : scalar(div($_[0], $_[1])) },
    '<=>' => sub {$_[2] ? pcmp($_[1], $_[0]) : pcmp($_[0], $_[1])},
    '""' => \&str
;

use Carp;

# doesn't do much beyond normalize and bless
sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my(@x) = @_;
    return bless [norm(@x)], $class;
}

# stringified P(x)
sub pretty {
    my(@x) = @{+shift};
    my $n = @x;
    local $_;
    return "0" if !@x;
    return join " + ", map {$n--; $_ ? ("x^$n") : ()} @x;
}

sub print {
    my $self = shift;
    print $self->pretty, "\n";
}

# This assumes normalization.
sub order {
    my $self = shift;
    return $#{$self};
}

sub str {
    return overload::StrVal($_[0]);
}

# strip leading zero coefficients
sub norm {
    my(@x) = @_;
    shift @x while @x && !$x[0];
    return @x;
}

# multiply $self by the single term of power $n
sub multerm {
    my($self, $n) = @_;
    return $self->new(@$self, (0) x $n);
}

# This is really an order comparison; different polys of same order
# compare equal.  It also assumes prior normalization.
sub pcmp {
    my @x = @{+shift};
    my @y = @{+shift};
    return @x <=> @y;
}

# convenience constructor; takes list of non-zero terms
sub powers2poly
{
    my $self = shift;
    my $poly = $self->new;
    my $n;
    foreach $n (@_) {
	$poly += $one->multerm($n);
    }
    return $poly;
}

sub add {
    my $self = shift;
    my @x = @$self;
    my @y = @{+shift};
    my @r;
    unshift @r, (pop @x || 0) ^ (pop @y || 0)
	while @x || @y;
    return $self->new(@r);
}

sub mul {
    my($self) = shift;
    my @y = @{+shift};
    my $r = $self->new;
    my $power = 0;
    while (@y) {
	$r += $self->multerm($power) if pop @y;
	$power++;
    }
    return $r;
}

sub oldmod {
    my($self, $div) = @_;
    my @num = @$self;
    my @div = @$div;
    my $r = $self->new(splice @num, 0, @div);
    do {
	push @$r, shift @num while @num && $r < $div;
	$r += $div if $r >= $div;
    } while @num;
    return $r;
}

sub div {
    my($self, $div) = @_;
    my $q = $self->new;
    my $r = $self->new(@$self);
    my $one = $self->new(1);
    my ($tp, $power);
    croak "divide by zero" if !@$div;
    while ($div <= $r) {
	$power = 0;
	$power++ while ($tp = $div->multerm($power)) < $r;
	$q += $one->multerm($power);
	$r -= $tp;
    }
    return wantarray ? ($q, $r) : $q;
}

sub mod {
    (&div)[1];
}

# bits and octets both big-endian
sub hex {
    my @x = @{+shift};
    my $minwidth = shift || 32;
    unshift @x, 0 while @x % 8 || @x < $minwidth;
    return unpack "H*", pack "B*", join "", @x;
}

# bit-reversal of above
sub revhex {
    my @x = @{+shift};
    my $minwidth = shift || 32;
    unshift @x, 0 while @x % 8 || @x < $minwidth;
    return unpack "H*", pack "B*", join "", reverse @x;
}

$one = Poly->new(1);

1;
