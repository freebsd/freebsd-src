tset -Q \?$TERM
stty crt erase ^\?
umask 2
echo "Don't login as root, use su"
