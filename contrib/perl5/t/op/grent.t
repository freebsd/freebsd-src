#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    eval {my @n = getgrgid 0};
    if ($@ && $@ =~ /(The \w+ function is unimplemented)/) {
	print "1..0 # Skip: $1\n";
	exit 0;
    }
    eval { require Config; import Config; };
    my $reason;
    if ($Config{'i_grp'} ne 'define') {
	$reason = '$Config{i_grp} not defined';
    }
    elsif (not -f "/etc/group" ) { # Play safe.
	$reason = 'no /etc/group file';
    }

    if (not defined $where) {	# Try NIS.
	foreach my $ypcat (qw(/usr/bin/ypcat /bin/ypcat /etc/ypcat)) {
	    if (-x $ypcat &&
		open(GR, "$ypcat group 2>/dev/null |") &&
		defined(<GR>)) {
		$where = "NIS group";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where) {	# Try NetInfo.
	foreach my $nidump (qw(/usr/bin/nidump)) {
	    if (-x $nidump &&
		open(GR, "$nidump group . 2>/dev/null |") &&
		defined(<GR>)) {
		$where = "NetInfo group";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where) {	# Try local.
	my $GR = "/etc/group";
	if (-f $GR && open(GR, $GR) && defined(<GR>)) {
	    undef $reason;
	    $where = $GR;
	}
    }
    if ($reason) {
	print "1..0 # Skip: $reason\n";
	exit 0;
    }
}

# By now the GR filehandle should be open and full of juicy group entries.

print "1..2\n";

# Go through at most this many groups.
# (note that the first entry has been read away by now)
my $max = 25;

my $n   = 0;
my $tst = 1;
my %perfect;
my %seen;

setgrent();
while (<GR>) {
    chomp;
    # LIMIT -1 so that groups with no users don't fall off
    my @s = split /:/, $_, -1;
    my ($name_s,$passwd_s,$gid_s,$members_s) = @s;
    if (@s) {
	push @{ $seen{$name_s} }, $.;
    } else {
	warn "# Your $where line $. is empty.\n";
	next;
    }
    if ($n == $max) {
	local $/;
	my $junk = <GR>;
	last;
    }
    # In principle we could whine if @s != 4 but do we know enough
    # of group file formats everywhere?
    if (@s == 4) {
	$members_s =~ s/\s*,\s*/,/g;
	$members_s =~ s/\s+$//;
	$members_s =~ s/^\s+//;
	@n = getgrgid($gid_s);
	# 'nogroup' et al.
	next unless @n;
	my ($name,$passwd,$gid,$members) = @n;
	# Protect against one-to-many and many-to-one mappings.
	if ($name_s ne $name) {
	    @n = getgrnam($name_s);
	    ($name,$passwd,$gid,$members) = @n;
	    next if $name_s ne $name;
	}
	# NOTE: group names *CAN* contain whitespace.
	$members =~ s/\s+/,/g;
	# what about different orders of members?
	$perfect{$name_s}++
	    if $name    eq $name_s    and
# Do not compare passwords: think shadow passwords.
# Not that group passwords are used much but better not assume anything.
               $gid     eq $gid_s     and
               $members eq $members_s;
    }
    $n++;
}

endgrent();

if (keys %perfect == 0) {
    $max++;
    print <<EOEX;
#
# The failure of op/grent test is not necessarily serious.
# It may fail due to local group administration conventions.
# If you are for example using both NIS and local groups,
# test failure is possible.  Any distributed group scheme
# can cause such failures.
#
# What the grent test is doing is that it compares the $max first
# entries of $where
# with the results of getgrgid() and getgrnam() call.  If it finds no
# matches at all, it suspects something is wrong.
# 
EOEX
    print "not ";
    $not = 1;
} else {
    $not = 0;
}
print "ok ", $tst++;
print "\t# (not necessarily serious: run t/op/grent.t by itself)" if $not;
print "\n";

# Test both the scalar and list contexts.

my @gr1;

setgrent();
for (1..$max) {
    my $gr = scalar getgrent();
    last unless defined $gr;
    push @gr1, $gr;
}
endgrent();

my @gr2;

setgrent();
for (1..$max) {
    my ($gr) = (getgrent());
    last unless defined $gr;
    push @gr2, $gr;
}
endgrent();

print "not " unless "@gr1" eq "@gr2";
print "ok ", $tst++, "\n";

close(GR);
