#!./perl

# $RCSfile: dbm.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:43 $

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bNDBM_File\b/) {
	print "1..0 # Skip: NDBM_File was not built\n";
	exit 0;
    }
}

use strict;
use warnings;

sub ok
{
    my $no = shift ;
    my $result = shift ;

    print "not " unless $result ;
    print "ok $no\n" ;
}

require NDBM_File;
#If Fcntl is not available, try 0x202 or 0x102 for O_RDWR|O_CREAT
use Fcntl;

print "1..65\n";

unlink <Op.dbmx*>;

umask(0);
my %h;
ok(1, tie(%h,'NDBM_File','Op.dbmx', O_RDWR|O_CREAT, 0640));

my $Dfile = "Op.dbmx.pag";
if (! -e $Dfile) {
	($Dfile) = <Op.dbmx*>;
}
if ($^O eq 'amigaos' || $^O eq 'os2' || $^O eq 'MSWin32') {
    print "ok 2 # Skipped: different file permission semantics\n";
}
else {
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat($Dfile);
    print (($mode & 0777) == 0640 ? "ok 2\n" : "not ok 2\n");
}
my $i = 0;
while (my ($key,$value) = each(%h)) {
    $i++;
}
print (!$i ? "ok 3\n" : "not ok 3\n");

$h{'goner1'} = 'snork';

$h{'abc'} = 'ABC';
$h{'def'} = 'DEF';
$h{'jkl','mno'} = "JKL\034MNO";
$h{'a',2,3,4,5} = join("\034",'A',2,3,4,5);
$h{'a'} = 'A';
$h{'b'} = 'B';
$h{'c'} = 'C';
$h{'d'} = 'D';
$h{'e'} = 'E';
$h{'f'} = 'F';
$h{'g'} = 'G';
$h{'h'} = 'H';
$h{'i'} = 'I';

$h{'goner2'} = 'snork';
delete $h{'goner2'};

untie(%h);
print (tie(%h,'NDBM_File','Op.dbmx', &O_RDWR, 0640) ? "ok 4\n" : "not ok 4\n");

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
delete $h{'goner3'};

my @keys = keys(%h);
my @values = values(%h);

if ($#keys == 29 && $#values == 29) {print "ok 5\n";} else {print "not ok 5\n";}

while (my ($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

if ($i == 30) {print "ok 6\n";} else {print "not ok 6\n";}

@keys = ('blurfl', keys(%h), 'dyick');
if ($#keys == 31) {print "ok 7\n";} else {print "not ok 7\n";}

$h{'foo'} = '';
$h{''} = 'bar';

# check cache overflow and numeric keys and contents
my $ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
print ($ok ? "ok 8\n" : "not ok 8\n");

my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print ($size > 0 ? "ok 9\n" : "not ok 9\n");

@h{0..200} = 200..400;
my @foo = @h{0..200};
print join(':',200..400) eq join(':',@foo) ? "ok 10\n" : "not ok 10\n";

print ($h{'foo'} eq '' ? "ok 11\n" : "not ok 11\n");
print ($h{''} eq 'bar' ? "ok 12\n" : "not ok 12\n");

untie %h;
unlink 'Op.dbmx.dir', $Dfile;

{
   # sub-class test

   package Another ;

   use strict ;
   use warnings ;

   open(FILE, ">SubDB.pm") or die "Cannot open SubDB.pm: $!\n" ;
   print FILE <<'EOM' ;

   package SubDB ;

   use strict ;
   use warnings ;
   use vars qw(@ISA @EXPORT) ;

   require Exporter ;
   use NDBM_File;
   @ISA=qw(NDBM_File);
   @EXPORT = @NDBM_File::EXPORT if defined @NDBM_File::EXPORT ;

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

    eval 'use SubDB ; use Fcntl ; ';
    main::ok(13, $@ eq "") ;
    my %h ;
    my $X ;
    eval '
	$X = tie(%h, "SubDB","dbhash.tmp", O_RDWR|O_CREAT, 0640 );
	' ;

    main::ok(14, $@ eq "") ;

    my $ret = eval '$h{"fred"} = 3 ; return $h{"fred"} ' ;
    main::ok(15, $@ eq "") ;
    main::ok(16, $ret == 5) ;

    $ret = eval '$X->A_new_method("fred") ' ;
    main::ok(17, $@ eq "") ;
    main::ok(18, $ret eq "[[5]]") ;

    undef $X;
    untie(%h);
    unlink "SubDB.pm", <dbhash.tmp*> ;

}

{
   # DBM Filter tests
   use strict ;
   use warnings ;
   my (%h, $db) ;
   my ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;

   sub checkOutput
   {
       my($fk, $sk, $fv, $sv) = @_ ;
       return
           $fetch_key eq $fk && $store_key eq $sk && 
	   $fetch_value eq $fv && $store_value eq $sv &&
	   $_ eq 'original' ;
   }
   
   unlink <Op.dbmx*>;
   ok(19, $db = tie(%h, 'NDBM_File','Op.dbmx', O_RDWR|O_CREAT, 0640)) ;

   $db->filter_fetch_key   (sub { $fetch_key = $_ }) ;
   $db->filter_store_key   (sub { $store_key = $_ }) ;
   $db->filter_fetch_value (sub { $fetch_value = $_}) ;
   $db->filter_store_value (sub { $store_value = $_ }) ;

   $_ = "original" ;

   $h{"fred"} = "joe" ;
   #                   fk   sk     fv   sv
   ok(20, checkOutput( "", "fred", "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(21, $h{"fred"} eq "joe");
   #                   fk    sk     fv    sv
   ok(22, checkOutput( "", "fred", "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(23, $db->FIRSTKEY() eq "fred") ;
   #                    fk     sk  fv  sv
   ok(24, checkOutput( "fred", "", "", "")) ;

   # replace the filters, but remember the previous set
   my ($old_fk) = $db->filter_fetch_key   
   			(sub { $_ = uc $_ ; $fetch_key = $_ }) ;
   my ($old_sk) = $db->filter_store_key   
   			(sub { $_ = lc $_ ; $store_key = $_ }) ;
   my ($old_fv) = $db->filter_fetch_value 
   			(sub { $_ = "[$_]"; $fetch_value = $_ }) ;
   my ($old_sv) = $db->filter_store_value 
   			(sub { s/o/x/g; $store_value = $_ }) ;
   
   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"Fred"} = "Joe" ;
   #                   fk   sk     fv    sv
   ok(25, checkOutput( "", "fred", "", "Jxe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(26, $h{"Fred"} eq "[Jxe]");
   #                   fk   sk     fv    sv
   ok(27, checkOutput( "", "fred", "[Jxe]", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(28, $db->FIRSTKEY() eq "FRED") ;
   #                   fk   sk     fv    sv
   ok(29, checkOutput( "FRED", "", "", "")) ;

   # put the original filters back
   $db->filter_fetch_key   ($old_fk);
   $db->filter_store_key   ($old_sk);
   $db->filter_fetch_value ($old_fv);
   $db->filter_store_value ($old_sv);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok(30, checkOutput( "", "fred", "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(31, $h{"fred"} eq "joe");
   ok(32, checkOutput( "", "fred", "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(33, $db->FIRSTKEY() eq "fred") ;
   ok(34, checkOutput( "fred", "", "", "")) ;

   # delete the filters
   $db->filter_fetch_key   (undef);
   $db->filter_store_key   (undef);
   $db->filter_fetch_value (undef);
   $db->filter_store_value (undef);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok(35, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(36, $h{"fred"} eq "joe");
   ok(37, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(38, $db->FIRSTKEY() eq "fred") ;
   ok(39, checkOutput( "", "", "", "")) ;

   undef $db ;
   untie %h;
   unlink <Op.dbmx*>;
}

{    
    # DBM Filter with a closure

    use strict ;
    use warnings ;
    my (%h, $db) ;

    unlink <Op.dbmx*>;
    ok(40, $db = tie(%h, 'NDBM_File','Op.dbmx', O_RDWR|O_CREAT, 0640)) ;

    my %result = () ;

    sub Closure
    {
        my ($name) = @_ ;
	my $count = 0 ;
	my @kept = () ;

	return sub { ++$count ; 
		     push @kept, $_ ; 
		     $result{$name} = "$name - $count: [@kept]" ;
		   }
    }

    $db->filter_store_key(Closure("store key")) ;
    $db->filter_store_value(Closure("store value")) ;
    $db->filter_fetch_key(Closure("fetch key")) ;
    $db->filter_fetch_value(Closure("fetch value")) ;

    $_ = "original" ;

    $h{"fred"} = "joe" ;
    ok(41, $result{"store key"} eq "store key - 1: [fred]");
    ok(42, $result{"store value"} eq "store value - 1: [joe]");
    ok(43, !defined $result{"fetch key"} );
    ok(44, !defined $result{"fetch value"} );
    ok(45, $_ eq "original") ;

    ok(46, $db->FIRSTKEY() eq "fred") ;
    ok(47, $result{"store key"} eq "store key - 1: [fred]");
    ok(48, $result{"store value"} eq "store value - 1: [joe]");
    ok(49, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(50, ! defined $result{"fetch value"} );
    ok(51, $_ eq "original") ;

    $h{"jim"}  = "john" ;
    ok(52, $result{"store key"} eq "store key - 2: [fred jim]");
    ok(53, $result{"store value"} eq "store value - 2: [joe john]");
    ok(54, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(55, ! defined $result{"fetch value"} );
    ok(56, $_ eq "original") ;

    ok(57, $h{"fred"} eq "joe");
    ok(58, $result{"store key"} eq "store key - 3: [fred jim fred]");
    ok(59, $result{"store value"} eq "store value - 2: [joe john]");
    ok(60, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(61, $result{"fetch value"} eq "fetch value - 1: [joe]");
    ok(62, $_ eq "original") ;

    undef $db ;
    untie %h;
    unlink <Op.dbmx*>;
}		

{
   # DBM Filter recursion detection
   use strict ;
   use warnings ;
   my (%h, $db) ;
   unlink <Op.dbmx*>;

   ok(63, $db = tie(%h, 'NDBM_File','Op.dbmx', O_RDWR|O_CREAT, 0640)) ;

   $db->filter_store_key (sub { $_ = $h{$_} }) ;

   eval '$h{1} = 1234' ;
   ok(64, $@ =~ /^recursion detected in filter_store_key at/ );
   
   undef $db ;
   untie %h;
   unlink <Op.dbmx*>;
}

{
    # Bug ID 20001013.009
    #
    # test that $hash{KEY} = undef doesn't produce the warning
    #     Use of uninitialized value in null operation 
    use warnings ;
    use strict ;
    use NDBM_File ;

    unlink <Op.dbmx*>;
    my %h ;
    my $a = "";
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    
    ok(65, tie(%h, 'NDBM_File','Op.dbmx', O_RDWR|O_CREAT, 0640)) ;
}
