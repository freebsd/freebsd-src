#!/stand/sh
#
# Written:  November 6th, 1994
# Copyright (C) 1994 by Michael Reifenberger
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).

########################
# First set some globals
startuid=1000;
startgid=1000;
gname=guest
uname=guest
shell="/bin/csh"
needgentry="NO"

. /stand/miscfuncs.sh

#########################
# Some Functions we need.
#
###########################
# Show the User all options
usage() {
message "
adduser -h	Prints help
adduser -i	For interactively adding users

Command line options:
adduser [-u UserName][-g GroupName][-s Shell]"
  exit 1
}
##########################
# Get the next free UserID
getuid() {
local xx=$startuid;
uid=$startuid;
for i in `cut -f 3 -d : /etc/master.passwd | cut -c 2- | sort -n`; do
  if [ $i -lt $xx ]; then
  elif [ $i -eq $xx ]; then xx=`expr $xx + 1`
  else uid=$xx; return 0
  fi
done
}
#######################################################
# Get the next free GroupID or the GroupID of GroupName
getgid() {
local xx=$startgid;
gid=$startgid;
needgentry="YES"
if grep -q \^$gname: /etc/group; then
  gid=`grep \^$gname: /etc/group | cut -f 3 -d:`
  needgentry="NO"
else
  for i in `cut -f 3 -d : /etc/group | cut -c 2- | sort -n`; do
    if [ $i -lt $xx ]; then
    elif [ $i -eq $xx ]; then xx=`expr $xx + 1`
    else gid=$xx; return 0
    fi
  done
fi
}
##########################################
# Ask the User interactively what he wants
interact() {
dialog --title "Add New User Name" --clear \
--inputbox "Please specify a login name for the user:\n\
Hit [return] for a default of <$uname>" -1 -1 2> /tmp/i.$$
ret=$?
case $ret in
  0)
   if [ x`cat /tmp/i.$$` != x ]; then 
     uname=`cat /tmp/i.$$`; fi;;
  1|255)
    exit 1;;
esac
if grep -q \^$uname: /etc/master.passwd; then
  error "Username $uname already exists."
  exit 1
fi
dialog --title "Group Name" --clear \
--inputbox "Which group should $uname belong to?\n\
Hit [return] for default of <$gname>" -1 -1 2> /tmp/i.$$
ret=$?
case $ret in
  0)
   if [ x`cat /tmp/i.$$` != x ]; then
     gname=`cat /tmp/i.$$`; fi;;
  1|255)
    exit 1;;
esac
dialog --title "Login Shell" --clear \
--inputbox "Please specify which login shell\n<$uname> should use\n\
Hit [return] for default of <$shell>" -1 -1 2> /tmp/i.$$
ret=$?
case $ret in
  0)
    if [ x`cat /tmp/i.$$` != x ]; then
      shell=`cat /tmp/i.$$`; fi;;
  1|255)
    exit 1;;
esac
##############
# Remove junk
rm -f /tmp/i.$$
}

#########
# START #
#########

###################################
# Parse the commandline for options
set -- `getopt hiu:g:s: $*`
if [ $? != 0 ]; then
  usage
fi
for i; do
  case "$i"
  in
    -h)
      usage; shift;;
    -i)
      interact; shift; iflag=yes; break;;
    -u)
      uname=$2; shift; shift;;
    -g)
      gname=$2; shift; shift;;
    -s)
      shell=$2; shift; shift;;
    --)
      shift; break;;
#    *)
#     usage; shift;;
  esac
done
#####################
# This is no Edituser
if grep -q \^$uname: /etc/master.passwd; then
  error "This user already exists in the master password file.\n
Use 'chpass' to edit an existing user rather than adduser.."
  exit 1;
fi

###############
# Get Free ID's
getuid;
getgid;
###################
# Only if necessary
if [ $needgentry = "YES" ]; then
  echo "$gname:*:$gid:$uname" >> /etc/group
fi
################
# Make /home BTW
mkdir -p -m755 /home/$uname
if [ ! -d /home/$uname ]; then
  error "Could not create /home/$uname"
  exit 1
else
  for xx in /usr/share/skel/*; do
    cp $xx /home/$uname/.`basename $xx | cut -f 2 -d .`
  done
fi
#####################
# Make the User happy
if [ ! -x $shell ]; then
  message "There is no <$shell> shell, using /bin/sh instead.\n
  If you wish, you can change this choice later with 'chpass'"
  shell="/bin/csh"
elif ! grep -q $shell /etc/shells; then
  echo $shell >> /etc/shells
  echo "<$shell> added to /etc/shells"
fi
echo "$uname:*:$uid:$gid::0:0:User &:/home/$uname:$shell" >> /etc/master.passwd
pwd_mkdb /etc/master.passwd
chown -R $uname.$gname /home/$uname
chmod -R 644 /home/$uname
chmod 755 /home/$uname
passwd $uname
