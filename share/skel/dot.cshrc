#csh .cshrc file

alias h		history 25
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lA
alias su	su -m

setenv	EDITOR	vi
setenv	EXINIT	'set autoindent'
setenv	PAGER	more

set path = (~/bin /bin /usr/{bin,X11/bin,contrib/bin,games,old/bin} /usr/local/bin)

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set filec
	set history = 1000
	set ignoreeof
	set mail = (/var/mail/$USER)
	set mch = `hostname -s`
	set prompt = "${mch:q}: {\!} "
	umask 2
endif
