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
use strict ;
use vars qw($dbh $Dfile $bad_ones $FA) ;

# full tied array support started in Perl 5.004_57
# Double check to see if it is available.

{
    sub try::TIEARRAY { bless [], "try" }
    sub try::FETCHSIZE { $FA = 1 }
    $FA = 0 ;
    my @a ; 
    tie @a, 'try' ;
    my $a = @a ;
}


sub ok
{
    my $no = shift ;
    my $result = shift ;

    print "not " unless $result ;
    print "ok $no\n" ;

    return $result ;
}

sub bad_one
{
    print STDERR <<EOM unless $bad_ones++ ;
#
# Some older versions of Berkeley DB version 1 will fail tests 51,
# 53 and 55.
#
# You can safely ignore the errors if you're never going to use the
# broken functionality (recno databases with a modified bval).
# Otherwise you'll have to upgrade your DB library.
#
# If you want to use Berkeley DB version 1, then 1.85 and 1.86 are the
# last versions that were released. Berkeley DB version 2 is continually
# being updated -- Check out http://www.sleepycat.com/ for more details.
#
EOM
}

print "1..78\n";

my $Dfile = "recno.tmp";
unlink $Dfile ;

umask(0);

# Check the interface to RECNOINFO

my $dbh = new DB_File::RECNOINFO ;
ok(1, ! defined $dbh->{bval}) ;
ok(2, ! defined $dbh->{cachesize}) ;
ok(3, ! defined $dbh->{psize}) ;
ok(4, ! defined $dbh->{flags}) ;
ok(5, ! defined $dbh->{lorder}) ;
ok(6, ! defined $dbh->{reclen}) ;
ok(7, ! defined $dbh->{bfname}) ;

$dbh->{bval} = 3000 ;
ok(8, $dbh->{bval} == 3000 );

$dbh->{cachesize} = 9000 ;
ok(9, $dbh->{cachesize} == 9000 );

$dbh->{psize} = 400 ;
ok(10, $dbh->{psize} == 400 );

$dbh->{flags} = 65 ;
ok(11, $dbh->{flags} == 65 );

$dbh->{lorder} = 123 ;
ok(12, $dbh->{lorder} == 123 );

$dbh->{reclen} = 1234 ;
ok(13, $dbh->{reclen} == 1234 );

$dbh->{bfname} = 1234 ;
ok(14, $dbh->{bfname} == 1234 );


# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
ok(15, $@ =~ /^DB_File::RECNOINFO::STORE - Unknown element 'fred' at/ );
eval 'my $q = $dbh->{fred}' ;
ok(16, $@ =~ /^DB_File::RECNOINFO::FETCH - Unknown element 'fred' at/ );

# Now check the interface to RECNOINFO

my $X  ;
my @h ;
ok(17, $X = tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) ;

ok(18, ((stat($Dfile))[2] & 0777) == ($^O eq 'os2' ? 0666 : 0640)
	||  $^O eq 'MSWin32' || $^O eq 'amigaos') ;

#my $l = @h ;
my $l = $X->length ;
ok(19, ($FA ? @h == 0 : !$l) );

my @data = qw( a b c d ever f g h  i j k longername m n o p) ;

$h[0] = shift @data ;
ok(20, $h[0] eq 'a' );

my $ i;
foreach (@data)
  { $h[++$i] = $_ }

unshift (@data, 'a') ;

ok(21, defined $h[1] );
ok(22, ! defined $h[16] );
ok(23, $FA ? @h == @data : $X->length == @data );


# Overwrite an entry & check fetch it
$h[3] = 'replaced' ;
$data[3] = 'replaced' ;
ok(24, $h[3] eq 'replaced' );

#PUSH
my @push_data = qw(added to the end) ;
($FA ? push(@h, @push_data) : $X->push(@push_data)) ;
push (@data, @push_data) ;
ok(25, $h[++$i] eq 'added' );
ok(26, $h[++$i] eq 'to' );
ok(27, $h[++$i] eq 'the' );
ok(28, $h[++$i] eq 'end' );

# POP
my $popped = pop (@data) ;
my $value = ($FA ? pop @h : $X->pop) ;
ok(29, $value eq $popped) ;

# SHIFT
$value = ($FA ? shift @h : $X->shift) ;
my $shifted = shift @data ;
ok(30, $value eq $shifted );

# UNSHIFT

# empty list
($FA ? unshift @h : $X->unshift) ;
ok(31, ($FA ? @h == @data : $X->length == @data ));

my @new_data = qw(add this to the start of the array) ;
$FA ? unshift (@h, @new_data) : $X->unshift (@new_data) ;
unshift (@data, @new_data) ;
ok(32, $FA ? @h == @data : $X->length == @data );
ok(33, $h[0] eq "add") ;
ok(34, $h[1] eq "this") ;
ok(35, $h[2] eq "to") ;
ok(36, $h[3] eq "the") ;
ok(37, $h[4] eq "start") ;
ok(38, $h[5] eq "of") ;
ok(39, $h[6] eq "the") ;
ok(40, $h[7] eq "array") ;
ok(41, $h[8] eq $data[8]) ;

# SPLICE

# Now both arrays should be identical

my $ok = 1 ;
my $j = 0 ;
foreach (@data)
{
   $ok = 0, last if $_ ne $h[$j ++] ; 
}
ok(42, $ok );

# Neagtive subscripts

# get the last element of the array
ok(43, $h[-1] eq $data[-1] );
ok(44, $h[-1] eq $h[ ($FA ? @h : $X->length) -1] );

# get the first element using a negative subscript
eval '$h[ - ( $FA ? @h : $X->length)] = "abcd"' ;
ok(45, $@ eq "" );
ok(46, $h[0] eq "abcd" );

# now try to read before the start of the array
eval '$h[ - (1 + ($FA ? @h : $X->length))] = 1234' ;
ok(47, $@ =~ '^Modification of non-creatable array value attempted' );

# IMPORTANT - $X must be undefined before the untie otherwise the
#             underlying DB close routine will not get called.
undef $X ;
untie(@h);

unlink $Dfile;

sub docat
{
    my $file = shift;
    local $/ = undef;
    open(CAT,$file) || die "Cannot open $file:$!";
    my $result = <CAT>;
    close(CAT);
    return $result;
}


{
    # Check bval defaults to \n

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    ok(48, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    ok(49, $x eq "abc\ndef\n\nghi\n") ;
}

{
    # Change bval

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    $dbh->{bval} = "-" ;
    ok(50, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    my $ok = ($x eq "abc-def--ghi-") ;
    bad_one() unless $ok ;
    ok(51, $ok) ;
}

{
    # Check R_FIXEDLEN with default bval (space)

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    $dbh->{flags} = R_FIXEDLEN ;
    $dbh->{reclen} = 5 ;
    ok(52, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    my $ok = ($x eq "abc  def       ghi  ") ;
    bad_one() unless $ok ;
    ok(53, $ok) ;
}

{
    # Check R_FIXEDLEN with user-defined bval

    my @h = () ;
    my $dbh = new DB_File::RECNOINFO ;
    $dbh->{flags} = R_FIXEDLEN ;
    $dbh->{bval} = "-" ;
    $dbh->{reclen} = 5 ;
    ok(54, tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $dbh ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[3] = "ghi" ;
    untie @h ;
    my $x = docat($Dfile) ;
    unlink $Dfile;
    my $ok = ($x eq "abc--def-------ghi--") ;
    bad_one() unless $ok ;
    ok(55, $ok) ;
}

{
    # check that attempting to tie an associative array to a DB_RECNO will fail

    my $filename = "xyz" ;
    my %x ;
    eval { tie %x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $DB_RECNO ; } ;
    ok(56, $@ =~ /^DB_File can only tie an array to a DB_RECNO database/) ;
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
    main::ok(57, $@ eq "") ;
    my @h ;
    my $X ;
    eval '
	$X = tie(@h, "SubDB","recno.tmp", O_RDWR|O_CREAT, 0640, $DB_RECNO );
	' ;

    main::ok(58, $@ eq "") ;

    my $ret = eval '$h[3] = 3 ; return $h[3] ' ;
    main::ok(59, $@ eq "") ;
    main::ok(60, $ret == 5) ;

    my $value = 0;
    $ret = eval '$X->put(1, 4) ; $X->get(1, $value) ; return $value' ;
    main::ok(61, $@ eq "") ;
    main::ok(62, $ret == 10) ;

    $ret = eval ' R_NEXT eq main::R_NEXT ' ;
    main::ok(63, $@ eq "" ) ;
    main::ok(64, $ret == 1) ;

    $ret = eval '$X->A_new_method(1) ' ;
    main::ok(65, $@ eq "") ;
    main::ok(66, $ret eq "[[11]]") ;

    undef $X;
    untie(@h);
    unlink "SubDB.pm", "recno.tmp" ;

}

{

    # test $#
    my $self ;
    unlink $Dfile;
    ok(67, $self = tie @h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO ) ;
    $h[0] = "abc" ;
    $h[1] = "def" ;
    $h[2] = "ghi" ;
    $h[3] = "jkl" ;
    ok(68, $FA ? $#h == 3 : $self->length() == 4) ;
    undef $self ;
    untie @h ;
    my $x = docat($Dfile) ;
    ok(69, $x eq "abc\ndef\nghi\njkl\n") ;

    # $# sets array to same length
    ok(70, $self = tie @h, 'DB_File', $Dfile, O_RDWR, 0640, $DB_RECNO ) ;
    if ($FA)
      { $#h = 3 }
    else 
      { $self->STORESIZE(4) }
    ok(71, $FA ? $#h == 3 : $self->length() == 4) ;
    undef $self ;
    untie @h ;
    $x = docat($Dfile) ;
    ok(72, $x eq "abc\ndef\nghi\njkl\n") ;

    # $# sets array to bigger
    ok(73, $self = tie @h, 'DB_File', $Dfile, O_RDWR, 0640, $DB_RECNO ) ;
    if ($FA)
      { $#h = 6 }
    else 
      { $self->STORESIZE(7) }
    ok(74, $FA ? $#h == 6 : $self->length() == 7) ;
    undef $self ;
    untie @h ;
    $x = docat($Dfile) ;
    ok(75, $x eq "abc\ndef\nghi\njkl\n\n\n\n") ;

    # $# sets array smaller
    ok(76, $self = tie @h, 'DB_File', $Dfile, O_RDWR, 0640, $DB_RECNO ) ;
    if ($FA)
      { $#h = 2 }
    else 
      { $self->STORESIZE(3) }
    ok(77, $FA ? $#h == 2 : $self->length() == 3) ;
    undef $self ;
    untie @h ;
    $x = docat($Dfile) ;
    ok(78, $x eq "abc\ndef\nghi\n") ;

    unlink $Dfile;


}

exit ;
