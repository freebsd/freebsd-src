# $FreeBSD$
#
#csh .cshrc file

alias h		history 25
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lA

set path = (/sbin /bin /usr/sbin /usr/bin /usr/games /usr/local/sbin /usr/local/bin /usr/X11R6/bin $HOME/bin)
setenv MANPATH "/usr/share/man:/usr/X11R6/man:/usr/local/man"

setenv	PAGER	more
setenv	BLOCKSIZE	K

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set prompt = "`hostname -s`# "
	set filec
	set history = 100
	set savehist = 100
	set mail = (/var/mail/$USER)
endif
