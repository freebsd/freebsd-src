#!./perl

# NOTE: Please don't add tests to this file unless they *need* to be run in
# separate executable and can't simply use eval.

BEGIN
 {
  chdir 't' if -d 't';
  @INC = '../lib';
  require Config;
  import Config;
  if ($Config{'use5005threads'})
   {
    print "1..0 # Skip: this perl is threaded\n";
    exit 0;
   }
 }


$|=1;

print "1..9\n";
$t = 1;
sub foo { local(@_) = ('p', 'q', 'r'); }
sub bar { unshift @_, 'D'; @_ }
sub baz { push @_, 'E'; return @_ }
for (1..3) 
 { 
   print "not " unless join('',foo('a', 'b', 'c')) eq 'pqr';
   print "ok ",$t++,"\n";
   print "not" unless join('',bar('d')) eq 'Dd';
   print "ok ",$t++,"\n";
   print "not" unless join('',baz('e')) eq 'eE';
   print "ok ",$t++,"\n";
 } 
