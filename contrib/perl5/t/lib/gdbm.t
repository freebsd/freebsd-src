#!./perl

# $RCSfile: dbm.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:43 $

BEGIN {
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bGDBM_File\b/) {
	print "1..0\n";
	exit 0;
    }
}

use GDBM_File;

print "1..20\n";

unlink <Op.dbmx*>;

umask(0);
print (tie(%h,GDBM_File,'Op.dbmx', &GDBM_WRCREAT, 0640) ? "ok 1\n" : "not ok 1\n");

$Dfile = "Op.dbmx.pag";
if (! -e $Dfile) {
	($Dfile) = <Op.dbmx*>;
}
if ($^O eq 'amigaos' || $^O eq 'os2' || $^O eq 'MSWin32' || $^O eq 'dos') {
    print "ok 2 # Skipped: different file permission semantics\n";
}
else {
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat($Dfile);
    print (($mode & 0777) == 0640 ? "ok 2\n" : "not ok 2\n");
}
while (($key,$value) = each(%h)) {
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
print (tie(%h,GDBM_File,'Op.dbmx', &GDBM_WRCREAT, 0640) ? "ok 4\n" : "not ok 4\n");

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

@keys = keys(%h);
@values = values(%h);

if ($#keys == 29 && $#values == 29) {print "ok 5\n";} else {print "not ok 5\n";}

while (($key,$value) = each(%h)) {
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
$ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
print ($ok ? "ok 8\n" : "not ok 8\n");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print ($size > 0 ? "ok 9\n" : "not ok 9\n");

@h{0..200} = 200..400;
@foo = @h{0..200};
print join(':',200..400) eq join(':',@foo) ? "ok 10\n" : "not ok 10\n";

print ($h{'foo'} eq '' ? "ok 11\n" : "not ok 11\n");
print ($h{''} eq 'bar' ? "ok 12\n" : "not ok 12\n");

untie %h;
unlink 'Op.dbmx.dir', $Dfile;

sub ok
{
    my $no = shift ;
    my $result = shift ;

    print "not " unless $result ;
    print "ok $no\n" ;
}

{
   # sub-class test

   package Another ;

   use strict ;

   open(FILE, ">SubDB.pm") or die "Cannot open SubDB.pm: $!\n" ;
   print FILE <<'EOM' ;

   package SubDB ;

   use strict ;
   use vars qw(@ISA @EXPORT) ;

   require Exporter ;
   use GDBM_File;
   @ISA=qw(GDBM_File);
   @EXPORT = @GDBM_File::EXPORT ;

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

    eval 'use SubDB ; ';
    main::ok(13, $@ eq "") ;
    my %h ;
    my $X ;
    eval '
	$X = tie(%h, "SubDB","dbhash.tmp", &GDBM_WRCREAT, 0640 );
	' ;

    main::ok(14, $@ eq "") ;

    my $ret = eval '$h{"fred"} = 3 ; return $h{"fred"} ' ;
    main::ok(15, $@ eq "") ;
    main::ok(16, $ret == 5) ;

    $ret = eval ' &GDBM_WRCREAT eq &main::GDBM_WRCREAT ' ;
    main::ok(17, $@ eq "" ) ;
    main::ok(18, $ret == 1) ;

    $ret = eval '$X->A_new_method("fred") ' ;
    main::ok(19, $@ eq "") ;
    main::ok(20, $ret eq "[[5]]") ;

    undef $X;
    untie(%h);
    unlink "SubDB.pm", <dbhash.tmp*> ;

}
