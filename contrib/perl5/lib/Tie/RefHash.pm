package Tie::RefHash;

=head1 NAME

Tie::RefHash - use references as hash keys

=head1 SYNOPSIS

    require 5.004;
    use Tie::RefHash;
    tie HASHVARIABLE, 'Tie::RefHash', LIST;
    tie HASHVARIABLE, 'Tie::RefHash::Nestable', LIST;

    untie HASHVARIABLE;

=head1 DESCRIPTION

This module provides the ability to use references as hash keys if you
first C<tie> the hash variable to this module.  Normally, only the
keys of the tied hash itself are preserved as references; to use
references as keys in hashes-of-hashes, use Tie::RefHash::Nestable,
included as part of Tie::RefHash.

It is implemented using the standard perl TIEHASH interface.  Please
see the C<tie> entry in perlfunc(1) and perltie(1) for more information.

The Nestable version works by looking for hash references being stored
and converting them to tied hashes so that they too can have
references as keys.  This will happen without warning whenever you
store a reference to one of your own hashes in the tied hash.

=head1 EXAMPLE

    use Tie::RefHash;
    tie %h, 'Tie::RefHash';
    $a = [];
    $b = {};
    $c = \*main;
    $d = \"gunk";
    $e = sub { 'foo' };
    %h = ($a => 1, $b => 2, $c => 3, $d => 4, $e => 5);
    $a->[0] = 'foo';
    $b->{foo} = 'bar';
    for (keys %h) {
       print ref($_), "\n";
    }

    tie %h, 'Tie::RefHash::Nestable';
    $h{$a}->{$b} = 1;
    for (keys %h, keys %{$h{$a}}) {
       print ref($_), "\n";
    }

=head1 AUTHOR

Gurusamy Sarathy        gsar@activestate.com

=head1 VERSION

Version 1.3    8 Apr 2001

=head1 SEE ALSO

perl(1), perlfunc(1), perltie(1)

=cut

use v5.6.0;
use Tie::Hash;
use strict;

our @ISA = qw(Tie::Hash);
our $VERSION = '1.3';

sub TIEHASH {
  my $c = shift;
  my $s = [];
  bless $s, $c;
  while (@_) {
    $s->STORE(shift, shift);
  }
  return $s;
}

sub FETCH {
  my($s, $k) = @_;
  if (ref $k) {
      if (defined $s->[0]{"$k"}) {
        $s->[0]{"$k"}[1];
      }
      else {
        undef;
      }
  }
  else {
      $s->[1]{$k};
  }
}

sub STORE {
  my($s, $k, $v) = @_;
  if (ref $k) {
    $s->[0]{"$k"} = [$k, $v];
  }
  else {
    $s->[1]{$k} = $v;
  }
  $v;
}

sub DELETE {
  my($s, $k) = @_;
  (ref $k) ? delete($s->[0]{"$k"}) : delete($s->[1]{$k});
}

sub EXISTS {
  my($s, $k) = @_;
  (ref $k) ? exists($s->[0]{"$k"}) : exists($s->[1]{$k});
}

sub FIRSTKEY {
  my $s = shift;
  keys %{$s->[0]};	# reset iterator
  keys %{$s->[1]};	# reset iterator
  $s->[2] = 0;
  $s->NEXTKEY;
}

sub NEXTKEY {
  my $s = shift;
  my ($k, $v);
  if (!$s->[2]) {
    if (($k, $v) = each %{$s->[0]}) {
      return $s->[0]{"$k"}[0];
    }
    else {
      $s->[2] = 1;
    }
  }
  return each %{$s->[1]};
}

sub CLEAR {
  my $s = shift;
  $s->[2] = 0;
  %{$s->[0]} = ();
  %{$s->[1]} = ();
}

package Tie::RefHash::Nestable;
our @ISA = qw(Tie::RefHash);

sub STORE {
  my($s, $k, $v) = @_;
  if (ref($v) eq 'HASH' and not tied %$v) {
      my @elems = %$v;
      tie %$v, ref($s), @elems;
  }
  $s->SUPER::STORE($k, $v);
}

1;
