#	$Id: dot.profile,v 1.7.2.3 1997/09/27 15:35:54 joerg Exp $
#
# .profile - Bourne Shell startup script for login shells
#
# see also sh(1), environ(7).
#

# add /usr/games or /usr/X11R6/bin if you want
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin:$HOME/bin; export PATH

# Setting TERM is normally done through /etc/ttys.  Do only override
# if you're sure that you'll never log in via telnet or xterm or a
# serial line.
# Use cons25l1 for iso-* fonts
# TERM=cons25; 	export TERM

BLOCKSIZE=K;	export BLOCKSIZE
EDITOR=vi;   	export EDITOR
PAGER=more;  	export PAGER
# make mail(1) happy:
crt=24;		export crt

# set ENV to a file invoked each time sh is started for interactive use.
ENV=$HOME/.shrc; export ENV
