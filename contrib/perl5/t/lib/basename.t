#!./perl -T

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use File::Basename qw(fileparse basename dirname);

print "1..36\n";

# import correctly?
print +(defined(&basename) && !defined(&fileparse_set_fstype) ?
        '' : 'not '),"ok 1\n";

# set fstype -- should replace non-null default
print +(length(File::Basename::fileparse_set_fstype('unix')) ?
        '' : 'not '),"ok 2\n";

# Unix syntax tests
($base,$path,$type) = fileparse('/virgil/aeneid/draft.book7','\.book\d+');
if ($base eq 'draft' and $path eq '/virgil/aeneid/' and $type eq '.book7') {
  print "ok 3\n";
}
else {
  print "not ok 3	|$base|$path|$type|\n";
}
print +(basename('/arma/virumque.cano') eq 'virumque.cano' ?
        '' : 'not '),"ok 4\n";
print +(dirname('/arma/virumque.cano') eq '/arma' ? '' : 'not '),"ok 5\n";
print +(dirname('arma/') eq '.' ? '' : 'not '),"ok 6\n";
print +(dirname('/') eq '/' ? '' : 'not '),"ok 7\n";


# set fstype -- should replace non-null default
print +(File::Basename::fileparse_set_fstype('VMS') eq 'unix' ?
        '' : 'not '),"ok 8\n";

# VMS syntax tests
($base,$path,$type) = fileparse('virgil:[aeneid]draft.book7','\.book\d+');
if ($base eq 'draft' and $path eq 'virgil:[aeneid]' and $type eq '.book7') {
  print "ok 9\n";
}
else {
  print "not ok 9	|$base|$path|$type|\n";
}
print +(basename('arma:[virumque]cano.trojae') eq 'cano.trojae' ?
        '' : 'not '),"ok 10\n";
print +(dirname('arma:[virumque]cano.trojae') eq 'arma:[virumque]' ?
        '' : 'not '),"ok 11\n";
print +(dirname('arma:<virumque>cano.trojae') eq 'arma:<virumque>' ?
        '' : 'not '),"ok 12\n";
print +(dirname('arma:virumque.cano') eq 'arma:' ? '' : 'not '),"ok 13\n";
$ENV{DEFAULT} = '' unless exists $ENV{DEFAULT};
print +(dirname('virumque.cano') eq $ENV{DEFAULT} ? '' : 'not '),"ok 14\n";
print +(dirname('arma/') eq '.' ? '' : 'not '),"ok 15\n";

# set fstype -- should replace non-null default
print +(File::Basename::fileparse_set_fstype('MSDOS') eq 'VMS' ?
        '' : 'not '),"ok 16\n";

# MSDOS syntax tests
($base,$path,$type) = fileparse('C:\\virgil\\aeneid\\draft.book7','\.book\d+');
if ($base eq 'draft' and $path eq 'C:\\virgil\\aeneid\\' and $type eq '.book7') {
  print "ok 17\n";
}
else {
  print "not ok 17	|$base|$path|$type|\n";
}
print +(basename('A:virumque\\cano.trojae') eq 'cano.trojae' ?
        '' : 'not '),"ok 18\n";
print +(dirname('A:\\virumque\\cano.trojae') eq 'A:\\virumque' ?
        '' : 'not '),"ok 19\n";
print +(dirname('A:\\') eq 'A:\\' ? '' : 'not '),"ok 20\n";
print +(dirname('arma\\') eq '.' ? '' : 'not '),"ok 21\n";

# Yes "/" is a legal path separator under MSDOS
basename("lib/File/Basename.pm") eq "Basename.pm" or print "not ";
print "ok 22\n";



# set fstype -- should replace non-null default
print +(File::Basename::fileparse_set_fstype('MacOS') eq 'MSDOS' ?
        '' : 'not '),"ok 23\n";

# MacOS syntax tests
($base,$path,$type) = fileparse('virgil:aeneid:draft.book7','\.book\d+');
if ($base eq 'draft' and $path eq 'virgil:aeneid:' and $type eq '.book7') {
  print "ok 24\n";
}
else {
  print "not ok 24	|$base|$path|$type|\n";
}
print +(basename(':arma:virumque:cano.trojae') eq 'cano.trojae' ?
        '' : 'not '),"ok 25\n";
print +(dirname(':arma:virumque:cano.trojae') eq ':arma:virumque:' ?
        '' : 'not '),"ok 26\n";
print +(dirname('arma:') eq 'arma:' ? '' : 'not '),"ok 27\n";
print +(dirname(':') eq ':' ? '' : 'not '),"ok 28\n";


# Check quoting of metacharacters in suffix arg by basename()
print +(basename(':arma:virumque:cano.trojae','.trojae') eq 'cano' ?
        '' : 'not '),"ok 29\n";
print +(basename(':arma:virumque:cano_trojae','.trojae') eq 'cano_trojae' ?
        '' : 'not '),"ok 30\n";

# extra tests for a few specific bugs

File::Basename::fileparse_set_fstype 'MSDOS';
# perl5.003_18 gives C:/perl/.\
print +((fileparse 'C:/perl/lib')[1] eq 'C:/perl/' ? '' : 'not '), "ok 31\n";
# perl5.003_18 gives C:\perl\
print +(dirname('C:\\perl\\lib\\') eq 'C:\\perl' ? '' : 'not '), "ok 32\n";

File::Basename::fileparse_set_fstype 'UNIX';
# perl5.003_18 gives '.'
print +(dirname('/perl/') eq '/' ? '' : 'not '), "ok 33\n";
# perl5.003_18 gives '/perl/lib'
print +(dirname('/perl/lib//') eq '/perl' ? '' : 'not '), "ok 34\n";

#   The empty tainted value, for tainting strings
my $TAINT = substr($^X, 0, 0);
# How to identify taint when you see it
sub any_tainted (@) {
    not eval { join("",@_), kill 0; 1 };
}
sub tainted ($) {
    any_tainted @_;
}
sub all_tainted (@) {
    for (@_) { return 0 unless tainted $_ }
    1;
}

print +(tainted(dirname($TAINT.'/perl/lib//')) ? '' : 'not '), "ok 35\n";
print +(all_tainted(fileparse($TAINT.'/dir/draft.book7','\.book\d+'))
		? '' : 'not '), "ok 36\n";
