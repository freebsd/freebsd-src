#	$Id: dot.profile,v 1.6 1996/05/12 14:32:23 wosch Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
export PATH
TERM=cons25
export TERM


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


# # 8-bit locale (English, USA)
# LANG=en_US.ISO_8859-1; export LANG
 

# # set prompt: ``username@hostname$ '' 
# PS1="`whoami`@`hostname | sed 's/\..*//'`"
# case `id -u` in
# 	0) PS1="${PS1}# ";;
# 	*) PS1="${PS1}$ ";;
# esac

