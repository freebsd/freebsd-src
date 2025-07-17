/* -*- buffer-read-only: t -*- vi: set ro:
 *
 * Prototypes for autoopts
 * Generated Sun Aug 26 10:44:39 PDT 2018
 */
#ifndef AUTOOPTS_PROTO_H_GUARD
#define AUTOOPTS_PROTO_H_GUARD 1


/*
 * Static declarations from alias.c
 */
static tSuccess
too_many_occurrences(tOptions * opts, tOptDesc * od);

/*
 * Static declarations from autoopts.c
 */
static void *
ao_malloc(size_t sz);

static void *
ao_realloc(void *p, size_t sz);

static char *
ao_strdup(char const *str);

static tSuccess
handle_opt(tOptions * opts, tOptState * o_st);

static tSuccess
next_opt(tOptions * opts, tOptState * o_st);

static tSuccess
regular_opts(tOptions * opts);

/*
 * Static declarations from check.c
 */
static bool
has_conflict(tOptions * pOpts, tOptDesc * od);

static bool
occurs_enough(tOptions * pOpts, tOptDesc * pOD);

static bool
is_consistent(tOptions * pOpts);

/*
 * Static declarations from configfile.c
 */
static void
file_preset(tOptions * opts, char const * fname, int dir);

static char *
handle_comment(char * txt);

static char *
handle_cfg(tOptions * opts, tOptState * ost, char * txt, int dir);

static char *
handle_directive(tOptions * opts, char * txt);

static char *
aoflags_directive(tOptions * opts, char * txt);

static char *
program_directive(tOptions * opts, char * txt);

static char *
handle_section(tOptions * opts, char * txt);

static int
parse_xml_encoding(char ** ppz);

static char *
trim_xml_text(char * intxt, char const * pznm, tOptionLoadMode mode);

static void
cook_xml_text(char * pzData);

static char *
handle_struct(tOptions * opts, tOptState * ost, char * txt, int dir);

static void
intern_file_load(tOptions * opts);

static char const *
parse_attrs(tOptions * opts, char const * txt, tOptionLoadMode * pMode,
            tOptionValue * pType);

static char const *
parse_keyword(tOptions * opts, char const * txt, tOptionValue * typ);

static char const *
parse_set_mem(tOptions * opts, char const * txt, tOptionValue * typ);

static char const *
parse_value(char const * txt, tOptionValue * typ);

/*
 * Static declarations from cook.c
 */
static char *
nl_count(char * start, char * end, int * lnct_p);

static bool
contiguous_quote(char ** pps, char * pq, int * lnct_p);

/*
 * Static declarations from enum.c
 */
static void
enum_err(tOptions * pOpts, tOptDesc * pOD,
         char const * const * paz_names, int name_ct);

static uintptr_t
find_name(char const * name, tOptions * pOpts, tOptDesc * pOD,
          char const * const *  paz_names, unsigned int name_ct);

static void
set_memb_shell(tOptions * pOpts, tOptDesc * pOD, char const * const * paz_names,
               unsigned int name_ct);

static void
set_memb_names(tOptions * opts, tOptDesc * od, char const * const * nm_list,
               unsigned int nm_ct);

static uintptr_t
check_membership_start(tOptDesc * od, char const ** argp, bool * invert);

static uintptr_t
find_member_bit(tOptions * opts, tOptDesc * od, char const * pz, int len,
                char const * const * nm_list, unsigned int nm_ct);

/*
 * Static declarations from env.c
 */
static void
doPrognameEnv(tOptions * pOpts, teEnvPresetType type);

static void
do_env_opt(tOptState * os, char * env_name,
            tOptions * pOpts, teEnvPresetType type);

static void
env_presets(tOptions * pOpts, teEnvPresetType type);

/*
 * Static declarations from file.c
 */
static void
check_existence(teOptFileType ftype, tOptions * pOpts, tOptDesc * pOD);

static void
open_file_fd(tOptions * pOpts, tOptDesc * pOD, tuFileMode mode);

static void
fopen_file_fp(tOptions * pOpts, tOptDesc * pOD, tuFileMode mode);

/*
 * Static declarations from find.c
 */
static int
parse_opt(char const ** nm_pp, char ** arg_pp, char * buf, size_t bufsz);

static void
opt_ambiguities(tOptions * opts, char const * name, int nm_len);

static int
opt_match_ct(tOptions * opts, char const * name, int nm_len,
             int * ixp, bool * disable);

static tSuccess
opt_set(tOptions * opts, char * arg, int idx, bool disable, tOptState * st);

static tSuccess
opt_unknown(tOptions * opts, char const * name, char * arg, tOptState * st);

static tSuccess
opt_ambiguous(tOptions * opts, char const * name, int match_ct);

static tSuccess
opt_find_long(tOptions * opts, char const * opt_name, tOptState * state);

static tSuccess
opt_find_short(tOptions * pOpts, uint_t optValue, tOptState * pOptState);

static tSuccess
get_opt_arg_must(tOptions * opts, tOptState * o_st);

static tSuccess
get_opt_arg_may(tOptions * pOpts, tOptState * o_st);

static tSuccess
get_opt_arg_none(tOptions * pOpts, tOptState * o_st);

static tSuccess
get_opt_arg(tOptions * opts, tOptState * o_st);

static tSuccess
find_opt(tOptions * opts, tOptState * o_st);

/*
 * Static declarations from init.c
 */
static tSuccess
validate_struct(tOptions * opts, char const * pname);

static tSuccess
immediate_opts(tOptions * opts);

static tSuccess
do_presets(tOptions * opts);

static bool
ao_initialize(tOptions * opts, int a_ct, char ** a_v);

/*
 * Static declarations from load.c
 */
static bool
get_realpath(char * buf, size_t b_sz);

static bool
add_prog_path(char * buf, int b_sz, char const * fname, char const * prg_path);

static bool
add_env_val(char * buf, int buf_sz, char const * name);

static void
munge_str(char * txt, tOptionLoadMode mode);

static char *
assemble_arg_val(char * txt, tOptionLoadMode mode);

static char *
trim_quotes(char * arg);

static bool
direction_ok(opt_state_mask_t f, int dir);

static void
load_opt_line(tOptions * opts, tOptState * opt_state, char * line,
              tDirection direction, tOptionLoadMode load_mode );

/*
 * Static declarations from makeshell.c
 */
lo_noreturn static void
option_exits(int exit_code);

lo_noreturn static void
ao_bug(char const * msg);

static void
fserr_warn(char const * prog, char const * op, char const * fname);

lo_noreturn static void
fserr_exit(char const * prog, char const * op, char const * fname);

static void
emit_var_text(char const * prog, char const * var, int fdin);

static void
text_to_var(tOptions * opts, teTextTo which, tOptDesc * od);

static void
emit_usage(tOptions * opts);

static void
emit_wrapup(tOptions * opts);

static void
emit_setup(tOptions * opts);

static void
emit_action(tOptions * opts, tOptDesc * od);

static void
emit_inaction(tOptions * opts, tOptDesc * od);

static void
emit_flag(tOptions * opts);

static void
emit_match_expr(char const * name, tOptDesc * cod, tOptions * opts);

static void
emit_long(tOptions * opts);

static char *
load_old_output(char const * fname, char const * pname);

static void
open_out(char const * fname, char const * pname);

/*
 * Static declarations from nested.c
 */
static void
remove_continuation(char * src);

static char const *
scan_q_str(char const * pzTxt);

static tOptionValue *
add_string(void ** pp, char const * name, size_t nm_len,
           char const * val, size_t d_len);

static tOptionValue *
add_bool(void ** pp, char const * name, size_t nm_len,
         char const * val, size_t d_len);

static tOptionValue *
add_number(void ** pp, char const * name, size_t nm_len,
           char const * val, size_t d_len);

static tOptionValue *
add_nested(void ** pp, char const * name, size_t nm_len,
           char * val, size_t d_len);

static char const *
scan_name(char const * name, tOptionValue * res);

static char const *
unnamed_xml(char const * txt);

static char const *
scan_xml_name(char const * name, size_t * nm_len, tOptionValue * val);

static char const *
find_end_xml(char const * src, size_t nm_len, char const * val, size_t * len);

static char const *
scan_xml(char const * xml_name, tOptionValue * res_val);

static void
unload_arg_list(tArgList * arg_list);

static void
sort_list(tArgList * arg_list);

static tOptionValue *
optionLoadNested(char const * text, char const * name, size_t nm_len);

static int
get_special_char(char const ** ppz, int * ct);

static void
emit_special_char(FILE * fp, int ch);

/*
 * Static declarations from parse-duration.c
 */
static unsigned long
str_const_to_ul (cch_t * str, cch_t ** ppz, int base);

static long
str_const_to_l (cch_t * str, cch_t ** ppz, int base);

static time_t
scale_n_add (time_t base, time_t val, int scale);

static time_t
parse_hr_min_sec (time_t start, cch_t * pz);

static time_t
parse_scaled_value (time_t base, cch_t ** ppz, cch_t * endp, int scale);

static time_t
parse_year_month_day (cch_t * pz, cch_t * ps);

static time_t
parse_yearmonthday (cch_t * in_pz);

static time_t
parse_YMWD (cch_t * pz);

static time_t
parse_hour_minute_second (cch_t * pz, cch_t * ps);

static time_t
parse_hourminutesecond (cch_t * in_pz);

static time_t
parse_HMS (cch_t * pz);

static time_t
parse_time (cch_t * pz);

static char *
trim (char * pz);

static time_t
parse_period (cch_t * in_pz);

static time_t
parse_non_iso8601 (cch_t * pz);

/*
 * Static declarations from pgusage.c
 */
static inline FILE *
open_tmp_usage(char ** buf);

static inline char *
mk_pager_cmd(char const * fname);

/*
 * Static declarations from putshell.c
 */
static size_t
string_size(char const * scan, size_t nl_len);

static char const *
print_quoted_apostrophes(char const * str);

static void
print_quot_str(char const * str);

static void
print_enumeration(tOptions * pOpts, tOptDesc * pOD);

static void
print_membership(tOptions * pOpts, tOptDesc * pOD);

static void
print_stacked_arg(tOptions * pOpts, tOptDesc * pOD);

static void
print_reordering(tOptions * opts);

/*
 * Static declarations from reset.c
 */
static void
optionReset(tOptions * pOpts, tOptDesc * pOD);

static void
optionResetEverything(tOptions * pOpts);

/*
 * Static declarations from restore.c
 */
static void
fixupSavedOptionArgs(tOptions * pOpts);

/*
 * Static declarations from save.c
 */
static char const *
find_dir_name(tOptions * opts, int * p_free);

static char const *
find_file_name(tOptions * opts, int * p_free_name);

static void
prt_entry(FILE * fp, tOptDesc * od, char const * l_arg, save_flags_mask_t save_fl);

static void
prt_value(FILE * fp, int depth, tOptDesc * od, tOptionValue const * ovp);

static void
prt_string(FILE * fp, char const * name, char const * pz);

static void
prt_val_list(FILE * fp, char const * name, tArgList * al);

static void
prt_nested(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl);

static void
remove_settings(tOptions * opts, char const * fname);

static FILE *
open_sv_file(tOptions * opts, save_flags_mask_t save_fl);

static void
prt_no_arg_opt(FILE * fp, tOptDesc * vod, tOptDesc * pod, save_flags_mask_t save_fl);

static void
prt_str_arg(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl);

static void
prt_enum_arg(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl);

static void
prt_set_arg(FILE * fp, tOptDesc * od, save_flags_mask_t save_fl);

static void
prt_file_arg(FILE * fp, tOptDesc * od, tOptions * opts, save_flags_mask_t save_fl);

/*
 * Static declarations from sort.c
 */
static tSuccess
must_arg(tOptions * opts, char * arg_txt, tOptState * pOS,
         char ** opt_txt, uint32_t * opt_idx);

static tSuccess
maybe_arg(tOptions * opts, char * arg_txt, tOptState * pOS,
          char ** opt_txt, uint32_t * opt_idx);

static tSuccess
short_opt_ck(tOptions * opts, char * arg_txt, tOptState * pOS,
             char ** opt_txt, uint32_t * opt_idx);

static void
optionSort(tOptions * opts);

/*
 * Static declarations from stack.c
 */
static void
addArgListEntry(void ** ppAL, void * entry);

/*
 * Static declarations from text_mmap.c
 */
static void
load_text_file(tmap_info_t * mapinfo, char const * pzFile);

static void
validate_mmap(char const * fname, int prot, int flags, tmap_info_t * mapinfo);

static void
close_mmap_files(tmap_info_t * mi);

/*
 * Static declarations from tokenize.c
 */
static void
copy_cooked(ch_t ** ppDest, char const ** ppSrc);

static void
copy_raw(ch_t ** ppDest, char const ** ppSrc);

static token_list_t *
alloc_token_list(char const * str);

/*
 * Static declarations from usage.c
 */
static unsigned int
parse_usage_flags(ao_flag_names_t const * fnt, char const * txt);

static void
set_usage_flags(tOptions * opts, char const * flg_txt);

static inline bool
do_gnu_usage(tOptions * pOpts);

static inline bool
skip_misuse_usage(tOptions * pOpts);

static void
print_offer_usage(tOptions * opts);

static void
print_usage_details(tOptions * opts, int exit_code);

static void
print_one_paragraph(char const * text, bool plain, FILE * fp);

static void
prt_conflicts(tOptions * opts, tOptDesc * od);

static void
prt_one_vendor(tOptions *    opts,  tOptDesc *   od,
               arg_types_t * argtp, char const * usefmt);

static void
prt_vendor_opts(tOptions * opts, char const * title);

static void
prt_extd_usage(tOptions * opts, tOptDesc * od, char const * title);

static void
prt_ini_list(char const * const * papz, char const * ini_file,
             char const * path_nm);

static void
prt_preamble(tOptions * opts, tOptDesc * od, arg_types_t * at);

static void
prt_one_usage(tOptions * opts, tOptDesc * od, arg_types_t * at);

static void
prt_opt_usage(tOptions * opts, int ex_code, char const * title);

static void
prt_prog_detail(tOptions * opts);

static int
setGnuOptFmts(tOptions * opts, char const ** ptxt);

static int
setStdOptFmts(tOptions * opts, char const ** ptxt);

/*
 * Static declarations from version.c
 */
static void
emit_first_line(
    FILE * fp, char const * alt1, char const * alt2, char const * alt3);

static void
emit_simple_ver(tOptions * o, FILE * fp);

static void
emit_copy_full(tOptions * o, FILE * fp);

static void
emit_copy_note(tOptions * opts, FILE * fp);

static void
print_ver(tOptions * opts, tOptDesc * od, FILE * fp, bool call_exit);

#endif /* AUTOOPTS_PROTO_H_GUARD */
