/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* %#pragma ident	"@(#)iprop.x	1.2	04/02/20 SMI" */

/*
 * Main source:
 * lib/kdb/iprop.x
 *
 * Generated files:
 * lib/kdb/iprop_xdr.c
 * include/iprop.h
 * slave/kpropd_rpc.c (clnt)
 *
 * Derived files:
 * kadmin/server/ipropd_svc.c
 */

/*
 * This file gets fed through the preprocessor to handle RPC_*
 * symbols, but we don't want it to chew on __GNUC__ in this phase.
 */
#undef __GNUC__

#ifdef RPC_XDR
/*
 * Sloppy rpcgen code declares "buf" and rarely uses it.  As it's
 * generated code, and not presented to code building against the
 * Kerberos code, it's not a problem we need to fix, so suppress the
 * complaint.
 */
%#ifdef __GNUC__
%#pragma GCC diagnostic ignored "-Wunused-variable"
%#endif
#endif /* RPC_XDR */

/*
 * Initial declarations
 */

#ifndef RPC_HDR
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
/*typedef hyper int64_t;*/
/*typedef unsigned hyper uint64_t;*/
#endif  /* !RPC_HDR */

typedef opaque	 utf8str_t<>;

/*
 * Transaction log serial no.
 */
typedef uint32_t	kdb_sno_t;

/* Timestamp */
struct kdbe_time_t {
	uint32_t	seconds;
	uint32_t	useconds;
};

/* Key Data */
struct kdbe_key_t {
	int32_t		k_ver;	/* Version */
	int32_t		k_kvno;	/* Key version no. */
	int32_t		k_enctype<>;
	utf8str_t	k_contents<>;
};

/* Content data */
struct kdbe_data_t {
	int32_t		k_magic;
	utf8str_t	k_data;
};

/* Principal Data */
struct kdbe_princ_t {
	utf8str_t	k_realm;
	kdbe_data_t	k_components<>;
	int32_t		k_nametype;
};

/* TL data (pre-auth specific data) */
struct kdbe_tl_t {
	int16_t		tl_type;
	opaque		tl_data<>;
};

/* Structure to store pwd history */
typedef kdbe_key_t kdbe_pw_hist_t<>;

/* Basic KDB entry attributes */
enum kdbe_attr_type_t {
	AT_ATTRFLAGS = 0,
	AT_MAX_LIFE = 1,
	AT_MAX_RENEW_LIFE = 2,
	AT_EXP = 3,
	AT_PW_EXP = 4,
	AT_LAST_SUCCESS = 5,
	AT_LAST_FAILED = 6,
	AT_FAIL_AUTH_COUNT = 7,
	AT_PRINC = 8,
	AT_KEYDATA = 9,
	AT_TL_DATA = 10,
	AT_LEN = 11,
	AT_MOD_PRINC = 12,
	AT_MOD_TIME = 13,
	AT_MOD_WHERE = 14,
	AT_PW_LAST_CHANGE = 15,
	AT_PW_POLICY = 16,
	AT_PW_POLICY_SWITCH = 17,
	AT_PW_HIST_KVNO = 18,
	AT_PW_HIST = 19
};

/* KDB entry, Attribute=value */
union kdbe_val_t switch (kdbe_attr_type_t av_type) {
case AT_ATTRFLAGS:
	uint32_t	av_attrflags;
case AT_MAX_LIFE:
	uint32_t	av_max_life;
case AT_MAX_RENEW_LIFE:
	uint32_t	av_max_renew_life;
case AT_EXP:
	uint32_t	av_exp;
case AT_PW_EXP:
	uint32_t	av_pw_exp;
case AT_LAST_SUCCESS:
	uint32_t	av_last_success;
case AT_LAST_FAILED:
	uint32_t	av_last_failed;
case AT_FAIL_AUTH_COUNT:
	uint32_t	av_fail_auth_count;
case AT_PRINC:
	kdbe_princ_t	av_princ;
case AT_KEYDATA:
	kdbe_key_t	av_keydata<>;	/* array of keys */
case AT_TL_DATA:
	kdbe_tl_t	av_tldata<>;	/* array of TL data */
case AT_LEN:
	int16_t		av_len;
case AT_PW_LAST_CHANGE:
	uint32_t	av_pw_last_change;
case AT_MOD_PRINC:
	kdbe_princ_t	av_mod_princ;
case AT_MOD_TIME:
	uint32_t	av_mod_time;
case AT_MOD_WHERE:
	utf8str_t	av_mod_where;
case AT_PW_POLICY:
	utf8str_t	av_pw_policy;
case AT_PW_POLICY_SWITCH:
	bool		av_pw_policy_switch;
case AT_PW_HIST_KVNO:
	uint32_t	av_pw_hist_kvno;
case AT_PW_HIST:
	kdbe_pw_hist_t	av_pw_hist<>;	/* array of pw history */
default:
	opaque		av_extension<>;	/* futures */
};

typedef kdbe_val_t kdbe_t<>;	    /* Array of attr/val makes a KDB entry */

/*
 * Incremental update
 */
struct kdb_incr_update_t {
	utf8str_t	kdb_princ_name;	/* Principal name */
	kdb_sno_t	kdb_entry_sno;	/* Serial # of entry */
	kdbe_time_t	kdb_time;	/* Timestamp of update */
	kdbe_t		kdb_update; 	/* Attributes modified */
	bool		kdb_deleted;	/* Is this update a DELETION ? */
	bool		kdb_commit;	/* Is the entry committed or not ? */
	utf8str_t	kdb_kdcs_seen_by<>; /* Names of slaves that have */
					    /* seen this update - for */
					    /* future use */
	opaque		kdb_futures<>;	/* futures */
};

/*
 * Update log body
 */
typedef kdb_incr_update_t kdb_ulog_t<>;

enum update_status_t {
	UPDATE_OK = 0,
	UPDATE_ERROR = 1,
	UPDATE_FULL_RESYNC_NEEDED = 2,
	UPDATE_BUSY = 3,
	UPDATE_NIL = 4,
	UPDATE_PERM_DENIED = 5
};

struct kdb_last_t {
	kdb_sno_t	last_sno;
	kdbe_time_t	last_time;
};

struct kdb_incr_result_t {
	kdb_last_t		lastentry;
	kdb_ulog_t		updates;
	update_status_t		ret;
};

struct kdb_fullresync_result_t {
	kdb_last_t		lastentry;
	update_status_t 	ret;
};

program KRB5_IPROP_PROG {
	version KRB5_IPROP_VERS {
		/*
		 * NULL procedure
		 */
		void
		IPROP_NULL(void) = 0;

		/*
		 * Keep waiting for and get next incremental update(s)
		 *
		 * Will return latest kdb_vers on the master (if different),
		 * alongwith return value and affected db entries.
		 */
		kdb_incr_result_t
		IPROP_GET_UPDATES(kdb_last_t) = 1;

		/*
		 * We need to do the full-resync of the db, since the
		 * serial nos./timestamps are way out-of-whack
		 */
		kdb_fullresync_result_t
		IPROP_FULL_RESYNC(void) = 2;

		/*
		 * Full resync with version marker
		 */
		kdb_fullresync_result_t
		IPROP_FULL_RESYNC_EXT(uint32_t) = 3;
	} = 1;
} = 100423;
