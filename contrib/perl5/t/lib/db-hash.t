#!./perl -w

BEGIN {
    @INC = '../lib' if -d '../lib' ;
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bDB_File\b/) {
	print "1..0\n";
	exit 0;
    }
}

use DB_File; 
use Fcntl;

print "1..62\n";

sub ok
{
    my $no = shift ;
    my $result = shift ;
 
    print "not " unless $result ;
    print "ok $no\n" ;
}

$Dfile = "dbhash.tmp";
unlink $Dfile;

umask(0);

# Check the interface to HASHINFO

my $dbh = new DB_File::HASHINFO ;

ok(1, ! defined $dbh->{bsize}) ;
ok(2, ! defined $dbh->{ffactor}) ;
ok(3, ! defined $dbh->{nelem}) ;
ok(4, ! defined $dbh->{cachesize}) ;
ok(5, ! defined $dbh->{hash}) ;
ok(6, ! defined $dbh->{lorder}) ;

$dbh->{bsize} = 3000 ;
ok(7, $dbh->{bsize} == 3000 );

$dbh->{ffactor} = 9000 ;
ok(8, $dbh->{ffactor} == 9000 );

$dbh->{nelem} = 400 ;
ok(9, $dbh->{nelem} == 400 );

$dbh->{cachesize} = 65 ;
ok(10, $dbh->{cachesize} == 65 );

$dbh->{hash} = "abc" ;
ok(11, $dbh->{hash} eq "abc" );

$dbh->{lorder} = 1234 ;
ok(12, $dbh->{lorder} == 1234 );

# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
ok(13, $@ =~ /^DB_File::HASHINFO::STORE - Unknown element 'fred' at/ );
eval 'my $q = $dbh->{fred}' ;
ok(14, $@ =~ /^DB_File::HASHINFO::FETCH - Unknown element 'fred' at/ );


# Now check the interface to HASH

ok(15, $X = tie(%h, 'DB_File',$Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
ok(16, ($mode & 0777) == ($^O eq 'os2' ? 0666 : 0640) || $^O eq 'amigaos' || $^O eq 'MSWin32');

while (($key,$value) = each(%h)) {
    $i++;
}
ok(17, !$i );

$h{'goner1'} = 'snork';

$h{'abc'} = 'ABC';
ok(18, $h{'abc'} eq 'ABC' );
ok(19, !defined $h{'jimmy'} );
ok(20, !exists $h{'jimmy'} );
ok(21, exists $h{'abc'} );

$h{'def'} = 'DEF';
$h{'jkl','mno'} = "JKL\034MNO";
$h{'a',2,3,4,5} = join("\034",'A',2,3,4,5);
$h{'a'} = 'A';

#$h{'b'} = 'B';
$X->STORE('b', 'B') ;

$h{'c'} = 'C';

#$h{'d'} = 'D';
$X->put('d', 'D') ;

$h{'e'} = 'E';
$h{'f'} = 'F';
$h{'g'} = 'X';
$h{'h'} = 'H';
$h{'i'} = 'I';

$h{'goner2'} = 'snork';
delete $h{'goner2'};


# IMPORTANT - $X must be undefined before the untie otherwise the
#             underlying DB close routine will not get called.
undef $X ;
untie(%h);


# tie to the same file again, do not supply a type - should default to HASH
ok(22, $X = tie(%h,'DB_File',$Dfile, O_RDWR, 0640) );

# Modify an entry from the previous tie
$h{'g'} = 'G';

$h{'j'} = 'J';
$h{'k'} = 'K';
$h{'l'} = 'L';
$h{'m'} = 'M';
$h{'n'} = 'N';
$h{'o'} = 'O';
$h{'p'} = 'P';
$h{'q'} = 'Q';
$h{'r'} = 'R';
$h{'s'} = 'S';
$h{'t'} = 'T';
$h{'u'} = 'U';
$h{'v'} = 'V';
$h{'w'} = 'W';
$h{'x'} = 'X';
$h{'y'} = 'Y';
$h{'z'} = 'Z';

$h{'goner3'} = 'snork';

delete $h{'goner1'};
$X->DELETE('goner3');

@keys = keys(%h);
@values = values(%h);

ok(23, $#keys == 29 && $#values == 29) ;

$i = 0 ;
while (($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

ok(24, $i == 30) ;

@keys = ('blurfl', keys(%h), 'dyick');
ok(25, $#keys == 31) ;

$h{'foo'} = '';
ok(26, $h{'foo'} eq '' );

#$h{''} = 'bar';
#ok(27, $h{''} eq 'bar' );
ok(27,1) ;

# check cache overflow and numeric keys and contents
$ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
ok(28, $ok );

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
ok(29, $size > 0 );

@h{0..200} = 200..400;
@foo = @h{0..200};
ok(30, join(':',200..400) eq join(':',@foo) );


# Now check all the non-tie specific stuff

# Check NOOVERWRITE will make put fail when attempting to overwrite
# an existing record.
 
$status = $X->put( 'x', 'newvalue', R_NOOVERWRITE) ;
ok(31, $status == 1 );
 
# check that the value of the key 'x' has not been changed by the 
# previous test
ok(32, $h{'x'} eq 'X' );

# standard put
$status = $X->put('key', 'value') ;
ok(33, $status == 0 );

#check that previous put can be retrieved
$value = 0 ;
$status = $X->get('key', $value) ;
ok(34, $status == 0 );
ok(35, $value eq 'value' );

# Attempting to delete an existing key should work

$status = $X->del('q') ;
ok(36, $status == 0 );

# Make sure that the key deleted, cannot be retrieved
$^W = 0 ;
ok(37, $h{'q'} eq undef );
$^W = 1 ;

# Attempting to delete a non-existant key should fail

$status = $X->del('joe') ;
ok(38, $status == 1 );

# Check the get interface

# First a non-existing key
$status = $X->get('aaaa', $value) ;
ok(39, $status == 1 );

# Next an existing key
$status = $X->get('a', $value) ;
ok(40, $status == 0 );
ok(41, $value eq 'A' );

# seq
# ###

# ditto, but use put to replace the key/value pair.

# use seq to walk backwards through a file - check that this reversed is

# check seq FIRST/LAST

# sync
# ####

$status = $X->sync ;
ok(42, $status == 0 );


# fd
# ##

$status = $X->fd ;
ok(43, $status != 0 );

undef $X ;
untie %h ;

unlink $Dfile;

# clear
# #####

ok(44, tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );
foreach (1 .. 10)
  { $h{$_} = $_ * 100 }

# check that there are 10 elements in the hash
$i = 0 ;
while (($key,$value) = each(%h)) {
    $i++;
}
ok(45, $i == 10);

# now clear the hash
%h = () ;

# check it is empty
$i = 0 ;
while (($key,$value) = each(%h)) {
    $i++;
}
ok(46, $i == 0);

untie %h ;
unlink $Dfile ;


# Now try an in memory file
ok(47, $X = tie(%h, 'DB_File',undef, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

# fd with an in memory file should return fail
$status = $X->fd ;
ok(48, $status == -1 );

undef $X ;
untie %h ;

{
    # check ability to override the default hashing
    my %x ;
    my $filename = "xyz" ;
    my $hi = new DB_File::HASHINFO ;
    $::count = 0 ;
    $hi->{hash} = sub { ++$::count ; length $_[0] } ;
    ok(49, tie %x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $hi ) ;
    $h{"abc"} = 123 ;
    ok(50, $h{"abc"} == 123) ;
    untie %x ;
    unlink $filename ;
    ok(51, $::count >0) ;
}

{
    # check that attempting to tie an array to a DB_HASH will fail

    my $filename = "xyz" ;
    my @x ;
    eval { tie @x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $DB_HASH ; } ;
    ok(52, $@ =~ /^DB_File can only tie an associative array to a DB_HASH database/) ;
    unlink $filename ;
}

{
   # sub-class test

   package Another ;

   use strict ;

   open(FILE, ">SubDB.pm") or die "Cannot open SubDB.pm: $!\n" ;
   print FILE <<'EOM' ;

   package SubDB ;

   use strict ;
   use vars qw( @ISA @EXPORT) ;

   require Exporter ;
   use DB_File;
   @ISA=qw(DB_File);
   @EXPORT = @DB_File::EXPORT ;

   sub STORE { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::STORE($key, $value * 2) ;
   }

   sub FETCH { 
	my $self = shift ;
        my $key = shift ;
        $self->SUPER::FETCH($key) - 1 ;
   }

   sub put { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::put($key, $value * 3) ;
   }

   sub get { 
	my $self = shift ;
        $self->SUPER::get($_[0], $_[1]) ;
	$_[1] -= 2 ;
   }

   sub A_new_method
   {
	my $self = shift ;
        my $key = shift ;
        my $value = $self->FETCH($key) ;
	return "[[$value]]" ;
   }

   1 ;
EOM

    close FILE ;

    BEGIN { push @INC, '.'; }             
    eval 'use SubDB ; ';
    main::ok(53, $@ eq "") ;
    my %h ;
    my $X ;
    eval '
	$X = tie(%h, "SubDB","dbhash.tmp", O_RDWR|O_CREAT, 0640, $DB_HASH );
	' ;

    main::ok(54, $@ eq "") ;

    my $ret = eval '$h{"fred"} = 3 ; return $h{"fred"} ' ;
    main::ok(55, $@ eq "") ;
    main::ok(56, $ret == 5) ;

    my $value = 0;
    $ret = eval '$X->put("joe", 4) ; $X->get("joe", $value) ; return $value' ;
    main::ok(57, $@ eq "") ;
    main::ok(58, $ret == 10) ;

    $ret = eval ' R_NEXT eq main::R_NEXT ' ;
    main::ok(59, $@ eq "" ) ;
    main::ok(60, $ret == 1) ;

    $ret = eval '$X->A_new_method("joe") ' ;
    main::ok(61, $@ eq "") ;
    main::ok(62, $ret eq "[[11]]") ;

    undef $X;
    untie(%h);
    unlink "SubDB.pm", "dbhash.tmp" ;

}
exit ;
