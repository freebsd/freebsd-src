#!./perl

print "1..3\n";
sub context {
  my ( $cona, $testnum ) = @_;
  my $conb = (defined wantarray) ? ( wantarray ? 'A' : 'S' ) : 'V';
  unless ( $cona eq $conb ) {
	print "# Context $conb should be $cona\nnot ";
  }
  print "ok $testnum\n";
}

context('V',1);
$a = context('S',2);
@a = context('A',3);
1;
