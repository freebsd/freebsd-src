#!./perl


BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

my %seen;

package Implement;

sub TIEARRAY
{
 $seen{'TIEARRAY'}++;
 my ($class,@val) = @_;
 return bless \@val,$class;
}

sub STORESIZE
{        
 $seen{'STORESIZE'}++;
 my ($ob,$sz) = @_; 
 return $#{$ob} = $sz-1;
}

sub EXTEND
{        
 $seen{'EXTEND'}++;
 my ($ob,$sz) = @_; 
 return @$ob = $sz;
}

sub FETCHSIZE
{        
 $seen{'FETCHSIZE'}++;
 return scalar(@{$_[0]});
}

sub FETCH
{
 $seen{'FETCH'}++;
 my ($ob,$id) = @_;
 return $ob->[$id]; 
}

sub STORE
{
 $seen{'STORE'}++;
 my ($ob,$id,$val) = @_;
 $ob->[$id] = $val; 
}                 

sub UNSHIFT
{
 $seen{'UNSHIFT'}++;
 my $ob = shift;
 unshift(@$ob,@_);
}                 

sub PUSH
{
 $seen{'PUSH'}++;
 my $ob = shift;;
 push(@$ob,@_);
}                 

sub CLEAR
{
 $seen{'CLEAR'}++;
 @{$_[0]} = ();
}

sub DESTROY
{
 $seen{'DESTROY'}++;
}

sub POP
{
 $seen{'POP'}++;
 my ($ob) = @_;
 return pop(@$ob);
}

sub SHIFT
{
 $seen{'SHIFT'}++;
 my ($ob) = @_;
 return shift(@$ob);
}

sub SPLICE
{
 $seen{'SPLICE'}++;
 my $ob  = shift;                    
 my $off = @_ ? shift : 0;
 my $len = @_ ? shift : @$ob-1;
 return splice(@$ob,$off,$len,@_);
}

package main;

print "1..31\n";                   
my $test = 1;

{my @ary;

{ my $ob = tie @ary,'Implement',3,2,1;
  print "not " unless $ob;
  print "ok ", $test++,"\n";
  print "not " unless tied(@ary) == $ob;
  print "ok ", $test++,"\n";
}


print "not " unless @ary == 3;
print "ok ", $test++,"\n";

print "not " unless $#ary == 2;
print "ok ", $test++,"\n";

print "not " unless join(':',@ary) eq '3:2:1';
print "ok ", $test++,"\n";         

print "not " unless $seen{'FETCH'} >= 3;
print "ok ", $test++,"\n";

@ary = (1,2,3);

print "not " unless $seen{'STORE'} >= 3;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:2:3';
print "ok ", $test++,"\n";         

{my @thing = @ary;
print "not " unless join(':',@thing) eq '1:2:3';
print "ok ", $test++,"\n";         

tie @thing,'Implement';
@thing = @ary;
print "not " unless join(':',@thing) eq '1:2:3';
print "ok ", $test++,"\n";
} 

print "not " unless pop(@ary) == 3;
print "ok ", $test++,"\n";
print "not " unless $seen{'POP'} == 1;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:2';
print "ok ", $test++,"\n";

push(@ary,4);
print "not " unless $seen{'PUSH'} == 1;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:2:4';
print "ok ", $test++,"\n";

my @x = splice(@ary,1,1,7);


print "not " unless $seen{'SPLICE'} == 1;
print "ok ", $test++,"\n";

print "not " unless @x == 1;
print "ok ", $test++,"\n";
print "not " unless $x[0] == 2;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:7:4';
print "ok ", $test++,"\n";             

print "not " unless shift(@ary) == 1;
print "ok ", $test++,"\n";
print "not " unless $seen{'SHIFT'} == 1;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '7:4';
print "ok ", $test++,"\n";             

my $n = unshift(@ary,5,6);
print "not " unless $seen{'UNSHIFT'} == 1;
print "ok ", $test++,"\n";
print "not " unless $n == 4;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '5:6:7:4';
print "ok ", $test++,"\n";

@ary = split(/:/,'1:2:3');
print "not " unless join(':',@ary) eq '1:2:3';
print "ok ", $test++,"\n";         
  
my $t = 0;
foreach $n (@ary)
 {
  print "not " unless $n == ++$t;
  print "ok ", $test++,"\n";         
 }

@ary = qw(3 2 1);
print "not " unless join(':',@ary) eq '3:2:1';
print "ok ", $test++,"\n";         

untie @ary;   

}
                           
print "not " unless $seen{'DESTROY'} == 2;
print "ok ", $test++,"\n";         



