#	$Id: dot.cshrc,v 1.18.2.3 1998/09/13 23:10:08 brian Exp $
#
#csh .cshrc file

alias h		history 25
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lA

setenv	EDITOR	/usr/bin/ee
setenv	PAGER	more
setenv	BLOCKSIZE	K

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set prompt = "`hostname -s`# "
	set filec
	set history = 100
	set savehist = 100
	set mail = (/var/mail/$USER)

	# make mail(1) happy:
	setenv	crt	24
endif
