#!./perl

print "1..7\n";
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
scalar context('S',4);
$a = scalar context('S',5);
($a) = context('A',6);
($a) = scalar context('S',7);
1;
