#	dot.login,v 1.6 1994/09/16 04:20:13 rgrimes Exp
#
tset -Q \?$TERM
stty crt erase ^H
umask 2
echo "Don't login as root, use su"
