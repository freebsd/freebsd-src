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

package CRC;

# CRC: implement a CRC using the Poly package (yes this is slow)
#
# message M(x) = m_0 * x^0 + m_1 * x^1 + ... + m_(k-1) * x^(k-1)
# generator P(x) = p_0 * x^0 + p_1 * x^1 + ... + p_n * x^n
# remainder R(x) = r_0 * x^0 + r_1 * x^1 + ... + r_(n-1) * x^(n-1)
#
# R(x) = (x^n * M(x)) % P(x)
#
# Note that if F(x) = x^n * M(x) + R(x), then F(x) = 0 mod P(x) .
#
# In MIT Kerberos 5, R(x) is taken as the CRC, as opposed to what
# ISO 3309 does.
#
# ISO 3309 adds a precomplement and a postcomplement.
#
# The ISO 3309 postcomplement is of the form
#
# A(x) = x^0 + x^1 + ... + x^(n-1) .
#
# The ISO 3309 precomplement is of the form
#
# B(x) = x^k * A(x) .
#
# The ISO 3309 FCS is then
#
# (x^n * M(x)) % P(x) + B(x) % P(x) + A(x) ,
#
# which is equivalent to
#
# (x^n * M(x) + B(x)) % P(x) + A(x) .
#
# In ISO 3309, the transmitted frame is
#
# F'(x) = x^n * M(x) + R(x) + R'(x) + A(x) ,
#
# where
#
# R'(x) = B(x) % P(x) .
#
# Note that this means that if a new remainder is computed over the
# frame F'(x) (treating F'(x) as the new M(x)), it will be equal to a
# constant.
#
# F'(x) = 0 + R'(x) + A(x) mod P(x) ,
#
# then
#
# (F'(x) + x^k * A(x)) * x^n
#
# = ((R'(x) + A(x)) + x^k * A(x)) * x^n mod P(x)
#
# = (x^k * A(x) + A(x) + x^k * A(x)) * x^n mod P(x)
#
# = (0 + A(x)) * x^n mod P(x)
#
# Note that (A(x) * x^n) % P(x) is a constant, and that this result
# depends on B(x) being x^k * A(x).

use Carp;
use Poly;

sub new {
    my $self = shift;
    my $class = ref($self) || $self;
    my %args = @_;
    $self = {bitsendian => "little"};
    bless $self, $class;
    $self->setpoly($args{"Poly"}) if exists $args{"Poly"};
    $self->bitsendian($args{"bitsendian"})
	if exists $args{"bitsendian"};
    $self->{precomp} = $args{precomp} if exists $args{precomp};
    $self->{postcomp} = $args{postcomp} if exists $args{postcomp};
    return $self;
}

sub setpoly {
    my $self = shift;
    my($arg) = @_;
    croak "need a polynomial" if !$arg->isa("Poly");
    $self->{Poly} = $arg;
    return $self;
}

sub crc {
    my $self = shift;
    my $msg = Poly->new(@_);
    my($order, $r, $precomp);
    $order = $self->{Poly}->order;
    # B(x) = x^k * precomp
    $precomp = $self->{precomp} ?
	$self->{precomp} * Poly->powers2poly(scalar(@_)) : Poly->new;
    # R(x) = (x^n * M(x)) % P(x)
    $r = ($msg * Poly->powers2poly($order)) % $self->{Poly};
    # B(x) % P(x)
    $r += $precomp % $self->{Poly};
    $r += $self->{postcomp} if exists $self->{postcomp};
    return $r;
}

# endianness of bits of each octet
#
# Note that the message is always treated as being sent in big-endian
# octet order.
#
# Usually, the message will be treated as bits being little-endian,
# since that is the common case for serial implementations that
# present data in octets; e.g., most UARTs shift octets onto the line
# in little-endian order, and protocols such as ISO 3309, V.42,
# etc. treat individual octets as being sent LSB-first.

sub bitsendian {
    my $self = shift;
    my($arg) = @_;
    croak "bad bit endianness" if $arg !~ /big|little/;
    $self->{bitsendian} = $arg;
    return $self;
}

sub crcstring {
    my $self = shift;
    my($arg) = @_;
    my($packstr, @m);
    {
	$packstr = "B*", last if $self->{bitsendian} =~ /big/;
	$packstr = "b*", last if $self->{bitsendian} =~ /little/;
	croak "bad bit endianness";
    };
    @m = split //, unpack $packstr, $arg;
    return $self->crc(@m);
}

1;
