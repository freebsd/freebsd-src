
# X startup file .xsession
#

PATH="/sbin:/bin:/usr/bin:/usr/sbin:/usr/games:/usr/local/bin:/usr/X11R6/bin:$HOME/bin"
MANPATH="/usr/share/man:/usr/X11R6/man:/usr/local/man"
export PATH MANPATH

CPU="FREEBSD"
PATH="$PATH:/usr/local/interviews/bin/$CPU"
MANPATH="$MANPATH:/usr/local/interviews/man"
export CPU

# 8-bit locale (germany)
#LANG=de_DE.ISO_8859-1
#export LANG

# A rightous umask
umask 22

# Useful clients.
xterm -geometry 80x24 -ls &
xclock -geometry -0+0 &
xterm -C -name console -T console -g 80x12+0-0 -fn 6x10 &
xsetroot -solid grey40
exec fvwm
