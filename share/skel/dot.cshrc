#csh .cshrc file

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
	
	set noglob; eval `tset -s`; unset noglob
	stty -istrip
	
	set filec
	set history = 100
	set savehist = 100
	set mail = (/var/mail/$USER)

	# customize prompt - works with tcsh only
	set machine = `hostname -s`
	if ("$TERM" == xterm) then
		alias cwdcmd 'echo -n "]0;${USER}@${machine}: `dirs`"'
		set prompt = "> "
	else
		alias cwdcmd 'set prompt = "${USER}@${machine}:${cwd}> "'
	endif
	cwdcmd

	# fix broken prompt after su
	alias su 'su \!* ; cwdcmd'
endif
