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
#    This product includes software developed by Wolfram Schneider
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
# $Id: adduser,v 1.40 1995/01/08 17:40:09 w Exp w $
#

# read variables
sub variables {
    $verbose = 1;                       # verbose = [0-2]
    $defaultpasswd = "no";              # use password for new users
    $dotdir = "/usr/share/skel";        # copy dotfiles from this dir
    $send_message = "/etc/adduser.message";  # send message to new user
    $config = "/etc/adduser.conf";      # config file for adduser
    $config_read = 1;                   # read config file
    $logfile = "/var/log/adduser";      # logfile
    $home = "/home";                    # default HOME
    $etc_shells = "/etc/shells";
    $etc_passwd = "/etc/master.passwd";
    $group = "/etc/group";
    $pwd_mkdb = "pwd_mkdb -p";          # program for building passwd database

    if ($test) {
    $home = "/home/w/tmp/adduser/home";
    $etc_shells = "./shells";
    $etc_passwd = "./master.passwd";
    $group = "./group";
    $pwd_mkdb = "pwd_mkdb -p -d .";
    $config = "adduser.conf";
    $send_message = "./adduser.message";
    $logfile = "./log.adduser";
    }

    $ENV{'PATH'} = "/sbin:/bin:/usr/sbin:/usr/bin";
    # List of directories where shells located
    @path = ('/bin', '/usr/bin', '/usr/local/bin');
    # common shells, first element has higher priority
    @shellpref = ('bash', 'tcsh', 'ksh', 'csh', 'sh');

    $defaultshell = 'bash'; # defaultshell if not empty

    $uid_start = 1000;      # new users get this uid
    $uid_end   = 32000;     # max. uid

    # global variables
    $username = '';         # $username{username} = uid
    $uid = '';              # $uid{uid} = username
    $pwgid = '';            # $pwgid{pwgid} = username; gid from passwd db
    $groupname ='';         # $groupname{groupname} = gid
    $gid = '';              # $gid{gid} = groupname;    gid form group db
    @passwd_backup = '';
    @message_buffer = '';
    @user_variable_list = '';   # user variables in /etc/adduser.conf
    $do_not_delete = '## DO NOT DELETE THIS LINE!';
}

# read shell databasem, see also: shells(5)
sub shells_read {
    local($s, @dummy);
    open(S, $etc_shells) || die "$shells:$!\n";
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
                    if ($verbose) {
                        if (&confirm_yn("Found shell: $dir/$e. Add to $etc_shells?", "yes")) {
                            push(@list, "$dir/$e");
                            push(@shellpref, "$dir/$e");
                            $shell{&basename("$dir/$e")} = "$dir/$e";
                            $changes++;
                        }
                    } else {
                        print "Found unused shell: $dir/$e\n";
                    }
                }
            }
        }
    }
    &append_file($etc_shells, @list) if $#list >= 0;
}

# choise your favourite shell an return the shell
sub shell_default {
    local($e,$i,$s);
    local($sh) = $defaultshell;

    # $defaultshell is empty, looking for another shell
    if (!$sh || !$shell{$sh}) {
        $i = 0;
        while($i < $#shellpref) {
            last if $shell{$shellpref[$i]};
            $i++;
        }
        $sh = $shellpref[$i];

        if (!$sh || !$shell{$sh}) {
            warn "No valid shell found in $etc_shells.\n";
            $sh = '';
        }
    }
    if ($verbose) {
        $s = &confirm_list("Enter your default shell:", 0,
                            $sh, sort(keys %shell));
        print "Your default shell is: $s -> $shell{$s}\n"; 
        $changes++ if $s ne $sh;
        return $s;
    } else {
        return $sh;
    }
}

# return default home partition, create base directory if nesseccary
sub home_partition {
    local($h);
    $oldverbose = $verbose if !defined $oldverbose;

    if ($verbose) {
        $h = &confirm_list("Enter your default HOME partition:", 1, $home, "");
    } else {
        $h = $home;
    }

    if ($h !~ "^/") {
        warn "Please use absolute path for home: ``$h''.\n";
        $verbose++;
        $h =~ s|^[^/]+||;
        $home = $h;
        return &home_partition;
    }

    if (-e $h) {
        if (!(-d _ || -l $h)) {
            warn "$h exist, but is it not a directory or symlink!\n";
            $verbose++;
            return &home_partition;
        }
        if (! -w _) {
            warn "$h is not writable!\n";
            $verbose++;
            return &home_partition;
        }
    } else {
        $verbose++;
        return &home_partition unless &mkdirhier($h);
    }

    $verbose = $oldverbose;
    undef $oldverbose;
    $changes++ if $h ne $home;
    return $h;
}

# check for valid passwddb
sub passwd_check {
    print "Check $etc_passwd\n" if $verbose > 0;
    system("$pwd_mkdb $etc_passwd");
    die "\nInvalid $etc_passwd - cannot add any users!\n" if $?;
}

# read /etc/passwd
sub passwd_read {
    local($un, $pw, $ui, $gi, $sh, %shlist);

    print "Check $group\n" if $verbose;
    foreach $sh (keys %shell) {
        $shlist{$shell{$sh}} = $sh; #print "$sh $shell{$sh}\n";
    }

    open(P, "$etc_passwd") || die "$passwd: $!\n";
    while(<P>) {
        chop;
        push(@passwd_backup, $_);
        ($un, $pw, $ui, $gi, $sh) = (split(/:/, $_))[0..3,9];
        print "$un already exist with uid: $username{$un}!\n"
            if $username{$un};
        $username{$un} = $ui;
        print "User $un: uid $ui exist twice: $uid{$ui}\n"
            if $uid{$ui} && $verbose && $ui;    # don't warn for uid 0
        print "User $un: illegal shell: ``$sh''\n" 
            if ((!$shlist{$sh} && $sh) &&
                ($un !~ /^(bin|uucp|falcon|nobody)$/));
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
            warn "User ``$user'' has gid $e but a group with this " .
                 "gid does not exist.\n";
            #warn "Gid $e is defined in $etc_passwd for user ``$user''\n";
            #warn "but not in $group!\n";
            if ($groupname{$user}) {
                warn <<EOF;
Maybe the gids ($e <-> $groupname{$user}) for user+group ``$user'' are wrong.
EOF
            } else {
                push(@list, "$user:*:$e:$user")
                if $verbose && 
                    &confirm_yn("Add group``$user'' gid $e to $group?", "y");
            }
        }
    }
    &append_file($group, @list) if $#list >= 0;
}

#
# main loop for creating new users
#
sub new_users {
    local(@userlist) = @_;
    local($name);
    local($defaultname) = "a-z0-9";

    print "\n" if $verbose;
    print "Ok, let's go.\n" .
          "Don't worry about mistakes. I ask you later for " .
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
Passwd:   no, is empty
Fullname: $fullname
Uid:      $u_id
Gid:      $g_id ($name)
HOME:     $home/$name
Shell:    $sh
EOF
    if (&confirm_yn("Ok?", "yes")) {
        local($new_entry) =
            "$name::$u_id:$g_id::0:0:$fullname:$home/$name:$sh";

        &append_file($etc_passwd, $new_entry);

        system("$pwd_mkdb $etc_passwd");
        if ($?) {
            local($crash) = "$etc_passwd.crash$$";
            warn "$pwd_mkdb failed, try to restore ...\n";

            open(R, "> $crash") || die "Sorry, give up\n";
            $j = join("\n", @passwd_backup);
            $j =~ s/\n//;
            print R $j . "\n";
            close R;

            system("$pwd_mkdb $crash");
            die "Sorry, give up\n" if $?;
            die "Successfully restore $etc_passwd. Exit.\n";
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

        if ($send_message && $send_message ne "no") {
            local($m) = &confirm_list("Send message to ``$name'' and:",
                1, "no", ("second_mail_address", "no"));
            if ($verbose) {
                print "send message to: ``$name''";
                if ($m eq "no") { print "\n" } 
                    else { print " and ``$m''\n"}
            }
            $m = "" if $m eq "no";
            local($e);
            if (!open(M, "| mail -s Welcome $name $m")) {
                warn "Cannot send mail to: $name $m!\n";
            } else {
                foreach $e (@message_buffer) {
                    print M eval "\"$e\"";
                }
                close M;
            }
        }

        local($a) = &confirm_yn("Change password", $defaultpasswd);
        local($empty_password) = "";
        if (($a && $defaultpasswd ne "no" && $defaultpasswd) || 
            (!$a && $defaultpasswd eq "no")) {
            while(1) {
                system("passwd $name");
                if (!$?) { $empty_password = "*"; last }
                last unless
                    &confirm_yn("Passwd $name failed. Try again?", "yes");
            }
        }
        &adduser_log("$name:$empty_password:$u_id:$g_id($name):$fullname");
        &home_create($name);
    }
    if (&confirm_yn("Continue with next user?", "yes")) {
        print "\n" if !$verbose;
        &new_users;
    } else {
        print "Good by.\n" if $verbose;
    }
}

# ask for password usage
sub password_default {
    local($p) = $defaultpasswd;
    if ($verbose) {
        $p = &confirm_yn("Use passwords", $defaultpasswd);
        $changes++ unless $p;
    }
    return "yes" if (($defaultpasswd eq "yes" && $p) ||
                     ($defaultpasswd eq "no" && !$p));
    return "no";    # otherwise
}

# misc
sub check_root {
    die "You are not root!\n" if $< && !$test;
}

sub usage {
    warn <<USAGE;

usage: adduser [options]

OPTIONS:
-help                   this help
-silent                 opposite of verbose
-verbose                verbose
-debug                  verbose verbose
-noconfig               don't read config-file
-home home              default HOME partition [$home]
-shell shell            default SHELL [$defaultshell]
-dotdir dir             copy files from dir, default [$dotdir]
-message file           send message to new users [$send_message]
-create_conf            create configuration/message file and exit
USAGE
    exit 1;
}

# print banner
sub copyright {
    print <<EOF;
(c) Copyright 1995 Wolfram Schneider <wosch@cs.tu-berlin.de>
EOF
}

# hints
sub hints {
    if ($verbose) {
        print "Use option ``-silent'' if you don't want see " .
              "some warnings & questions.\n\n";
    } else {
        print "Use option ``-verbose'' if you want see more warnings & " .
              "questions \nor try to repair bugs.\n\n";
    }
}

#
sub parse_arguments {
    local(@argv) = @_;

    while ($_ = $argv[0], /^-/) {
        shift @argv;
        last if /^--$/;
        if    (/^--?(verbose)$/)        { $verbose = 1 }
        elsif (/^--?(silent|guru|wizard|quit)$/) { $verbose = 0 }
        elsif (/^--?(debug)$/)          { $verbose = 2 }
        elsif (/^--?(h|help|\?)$/)      { &usage }
        elsif (/^--?(home)$/)           { $home = $argv[0]; shift @argv }
        elsif (/^--?(shell)$/)          { $defaultshell = $argv[0]; 
                                          shift @argv }
        elsif (/^--?(dotdir)$/)         { $dotdir = $argv[0]; shift @argv }
        elsif (/^--?(message)$/)        { $send_message = $argv[0]; shift @argv;
                                          $sendmessage = 1; }
        # see &config_read
        elsif (/^--?(create_conf)$/)    { &create_conf; }
        elsif (/^--?(noconfig)$/)       { $config_read = 0; }
        else                            { &usage }
    }
    #&usage if $#argv < 0;
}

sub basename {
    local($name) = @_;
    $name =~ s|.*/||;
    return $name;
}

sub dirname {
    local($name) = @_;
    $name =~ s|[/]+$||;             # delete any / at end 
    $name =~ s|[^/]+$||;
    $name = "/" unless $name;       # dirname of / is / 
    return $name;
}

# return 1 if $file is a readable file or link
sub filetest {
    local($file, $verb) = @_;

    if (-e $file) {
        if (-f $file || -l $file) {
            return 1 if -r _;
            warn "$file unreadable\n" if $verb;
        } else {
            warn "$file is not a plain file or link\n" if $verb;
        }
    }
    return 0; 
}

# create configuration files and exit
sub create_conf {
    $create_conf = 1;
    &message_create($send_message);
    &config_write(1);
    exit(0);
}

# log for new user in /var/log/adduser
sub adduser_log {
    local($string) = @_;
    local($e);

    return 1 if $logfile eq "no";

    local($sec, $min, $hour, $mday, $mon, $year) = localtime;
    $mon++;

    foreach $e ('sec', 'min', 'hour', 'mday', 'mon', 'year') {
        # '7' -> '07'
        eval "\$$e = 0 . \$$e" if (eval "\$$e" < 10);   
    }

    &append_file($logfile, "$year/$mon/$mday $hour:$min:$sec $string");
}

# create HOME directory, copy dotfiles from $dotdir to $HOME
sub home_create {
    local($name) = @_;
    local(@list);
    local($e,$from, $to);

    print "Create HOME directory\n" if $verbose;
    if(!mkdir("$home/$name", 0755)) {
        warn "Cannot create HOME directory for user ``$home/$name'': $!\n";
        return 0;
    }
    push(@list, "$home/$name");
    if ($dotdir && $dotdir ne "no") {
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
    local($read, $c, $print);

    $print = "$message " if $message;
    $print .= "@list ";
    print "$print";
    print "\n " if length($print) > 50;
    print "[$confirm]: ";

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
# return 1 if &confirm("message", "yes") and answer is yes
#       or if &confirm("message", "no") an answer is no
# otherwise 0
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
sub dotdir_default {
    #$dotdir = "no" unless $dotdir;
    local($dir) = $dotdir;
    local($oldverbose) = $verbose;

    if ($dir !~ "^/") {
        warn "Please use absulote path for dotdir: ``$dir''\n";
        $dir = "no";
        $verbose++ unless $verbose;
    }
    while($verbose) {
        $dir = &confirm_list("Copy dotfiles from:", 1, 
            $dir, ("no", $dir));
        last if $dir eq "no";
        last if (-e $dir && -r _ && (-d _ || -l $dir));
        last if !&confirm_yn("Dotdir ``$dir'' is not a dir. Try again", "yes");
    }
    unless (-e $dir && -r _ && (-d _ || -l $dir)) {
        warn "Directory: $dir does not exist or unreadable.\n" 
            if $dir ne "no";
        warn "Do not copy dotfiles.\n"; 
        $dir = "no";
    }
    $changes++ if $dir ne $dotdir;
    $verbose = $oldverbose;
    return $dir;
}

# ask for messages to new users
sub message_default {
    local($file) = $send_message;
    local(@d); 

    push(@d, $file);
    push(@d, "no") if $file ne "no";
    if (!&filetest($file, 1) && $file ne "no") {
        if (! -e $file && 
                &confirm_yn("Create message file ``$file''?", "yes")) {
            &message_create($file);
        } else {
            $file = "no";
        }
    }

    while($verbose) {
        $file = &confirm_list("Send message from file: ", 1, 
            $file, @d);
        last if $file eq "no";
        if (&filetest($file, 1)) {
            last;
        } else {
            &message_create($file) if ! -e $file &&
                &confirm_yn("Create ``$file''?", "yes");
            last if &filetest($file, 0);
            last if !&confirm_yn(
                "File ``$file'' does not exist, try again?", "yes");
        }
    }
    if ($file eq "no" || !&filetest($file, 0)) {
        warn "Do not send message\n" if $verbose;
        $file = "no";
    } else {
        &message_read($file);
    }

    $changes++ if $file ne $send_message;
    return $file;
}

# create message file
sub message_create {
    local($file) = @_;
    local($dir) = &dirname($file);

    if (! -d $dir) {
        return 0 unless &mkdirhier($dir);
    }
    if (!open(M, "> $file")) {
        warn "Messagefile ``$file'': $!\n"; return 0;
    }
    print M <<EOF;
#
# Message file for adduser(8)
#   comment: ``#''
#   defaultvariables: \$name, \$fullname
#   other variables:  see /etc/adduser.conf after 
#                     line  ``$do_not_delete''
#

\$fullname,

your account ``\$name'' was created. Have fun!

See also chpass(1), finger(1), passwd(1)
EOF
    close M;
    return 1;
}

# read message file into buffer
sub message_read {
    local($file) = @_;
    @message_buffer = '';

    if (!open(R, "$file")) {
        warn "File ``$file'':$!\n"; return 0;
    }
    while(<R>) {
        push(@message_buffer, $_) unless /^[ \t]*#/;
    }
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

# cp(1)
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

# read config file
sub config_read {
    local($opt) = @_;
    local($ev);     # evaluate variable or list
    local($user_flag) = 0;
       
    return 1 if $opt =~ /-(noconfig|create_conf)/;    # don't read config file

    if ($config_read && -f $config) {
        if(!open(C, "$config")) {
            warn "$config: $!\n"; return 0;
        }
        print "Read config file: $config" if $verbose > 1;
        while(<C>) {
            chop;
            /^$do_not_delete/ && $user_flag++;
            if (/^[ \t]*[a-zA-Z_-]+[ \t]*=/) {
                # prepare for evaluating
                s/#.*$//;
                s/^[ \t]+//;
                s/[ \t;]+$//;
                s/[ \t]*=[ \t]*/=/;
                s/=(.*)/="$1"/ unless /=['"(]/;

                print "$_\n" if $verbose > 1;
                if (/=\(/) {    # perl list
                    $ev = "\@";
                } else {            # perl variable
                    $ev = "\$";
                }
                eval "$ev$_" || warn "Ignore garbage: $ev $_\n";
                push(@user_variable_list, "$_\n") if $user_flag;
            }
        }   
    }
}

# write config file
sub config_write {
    local($silent) = @_;

    if (($changes || ! -e $config || !$config_read) || $silent) {
        if (!$silent) {
            if (-e $config) {
                return 1 if 
                    &confirm_yn("\nWrite your changes to $config?", "no");
            } else {
                return 1 unless
            &confirm_yn("\nWrite your configuration to $config?", "yes");
            }
        }

        local($dir) = &dirname($config);
        if (! -d $dir && !&mkdirhier($dir)) {
            warn "Cannot save your configuration\n";
            return 0;
        }

        if(!open(C, "> $config")) {
            warn "$config: $!\n"; return 0;
        }
        # prepare some variables
        $send_message = "no" unless $send_message;
        $defaultpasswd = "no" unless $defaultpasswd;
        local($shpref) = "'" . join("', '", @shellpref) . "'";
        local($shpath) = "'" . join("', '", @path) . "'";
        local($user_var) = join('', @user_variable_list);

        print C <<EOF;
#
# $config - automatic generated by adduser(8)
#
# Note: adduser read *and* write this file.
#       You may change values, but don't add new things befor the
#       line ``## DO NOT DELETE THIS LINE!''
#

# verbose = [0-2]
verbose = $verbose

# use password for new users
# defaultpasswd =  yes | no
defaultpasswd = $defaultpasswd

# copy dotfiles from this dir ("/usr/share/skel" or "no")
dotdir = "$dotdir"

# send this file to new user ("/etc/adduser.message" or "no")
send_message = "$send_message"

# config file for adduser ("/etc/adduser.conf")
config = "$config"

# logfile ("/var/log/adduser" or "no")
logfile = "$logfile"

# default HOME directory ("/home")
home = "$home"

# List of directories where shells located
# path = ('/bin', '/usr/bin', '/usr/local/bin')
path = ($shpath)

# common shell list, first element has higher priority
# shellpref = ('bash', 'tcsh', 'ksh', 'csh', 'sh')
shellpref = ($shpref)

# defaultshell if not empty ("bash")
defaultshell = "$defaultshell"

# new users get this uid (1000)
uid_start = 1000

$do_not_delete
## your own variables, see /etc/adduser.message
## Warning: this may be a security hole!
$user_var

# end
EOF
        close C;
    }
}

################
# main
#
$test = 0;              # test mode, only for development

# init
#&usage if $ARGV[0] =~ /^--?(help|h|\?)$/; # help if you are not root
&check_root;            # you must be root to run this script!
&variables;             # initialize variables
&config_read(@ARGV);    # read variables form config-file
&parse_arguments(@ARGV);    # parse arguments

# 
&copyright;
&hints;

# check
$changes = 0;
&passwd_check;          # check for valid passwdb
&shells_read;           # read /etc/shells
&passwd_read;           # read /etc/master.passwd
&group_read;            # read /etc/group
&group_check;           # check for incon*

# some questions
&shells_add;                     # maybe add some new shells
$defaultshell = &shell_default;  # enter default shell
$home = &home_partition;         # find HOME partition
$dotdir = &dotdir_default;       # check $dotdir
$send_message = &message_default;   # send message to new user
$defaultpasswd = &password_default; # maybe use password
&config_write(0);                   # write variables in file

# main loop for creating new users
&new_users;             # add new users

#end
