#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

=head1 NAME

pl2pm - Rough tool to translate Perl4 .pl files to Perl5 .pm modules.

=head1 SYNOPSIS

B<pl2pm> F<files>

=head1 DESCRIPTION

B<pl2pm> is a tool to aid in the conversion of Perl4-style .pl
library files to Perl5-style library modules.  Usually, your old .pl
file will still work fine and you should only use this tool if you
plan to update your library to use some of the newer Perl 5 features,
such as AutoLoading.

=head1 LIMITATIONS

It's just a first step, but it's usually a good first step.

=head1 AUTHOR

Larry Wall <larry@wall.org>

=cut

while (<DATA>) {
    chop;
    $keyword{$_} = 1;
}

undef $/;
$* = 1;
while (<>) {
    $newname = $ARGV;
    $newname =~ s/\.pl$/.pm/ || next;
    $newname =~ s#(.*/)?(\w+)#$1\u$2#;
    if (-f $newname) {
	warn "Won't overwrite existing $newname\n";
	next;
    }
    $oldpack = $2;
    $newpack = "\u$2";
    @export = ();
    print "$oldpack => $newpack\n" if $verbose;

    s/\bstd(in|out|err)\b/\U$&/g;
    s/(sub\s+)(\w+)(\s*\{[ \t]*\n)\s*package\s+$oldpack\s*;[ \t]*\n+/${1}main'$2$3/ig;
    if (/sub\s+main'/) {
	@export = m/sub\s+main'(\w+)/g;
	s/(sub\s+)main'(\w+)/$1$2/g;
    }
    else {
	@export = m/sub\s+([A-Za-z]\w*)/g;
    }
    @export_ok = grep($keyword{$_}, @export);
    @export = grep(!$keyword{$_}, @export);
    @export{@export} = (1) x @export;
    s/(^\s*);#/$1#/g;
    s/(#.*)require ['"]$oldpack\.pl['"]/$1use $newpack/;
    s/(package\s*)($oldpack)\s*;[ \t]*\n+//ig;
    s/([\$\@%&*])'(\w+)/&xlate($1,"",$2)/eg;
    s/([\$\@%&*]?)(\w+)'(\w+)/&xlate($1,$2,$3)/eg;
    if (!/\$\[\s*\)?\s*=\s*[^0\s]/) {
	s/^\s*(local\s*\()?\s*\$\[\s*\)?\s*=\s*0\s*;[ \t]*\n//g;
	s/\$\[\s*\+\s*//g;
	s/\s*\+\s*\$\[//g;
	s/\$\[/0/g;
    }
    s/open\s+(\w+)/open($1)/g;
 
    if (s/\bdie\b/croak/g) {
	$carp = "use Carp;\n";
	s/croak "([^"]*)\\n"/croak "$1"/g;
    }
    else {
	$carp = "";
    }
    if (@export_ok) {
	$export_ok = "\@EXPORT_OK = qw(@export_ok);\n";
    }
    else {
	$export_ok = "";
    }

    open(PM, ">$newname") || warn "Can't create $newname: $!\n";
    print PM <<"END";
package $newpack;
require 5.000;
require Exporter;
$carp
\@ISA = qw(Exporter);
\@EXPORT = qw(@export);
$export_ok
$_
END
}

sub xlate {
    local($prefix, $pack, $ident) = @_;
    if ($prefix eq '' && $ident =~ /^(t|s|m|d|ing|ll|ed|ve|re)$/) {
	"${pack}'$ident";
    }
    elsif ($pack eq "" || $pack eq "main") {
	if ($export{$ident}) {
	    "$prefix$ident";
	}
	else {
	    "$prefix${pack}::$ident";
	}
    }
    elsif ($pack eq $oldpack) {
	"$prefix${newpack}::$ident";
    }
    else {
	"$prefix${pack}::$ident";
    }
}
__END__
AUTOLOAD
BEGIN
CORE
DESTROY
END
abs
accept
alarm
and
atan2
bind
binmode
bless
caller
chdir
chmod
chop
chown
chr
chroot
close
closedir
cmp
connect
continue
cos
crypt
dbmclose
dbmopen
defined
delete
die
do
dump
each
else
elsif
endgrent
endhostent
endnetent
endprotoent
endpwent
endservent
eof
eq
eval
exec
exit
exp
fcntl
fileno
flock
for
foreach
fork
format
formline
ge
getc
getgrent
getgrgid
getgrnam
gethostbyaddr
gethostbyname
gethostent
getlogin
getnetbyaddr
getnetbyname
getnetent
getpeername
getpgrp
getppid
getpriority
getprotobyname
getprotobynumber
getprotoent
getpwent
getpwnam
getpwuid
getservbyname
getservbyport
getservent
getsockname
getsockopt
glob
gmtime
goto
grep
gt
hex
if
index
int
ioctl
join
keys
kill
last
lc
lcfirst
le
length
link
listen
local
localtime
log
lstat
lt
m
mkdir
msgctl
msgget
msgrcv
msgsnd
my
ne
next
no
not
oct
open
opendir
or
ord
pack
package
pipe
pop
print
printf
push
q
qq
quotemeta
qw
qx
rand
read
readdir
readline
readlink
readpipe
recv
redo
ref
rename
require
reset
return
reverse
rewinddir
rindex
rmdir
s
scalar
seek
seekdir
select
semctl
semget
semop
send
setgrent
sethostent
setnetent
setpgrp
setpriority
setprotoent
setpwent
setservent
setsockopt
shift
shmctl
shmget
shmread
shmwrite
shutdown
sin
sleep
socket
socketpair
sort
splice
split
sprintf
sqrt
srand
stat
study
sub
substr
symlink
syscall
sysread
system
syswrite
tell
telldir
tie
time
times
tr
truncate
uc
ucfirst
umask
undef
unless
unlink
unpack
unshift
untie
until
use
utime
values
vec
wait
waitpid
wantarray
warn
while
write
x
xor
y
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
