#!/bin/sh
# $Id: rc.pl,v 1.1.1.1 1998/08/27 17:38:44 abial Exp $
### Special setup for one floppy PICOBSD ###
ifconfig lo0 inet 127.0.0.1 netmask 0xffffff00
hostname pico
echo ""
echo ""
echo '+----------- PicoBSD @VER@ (ROUTER) ------------+'
echo '|                                              |'
echo '| Ta wersja PicoBSD podlega w pelni licencji   |'
echo '| BSD. Wiecej informacji mozna znalezc na      |'
echo '| http://www.freebsd.org/~picobsd, lub u       |'
echo '| autora.                                      |'
echo '|                                              |'
echo '|                     abial@nask.pl            |'
echo '|                                              |'
echo '+----------------------------------------------+'
echo ""
