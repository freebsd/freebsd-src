#	$Id: dot.profile,v 1.12 1997/07/15 09:37:02 charnier Exp $
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

# # 8-bit locale (English, USA), to read umlauts in vi(1).
# LANG=en_US.ISO_8859-1; export LANG
 
# set ENV to a file invoked each time sh is started for interactive use.
ENV=$HOME/.shrc; export ENV
