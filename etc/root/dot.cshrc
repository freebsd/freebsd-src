# $FreeBSD: src/etc/root/dot.cshrc,v 1.22.2.2 1999/08/29 14:19:58 peter Exp $
#
#csh .cshrc file

alias h		history 25
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lA

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
