to install an Amiga cross-compiler in a Linux distribution, there are instructions at

http://utilitybase.com/article/show/2007/06/23/231/Installing+an+AmigaOS+4+cross+compiler

a more Mac-oriented article [though of potentially general utility] is at
http://utilitybase.com/article/show/2006/05/21/188/Building+Amiga+OS+4+GCC+Cross+Compiler+for+UNIX%252FMAC

more background at
http://cross.zerohero.se/os4.html

cross-compile additional libs/tools
SDK
http://www.hyperion-entertainment.biz/

newlib
http://sources.redhat.com/newlib/

clib2
http://sourceforge.net/projects/clib2/

ixemul
http://strohmayer.org/sfs/

libnix
http://sourceforge.net/projects/libnix/

though newlib / clib2 are apparently already included in the ppc-amigaos-gcc tarball

lha utility is debian package lha

then install linked libs in the correct place

[normally /usr/local/amiga]
so
sudo chmod --recursive 775 /usr/local/amiga
sudo chmod --recursive +s /usr/local/amiga
sudo chown --recursive `whoami` /usr/local/amiga
sudo chgrp --recursive root /usr/local/amiga
[mkdir /usr/local/amiga/include]

[may need to set ppc-amigaos-gcc libpaths]

zlib
download tarball from project homepage, untar in a storage directory /
download source from your distribution's repository [zlib1g in Ubuntu]
[cd to top-level directory of zlib containing configure script]
CC=ppc-amigaos-gcc AR=ppc-amigaos-ar RANLIB=ppc-amigaos-ranlib \
CFLAGS="-DNO_FSEEKO" ./configure --prefix=/usr/local/amiga
make
make install

regex [pre-compiled]
http://aminet.net/dev/lib/libregex-4.4.3.lha

libcurl
download the tarball from the project's homepage, untar in a storage directory /
download source from your distribution's repository
cd into the directory containing the configure file
./configure --prefix=/usr/local/amiga --host=ppc-amigaos
$ make
[you MUST have either POSIX or glibc strerror_r if strerror_r is found]
$ make install

alternative
http://www.aminet.net/dev/lib/libcurl.lha

libiconv [unnecessary as a non-overridable limited version is included in newlib]

openssl

libpng

liblcms
http://www.aminet.net/dev/lib/liblcms_so.lha
http://www.aminet.net/dev/lib/liblcms_so.lha

libjpeg

libparserutils
libhubbub
libcss
libnsbmp
libnsgif
