#	$FreeBSD$
#
# .profile - Bourne Shell startup script for login shells
#
# see also sh(1), environ(7).
#

# add /usr/games or /usr/X11R6/bin if you want
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin:$HOME/bin; export PATH

# use cons25l1 for iso-* fonts
TERM=cons25; 	export TERM

BLOCKSIZE=K;	export BLOCKSIZE
EDITOR=vi;   	export EDITOR
PAGER=more;  	export PAGER

# file permissions: rwxr-xr-x
#
# umask	022

# Uncomment next line to enable the builtin emacs(1) command line editor
# in sh(1), e.g. C-a -> beginning-of-line.
# set -o emacs


# # some useful aliases
# alias h='fc -l'
# alias j=jobs
# alias m=$PAGER
# alias ll='ls -lagFo'
# alias g='egrep -i'
 
# # be paranoid
# alias cp='cp -ip'
# alias mv='mv -i'
# alias rm='rm -i'


# # 8-bit locale (English, USA), to read umlauts in vi(1).
# LANG=en_US.ISO_8859-1; export LANG
 

# # set prompt: ``username@hostname$ '' 
# PS1="`whoami`@`hostname | sed 's/\..*//'`"
# case `id -u` in
# 	0) PS1="${PS1}# ";;
# 	*) PS1="${PS1}$ ";;
# esac
