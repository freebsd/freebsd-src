/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SBIN_IPFW_DIFFUSE_UI_H_
#define _SBIN_IPFW_DIFFUSE_UI_H_

#define	TOK_DI_FEATURE_MOD_START 256
/* Tokens used by the basic diffuse rules. */
enum diffuse_tokens {
	/* TOK_NULL = 0, in ipfw2.h */
	TOK_DI_MODULE = 1,
	TOK_DI_ALGORITHM,
	TOK_DI_USE_FEATURE_STATS,
	TOK_DI_CLASS_NAMES,
	TOK_DI_EXPIRED,
	TOK_DI_EXP_TARGET,
	TOK_DI_EXP_LIMIT,
	TOK_DI_EXP_FLOW_LIMIT,
	TOK_DI_CONFIRM,
	TOK_DI_EXP_FLOW_KEY,
	TOK_DI_EXP_FEATURES,
	TOK_DI_EXP_ACTION,
	TOK_DI_EXP_ACTION_PARAMS,
	TOK_DI_EXP_MIN_BATCH,
	TOK_DI_EXP_MAX_BATCH,
	TOK_DI_EXP_MAX_DELAY,
	TOK_DI_EXP_ACTION_UNIDIR,
	TOK_DI_OPTS_INIT /* Used for initialising module options. */
};

/* Argument types. */
enum diffuse_option_arg_types {
	DI_OPTION_ARG_NOARG = 0,
	DI_OPTION_ARG_STR,
	DI_OPTION_ARG_CHAR,
	DI_OPTION_ARG_INT,
	DI_OPTION_ARG_UINT,
	DI_OPTION_ARG_DOUBLE
};

/* Definition of an option. */
struct di_option {
	char	*name;
	int	arg_type; /* diffuse_option_arg_types. */
	double	arg_min;
	double	arg_max;
	int	token;
};

/*
 * Load a model.
 * param1: pointer to config buffer
 * param2: pointer to model loaded
 * param3: pointer to feature name string
 * param4: pointer to class name string
 * return: model size, -ve if error
 */
typedef int (*load_model_fn_t)(struct di_oid *, char **, char **, char **);

/*
 * External init function.
 * param1: pointer to pointer to options
 * return: size of returned options in byte
 */
typedef int (*get_options_fn_t)(struct di_option **opts);

/*
 * External parse function.
 * param1: token
 * param2: arg value (can be NULL)
 * param3: pointer to record buffer
 * return: 0 if parsed successfully, non-zero otherwise
 */
typedef int (*parse_opts_fn_t)(int, char *, struct di_oid *);

/*
 * Returns size of config record.
 */
typedef int (*get_config_size_fn_t)(void);

/*
 * Prints options.
 * param1: pointer to option record
 */
typedef void (*print_opts_fn_t)(struct di_oid *);

/*
 * Print usage.
 */
typedef void (*print_usage_fn_t)(void);

/*
 * Get stat name.
 * param1: stat number
 * return: stat name
 */
typedef char * (*get_stat_name_fn_t)(int);

/* For listing flow table. */
struct di_feature_arr_entry {
	char name [DI_MAX_NAME_STR_LEN];
	char mod_name [DI_MAX_NAME_STR_LEN];
};

struct di_feature_module {
	char			name[DI_MAX_NAME_STR_LEN];
	int			type;
	get_config_size_fn_t	get_conf_size;
	get_options_fn_t	get_opts;
	parse_opts_fn_t		parse_opts;
	print_opts_fn_t		print_opts;
	print_usage_fn_t	print_usage;
	get_stat_name_fn_t	get_stat_name;
	SLIST_ENTRY(di_feature_module) next;
};

struct di_classifier_module {
	char			name[DI_MAX_NAME_STR_LEN];
	get_config_size_fn_t	get_conf_size;
	get_options_fn_t	get_opts;
	parse_opts_fn_t		parse_opts;
	print_opts_fn_t		print_opts;
	print_usage_fn_t	print_usage;
	load_model_fn_t		load_model;
	SLIST_ENTRY(di_classifier_module) next;
};

/* Called from main.c to initialise DIFFUSE. */
void diffuse_init();

/* User command handling. */
void diffuse_config(int ac, char **av, int type);
void diffuse_show(int ac, char **av, int type, int counters);
void diffuse_list(int ac, char *av[], int type, int show_counters);
void diffuse_flush(int ac, char *av[], int reset_counters_only);
void diffuse_delete(int ac, char *av[], int type);

/* Called by ipfw for unknown opcodes. */
int diffuse_show_cmd(ipfw_insn *cmd);

/* Parse methods for rule extensions. */
int diffuse_parse_action(int token, ipfw_insn *action, char **av[]);
int diffuse_parse_cmd(int token, int open_par, ipfw_insn **cmd, char **av[]);

/* Called when building the rule. */
void diffuse_rule_build_1(uint32_t cmdbuf[], ipfw_insn *cmd, ipfw_insn **dst);
void diffuse_rule_build_2(uint32_t cmdbuf[], ipfw_insn *cmd, ipfw_insn **dst);

/* diffuse_modules.c */
void diffuse_modules_init(void);
void diffuse_classifier_modules_init(void);
struct di_classifier_module * find_classifier_module(const char *name);
void print_classifier_modules(void);
void diffuse_feature_modules_init(void);
struct di_feature_module * find_feature_module(const char *name);
void print_feature_modules(void);

#endif /* _SBIN_IPFW_DIFFUSE_UI_H_ */
