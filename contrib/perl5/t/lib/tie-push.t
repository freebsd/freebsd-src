#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '.'; 
    push @INC, '../lib';
}    

{
 package Basic;
 use Tie::Array;
 @ISA = qw(Tie::Array);

 sub TIEARRAY  { return bless [], shift }
 sub FETCH     { $_[0]->[$_[1]] }
 sub STORE     { $_[0]->[$_[1]] = $_[2] }
 sub FETCHSIZE { scalar(@{$_[0]}) }
 sub STORESIZE { $#{$_[0]} = $_[1]-1 }
}

tie @x,Basic;
tie @get,Basic;
tie @got,Basic;
tie @tests,Basic;
require "op/push.t"
