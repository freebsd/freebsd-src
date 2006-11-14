/*
 * prototypes of all externally visible dc functions
 * 
 * Copyright (C) 1994, 1997, 1998 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to:
 *
 *    The Free Software Foundation, Inc.
 *    59 Temple Place, Suite 330
 *    Boston, MA 02111 USA
 */

extern const char *dc_str2charp DC_PROTO((dc_str));
extern const char *dc_system DC_PROTO((const char *));
extern void *dc_malloc DC_PROTO((size_t));
extern struct dc_array *dc_get_stacked_array DC_PROTO((int));

extern void dc_array_set DC_PROTO((int, int, dc_data));
extern void dc_array_free DC_PROTO((struct dc_array *));
extern void dc_array_init DC_PROTO((void));
extern void dc_binop DC_PROTO((int (*)(dc_num, dc_num, int, dc_num *), int));
extern void dc_binop2 DC_PROTO((int (*)(dc_num, dc_num, int,
								dc_num *, dc_num *), int));
extern void dc_triop DC_PROTO((int (*)(dc_num, dc_num, dc_num, int,
								dc_num *), int));
extern void dc_clear_stack DC_PROTO((void));
extern void dc_dump_num(dc_num, dc_discard);
extern void dc_free_num DC_PROTO((dc_num *));
extern void dc_free_str DC_PROTO((dc_str *));
extern void dc_garbage DC_PROTO((const char *, int));
extern void dc_math_init DC_PROTO((void));
extern void dc_memfail DC_PROTO((void));
extern void dc_out_num DC_PROTO((dc_num, int, dc_newline, dc_discard));
extern void dc_out_str DC_PROTO((dc_str, dc_newline, dc_discard));
extern void dc_print DC_PROTO((dc_data, int, dc_newline, dc_discard));
extern void dc_printall DC_PROTO((int));
extern void dc_push DC_PROTO((dc_data));
extern void dc_register_init DC_PROTO((void));
extern void dc_register_push DC_PROTO((int, dc_data));
extern void dc_register_set DC_PROTO((int, dc_data));
extern void dc_set_stacked_array DC_PROTO((int, struct dc_array *));
extern void dc_show_id DC_PROTO((FILE *, int, const char *));
extern void dc_string_init DC_PROTO((void));

extern int  dc_cmpop DC_PROTO((void));
extern int  dc_compare DC_PROTO((dc_num, dc_num));
extern int  dc_evalfile DC_PROTO((FILE *));
extern int  dc_evalstr DC_PROTO((dc_data));
extern int  dc_num2int DC_PROTO((dc_num, dc_discard));
extern int  dc_numlen DC_PROTO((dc_num));
extern int  dc_pop DC_PROTO((dc_data *));
extern int  dc_register_get DC_PROTO((int, dc_data *));
extern int  dc_register_pop DC_PROTO((int, dc_data *));
extern int  dc_tell_length DC_PROTO((dc_data, dc_discard));
extern int  dc_tell_scale DC_PROTO((dc_num, dc_discard));
extern int  dc_tell_stackdepth DC_PROTO((void));
extern int  dc_top_of_stack DC_PROTO((dc_data *));

extern size_t dc_strlen DC_PROTO((dc_str));

extern dc_data dc_array_get DC_PROTO((int, int));
extern dc_data dc_dup DC_PROTO((dc_data));
extern dc_data dc_dup_num DC_PROTO((dc_num));
extern dc_data dc_dup_str DC_PROTO((dc_str));
extern dc_data dc_getnum DC_PROTO((int (*)(void), int, int *));
extern dc_data dc_int2data DC_PROTO((int));
extern dc_data dc_makestring DC_PROTO((const char *, size_t));
extern dc_data dc_readstring DC_PROTO((FILE *, int , int));

extern int dc_add DC_PROTO((dc_num, dc_num, int, dc_num *));
extern int dc_div DC_PROTO((dc_num, dc_num, int, dc_num *));
extern int dc_divrem DC_PROTO((dc_num, dc_num, int, dc_num *, dc_num *));
extern int dc_exp DC_PROTO((dc_num, dc_num, int, dc_num *));
extern int dc_modexp DC_PROTO((dc_num, dc_num, dc_num, int, dc_num *));
extern int dc_mul DC_PROTO((dc_num, dc_num, int, dc_num *));
extern int dc_rem DC_PROTO((dc_num, dc_num, int, dc_num *));
extern int dc_sub DC_PROTO((dc_num, dc_num, int, dc_num *));
extern int dc_sqrt DC_PROTO((dc_num, int, dc_num *));
