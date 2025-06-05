#!/afs/athena/contrib/perl/p

#
# $Id$
#

$debug = $ARGV[1] || $ENV{'VERBOSE_TEST'};

die "Need a number.\n" if !$ARGV[0];

die "Neither \$TOP nor \$TESTDIR is set.\n" 
    if (! ($ENV{'TOP'} || $ENV{'TESTDIR'}));

$TESTDIR = ($ENV{'TESTDIR'} || "$ENV{'TOP'}/testing");
$INITDB = ($ENV{'INITDB'} || "$TESTDIR/scripts/init_db");

for ($i=0; $i<$ARGV[0]; $i++) {
    print "Trial $i\n" if $debug;

    system("$INITDB > /dev/null 2>&1") &&
	die "Error in init_db\n";

    open(KEYS,"../dbutil/kdb5_util -R dump_db|") ||
	die "Couldn't run kdb5_util: $!\n";
    chop($header = <KEYS>);
    if ($header ne "kdb5_util load_dump version 4") {
	die "Cannot operate on dump version \"$header\"; version 4 required.";
    }
    while(<KEYS>) {
	next if ((!/^princ.*kadmin\//) && (!/^princ.*krbtgt/));

	print if $debug > 1;

	split;

	$princ = $_[6];
	$nkeys = $_[4];
	$ntls = $_[3];
	print "$princ: nkeys $nkeys, ntls $ntls\n" if $debug;
	for ($j = 15 + $ntls*3; $nkeys > 0; $nkeys--) {
	    $ver = $_[$j++];
	    $kvno = $_[$j++];
	    $keytype = $_[$j++];
	    $keylen = $_[$j++];
	    $keydata = $_[$j++];
	    $j += 3 if ($ver > 1);

	    print "$princ, ver $ver, kvno $kvno, type $keytype, len $keylen, "
		. "data $keydata\n" if $debug;
	    
	    die "Duplicated key $princ = $keydata\n" if
		$keys{$keydata}++;
	}
    }
    close(KEYS);
}
