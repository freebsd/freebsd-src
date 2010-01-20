#---------------------------------------------------------------------------
#
#	configure pcvt on system startup example
#	----------------------------------------
#
#	This script can be moved to /usr/local/etc/rc.d to 
#	configure the pcvt driver at system startup time.
#
#	Please adjust the values in the configuration 
#	section below to suit your needs!
#
#---------------------------------------------------------------------------
#
#	last edit-date: [Fri Mar 31 10:40:18 2000]
#
# $FreeBSD$
#
#---------------------------------------------------------------------------

############################################################################
#	configuration section
############################################################################

# path for pcvt's EGA/VGA download fonts
FONTP=/usr/share/misc/pcvtfonts

pcvt_keymap="de"        # keyboard map in /usr/share/misc/keycap.pcvt (or NO).
pcvt_keydel="0"         # key repeat delay, 0-3 (250,500,750,1000 msec) (or NO).
pcvt_keyrate="5"        # keyboard repetition rate 31-0 (2-30 char/sec) (or NO).
pcvt_keyrepeat="ON"     # keyboard repeat ON or OFF (or NO).
pcvt_force24="NO"       # force a 24 line display (when 25 possible) (or NO).
pcvt_hpext="YES"        # use HP extensions (function keys labels) (or NO).
pcvt_lines="28"         # lines (25, 28, 40, 50 or NO).
pcvt_blanktime="60"     # blank time (in seconds) (or NO).
pcvt_cursorh="0"        # cursor top scanline (topmost line is 0) (or NO).
pcvt_cursorl="16"       # cursor low scanline (bottom line is 16) (or NO).
pcvt_monohigh="YES"     # set intensity to high on monochrome monitors (or NO).

############################################################################
#	nothing to configure from here
############################################################################

# check for correct driver and driver version matching

if ispcvt -d /dev/ttyv0 ; then
	echo ""
        echo "configuring pcvt console driver"

# get video adaptor type 

        adaptor=`scon -d /dev/ttyv0 -a`
        echo -n "  video adaptor type is $adaptor, " 

# get monitor type (mono/color)

        monitor=`scon -d /dev/ttyv0 -m`
        echo "monitor type is $monitor"

# load fonts into vga

        if [ $adaptor = VGA ] ; then
                echo -n '  loading fonts: 8x16:0,'
                loadfont -d /dev/ttyv0 -c0 -f $FONTP/vt220l.816
                echo -n '1 '
                loadfont -d /dev/ttyv0 -c1 -f $FONTP/vt220h.816
                echo -n ' 8x14:0,'
                loadfont -d /dev/ttyv0 -c2 -f $FONTP/vt220l.814
                echo -n '1 '
                loadfont -d /dev/ttyv0 -c3 -f $FONTP/vt220h.814
                echo -n ' 8x10:0,'
                loadfont -d /dev/ttyv0 -c4 -f $FONTP/vt220l.810
                echo -n '1 '
                loadfont -d /dev/ttyv0 -c5 -f $FONTP/vt220h.810
                echo -n ' 8x8:0,'
                loadfont -d /dev/ttyv0 -c6 -f $FONTP/vt220l.808
                echo '1 '
                loadfont -d /dev/ttyv0 -c7 -f $FONTP/vt220h.808

# setting screen sizes

                if [ "X${pcvt_lines}" = X"28" ]; then
                        size=-s28
                        echo '  switching to 28 lines'
                elif [ "X${pcvt_lines}" = X"40" ]; then
                        size=-s40
                        echo '  switching to 40 lines'
                elif [ "X${pcvt_lines}" = X"50" ]; then
                        size=-s50
                        echo '  switching to 50 lines'
                else
                        size=-s25
                        echo '  switching to 25 lines'
                fi
        fi

# use HP extensions to VT220 emulation ?
                
        if [ "X${pcvt_hpext}" != X"NO" ] ; then
                emulation=-H
                echo "  setting emulation to VT220 with HP extensions"
        else
                emulation=-V
                echo "  setting emulation to VT220"
        fi

# for all screens do

        for device in /dev/ttyv*
        do
                scon -d$device $size $emulation >/dev/null 2>&1
		if [ $? != 0 ]
		then
			break 1
		fi

                if [ X${pcvt_cursorh} != X"NO" -a X${pcvt_cursorl} != X"NO" ] ; then
                        cursor -d$device -s$pcvt_cursorh -e$pcvt_cursorl
                fi

# if monochrome monitor, set color palette to use a higher intensity

                if [ X${pcvt_monohigh} != X"NO" -a $monitor = MONO -a $adaptor = VGA ] ; then
                        scon -d$device -p8,60,60,60
                fi
        done

# switch to screen 0

        echo "  switching to screen 0"
        scon -d /dev/ttyv0

# set screensaver timeout

        if [ "X${pcvt_blanktime}" != X"NO" ]; then
                echo "  setting screensaver timeout to $pcvt_blanktime seconds"
                scon -d /dev/ttyv0 -t$pcvt_blanktime
        fi

# setup keyboard for national keyboard layout

        if [ "X${pcvt_keymap}" != X"NO" ]; then
                echo "  switching national keyboard layout to $pcvt_keymap"
                kcon -m $pcvt_keymap
        fi

# setup keyboard repeat delay value

        if [ "X${pcvt_keydel}" != X"NO" ]; then
                echo "  setting keyboard delay to $pcvt_keydel"
                kcon -d$pcvt_keydel
        fi

# setup keyboard repeat rate value

        if [ "X${pcvt_keyrate}" != X"NO" ]; then
                echo "  setting keyboard repeat rate to $pcvt_keyrate"
                kcon -r$pcvt_keyrate
        fi

        echo "finished configuring pcvt console driver"
fi

# EOF
