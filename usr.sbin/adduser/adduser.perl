#!/usr/bin/perl
#
# (c) Copyright 1995 Wolfram Schneider. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#   This product includes software developed by Wolfram Schneider
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# /usr/sbin/adduser - add new user(s)
#
# Bugs: sure (my english!)
#   Email: Wolfram Schneider <wosch@cs.tu-berlin.de>
#
# $Id: adduser,v 1.17 1995/01/02 00:08:43 w Exp w $
#

sub variables {
    $verbose = 1;
    $batch = 0;                         # batch mode
    $defaultpasswd = 0;
    $dotdir = "/usr/share/skel";

    if (1) {
    $home = "/home";
    $shells = "/etc/shells";
    $passwd = "/etc/master.passwd";
    $group = "/etc/group";
    $pwd_mkdb = "pwd_mkdb -p";
    } else {
    $home = "/home/w/tmp/adduser/home";
    $shells = "./shells";
    $passwd = "./master.passwd";
    $group = "./group";
    $pwd_mkdb = "pwd_mkdb -p -d .";
    }

    @path = ('/bin', '/usr/bin', '/usr/local/bin');
    @shellpref = ('bash', 'tcsh', 'ksh', 'csh', 'sh');
    $uid_start = 1000;      # new users get this uid
    $uid_end   = 32000;

    # global variables
    $username = '';         # $username{username} = uid
    $uid = '';              # $uid{uid} = username
    $pwgid = '';            # $pwgid{pwgid} = username; gid from passwd db
    $groupname ='';         # $groupname{groupname} = gid
    $gid = '';              # $gid{gid} = groupname;    gid form group db
    $defaultshell = '';
    @passwd_backup = '';
}

# read shell database
# See also: shells(5)
sub shells_read {
    local($s, @dummy);
    open(S, $shells) || die "$shells:$!\n";
    while(<S>) {
        if (/^[ \t]*\//) {
            ($s, @dummy) = split;
            if (-x  $s) {
                $shell{&basename($s)} = $s;
            } else {
                warn "Shell: $s not executable!\n";
            }
        }
    }
}

# add new/local shells
sub shells_add {
    local($e,$dir,@list);
    foreach $e (@shellpref) {
        if (!$shell{$e}) {
            foreach $dir (@path) {
                if (-x "$dir/$e") {
                    push(@list, "$dir/$e") if
                    &confirm_yn("Found shell: $dir/$e. Add to $shells?", "yes");
                }
            }
        }
    }
    if ($#list >= 0) {
        foreach $e (@list) {
            $shell{&basename($e)} = $e;
            #print "$e\n";
        }
        &append_file($shells, @list);
    }
}

# choise your favourite shell
sub shells_pref {
    local($e,$i,$s);

    $i = 0;
    while($i < $#shellpref) {
        last if $shell{$shellpref[$i]};
        $i++;
    }
    $s = &confirm_list("Enter Your default shell:", 0,
                        $shellpref[$i], sort(keys %shell));
    print "Your default shell is: $s -> $shell{$s}\n" if $verbose;
    $defaultshell = $s;
}

# return default home partition
sub home_partition {
    local($h);

    $h = &confirm_list("Enter Your default HOME partition:", 1, $home, "");
    if (-e "$h") {
        if (!(-d _ || -l $h)) {
            warn "$h exist, but is it not a directory or symlink!\n";
            return &home_partition;
        }
        if (! -w _) {
            warn "$h is not writable!\n";
            return &home_partition;
        }
    } else {
        return &home_partition unless &mkdirhier($h);
    }
	
	$home = $h;
    return $h;
}

# check for valid passwddb
sub passwd_check {
    print "Check $passwd\n" if $verbose > 0;
    system("$pwd_mkdb $passwd");
    die "\nInvalid $passwd - cannot add any users!\n" if $?;
}

# read /etc/passwd
sub passwd_read {
    local($un, $pw, $ui, $gi);

    open(P, "$passwd") || die "$passwd: $!\n";
    while(<P>) {
        chop;
        push(@passwd_backup, $_);
        ($un, $pw, $ui, $gi) = (split(/:/, $_))[0..3];
        print "$un already exist with uid: $username{$un}!\n"
            if $username{$un};
        $username{$un} = $ui;
        print "User $un: uid $ui exist twice: $uid{$ui}\n"
            if $uid{$ui} && $verbose;
        $uid{$ui} = $un;
        $pwgid{$gi} = $un;
    }
    close P;
}

# read /etc/group
sub group_read {
    local($gn,$pw,$gi);

    open(G, "$group") || die "$group: $!\n";
    while(<G>) {
        ($gn, $pw, $gi) = (split(/:/, $_))[0..2];
        warn "Groupname exist twice: $gn:$gi -> $gn:$groupname{$gn}\n"
            if $groupname{$gn};
        $groupname{$gn} = $gi;
        warn "Groupid exist twice:   $gn:$gi -> $gid{$gi}:$gi\n"
            if $gid{$gi};
        $gid{$gi} = $gn;
    }
    close G;
}

# check gids /etc/passwd <-> /etc/group
sub group_check {
    local($e, $user, @list);

    foreach $e (keys %pwgid) {
        if (!$gid{$e}) {
            $user = $pwgid{$e};
            warn "Gid $e is defined in $passwd for user ``$user''\n";
            warn "but not in $group!\n";
            if ($groupname{$user}) {
                warn <<EOF;
I'm confused! Maybe the gids ($e <-> $groupname{$user}) for user ``$user''
in $passwd & $group are wrong.
See $passwd ``$user:*:$username{$user}:$e''
See $group ``$user:*:$groupname{$user}''
EOF
            } else {
                push(@list, "$user:*:$e:$user")
                if (&confirm_yn("Add group``$user'' gid $e to $group?", "y"));
            }
        }
    }
    &append_file($group, @list) if $#list >= 0;
}

sub new_users {
    local(@userlist) = @_;
    local($name);
    local($defaultname) = "a-z0-9";

    print "\nOk, let's go.\n";
    print "Don't worry about mistakes. I ask You later for " .
          "correct input.\n" if $verbose;

    while(1) {
        $name = &confirm_list("Enter username", 1, $defaultname, "");
        if ($name !~ /^[a-z0-9]+$/) {
            warn "Wrong username. " .
                "Please use only lowercase characters or digits\n";
        } elsif ($username{$name}) {
            warn "Username ``$name'' already exists!\n";
        } else {
            last;
        }
    }
    local($fullname);
    while(($fullname = &confirm_list("Enter full name", 1, "", "")) =~ /:/) {
        warn "``:'' is not allowed!\n";
    }
    $fullname = $name unless $fullname;
    local($sh) = &confirm_list("Enter shell", 0, $defaultshell, keys %shell);
    $sh = $shell{$sh};
    local($u_id, $g_id) = &next_id($name);
    print <<EOF;

Name:     $name
Passwd:   none, is empty
Fullname: $fullname
Uid:      $u_id
Gid:      $g_id
HOME:     $home/$name
Shell:    $sh
EOF
    if (&confirm_yn("Ok?", "yes")) {
        local($new_entry) =
            "$name::$u_id:$g_id::0:0:$fullname:$home/$name:$sh";

        &append_file($passwd, $new_entry);

        system("$pwd_mkdb $passwd");
        if ($?) {
            local($crash) = "$passwd.crash$$";
            warn "$pwd_mkdb failed, try to restore ...\n";

            open(R, "> $crash") || die "Sorry, give up\n";
            $j = join("\n", @passwd_backup);
            $j =~ s/\n//;
            print R $j . "\n";
            close R;

            system("$pwd_mkdb $crash");
            die "Sorry, give up\n" if $?;
            die "Successfully restore $passwd. Exit.\n";
        }
        # Add new group
        &append_file($group, "$name:*:$g_id:$name")
            unless $groupname{$name};

        # update passwd/group variables
        push(@passwd_backup, $new_entry);
        $username{$name} = $u_id;
        $uid{$u_id} = $name;
        $pwgid{$g_id} = $name;
        $groupname{$name} = $g_id;
        $gid{$g_id} = $name;

        print "Added user ``$name''\n";
        local($a) = &confirm_yn("Change password", $defaultpasswd);
        if (($a && $defaultpasswd) || (!$a && !$defaultpasswd)) {
            while(1) {
                system("passwd $name");
                last unless $?;
                last unless
                    &confirm_yn("Passwd $name failed. Try again?", "yes");
            }
        }
    	&home_create($name);
	}
   	if (&confirm_yn("Continue with next user?", "yes")) {
       	&new_users;
    } else {
        print "Good by.\n" if $verbose;
    }
}

#
sub password_pref {
    $defaultpasswd = !&confirm_yn("Use empty passwords", "yes");
}

# misc
sub check_root {
    die "You are not root!\n" if $<;
}

sub usage {
    warn <<USAGE;

usage: adduser [options]

OPTIONS:
-help                   this help
-silent                 opposite of verbose
-verbose                verbose
-home home              default HOME partition [$home]
-shell shell            default SHELL
-dotdir dir             copy files from dir, default $dotdir
USAGE
    exit 1;
}

#
sub parse_arguments {
    local(@argv) = @_;

    while ($_ = $argv[0], /^-/) {
        shift @argv;
        last if /^--$/;
        if    (/^--?(debug|verbose)$/)  { $verbose = 1 }
        elsif (/^--?(silent|guru|wizard)$/) { $verbose = 0 }
        elsif (/^--?(verbose)$/)        { $verbose = 1 }
        elsif (/^--?(h|help|\?)$/)      { &usage }
        elsif (/^--?(home)$/)           { $home = $argv[0]; shift @argv }
        elsif (/^--?(shell)$/)          { $shell = $argv[0]; shift @argv }
        elsif (/^--?(dotdir)$/)         { $dotdir = $argv[0]; shift @argv }
        elsif (/^--?(batch)$/)          { $batch = 1; }
        else                            { &usage }
    }
    #&usage if $#argv < 0;
}

sub basename {
    local($name) = @_;
    $name =~ s|.*/||;
    return $name;
}

#
sub home_create {
    local($name) = @_;
    local(@list);
    local($e,$from, $to);

    print "Create HOME directory\n";
    if(!mkdir("$home/$name", 0755)) {
        warn "Cannot create HOME directory for $name: $!\n";
        return 0;
    }
    push(@list, "$home/$name");
    if ($dotdir) {
        opendir(D, "$dotdir") || warn "$dotdir: $!\n";
        foreach $from (readdir(D)) {
            if ($from !~ /^(\.|\.\.)$/) {
                $to = $from;
                $to =~ s/^dot\././;
                $to = "$home/$name/$to";
                push(@list, $to);
                &cp("$dotdir/$from", "$to", 1);
            }
        }
        closedir D;
    }
	#warn "Chown: $name, $name, @list\n";
	#chown in perl does not work 
    system("chown $name:$name @list") || warn "$!\n" && return 0;
    return 1;
}

# makes a directory hierarchy
sub mkdirhier {
    local($dir) = @_;

    if ($dir =~ "^/[^/]+$") {
        print "Create /usr/$dir\n" if $verbose;
        if (!mkdir("/usr$dir", 0755)) {
            warn "/usr/$dir: $!\n"; return 0;
        }
        print "Create symlink: /usr$dir -> $dir\n" if $verbose;
        if (!symlink("/usr$dir", $dir)) {
            warn "$dir: $!\n"; return 0;
        }
    } else {
        local($d,$p);
        foreach $d (split('/', $dir)) {
            $dir = "$p/$d";
            $dir =~ s|^//|/|;
            if (! -e "$dir") {
                print "Create $dir\n" if $verbose;
                if (!mkdir("$dir", 0755)) {
                    warn "$dir: $!\n"; return 0;
                }
            }
            $p .= "/$d";
        }
    }
    return 1;
}


# Read one of the elements from @list. $confirm is default.
# If !$allow accept only elements from @list.
sub confirm_list {
    local($message, $allow, $confirm, @list) = @_;
    local($read, $c);

    print "$message " if $message;
    print "@list [$confirm]: ";
    chop($read = <STDIN>);
    $read =~ s/^[ \t]*//;
    $read =~ s/[ \t\n]*$//;
    return $confirm unless $read;
    return $read if $allow;

    foreach $c (@list) {
        return $read if $c eq $read;
    }
    warn "$read: is not allowed!\n";
    return &confirm_list($message, $allow, $confirm, @list);
}

# YES or NO question
# $confirm => 'y' or 'n'. Return true if answer 'y' (or 'n')
sub confirm_yn {
    local($message, $confirm) = @_;
    local($yes) = "^(yes|YES|y|Y)$";
    local($no) = "^(no|NO|n|N)$";
    local($read, $c);

    if ($confirm && ($confirm =~ "$yes" || $confirm == 1)) {
        $confirm = "y";
    } else {
        $confirm = "n";
    }
    print "$message (y/n) [$confirm]: ";
    chop($read = <STDIN>);
    $read =~ s/^[ \t]*//;
    $read =~ s/[ \t\n]*$//;
    return 1 unless $read;

    if (($confirm eq "y" && $read =~ "$yes") ||
        ($confirm eq "n" && $read =~ "$no")) {
        return 1;
    }

    if ($read !~ "$yes" && $read !~ "$no") {
        warn "Wrong value. Enter again!\a\n";
        return &confirm_yn($message, $confirm);
    }
    return 0;
}

# test if $dotdir exist
sub dotdir_check {
    return 1 if -e $dotdir && -r _ && (-d _ || -l $dotdir);
    warn "Directory: $dotdir does not exist or unreadable. " .
         "Cannot copy dotfiles!\n";
    $dotdir = '';
    return 0;
}

# write @list to $file with file-locking
sub append_file {
    local($file,@list) = @_;
    local($e);
    local($LOCK_EX) = 2;
    local($LOCK_UN) = 8;

    open(F, ">> $file") || die "$file: $!\n";
    print "Lock $file.\n" if $verbose > 1;
    flock(F, $LOCK_EX);
    print F join("\n", @list) . "\n";
    close F;
    print "Unlock $file.\n" if $verbose > 1;
    flock(F, $LOCK_UN);
}

# return free uid+gid
# uid == gid if possible
sub next_id {
    local($group) = @_;

    # looking for next free uid
    while($uid{$uid_start}) {
        $uid_start++;
        print "$uid_start\n" if $verbose > 1;
    }

    local($gid_start) = $uid_start;
    # group for user (username==groupname) already exist
    if ($groupname{$group}) {
        $gid_start = $groupname{$group};
    }
    # gid is in use, looking for another gid.
    # Note: uid an gid are not equal
    elsif ($gid{$uid_start}) {
        while($gid{$gid_start} || $uid{$gid_start}) {
            $gid_start--;
            $gid_start = $uid_end if $gid_start < 100;
        }
    }
    return ($uid_start, $gid_start);
}

sub cp {
    local($from, $to, $tilde) = @_;

    if (-e "$to") {
        warn "cp: ``$to'' already exist, do not overwrite\n"; return 0;
    } elsif (!(-f $from || -l $from)) {
        warn "$from is not a file or symlink!\n"; return 0;
    } elsif (!open(F, "$from")) {
        warn "$from: $!\n"; return 0;
    } elsif (!open(T, "> $to")) {
        warn "$to: $!\n"; return 0;
    }

    if ($tilde) {
        $tilde = $to;
        $tilde =~ s|.*/([^/]+/[^/]+)$|~$1|;
    } else {
        $tilde = $to;
    }
    print "copy $from to $tilde\n" if $verbose;
    while(<F>) {
        print T $_;
    }

    close F;
    close T;
    return 1;
}

################
# main
#
&check_root;            # You must be root to run this script!
&variables;             # initialize variables
&parse_arguments(@ARGV);    # parse arguments

&passwd_check;          # check for valid passwdb
&passwd_read;           # read /etc/master.passwd
&group_read;            # read /etc/group
&group_check;           # check for incon*
&dotdir_check;          # check $dotdir
print "\n";
&home_partition;        # find HOME partition
&shells_read;           # read /etc/shells
&shells_add;            # maybe add some new shells
&shells_pref;           # enter default shell
&password_pref;         # maybe use password

&new_users;             # add new users

#end
