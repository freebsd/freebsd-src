#	$Id: dot.login,v 1.6 1994/09/16 04:20:13 rgrimes Exp $
#
tset -Q \?$TERM
stty crt erase ^H
umask 2
if ("$0" != "-su") then
  echo "Don't login as root, login as yourself an use the 'su' command"
endif

