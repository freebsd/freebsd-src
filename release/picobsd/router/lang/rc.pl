#!/bin/sh
# $Id: rc.pl,v 1.2 1998/08/10 19:17:55 abial Exp $
### Special setup for one floppy PICOBSD ###
ifconfig lo0 inet 127.0.0.1 netmask 0xffffff00
hostname pico
echo ""
echo ""
echo '+----------- PicoBSD 0.4 (ROUTER) -------------+'
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
