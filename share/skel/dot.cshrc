#	$Id: dot.cshrc,v 1.7 1997/02/22 13:56:32 peter Exp $
#
# .cshrc - csh resource script, read at beginning 
#	   of execution by each shell
#
# see also csh(1), environ(7).
#

alias h		history 25
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lA

setenv	EDITOR	vi
setenv	PAGER	more
setenv	BLOCKSIZE	K

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set filec
	set history = 100
	set savehist = 100
	set mail = (/var/mail/$USER)

	# make mail(1) happy:
	setenv	crt	24
endif
