#!/bin/sh
# $Id: rc.pl,v 1.3 1998/11/01 20:19:31 abial Exp $
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
