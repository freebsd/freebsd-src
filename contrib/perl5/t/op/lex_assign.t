#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

umask 0;
$xref = \ "";
$runme = ($^O eq 'VMS' ? 'MCR ' : '') . $^X;
@a = (1..5);
%h = (1..6);
$aref = \@a;
$href = \%h;
open OP, qq{$runme -le "print 'aaa Ok ok' for 1..100"|};
$chopit = 'aaaaaa';
@chopar = (113 .. 119);
$posstr = '123456';
$cstr = 'aBcD.eF';
pos $posstr = 3;
$nn = $n = 2;
sub subb {"in s"}

@INPUT = <DATA>;
@simple_input = grep /^\s*\w+\s*\$\w+\s*[#\n]/, @INPUT;
print "1..", (10 + @INPUT + @simple_input), "\n";
$ord = 0;

sub wrn {"@_"}

# Check correct optimization of ucfirst etc
$ord++;
my $a = "AB";
my $b = "\u\L$a";
print "not " unless $b eq 'Ab';
print "ok $ord\n";

# Check correct destruction of objects:
my $dc = 0;
sub A::DESTROY {$dc += 1}
$a=8;
my $b;
{ my $c = 6; $b = bless \$c, "A"}

$ord++;
print "not " unless $dc == 0;
print "ok $ord\n";

$b = $a+5;

$ord++;
print "not " unless $dc == 1;
print "ok $ord\n";

$ord++;
my $xxx = 'b';
$xxx = 'c' . ($xxx || 'e');
print "not " unless $xxx eq 'cb';
print "ok $ord\n";

{				# Check calling STORE
  my $sc = 0;
  sub B::TIESCALAR {bless [11], 'B'}
  sub B::FETCH { -(shift->[0]) }
  sub B::STORE { $sc++; my $o = shift; $o->[0] = 17 + shift }

  my $m;
  tie $m, 'B';
  $m = 100;

  $ord++;
  print "not " unless $sc == 1;
  print "ok $ord\n";

  my $t = 11;
  $m = $t + 89;
  
  $ord++;
  print "not " unless $sc == 2;
  print "ok $ord\n";

  $ord++;
  print "# $m\nnot " unless $m == -117;
  print "ok $ord\n";

  $m += $t;

  $ord++;
  print "not " unless $sc == 3;
  print "ok $ord\n";

  $ord++;
  print "# $m\nnot " unless $m == 89;
  print "ok $ord\n";

}

# Chains of assignments

my ($l1, $l2, $l3, $l4);
my $zzzz = 12;
$zzz1 = $l1 = $l2 = $zzz2 = $l3 = $l4 = 1 + $zzzz;

$ord++;
print "# $zzz1 = $l1 = $l2 = $zzz2 = $l3 = $l4 = 13\nnot "
  unless $zzz1 == 13 and $zzz2 == 13 and $l1 == 13
  and $l2 == 13 and $l3 == 13 and $l4 == 13;
print "ok $ord\n";

for (@INPUT) {
  $ord++;
  ($op, undef, $comment) = /^([^\#]+)(\#\s+(.*))?/;
  $comment = $op unless defined $comment;
  chomp;
  $op = "$op==$op" unless $op =~ /==/;
  ($op, $expectop) = $op =~ /(.*)==(.*)/;
  
  $skip = ($op =~ /^'\?\?\?'/ or $comment =~ /skip\(.*\Q$^O\E.*\)/i)
	  ? "skip" : "# '$_'\nnot";
  $integer = ($comment =~ /^i_/) ? "use integer" : '' ;
  (print "#skipping $comment:\nok $ord\n"), next if $skip eq 'skip';
  
  eval <<EOE;
  local \$SIG{__WARN__} = \\&wrn;
  my \$a = 'fake';
  $integer;
  \$a = $op;
  \$b = $expectop;
  if (\$a ne \$b) {
    print "# \$comment: got `\$a', expected `\$b'\n";
    print "\$skip " if \$a ne \$b or \$skip eq 'skip';
  }
  print "ok \$ord\\n";
EOE
  if ($@) {
    if ($@ =~ /is unimplemented/) {
      print "# skipping $comment: unimplemented:\nok $ord\n";
    } else {
      warn $@;
      print "# '$_'\nnot ok $ord\n";
    }
  }
}

for (@simple_input) {
  $ord++;
  ($op, undef, $comment) = /^([^\#]+)(\#\s+(.*))?/;
  $comment = $op unless defined $comment;
  chomp;
  ($operator, $variable) = /^\s*(\w+)\s*\$(\w+)/ or warn "misprocessed '$_'\n";
  eval <<EOE;
  local \$SIG{__WARN__} = \\&wrn;
  my \$$variable = "Ac# Ca\\nxxx";
  \$$variable = $operator \$$variable;
  \$toself = \$$variable;
  \$direct = $operator "Ac# Ca\\nxxx";
  print "# \\\$$variable = $operator \\\$$variable\\nnot "
    unless \$toself eq \$direct;
  print "ok \$ord\\n";
EOE
  if ($@) {
    if ($@ =~ /is unimplemented/) {
      print "# skipping $comment: unimplemented:\nok $ord\n";
    } elsif ($@ =~ /Can't (modify|take log of 0)/) {
      print "# skipping $comment: syntax not good for selfassign:\nok $ord\n";
    } else {
      warn $@;
      print "# '$_'\nnot ok $ord\n";
    }
  }
}
__END__
ref $xref			# ref
ref $cstr			# ref nonref
`$runme -e "print qq[1\\n]"`				# backtick skip(MSWin32)
`$undefed`			# backtick undef skip(MSWin32)
<*>				# glob
<OP>				# readline
'faked'				# rcatline
(@z = (1 .. 3))			# aassign
chop $chopit			# chop
(chop (@x=@chopar))		# schop
chomp $chopit			# chomp
(chop (@x=@chopar))		# schomp
pos $posstr			# pos
pos $chopit			# pos returns undef
$nn++==2			# postinc
$nn++==3			# i_postinc
$nn--==4			# postdec
$nn--==3			# i_postdec
$n ** $n			# pow
$n * $n				# multiply
$n * $n				# i_multiply
$n / $n				# divide
$n / $n				# i_divide
$n % $n				# modulo
$n % $n				# i_modulo
$n x $n				# repeat
$n + $n				# add
$n + $n				# i_add
$n - $n				# subtract
$n - $n				# i_subtract
$n . $n				# concat
$n . $a=='2fake'		# concat with self
"3$a"=='3fake'			# concat with self in stringify
"$n"				# stringify
$n << $n			# left_shift
$n >> $n			# right_shift
$n <=> $n			# ncmp
$n <=> $n			# i_ncmp
$n cmp $n			# scmp
$n & $n				# bit_and
$n ^ $n				# bit_xor
$n | $n				# bit_or
-$n				# negate
-$n				# i_negate
~$n				# complement
atan2 $n,$n			# atan2
sin $n				# sin
cos $n				# cos
'???'				# rand
exp $n				# exp
log $n				# log
sqrt $n				# sqrt
int $n				# int
hex $n				# hex
oct $n				# oct
abs $n				# abs
length $posstr			# length
substr $posstr, 2, 2		# substr
vec("abc",2,8)			# vec
index $posstr, 2		# index
rindex $posstr, 2		# rindex
sprintf "%i%i", $n, $n		# sprintf
ord $n				# ord
chr $n				# chr
crypt $n, $n			# crypt
ucfirst ($cstr . "a")		# ucfirst padtmp
ucfirst $cstr			# ucfirst
lcfirst $cstr			# lcfirst
uc $cstr			# uc
lc $cstr			# lc
quotemeta $cstr			# quotemeta
@$aref				# rv2av
@$undefed			# rv2av undef
(each %h) % 2 == 1		# each
values %h			# values
keys %h				# keys
%$href				# rv2hv
pack "C2", $n,$n		# pack
split /a/, "abad"		# split
join "a"; @a			# join
push @a,3==6			# push
unshift @aaa			# unshift
reverse	@a			# reverse
reverse	$cstr			# reverse - scal
grep $_, 1,0,2,0,3		# grepwhile
map "x$_", 1,0,2,0,3		# mapwhile
subb()				# entersub
caller				# caller
warn "ignore this\n"		# warn
'faked'				# die
open BLAH, "<non-existent"	# open
fileno STDERR			# fileno
umask 0				# umask
select STDOUT			# sselect
select "","","",0		# select
getc OP				# getc
'???'				# read
'???'				# sysread
'???'				# syswrite
'???'				# send
'???'				# recv
'???'				# tell
'???'				# fcntl
'???'				# ioctl
'???'				# flock
'???'				# accept
'???'				# shutdown
'???'				# ftsize
'???'				# ftmtime
'???'				# ftatime
'???'				# ftctime
chdir 'non-existent'		# chdir
'???'				# chown
'???'				# chroot
unlink 'non-existent'		# unlink
chmod 'non-existent'		# chmod
utime 'non-existent'		# utime
rename 'non-existent', 'non-existent1'	# rename
link 'non-existent', 'non-existent1' # link
'???'				# symlink
readlink 'non-existent', 'non-existent1' # readlink
'???'				# mkdir
'???'				# rmdir
'???'				# telldir
'???'				# fork
'???'				# wait
'???'				# waitpid
system "$runme -e 0"		# system skip(VMS)
'???'				# exec
'???'				# kill
getppid				# getppid
getpgrp				# getpgrp
'???'				# setpgrp
getpriority $$, $$		# getpriority
'???'				# setpriority
time				# time
localtime $^T			# localtime
gmtime $^T			# gmtime
'???'				# sleep: can randomly fail
'???'				# alarm
'???'				# shmget
'???'				# shmctl
'???'				# shmread
'???'				# shmwrite
'???'				# msgget
'???'				# msgctl
'???'				# msgsnd
'???'				# msgrcv
'???'				# semget
'???'				# semctl
'???'				# semop
'???'				# getlogin
'???'				# syscall
