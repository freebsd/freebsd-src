# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed \
		| sed	-e /VGA_DEBUG/d \
			-e /SC_RENDER_DEBUG/d \
			-e /SC_DEBUG_LEVEL/d \
			-e /SC_NO_SUSPEND_VTYSWITCH/d \
			-e /SC_NO_SYSMOUSE/d \
			-e /SC_NO_HISTORY/d \
			-e /SC_NO_FONT_LOADING/d \
			-e /SC_NO_CUTPASTE/d \
			-e /SC_TWOBUTTON_MOUSE/d \
			-e /SC_CUT_SEPCHARS/d \
			-e /SC_CUT_SPACES2TABS/d \
			-e /SC_KERNEL_CONS_REV_ATTR/d \
			-e /SC_KERNEL_CONS_ATTR/d \
			-e /SC_NORM_REV_ATTR/d \
			-e /SC_NORM_ATTR/d \
			-e /SC_PIXEL_MODE/d \
			-e /SC_MOUSE_CHAR/d \
			-e /SC_HISTORY_SIZE/d \
			-e /SC_DISABLE_REBOOT/d \
			-e /SC_DISABLE_DDBKEY/d \
			-e /SC_DFLT_FONT/d \
			-e /SC_ALT_MOUSE_IMAGE/d \
			-e /MAXCONS/d \
			-e /VGA_WIDTH90/d \
			-e /VGA_SLOW_IOACCESS/d \
			-e /VGA_ALT_SEQACCESS/d \
			-e /PSM_RESETAFTERSUSPEND/d \
			-e /PSM_HOOKRESUME/d \
			-e /KBD_DISABLE_KEYMAP_LOAD/d \
			-e /ATKBD_DFLT_KEYMAP/d \
			-e /EXT2FS/d \
		> ${.TARGET}
