#csh login file

if ( ! $?TERMCAP ) then
	tset -Q  '-mdialup:?vt100' $TERM
endif

stty	newcrt crterase

set	savehist=100
set	ignoreeof

setenv	EXINIT		'set ai sm noeb'
setenv	HOSTALIASES	 $HOME/.hostaliases

/usr/games/fortune
