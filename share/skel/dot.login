#csh .login file

set noglob
eval `tset -s -m 'network:?xterm'`
unset noglob
stty status '^T' crt -tostop

if (-x /usr/games/fortune) /usr/games/fortune
