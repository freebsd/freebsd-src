#!/usr/bin/perl
# -*- perl -*-
# Copyright 1995, 1996, 1997 Guy Helmer, Ames, Iowa 50014.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer as
#    the first lines of this file unmodified.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY GUY HELMER ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL GUY HELMER BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# rmuser - Perl script to remove users
#
# Guy Helmer <ghelmer@cs.iastate.edu>, 02/23/97
#
# $FreeBSD$

sub LOCK_SH {0x01;}
sub LOCK_EX {0x02;}
sub LOCK_NB {0x04;}
sub LOCK_UN {0x08;}
sub F_SETFD {2;}

$ENV{"PATH"} = "/bin:/sbin:/usr/bin:/usr/sbin";
umask(022);
$whoami = $0;
$passwd_file = "/etc/master.passwd";
$new_passwd_file = "${passwd_file}.new.$$";
$group_file = "/etc/group";
$new_group_file = "${group_file}.new.$$";
$mail_dir = "/var/mail";
$crontab_dir = "/var/cron/tabs";
$atjob_dir = "/var/at/jobs";
$affirm = 0;

#$debug = 1;

sub cleanup {
    local($sig) = @_;

    print STDERR "Caught signal SIG$sig -- cleaning up.\n";
    &unlockpw;
    if (-e $new_passwd_file) {
	unlink $new_passwd_file;
    }
    exit(0);
}

sub lockpw {
    # Open the password file for reading
    if (!open(MASTER_PW, "$passwd_file")) {
	print STDERR "${whoami}: Error: Couldn't open ${passwd_file}: $!\n";
	exit(1);
    }
    # Set the close-on-exec flag just in case
    fcntl(MASTER_PW, &F_SETFD, 1);
    # Apply an advisory lock the password file
    if (!flock(MASTER_PW, &LOCK_EX|&LOCK_NB)) {
	print STDERR "${whoami}: Error: Couldn't lock ${passwd_file}: $!\n";
	exit(1);
    }
}

sub unlockpw {
    flock(MASTER_PW, &LOCK_UN);
}

$SIG{'INT'} = 'cleanup';
$SIG{'QUIT'} = 'cleanup';
$SIG{'HUP'} = 'cleanup';
$SIG{'TERM'} = 'cleanup';

if ($#ARGV == 1 && $ARGV[0] eq '-y') {
    shift @ARGV;
    $affirm = 1;
}

if ($#ARGV > 0) {
    print STDERR "usage: ${whoami} [-y] [username]\n";
    exit(1);
}

if ($< != 0) {
    print STDERR "${whoami}: Error: you must be root to use ${whoami}\n";
    exit(1);
}

&lockpw;

if ($#ARGV == 0) {
    # Username was given as a parameter
    $login_name = pop(@ARGV);
} else {
    if ($affirm) {
	print STDERR "${whoami}: Error: -y option given without username!\n";
	&unlockpw;
	exit 1;
    }
    # Get the user name from the user
    $login_name = &get_login_name;
}

($name, $password, $uid, $gid, $change, $class, $gecos, $home_dir, $shell) =
    (getpwnam("$login_name"));

if ($?) {
    print STDERR "${whoami}: Error: User ${login_name} not in password database\n";
    &unlockpw;
    exit 1;
}

if ($uid == 0) {
    print "${whoami}: Error: I'd rather not remove a user with a uid of 0.\n";
    &unlockpw;
    exit 1;
}

if (! $affirm) {
    print "Matching password entry:\n\n$name\:$password\:$uid\:$gid\:$class\:$change\:0\:$gecos\:$home_dir\:$shell\n\n";

    $ans = &get_yn("Is this the entry you wish to remove? ");

    if ($ans eq 'N') {
	print "${whoami}: Informational: User ${login_name} not removed.\n";
	&unlockpw;
	exit 0;
    }
}

#
# Get owner of user's home directory; don't remove home dir if not
# owned by $login_name

$remove_directory = 1;

if (-l $home_dir) {
    $real_home_dir = &resolvelink($home_dir);
} else {
    $real_home_dir = $home_dir;
}

#
# If home_dir is a symlink and points to something that isn't a directory,
# or if home_dir is not a symlink and is not a directory, don't remove
# home_dir -- seems like a good thing to do, but probably isn't necessary...

if (((-l $home_dir) && ((-e $real_home_dir) && !(-d $real_home_dir))) ||
    (!(-l $home_dir) && !(-d $home_dir))) {
    print STDERR "${whoami}: Informational: Home ${home_dir} is not a directory, so it won't be removed\n";
    $remove_directory = 0;
}

if (length($real_home_dir) && -d $real_home_dir) {
    $dir_owner = (stat($real_home_dir))[4]; # UID
    if ($dir_owner != $uid) {
	print STDERR "${whoami}: Informational: Home dir ${real_home_dir} is" .
	    " not owned by ${login_name} (uid ${dir_owner})\n," .
		"\tso it won't be removed\n";
	$remove_directory = 0;
    }
}

if ($remove_directory && ! $affirm) {
    $ans = &get_yn("Remove user's home directory ($home_dir)? ");
    if ($ans eq 'N') {
	$remove_directory = 0;
    }
}

#exit 0 if $debug;

#
# Remove the user's crontab, if there is one
# (probably needs to be done before password databases are updated)

if (-e "$crontab_dir/$login_name") {
    print STDERR "Removing user's crontab:";
    system('/usr/bin/crontab', '-u', $login_name, '-r');
    print STDERR " done.\n";
}

#
# Remove the user's at jobs, if any
# (probably also needs to be done before password databases are updated)

&remove_at_jobs($login_name, $uid);

#
# Kill all the user's processes

&kill_users_processes($login_name, $uid);

#
# Copy master password file to new file less removed user's entry

&update_passwd_file;

#
# Remove the user from all groups in /etc/group

&update_group_file($login_name);

#
# Remove the user's home directory

if ($remove_directory) {
    print STDERR "Removing user's home directory ($home_dir):";
    &remove_dir($home_dir);
    print STDERR " done.\n";
}

#
# Remove files related to the user from the mail directory

#&remove_files_from_dir($mail_dir, $login_name, $uid);
$file = "$mail_dir/$login_name";
if (-e $file || -l $file) {
    print STDERR "Removing user's incoming mail file ${file}:";
    unlink $file ||
	print STDERR "\n${whoami}: Warning: unlink on $file failed ($!) - continuing\n";
    print STDERR " done.\n";
}

#
# Remove some pop daemon's leftover file

$file = "$mail_dir/.${login_name}.pop";
if (-e $file || -l $file) {
    print STDERR "Removing pop daemon's temporary mail file ${file}:";
    unlink $file ||
	print STDERR "\n${whoami}: Warning: unlink on $file failed ($!) - continuing\n";
    print STDERR " done.\n";
}

#
# Remove files belonging to the user from the directories /tmp, /var/tmp,
# and /var/tmp/vi.recover.  Note that this doesn't take care of the
# problem where a user may have directories or symbolic links in those
# directories -- only regular files are removed.

&remove_files_from_dir('/tmp', $login_name, $uid);
&remove_files_from_dir('/var/tmp', $login_name, $uid);
&remove_files_from_dir('/var/tmp/vi.recover', $login_name, $uid)
    if (-e '/var/tmp/vi.recover');

#
# All done!

exit 0;

sub get_login_name {
    #
    # Get new user's name
    local($done, $login_name);

    for ($done = 0; ! $done; ) {
	print "Enter login name for user to remove: ";
	$login_name = <>;
	chomp $login_name;
	if (not getpwnam("$login_name")) {
	    print STDERR "Sorry, login name not in password database.\n";
	} else {
	    $done = 1;
	}
    }

    print "User name is ${login_name}\n" if $debug;
    return($login_name);
}

sub get_yn {
    #
    # Get a yes or no answer; return 'Y' or 'N'
    local($prompt) = @_;
    local($done, $ans);

    for ($done = 0; ! $done; ) {
	print $prompt;
	$ans = <>;
	chop $ans;
	$ans =~ tr/a-z/A-Z/;
	if (!($ans =~ /^[YN]/)) {
	    print STDERR "Please answer (y)es or (n)o.\n";
	} else {
	    $done = 1;
	}
    }

    return(substr($ans, 0, 1));
}

sub update_passwd_file {
    local($skipped);

    print STDERR "Updating password file,";
    seek(MASTER_PW, 0, 0);
    open(NEW_PW, ">$new_passwd_file") ||
	die "\n${whoami}: Error: Couldn't open file ${new_passwd_file}:\n $!\n";
    chmod(0600, $new_passwd_file) ||
	print STDERR "\n${whoami}: Warning: couldn't set mode of $new_passwd_file to 0600 ($!)\n\tcontinuing, but please check mode of /etc/master.passwd!\n";
    $skipped = 0;
    while (<MASTER_PW>) {
	if (not /^\Q$login_name:/o) {
	    print NEW_PW;
	} else {
	    print STDERR "Dropped entry for $login_name\n" if $debug;
	    $skipped = 1;
	}
    }
    close(NEW_PW);
    seek(MASTER_PW, 0, 0);

    if ($skipped == 0) {
	print STDERR "\n${whoami}: Whoops! Didn't find ${login_name}'s entry second time around!\n";
	unlink($new_passwd_file) ||
	    print STDERR "\n${whoami}: Warning: couldn't unlink $new_passwd_file ($!)\n\tPlease investigate, as this file should not be left in the filesystem\n";
	&unlockpw;
	exit 1;
    }

    #
    # Run pwd_mkdb to install the updated password files and databases

    print STDERR " updating databases,";
    system('/usr/sbin/pwd_mkdb', '-p', ${new_passwd_file});
    print STDERR " done.\n";

    close(MASTER_PW);		# Not useful anymore
}

sub update_group_file {
    local($login_name) = @_;

    local($i, $j, $grmember_list, $new_grent, $changes);
    local($grname, $grpass, $grgid, $grmember_list, @grmembers);

    $changes = 0;
    print STDERR "Updating group file:";
    open(GROUP, $group_file) ||
	die "\n${whoami}: Error: couldn't open ${group_file}: $!\n";
    if (!flock(GROUP, &LOCK_EX|&LOCK_NB)) {
	print STDERR "\n${whoami}: Error: couldn't lock ${group_file}: $!\n";
	exit 1;
    }
    local($group_perms, $group_uid, $group_gid) =
	(stat(GROUP))[2, 4, 5]; # File Mode, uid, gid
    open(NEW_GROUP, ">$new_group_file") ||
	die "\n${whoami}: Error: couldn't open ${new_group_file}: $!\n";
    chmod($group_perms, $new_group_file) ||
	printf STDERR "\n${whoami}: Warning: could not set permissions of new group file to %o ($!)\n\tContinuing, but please check permissions of $group_file!\n", $group_perms;
    chown($group_uid, $group_gid, $new_group_file) ||
	print STDERR "\n${whoami}: Warning: could not set owner/group of new group file to ${group_uid}/${group_gid} ($!)\n\rContinuing, but please check ownership of $group_file!\n";
    while ($i = <GROUP>) {
	if (!($i =~ /$login_name/)) {
	    # Line doesn't contain any references to the user, so just add it
	    # to the new file
	    print NEW_GROUP $i;
	} else {
	    #
	    # Remove the user from the group
	    if ($i =~ /\n$/) {
		chop $i;
	    }
	    ($grname, $grpass, $grgid, $grmember_list) = split(/:/, $i);
	    @grmembers = split(/,/, $grmember_list);
	    undef @new_grmembers;
	    local(@new_grmembers);
	    foreach $j (@grmembers) {
		if ($j ne $login_name) {
		    push(@new_grmembers, $j);
		} else {
		    print STDERR " $grname";
		    $changes = 1;
		}
	    }
	    if ($grname eq $login_name && $#new_grmembers == -1) {
		# Remove a user's personal group if empty
		print STDERR " (removing group $grname -- personal group is empty)";
		$changes = 1;
	    } else {
		$grmember_list = join(',', @new_grmembers);
		$new_grent = join(':', $grname, $grpass, $grgid, $grmember_list);
		print NEW_GROUP "$new_grent\n";
	    }
	}
    }
    close(NEW_GROUP);
    rename($new_group_file, $group_file) || # Replace old group file with new
	die "\n${whoami}: Error: couldn't rename $new_group_file to $group_file ($!)\n";
    close(GROUP);			# File handle is worthless now
    print STDERR " (no changes)" if (! $changes);
    print STDERR " done.\n";
}

sub remove_dir {
    # Remove the user's home directory
    local($dir) = @_;
    local($linkdir);

    if (-l $dir) {
	$linkdir = &resolvelink($dir);
	# Remove the symbolic link
	unlink($dir) ||
	    warn "${whoami}: Warning: could not unlink symlink $dir: $!\n";
	if (!(-e $linkdir)) {
	    #
	    # Dangling symlink - just return now
	    return;
	}
	# Set dir to be the resolved pathname
	$dir = $linkdir;
    }
    if (!(-d $dir)) {
	print STDERR "${whoami}: Warning: $dir is not a directory\n";
	unlink($dir) || warn "${whoami}: Warning: could not unlink $dir: $!\n";
	return;
    }
    system('/bin/rm', '-rf', $dir);
}

sub remove_files_from_dir {
    local($dir, $login_name, $uid) = @_;
    local($path, $i, $owner);

    print STDERR "Removing files belonging to ${login_name} from ${dir}:";

    if (!opendir(DELDIR, $dir)) {
	print STDERR "\n${whoami}: Warning: couldn't open directory ${dir} ($!)\n";
	return;
    }
    while ($i = readdir(DELDIR)) {
	next if $i eq '.';
	next if $i eq '..';

	$owner = (stat("$dir/$i"))[4]; # UID
	if ($uid == $owner) {
	    if (-f "$dir/$i") {
		print STDERR " $i";
		unlink "$dir/$i" ||
		    print STDERR "\n${whoami}: Warning: unlink on ${dir}/${i} failed ($!) - continuing\n";
	    } else {
		print STDERR " ($i not a regular file - skipped)";
	    }
	}
    }
    closedir(DELDIR);

    printf STDERR " done.\n";
}

sub remove_at_jobs {
    local($login_name, $uid) = @_;
    local($i, $owner, $found);

    $found = 0;
    opendir(ATDIR, $atjob_dir) || return;
    while ($i = readdir(ATDIR)) {
	next if $i eq '.';
	next if $i eq '..';
	next if $i eq '.lockfile';

	$owner = (stat("$atjob_dir/$i"))[4]; # UID
	if ($uid == $owner) {
	    if (!$found) {
		print STDERR "Removing user's at jobs:";
		$found = 1;
	    }
	    # Use atrm to remove the job
	    print STDERR " $i";
	    system('/usr/bin/atrm', $i);
	}
    }
    closedir(ATDIR);
    if ($found) {
	print STDERR " done.\n";
    }
}

sub resolvelink {
    local($path) = @_;
    local($l);

    while (-l $path && -e $path) {
	if (!defined($l = readlink($path))) {
	    die "${whoami}: readlink on $path failed (but it should have worked!): $!\n";
	}
	if ($l =~ /^\//) {
	    # Absolute link
	    $path = $l;
	} else {
	    # Relative link
	    $path =~ s/\/[^\/]+\/?$/\/$l/; # Replace last component of path
	}
    }
    return $path;
}

sub kill_users_processes {
    local($login_name, $uid) = @_;
    local($pid, $result);

    #
    # Do something a little complex: fork a child that changes its
    # real and effective UID to that of the removed user, then issues
    # a "kill(9, -1)" to kill all processes of the same uid as the sender
    # (see kill(2) for details).
    # The parent waits for the exit of the child and then returns.

    if ($pid = fork) {
	# Parent process
	waitpid($pid, 0);
    } elsif (defined $pid) {
	# Child process
	$< = $uid;
	$> = $uid;
	if ($< != $uid || $> != $uid) {
	    print STDERR "${whoami}: Error (kill_users_processes):\n" .
		"\tCouldn't reset uid/euid to ${uid}: current uid/euid's are $< and $>\n";
	    exit 1;
	}
	$result = kill(9, -1);
	print STDERR "Killed process(es) belonging to $login_name.\n"
	    if $result;
	exit 0;
    } else {
	# Couldn't fork!
	print STDERR "${whoami}: Error: couldn't fork to kill ${login_name}'s processes - continuing\n";
    }
}
