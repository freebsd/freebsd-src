#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	dom

.PATH:		${NETSURF_DISTDIR}/libdom
SRCS=		bindings/hubbub/parser.c \
		bindings/xml/expat_xmlparser.c \
		src/core/string.c \
		src/core/node.c \
		src/core/attr.c \
		src/core/characterdata.c \
		src/core/element.c \
		src/core/implementation.c \
		src/core/text.c \
		src/core/typeinfo.c \
		src/core/comment.c \
		src/core/namednodemap.c \
		src/core/nodelist.c \
		src/core/cdatasection.c \
		src/core/document_type.c \
		src/core/entity_ref.c \
		src/core/pi.c \
		src/core/doc_fragment.c \
		src/core/document.c \
		src/events/event.c \
		src/events/dispatch.c \
		src/events/event_target.c \
		src/events/document_event.c \
		src/events/custom_event.c \
		src/events/keyboard_event.c \
		src/events/mouse_wheel_event.c \
		src/events/text_event.c \
		src/events/event_listener.c \
		src/events/mouse_event.c \
		src/events/mutation_event.c \
		src/events/ui_event.c \
		src/events/mouse_multi_wheel_event.c \
		src/events/mutation_name_event.c \
		src/html/html_document.c \
		src/html/html_collection.c \
		src/html/html_options_collection.c \
		src/html/html_element.c \
		src/html/html_html_element.c \
		src/html/html_head_element.c \
		src/html/html_link_element.c \
		src/html/html_title_element.c \
		src/html/html_meta_element.c \
		src/html/html_base_element.c \
		src/html/html_isindex_element.c \
		src/html/html_style_element.c \
		src/html/html_body_element.c \
		src/html/html_form_element.c \
		src/html/html_select_element.c \
		src/html/html_button_element.c \
		src/html/html_input_element.c \
		src/html/html_text_area_element.c \
		src/html/html_opt_group_element.c \
		src/html/html_option_element.c \
		src/html/html_hr_element.c \
		src/utils/namespace.c \
		src/utils/hashtable.c \
		src/utils/character_valid.c \
		src/utils/validate.c

WARNFLAGS=	-Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings \
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations\
		-Wnested-externs -Werror

CFLAGS+=	-I${NETSURF_DISTDIR}/libdom/include \
		-I${NETSURF_DISTDIR}/libdom/src \
		-I${NETSURF_DISTDIR}/libhubbub/include \
		-I${NETSURF_DISTDIR}/libparserutils/include \
		-I${NETSURF_DISTDIR}/libwapcaplet/include \
		-I/usr/local/include \
		-D_LONGLONG -D_BSD_SOURCE -DNDEBUG \
		'-D_ALIGNED=__attribute__((aligned))' $(WARNFLAGS) 

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
