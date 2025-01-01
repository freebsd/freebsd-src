/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __DBG_SIG_H__
#define __DBG_SIG_H__
#include <libecc/utils/print_curves.h>
#include <libecc/utils/print_keys.h>
#include <libecc/utils/print_buf.h>

/* Macro to allow inner values of tests vectors print */
#ifdef VERBOSE_INNER_VALUES
#ifndef EC_SIG_ALG
#define EC_SIG_ALG "UNKNOWN_ALG"
#endif

#define dbg_buf_print(msg, ...) do {\
	buf_print(EC_SIG_ALG " " msg,  __VA_ARGS__);\
} while(0)
#define dbg_nn_print(msg, ...) do {\
	nn_print(EC_SIG_ALG " " msg, __VA_ARGS__);\
} while(0)
#define dbg_ec_point_print(msg, ...) do {\
	ec_point_print(EC_SIG_ALG " " msg, __VA_ARGS__);\
} while(0)
#define dbg_ec_montgomery_point_print(msg, ...) do {\
	ec_montgomery_point_print(EC_SIG_ALG " " msg, __VA_ARGS__);\
} while(0)
#define dbg_ec_edwards_point_print(msg, ...) do {\
	ec_edwards_point_print(EC_SIG_ALG " " msg, __VA_ARGS__);\
} while(0)
#define dbg_priv_key_print(msg, ...) do {\
	priv_key_print(EC_SIG_ALG " " msg, __VA_ARGS__);\
} while(0)
#define dbg_pub_key_print(msg, ...) do {\
	pub_key_print(EC_SIG_ALG " " msg, __VA_ARGS__);\
} while(0)

#else /* VERBOSE_INNER_VALUES not defined */

#define dbg_buf_print(msg, ...)
#define dbg_nn_print(msg, ...)
#define dbg_ec_point_print(msg, ...)
#define dbg_ec_montgomery_point_print(msg, ...)
#define dbg_ec_edwards_point_print(msg, ...)
#define dbg_priv_key_print(msg, ...)
#define dbg_pub_key_print(msg, ...)

#endif
#endif /* __DBG_SIG_H__ */
