# Makefile for libform
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/form
.PATH:	${NCURSES}/include
.PATH:  ${NCURSES}/man

LIB=    form
AWK?=	awk

SRCS=	ncurses_def.h \
	fld_arg.c fld_attr.c fld_current.c fld_def.c fld_dup.c fld_ftchoice.c \
	fld_ftlink.c fld_info.c fld_just.c fld_link.c fld_max.c fld_move.c \
	fld_newftyp.c fld_opts.c fld_pad.c fld_page.c fld_stat.c fld_type.c \
	fld_user.c frm_cursor.c frm_data.c frm_def.c frm_driver.c frm_hook.c \
	frm_opts.c frm_page.c frm_post.c frm_req_name.c frm_scale.c frm_sub.c \
	frm_user.c frm_win.c fty_alnum.c fty_alpha.c fty_enum.c fty_int.c \
	fty_ipv4.c fty_num.c fty_regex.c
INCS=	${NCURSES}/form/form.h

CLEANFILES+=	ncurses_def.h
CFLAGS+= -I. -I${.CURDIR}/../libncurses -I${NCURSES}/form -I${NCURSES}/menu \
	-I${NCURSES}/include -Wall -DNDEBUG -DHAVE_CONFIG_H

ncurses_def.h:	MKncurses_def.sh ncurses_defs
	AWK=${AWK} sh ${NCURSES}/include/MKncurses_def.sh \
	    ${NCURSES}/include/ncurses_defs > ncurses_def.h

MANx=	form.3x form_cursor.3x form_data.3x form_driver.3x \
	form_field.3x form_field_attributes.3x form_field_buffer.3x \
	form_field_info.3x form_field_just.3x form_field_new.3x \
	form_field_opts.3x form_field_userptr.3x \
	form_field_validation.3x form_fieldtype.3x form_hook.3x \
	form_new.3x form_new_page.3x form_opts.3x form_page.3x \
	form_post.3x form_requestname.3x form_userptr.3x form_win.3x

# Generate the MAN list from MANx
.for page in ${MANx}
CLEANFILES+=${page:T:S/x$//g}
MAN+=${page:T:S/x$//g}
${page:T:S/x$//g}: ${page}
	ln -s ${.ALLSRC} ${.TARGET}
.endfor

MLINKS+=form_cursor.3 pos_form_cursor.3
MLINKS+=form_data.3 data_ahead.3 form_data.3 data_behind.3
MLINKS+=form_field.3 field_count.3 form_field.3 form_fields.3 \
	form_field.3 move_field.3 form_field.3 set_form_fields.3
MLINKS+=form_field_attributes.3 field_back.3 \
	form_field_attributes.3 field_fore.3 \
	form_field_attributes.3 field_pad.3 \
	form_field_attributes.3 set_field_back.3 \
	form_field_attributes.3 set_field_fore.3 \
	form_field_attributes.3 set_field_pad.3
MLINKS+=form_field_buffer.3 field_buffer.3 \
	form_field_buffer.3 field_status.3 \
	form_field_buffer.3 set_field_buffer.3 \
	form_field_buffer.3 set_field_status.3 \
	form_field_buffer.3 set_max_field.3
MLINKS+=form_field_info.3 dynamic_fieldinfo.3 form_field_info.3 field_info.3
MLINKS+=form_field_just.3 field_just.3 form_field_just.3 set_field_just.3
MLINKS+=form_field_new.3 dup_field.3 form_field_new.3 free_field.3 \
	form_field_new.3 link_field.3 form_field_new.3 new_field.3
MLINKS+=form_field_opts.3 field_opts.3 form_field_opts.3 field_opts_off.3 \
	form_field_opts.3 field_opts_on.3 form_field_opts.3 set_field_opts.3 \
	form_field_opts.3 set_form_opts.3
MLINKS+=form_field_userptr.3 field_userptr.3 \
	form_field_userptr.3 set_field_userptr.3
MLINKS+=form_field_validation.3 field_arg.3 \
	form_field_validation.3 field_type.3 \
	form_field_validation.3 set_field_type.3
MLINKS+=form_fieldtype.3 link_fieldtype.3 \
	form_fieldtype.3 set_fieldtype_arg.3 \
	form_fieldtype.3 set_fieldtype_choice.3
MLINKS+=form_hook.3 field_init.3 form_hook.3 field_term.3 \
	form_hook.3 form_init.3 form_hook.3 form_term.3 \
	form_hook.3 set_field_init.3 form_hook.3 set_field_term.3 \
	form_hook.3 set_form_init.3 form_hook.3 set_form_term.3
MLINKS+=form_new.3 free_form.3 form_new.3 new_form.3
MLINKS+=form_new_page.3 new_page.3 form_new_page.3 set_new_page.3
MLINKS+=form_opts.3 form_opts_off.3 form_opts.3 form_opts_on.3
MLINKS+=form_page.3 current_field.3 form_page.3 field_index.3 \
	form_page.3 set_current_field.3 form_page.3 set_form_page.3
MLINKS+=form_post.3 post_form.3 form_post.3 unpost_form.3
MLINKS+=form_requestname.3 form_request_by_name.3 \
	form_requestname.3 form_request_name.3
MLINKS+=form_userptr.3 set_form_userptr.3
MLINKS+=form_win.3 form_sub.3 form_win.3 scale_form.3 \
	form_win.3 set_form_sub.3 form_win.3 set_form_win.3

.include <bsd.lib.mk>
