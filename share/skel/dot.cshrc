# .cshrc initialization

alias df	df -k
alias du	du -k
alias f		finger
alias h		'history -r | more'
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lgsA
alias su	su -m
alias tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'
alias x		exit
alias z		suspend

set path = (~/bin /bin /usr/{bin,new,games,local,old} .)

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set filec
	set history = 1000
	set ignoreeof
	set mail = (/var/mail/$USER)
	set mch = `hostname -s`
	set prompt = "$mch:q:$cwd:t {\!} "
	umask 2
endif
