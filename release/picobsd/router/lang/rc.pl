#!/bin/sh
# $Id: rc.pl,v 1.2 1998/09/26 17:27:26 abial Exp $
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
echo '|                     abial@freebsd.org        |'
echo '|                                              |'
echo '+----------------------------------------------+'
echo ""
