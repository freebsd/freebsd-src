#!/stand/sh
#
# xf86311 - extract XFree86 3.1.1 onto a FreeBSD 2.1 system.
#
# Written:  February 2nd, 1995
# Copyright (C) 1995 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# under the terms and conditions stated by the XFree86 Project, Inc.
# copyright, which should also be in the file COPYRIGHT in this distribution.
#
# $Id: extract_xf86311.sh,v 1.3 1995/02/02 14:30:36 jkh Exp $

# Handle the return value from a dialog, doing some pre-processing
# so that each client doesn't have to.
handle_rval()
{ 
        case $1 in
        0)
                return 0
        ;;
        255)
                PS1="subshell# " /stand/sh
        ;;
        *)
                return 1
        ;;
        esac
}

do_selected_install()
{
	for xx in ${SELECTIONS}; do
	   DIST=`eval echo \`echo $xx\``
	   dialog --infobox "Installing XFree86 3.1.1 component: ${DIST}" -1 -1
	   tar --unlink -zxpf ${DIST}.tgz -C /usr
	done
}

do_configure()
{
	if [ -f /usr/X11R6/bin/xf86config ]; then
		dialog --clear
		/usr/X11R6/bin/xf86config
		dialog --clear
	else
		dialog --msgbox "You must first install the X311bin component" -1 -1
	fi
}

do_select_menu()
{
dialog --title "Please select components from XFree86 3.1.1" --checklist \
"Please check off each desired component of the XFree86 distribution\n\
for subsequent unpacking on your system.  Most people will typically need\n\
only one server, with the most reasonable choices already set for you by\n\
default.  When everything looks good, select OK to continue.\n\n\
Server notation: 4 bit = 16 color, 8 bit = 256 color,\n\
16 bit = 16k colors, 24 bit = true color." \
-1 -1 8 \
"X311bin" "client applications and shared libs" ON \
"X311fnts" "the misc and 75 dpi fonts" ON \
"X311lib" "data files needed at runtime" ON \
"X311xicf" "xinit runtime configuration file" ON \
"X311xdcf" "xdm runtime configuration file" ON \
"X3118514" "IBM 8514 and true compatibles - 8 bit" OFF \
"X311AGX"  "AGX boards - 8 bit" OFF \
"X311Ma64" "ATI Mach64 boards - 8/16 bit" OFF \
"X311Ma32" "ATI Mach32 boards - 8/16 bit" OFF \
"X311Ma8" "ATI Mach8 boards - 8bit" OFF \
"X311Mono" "VGA, Super-VGA, Hercules, and others - mono" OFF \
"X311P9K" "Weitek P9000 boards (Diamond Viper) - 8/16/24 bit" OFF \
"X311S3" "S3 boards: #9GXE, ActixGE32, SPEA Mercury - 8/16/24 bit" OFF \
"X311SVGA" "Super-VGA cards - 8 bit" ON \
"X311VG16" "VGA and Super-VGA cards - 4 bit" ON \
"X311W32" "ET4000/W32, /W32i and /W32p cards - 8 bit" OFF \
"X311nest" "A nested server running as a client." OFF \
"X311doc" "READMEs and XFree86 specific man pages" ON \
"X311man" "man pages except XFree86 specific ones in docs" OFF \
"X311f100" "100dpi fonts" OFF \
"X311fscl" "Speedo and Type1 fonts" OFF \
"X311fnon" "Japanese, Chinese and other non-english fonts" OFF \
"X311fsrv" "the font server and it's man page" OFF \
"X311prog" "config, lib*.a and *.h files needed only for compiling" OFF \
"X311link" "X server reconfiguration kit" OFF \
"X311pex"  "PEX fonts and shared libs needed by PEX apps" OFF \
"X311lbx" "low bandwidth X proxy server and libraries." OFF \
      2> ${TMP}/X-selections.$$
	RETVAL=$?
	SELECTIONS=`cat ${TMP}/X-selections.$$`
}

INSTALLING=yes
while [ "${INSTALLING}" = "yes" ]; do
dialog --title "XFree86 3.1.1 Installation" --menu \
"Welcome to the XFree86 3.1.1 installation menu for FreeBSD 2.x\n\n \
Please chose one of the following options.  It is also \n\
recommended that choices be followed in order on this menu,\n\
XFree86 3.1.1 having a pleasantly linear setup and configuration." \
-1 -1 7 \
  "COPYRIGHT" "Read the XFree86 Project, Inc.'s copyright notice" \
  "README" "General README file on XFree86 - recommended" \
  "FreeBSD" "XFree86 information specific to FreeBSD" \
  "Install" "Install selected components of XFree86" \
  "Configure" "Configure a newly installed XFree86" \
  "startx" "Try to run startx and bring X up all the way" \
  "Exit" "Exit the XFree86 installation." \
      2> ${TMP}/menu.tmp.$$
	RETVAL=$?
	ANSWER=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval ${RETVAL}; then continue; fi
   case ${ANSWER} in
   COPYRIGHT) dialog --title "COPYRIGHT NOTICE" --textbox COPYRIGHT 20 78 ;;
   README) dialog --title "README" --textbox README 20 78 ;;
   FreeBSD) dialog --title "XFree86 and FreeBSD" --textbox README.FreeBSD 20 78 ;;
   Install) do_select_menu; do_selected_install ;;
   Configure) do_configure ;;
   startx)
	if [ -x /usr/X11R6/bin/startx ]; then
		/usr/X11R6/bin/startx
	else
		dialog --title "Error" --msgbox "You must first install XFree86." -1 -1
	fi
	;;
   Exit) INSTALLING=no ;;
   esac
done
