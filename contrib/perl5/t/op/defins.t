#!./perl -w

#
# test auto defined() test insertion
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $SIG{__WARN__} = sub { $warns++; warn $_[0] };
    print "1..14\n";
}

$wanted_filename = $^O eq 'VMS' ? '0.' : '0';
    
print "not " if $warns;
print "ok 1\n";

open(FILE,">./0");
print FILE "1\n";
print FILE "0";
close(FILE);

open(FILE,"<./0");
my $seen = 0;
my $dummy;
while (my $name = <FILE>)
 {
  $seen++ if $name eq '0';
 }            
print "not " unless $seen;
print "ok 2\n";

seek(FILE,0,0);
$seen = 0;
my $line = '';
do 
 {
  $seen++ if $line eq '0';
 } while ($line = <FILE>);

print "not " unless $seen;
print "ok 3\n";


seek(FILE,0,0);
$seen = 0;    
while (($seen ? $dummy : $name) = <FILE>)
 {
  $seen++ if $name eq '0';
 }
print "not " unless $seen;
print "ok 4\n";

seek(FILE,0,0);
$seen = 0;    
my %where;    
while ($where{$seen} = <FILE>)
 {
  $seen++ if $where{$seen} eq '0';
 }
print "not " unless $seen;
print "ok 5\n";
close FILE;

opendir(DIR,'.');
$seen = 0;
while (my $name = readdir(DIR))
 {
  $seen++ if $name eq $wanted_filename;
 }            
print "not " unless $seen;
print "ok 6\n";

rewinddir(DIR);
$seen = 0;    
$dummy = '';
while (($seen ? $dummy : $name) = readdir(DIR))
 {
  $seen++ if $name eq $wanted_filename;
 }
print "not " unless $seen;
print "ok 7\n";

rewinddir(DIR);
$seen = 0;    
while ($where{$seen} = readdir(DIR))
 {
  $seen++ if $where{$seen} eq $wanted_filename;
 }
print "not " unless $seen;
print "ok 8\n";

$seen = 0;
while (my $name = glob('*'))
 {
  $seen++ if $name eq $wanted_filename;
 }            
print "not " unless $seen;
print "ok 9\n";

$seen = 0;    
$dummy = '';
while (($seen ? $dummy : $name) = glob('*'))
 {
  $seen++ if $name eq $wanted_filename;
 }
print "not " unless $seen;
print "ok 10\n";

$seen = 0;    
while ($where{$seen} = glob('*'))
 {
  $seen++ if $where{$seen} eq $wanted_filename;
 }
print "not " unless $seen;
print "ok 11\n";

unlink("./0");

my %hash = (0 => 1, 1 => 2);

$seen = 0;
while (my $name = each %hash)
 {
  $seen++ if $name eq '0';
 }            
print "not " unless $seen;
print "ok 12\n";

$seen = 0;    
$dummy = '';
while (($seen ? $dummy : $name) = each %hash)
 {
  $seen++ if $name eq '0';
 }
print "not " unless $seen;
print "ok 13\n";

$seen = 0;    
while ($where{$seen} = each %hash)
 {
  $seen++ if $where{$seen} eq '0';
 }
print "not " unless $seen;
print "ok 14\n";

