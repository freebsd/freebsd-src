package Tie::RefHash;

=head1 NAME

Tie::RefHash - use references as hash keys

=head1 SYNOPSIS

    require 5.004;
    use Tie::RefHash;
    tie HASHVARIABLE, 'Tie::RefHash', LIST;

    untie HASHVARIABLE;

=head1 DESCRIPTION

This module provides the ability to use references as hash keys if
you first C<tie> the hash variable to this module.

It is implemented using the standard perl TIEHASH interface.  Please
see the C<tie> entry in perlfunc(1) and perltie(1) for more information.

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


=head1 AUTHOR

Gurusamy Sarathy        gsar@umich.edu

=head1 VERSION

Version 1.2    15 Dec 1996

=head1 SEE ALSO

perl(1), perlfunc(1), perltie(1)

=cut

require 5.003_11;
use Tie::Hash;
@ISA = qw(Tie::Hash);
use strict;

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
  (ref $k) ? $s->[0]{"$k"}[1] : $s->[1]{$k};
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
  my $a = scalar(keys %{$s->[0]}) + scalar(keys %{$s->[1]});
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

1;
