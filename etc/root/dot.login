#	$Id: dot.login,v 1.5 1994/06/15 22:58:47 jkh Exp $
#
tset -Q \?$TERM
stty crt erase ^h
umask 2
echo "Don't login as root, use su"
