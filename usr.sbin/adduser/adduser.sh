#!/usr/bin/perl



#  Copyright (c) 1994 GB Data Systems
#  All rights reserved.
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the Author may not be used to endorse or promote products 
#     derived from this software without specific prior written permission.
#  THIS SOFTWARE IS PROVIDED BY GB DATA AND CONTRIBUTORS ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL GB DATA OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF    
#  SUCH DAMAGE.

#
# $Id: adduser,v 1.4 1994/12/28 17:27:21 gclarkii Exp $
#

$configfile = "\/etc\/adduser.conf";

if (-f $configfile) {
 open (CONFIG, "$configfile");
  while (<CONFIG>) {
    eval "$_";
  }
}

open (WHOAMI, "whoami|");

while (<WHOAMI>) {
$whoami = $_;
}
chop $whoami;

if ($whoami ne "root") {
system "clear";
print "\n\nYou must be root to add an user\n\n";
close WHOAMI;
exit;
}
close WHOAMI;

# Start getting information and print a banner

print "                                Adduser\n";
print "             A system utility for adding users with defaults\n";
print "\n\n";

#
# User ID
#


print "Please enter the login name of the user: ";
chop ($userlogin = <STDIN>);


sub subuid {
$userid = "";
print "Please enter the user id or hit enter for the next id: ";
chop ($userid = <STDIN>);
}

while (!$userid) {
&subuid;
if (!$userid) {
  if ($useautoids) { 
     open (USERID, "+<$userids");
     chop ($xxuserid = <USERID>);
     $userid = $xxuserid + 1;
     close USERID;
     open (USERID, "+>$userids");
     print (USERID "$userid\n");
     close USERID;
   } else { &subuid; }
}
}

#
# Group ID
#

sub groupids {
print "Please enter the group id or hit enter for the default id: ";
chop ($groupid = <STDIN>);
}

&groupids;

while (!$groupid) {
  if ($defgroupid) {
    if (!$groupid) {
     $groupid = "$defgroupid";
    } else { &groupids; } 
  } else { &groupids; }
}

#
#  User name
#

print "Please enter the user's name: ";
chop ($username = <STDIN>);

#
# Home directory
#

print "Please enter the users home directory or hit enter for default: ";
chop ($userdir = <STDIN>);

if (!$userdir) {
	$userdir = "$defusrdir\/$userlogin";
	print "$userdir\n";
}

#
# Login Shell
#

print "Please enter the users login shell or hit enter for default: ";
chop ($usershell = <STDIN>);

if (!$usershell) {
	$usershell = "$userdefshell";
	print "$usershell\n";
}

#
# Create password file entry
#

print "Opening and locking passwd file in blocking mode.\n";
open (PASS, '>>/etc/master.passwd');
flock (PASS, 2) || die "Can't lock passwd file, must be in use!!\n";
print (PASS "$userlogin::$userid:$groupid::0:0:$username,,,:$userdir:$usershell\n");
print "Unlocking and closing password file\n";
flock (PASS,8);
close PASS;
print "Re-indexing password databases\n";
system 'pwd_mkdb -p /etc/master.passwd';
system "passwd $userlogin";

#
# Create user directory
#
print "Creating user directory\n";
if (! -e $defusrdir) {
   system "mkdir -p $defusrdir\/$userdir"; 
} else {
   system "mkdir $userdir";
}


print "Copying user shell files\n";
system "cp $skel_location\/dot.login $userdir\/\.login";
system "cp $skel_location\/dot.profile $userdir\/\.profile";

if ($usershell eq "\/bin\/csh" || $usershell eq "\/usr\/local\/bin\/tcsh")
   {
   system "cp $skel_location\/dot.cshrc $userdir\/.cshrc";
   }
system  "chmod -R 664 $userdir";
system  "chown -R $userid.$groupid $userdir";



#
# Print out information used in creation of this account
#
print "\n\n";
print "Information used to create this account follows.\n";
print "\n";
print "Login Name:      $userlogin\n";
print "UserId:          $userid\n";
print "GroupId:         $groupid\n";
print "UserName:        $username\n";
print "HomeDir:         $userdir\n";
print "Shell:           $usershell\n";
print "\nDONE\n\n";

