#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../contrib/netsurf

PROG= nsfb

MAN=

CWARNFLAGS+=	-Wno-cast-qual


.if defined(NETSURF_X_SURFACE)
LDX=		-lxcb-icccm -lxcb-image -lxcb-shm -lxcb-keysyms -lxcb-util -lxcb
.else
LDX=
.endif

NSLIBS=	css dom hubbub nsbmp nsgif parserutils rosprite svgtiny wapcaplet
.for _LIB in ${NSLIBS}
LIB${_LIB:tu}=	${.OBJDIR}/lib${_LIB}/lib${_LIB}.a
LLVM_IR_FILE_${_LIB:tu}=	${.OBJDIR}/lib${_LIB}/lib${_LIB}.${LLVM_IR_TYPE}-a
LLVM_NATIVE_FILE_${_LIB:tu}=	${.OBJDIR}/lib${_LIB}/lib${_LIB}.${LLVM_IR_TYPE}-a
.endfor
LIBADD+=	${NSLIBS}

LDADD+=		\
		${NSLIBS:S;^;-L${.CURDIR}/lib;} \
		-ljpeg -lz -lpng -lm \
		-Wl,--whole-archive -lnsfb ${LDX} -Wl,--no-whole-archive \
		-lcurl -lbsdxml -lssl -lcrypto

.PATH:          ${.CURDIR}/gensrc
SRCS=		\
		caret_image.c \
		hand_image.c \
		history_image.c \
		history_image_g.c \
		left_arrow.c \
		left_arrow_g.c \
		menu_image.c \
		move_image.c \
		osk_image.c \
		pointer_image.c \
		progress_image.c \
		reload.c \
		reload_g.c \
		right_arrow.c \
		right_arrow_g.c \
		scrolld.c \
		scrolll.c \
		scrollr.c \
		scrollu.c \
		stop_image.c \
		stop_image_g.c \
		throbber0.c \
		throbber1.c \
		throbber2.c \
		throbber3.c \
		throbber4.c \
		throbber5.c \
		throbber6.c \
		throbber7.c \
		throbber8.c 

.PATH:          ${NETSURF_DISTDIR}/netsurf

SRCS+=		\
		content/content.c \
		content/content_factory.c \
		content/dirlist.c \
		content/fetch.c \
		content/hlcache.c \
		content/llcache.c \
		content/mimesniff.c \
		content/urldb.c \
		content/fetchers/about.c \
		content/fetchers/curl.c \
		content/fetchers/data.c \
		content/fetchers/file.c \
		content/fetchers/resource.c

SRCS+=		\
		css/css.c \
		css/dump.c \
		css/internal.c \
		css/select.c \
		css/utils.c

SRCS+=		\
		desktop/browser.c \
		desktop/browser_history.c \
		desktop/cookie_manager.c \
		desktop/download.c \
		desktop/frames.c \
		desktop/global_history.c \
		desktop/gui_factory.c \
		desktop/hotlist.c \
		desktop/knockout.c \
		desktop/mouse.c \
		desktop/netsurf.c \
		desktop/plot_style.c \
		desktop/print.c \
		desktop/save_complete.c \
		desktop/save_text.c \
		desktop/scrollbar.c \
		desktop/search.c \
		desktop/searchweb.c \
		desktop/selection.c \
		desktop/sslcert_viewer.c \
		desktop/system_colour.c \
		desktop/textarea.c \
		desktop/textinput.c \
		desktop/thumbnail.c \
		desktop/tree.c \
		desktop/treeview.c \
		desktop/version.c

SRCS+=		\
		framebuffer/bitmap.c \
		framebuffer/clipboard.c \
		framebuffer/fetch.c \
		framebuffer/findfile.c \
		framebuffer/font_internal.c \
		framebuffer/framebuffer.c \
		framebuffer/thumbnail.c \
		framebuffer/gui.c \
		framebuffer/localhistory.c \
		framebuffer/misc.c \
		framebuffer/nsfont_bold.c \
		framebuffer/nsfont_italic.c \
		framebuffer/nsfont_italic_bold.c \
		framebuffer/nsfont_regular.c \
		framebuffer/schedule.c \
		framebuffer/fbtk/bitmap.c \
		framebuffer/fbtk/event.c \
		framebuffer/fbtk/fbtk.c \
		framebuffer/fbtk/fill.c \
		framebuffer/fbtk/osk.c \
		framebuffer/fbtk/scroll.c \
		framebuffer/fbtk/text.c \
		framebuffer/fbtk/user.c \
		framebuffer/fbtk/window.c

SRCS+=		\
		image/bmp.c \
		image/gif.c \
		image/ico.c \
		image/image.c \
		image/image_cache.c \
		image/jpeg.c \
		image/nssprite.c \
		image/png.c

SRCS+=		\
		javascript/none.c

SRCS+=		\
		render/box.c \
		render/box_construct.c \
		render/box_normalise.c \
		render/box_textarea.c \
		render/font.c \
		render/form.c \
		render/html.c \
		render/html_css.c \
		render/html_css_fetcher.c \
		render/html_forms.c \
		render/html_interaction.c \
		render/html_object.c \
		render/html_redraw.c \
		render/html_script.c \
		render/imagemap.c \
		render/layout.c \
		render/list.c \
		render/search.c \
		render/table.c \
		render/textplain.c

SRCS+=		\
		utils/base64.c \
		utils/bloom.c \
		utils/corestrings.c \
		utils/filename.c \
		utils/filepath.c \
		utils/hashtable.c \
		utils/libdom.c \
		utils/locale.c \
		utils/log.c \
		utils/messages.c \
		utils/nsoption.c \
		utils/nsurl.c \
		utils/talloc.c \
		utils/url.c \
		utils/useragent.c \
		utils/utils.c \
		utils/utf8.c \
		utils/http/challenge.c \
		utils/http/content-disposition.c \
		utils/http/content-type.c \
		utils/http/generics.c \
		utils/http/parameter.c \
		utils/http/primitives.c \
		utils/http/www-authenticate.c

WARNS=1
#WARNFLAGS=	-W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings \
#		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations\
#		-Wredundant-decls -Wnested-externs -Wuninitialized \
#		-Wno-unused-parameter 

INCLUDES=	-I${NETSURF_DISTDIR}/libcss/include \
		-I${NETSURF_DISTDIR}/libdom/include \
		-I${NETSURF_DISTDIR}/libhubbub/include \
		-I${NETSURF_DISTDIR}/libnsbmp/include \
		-I${NETSURF_DISTDIR}/libnsfb/include \
		-I${NETSURF_DISTDIR}/libnsgif/include \
		-I${NETSURF_DISTDIR}/libparserutils/include \
		-I${NETSURF_DISTDIR}/librosprite/include \
		-I${NETSURF_DISTDIR}/libsvgtiny/include \
		-I${NETSURF_DISTDIR}/libwapcaplet/include \
		-I${.CURDIR}/../../lib/netsurf/libdom \
		-I${NETSURF_DISTDIR}/netsurf \
		-I${.CURDIR}/include

OPTIONS=	-Dnsframebuffer -DWITH_JPEG -DWITH_PNG -DWITH_BMP -DWITH_GIF \
		'-DNETSURF_UA_FORMAT_STRING="NetSurf/%d.%d (%s)"' \
		'-DNETSURF_HOMEPAGE="about:welcome"' \
		'-DNETSURF_FB_RESPATH="${HOME}/.netsurf/:${NETSURFRES}:/usr/local/share/netsurf/:./framebuffer/res"'
		
TTFONTS=	'-DNETSURF_FB_FONTPATH="/usr/share/fonts/truetype/ttf-dejavu:/usr/share/fonts/truetype/msttcorefonts"' \
		'-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' \
		'-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' \
		'-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans-Oblique.ttf"' \
		'-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-BoldOblique.ttf"' \
		'-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' \
		'-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' \
		'-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' \
		'-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' \
		'-DNETSURF_FB_FONT_CURSIVE="Comic_Sans_MS.ttf"' \
		'-DNETSURF_FB_FONT_FANTASY="Impact.ttf"'

CFLAGS+=	-D_LONGLONG -Dsmall -DLIBICONV_PLUG \
		$(OPTIONS) $(TTFONTS) $(INCLUDES) $(WARNFLAGS)

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS:=	${CFLAGS:S/-O/-O2/}

.c.o:
	${CC} ${STATIC_CFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
