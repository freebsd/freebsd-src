\ Example of the file which is automatically loaded by /boot/loader
\ on startup.
\ $FreeBSD: src/share/examples/bootforth/boot.4th,v 1.3.30.1 2008/10/02 02:57:24 kensmith Exp $

\ Load the screen manipulation words

cr .( Loading Forth extensions:)

cr .( - screen.4th...)
s" /boot/screen.4th" O_RDONLY fopen dup fload fclose

\ Load frame support
cr .( - frames.4th...)
s" /boot/frames.4th" O_RDONLY fopen dup fload fclose

\ Load our little menu
cr .( - menu.4th...)
s" /boot/menu.4th" O_RDONLY fopen dup fload fclose

\ Show it
cr
main_menu
