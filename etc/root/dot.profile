#	$Id: dot.profile,v 1.4 1994/02/21 20:36:03 rgrimes Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin:.
echo 'erase ^?, kill ^U, intr ^C'
stty crt erase  kill  intr 
export PATH
HOME=/root
export HOME
TERM=pc3
export TERM
