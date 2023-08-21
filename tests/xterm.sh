# hacks so we don't have to distribute huge chunks of XTerm

if [ ! -f xterm-really ]
then
	cat xterm.expout
	cat xterm.experr 1>&2
	exit $(cat xterm.exprc)
fi
if [ -f xterm-clean ]
then
	rm xterm.tar.gz xterm-defs.h xterm-main.c
fi

if [ ! -f xterm.tar.gz ]
then
	wget -q http://invisible-island.net/datafiles/release/xterm.tar.gz
fi
if [ ! -f xterm-main.c ]
then
	tar xf xterm.tar.gz
	cd xterm-[0-9][0-9][0-9]
	gcc -I/usr/X11R6/include -I. -E -dM \
		main.c > ../xterm-defs.h
	cat     main.c > ../xterm-main.c
	cd ..
	rm -r xterm-[0-9][0-9][0-9]
fi

unifdef -s xterm-main.c | sed 's/^/#undef /' >xterm-undefs.h
echo $? 1>&2
unifdef -f xterm-undefs.h -f xterm-defs.h xterm-main.c >xterm-out.c
echo $? 1>&2
grep '#' xterm-out.c
echo $? 1>&2
rm -f xterm-undefs.h xterm-out.c
