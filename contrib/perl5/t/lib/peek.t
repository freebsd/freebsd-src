#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bPeek\b/) {
        print "1..0 # Skip: Devel::Peek was not built\n";
        exit 0;
    }
}

use Devel::Peek;

print "1..17\n";

our $DEBUG = 0;
open(SAVERR, ">&STDERR") or die "Can't dup STDERR: $!";

sub do_test {
    my $pattern = pop;
    if (open(OUT,">peek$$")) {
	open(STDERR, ">&OUT") or die "Can't dup OUT: $!";
	Dump($_[1]);
	open(STDERR, ">&SAVERR") or die "Can't restore STDERR: $!";
	close(OUT);
	if (open(IN, "peek$$")) {
	    local $/;
	    $pattern =~ s/\$ADDR/0x[[:xdigit:]]+/g;
	    print $pattern, "\n" if $DEBUG;
	    my $dump = <IN>;
	    print $dump, "\n"    if $DEBUG;
	    print "[$dump] vs [$pattern]\nnot " unless $dump =~ /$pattern/ms;
	    print "ok $_[0]\n";
	    close(IN);
	} else {
	    die "$0: failed to open peek$$: !\n";
	}
    } else {
	die "$0: failed to create peek$$: $!\n";
    }
}

our   $a;
our   $b;
my    $c;
local $d = 0;

do_test( 1,
	$a = "foo",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(POK,pPOK\\)
  PV = $ADDR "foo"\\\0
  CUR = 3
  LEN = 4'
       );

do_test( 2,
        "bar",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*POK,READONLY,pPOK\\)
  PV = $ADDR "bar"\\\0
  CUR = 3
  LEN = 4');

do_test( 3,
        $b = 123,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK\\)
  IV = 123');

do_test( 4,
        456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 456');

do_test( 5,
        $c = 456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADBUSY,PADMY,IOK,pIOK\\)
  IV = 456');

do_test( 6,
        $c + $d,
'SV = NV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADTMP,NOK,pNOK\\)
  NV = 456');

($d = "789") += 0.1;

do_test( 7,
       $d,
'SV = PVNV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(NOK,pNOK\\)
  IV = 0
  NV = 789\\.(?:1(?:000+\d+)?|0999+\d+)
  PV = $ADDR "789"\\\0
  CUR = 3
  LEN = 4');

do_test( 8,
        0xabcd,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK,IsUV\\)
  UV = 43981');

do_test( 9,
        undef,
'SV = NULL\\(0x0\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(\\)');

do_test(10,
        \$a,
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(POK,pPOK\\)
    PV = $ADDR "foo"\\\0
    CUR = 3
    LEN = 4');

do_test(11,
       [$b,$c],
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVAV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(\\)
    IV = 0
    NV = 0
    ARRAY = $ADDR
    FILL = 1
    MAX = 1
    ARYLEN = 0x0
    FLAGS = \\(REAL\\)
    Elt No. 0
    SV = IV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,pIOK\\)
      IV = 123
    Elt No. 1
    SV = PVNV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,NOK,pIOK,pNOK\\)
      IV = 456
      NV = 456
      PV = 0');

do_test(12,
       {$b=>$c},
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(SHAREKEYS\\)
    IV = 1
    NV = 0
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 150.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = 0x0
    Elt "123" HASH = $ADDR
    SV = PVNV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,NOK,pIOK,pNOK\\)
      IV = 456
      NV = 456
      PV = 0');

do_test(13,
        sub(){@_},
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(PADBUSY,PADMY,POK,pPOK,ANON\\)
    IV = 0
    NV = 0
    PROTOTYPE = ""
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0
    XSUBANY = 0
    GVGV::GV = $ADDR\\t"main" :: "__ANON__[^"]*"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 0
(?:    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0x4
    PADLIST = $ADDR
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test(14,
        \&do_test,
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (3|4)
    FLAGS = \\(\\)
    IV = 0
    NV = 0
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0
    XSUBANY = 0
    GVGV::GV = $ADDR\\t"main" :: "do_test"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 1
(?:    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0x0
    PADLIST = $ADDR
      \\d+\\. $ADDR \\("\\$pattern" \\d+-\\d+\\)
     \\d+\\. $ADDR \\(FAKE "\\$DEBUG" 0-\\d+\\)
     \\d+\\. $ADDR \\("\\$dump" \\d+-\\d+\\)
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test(15,
        qr(tic),
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVMG\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(OBJECT,RMG\\)
    IV = 0
    NV = 0
    PV = 0
    MAGIC = $ADDR
      MG_VIRTUAL = $ADDR
      MG_TYPE = \'r\'
      MG_OBJ = $ADDR
    STASH = $ADDR\\t"Regexp"');

do_test(16,
        (bless {}, "Tac"),
'SV = RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(OBJECT,SHAREKEYS\\)
    IV = 0
    NV = 0
    STASH = $ADDR\\t"Tac"
    ARRAY = 0x0
    KEYS = 0
    FILL = 0
    MAX = 7
    RITER = -1
    EITER = 0x0');

do_test(17,
	*a,
'SV = PVGV\\($ADDR\\) at $ADDR
  REFCNT = 5
  FLAGS = \\(GMG,SMG,MULTI(?:,IN_PAD)?\\)
  IV = 0
  NV = 0
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_glob
    MG_TYPE = \'\\*\'
    MG_OBJ = $ADDR
  NAME = "a"
  NAMELEN = 1
  GvSTASH = $ADDR\\t"main"
  GP = $ADDR
    SV = $ADDR
    REFCNT = 1
    IO = 0x0
    FORM = 0x0  
    AV = 0x0
    HV = 0x0
    CV = 0x0
    CVGEN = 0x0
    GPFLAGS = 0x0
    LINE = \\d+
    FILE = ".*\\b(?i:peek\\.t)"
    FLAGS = $ADDR
    EGV = $ADDR\\t"a"');

END {
  1 while unlink("peek$$");
}
