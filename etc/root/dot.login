#	$Id: dot.login,v 1.4 1994/02/21 20:36:02 rgrimes Exp $
#
tset -Q \?$TERM
stty crt erase ^\?
umask 2
echo "Don't login as root, use su"
