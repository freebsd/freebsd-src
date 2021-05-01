/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>

//#include <netinet6/rte_tailq.h>
int errno = 0, rte_errno = 0;

#include "rte_shim.h"
#include "rte_lpm6.h"

#define RTE_LPM6_TBL24_NUM_ENTRIES        (1 << 24)
#define RTE_LPM6_TBL8_GROUP_NUM_ENTRIES         256
#define RTE_LPM6_TBL8_MAX_NUM_GROUPS      (1 << 21)

#define RTE_LPM6_VALID_EXT_ENTRY_BITMASK 0xA0000000
#define RTE_LPM6_LOOKUP_SUCCESS          0x20000000
#define RTE_LPM6_TBL8_BITMASK            0x001FFFFF

#define ADD_FIRST_BYTE                            3
#define LOOKUP_FIRST_BYTE                         4
#define BYTE_SIZE                                 8
#define BYTES2_SIZE                              16

#define RULE_HASH_TABLE_EXTRA_SPACE              64
#define TBL24_IND                        UINT32_MAX

#define lpm6_tbl8_gindex next_hop

/** Flags for setting an entry as valid/invalid. */
enum valid_flag {
	INVALID = 0,
	VALID
};

#if 0
TAILQ_HEAD(rte_lpm6_list, rte_tailq_entry);

static struct rte_tailq_elem rte_lpm6_tailq = {
	.name = "RTE_LPM6",
};
EAL_REGISTER_TAILQ(rte_lpm6_tailq)
#endif

/** Tbl entry structure. It is the same for both tbl24 and tbl8 */
struct rte_lpm6_tbl_entry {
	uint32_t next_hop:	21;  /**< Next hop / next table to be checked. */
	uint32_t depth	:8;      /**< Rule depth. */

	/* Flags. */
	uint32_t valid     :1;   /**< Validation flag. */
	uint32_t valid_group :1; /**< Group validation flag. */
	uint32_t ext_entry :1;   /**< External entry. */
};

/** Rules tbl entry structure. */
struct rte_lpm6_rule {
	uint8_t ip[RTE_LPM6_IPV6_ADDR_SIZE]; /**< Rule IP address. */
	uint32_t next_hop; /**< Rule next hop. */
	uint8_t depth; /**< Rule depth. */
};

/** Rules tbl entry key. */
struct rte_lpm6_rule_key {
	uint8_t ip[RTE_LPM6_IPV6_ADDR_SIZE]; /**< Rule IP address. */
	uint8_t depth; /**< Rule depth. */
};

/* Header of tbl8 */
struct rte_lpm_tbl8_hdr {
	uint32_t owner_tbl_ind; /**< owner table: TBL24_IND if owner is tbl24,
				  *  otherwise index of tbl8
				  */
	uint32_t owner_entry_ind; /**< index of the owner table entry where
				    *  pointer to the tbl8 is stored
				    */
	uint32_t ref_cnt; /**< table reference counter */
};

/** LPM6 structure. */
struct rte_lpm6 {
	struct rte_lpm6_external ext;	/* Storage used by the algo wrapper */
	/* LPM metadata. */
	char name[RTE_LPM6_NAMESIZE];    /**< Name of the lpm. */
	uint32_t max_rules;              /**< Max number of rules. */
	uint32_t used_rules;             /**< Used rules so far. */
	uint32_t number_tbl8s;           /**< Number of tbl8s to allocate. */

	/* LPM Tables. */
	//struct rte_hash *rules_tbl; /**< LPM rules. */
	struct rte_lpm6_tbl_entry tbl24[RTE_LPM6_TBL24_NUM_ENTRIES]
			__rte_cache_aligned; /**< LPM tbl24 table. */

	uint32_t *tbl8_pool; /**< pool of indexes of free tbl8s */
	uint32_t tbl8_pool_pos; /**< current position in the tbl8 pool */

	struct rte_lpm_tbl8_hdr *tbl8_hdrs; /* array of tbl8 headers */

	struct rte_lpm6_tbl_entry tbl8[0]
			__rte_cache_aligned; /**< LPM tbl8 table. */
};

/*
 * Takes an array of uint8_t (IPv6 address) and masks it using the depth.
 * It leaves untouched one bit per unit in the depth variable
 * and set the rest to 0.
 */
static inline void
ip6_mask_addr(uint8_t *ip, uint8_t depth)
{
	int16_t part_depth, mask;
	int i;

	part_depth = depth;

	for (i = 0; i < RTE_LPM6_IPV6_ADDR_SIZE; i++) {
		if (part_depth < BYTE_SIZE && part_depth >= 0) {
			mask = (uint16_t)(~(UINT8_MAX >> part_depth));
			ip[i] = (uint8_t)(ip[i] & mask);
		} else if (part_depth < 0)
			ip[i] = 0;

		part_depth -= BYTE_SIZE;
	}
}

/* copy ipv6 address */
static inline void
ip6_copy_addr(uint8_t *dst, const uint8_t *src)
{
	rte_memcpy(dst, src, RTE_LPM6_IPV6_ADDR_SIZE);
}

#if 0
/*
 * LPM6 rule hash function
 *
 * It's used as a hash function for the rte_hash
 *	containing rules
 */
static inline uint32_t
rule_hash(const void *data, __rte_unused uint32_t data_len,
		  uint32_t init_val)
{
	return rte_jhash(data, sizeof(struct rte_lpm6_rule_key), init_val);
}
#endif

/*
 * Init pool of free tbl8 indexes
 */
static void
tbl8_pool_init(struct rte_lpm6 *lpm)
{
	uint32_t i;

	/* put entire range of indexes to the tbl8 pool */
	for (i = 0; i < lpm->number_tbl8s; i++)
		lpm->tbl8_pool[i] = i;

	lpm->tbl8_pool_pos = 0;
}

/*
 * Get an index of a free tbl8 from the pool
 */
static inline uint32_t
tbl8_get(struct rte_lpm6 *lpm, uint32_t *tbl8_ind)
{
	if (lpm->tbl8_pool_pos == lpm->number_tbl8s)
		/* no more free tbl8 */
		return -ENOSPC;

	/* next index */
	*tbl8_ind = lpm->tbl8_pool[lpm->tbl8_pool_pos++];
	return 0;
}

/*
 * Put an index of a free tbl8 back to the pool
 */
static inline uint32_t
tbl8_put(struct rte_lpm6 *lpm, uint32_t tbl8_ind)
{
	if (lpm->tbl8_pool_pos == 0)
		/* pool is full */
		return -ENOSPC;

	lpm->tbl8_pool[--lpm->tbl8_pool_pos] = tbl8_ind;
	return 0;
}

/*
 * Returns number of tbl8s available in the pool
 */
static inline uint32_t
tbl8_available(struct rte_lpm6 *lpm)
{
	return lpm->number_tbl8s - lpm->tbl8_pool_pos;
}

#if 0
/*
 * Init a rule key.
 *	  note that ip must be already masked
 */
static inline void
rule_key_init(struct rte_lpm6_rule_key *key, uint8_t *ip, uint8_t depth)
{
	ip6_copy_addr(key->ip, ip);
	key->depth = depth;
}

/*
 * Rebuild the entire LPM tree by reinserting all rules
 */
static void
rebuild_lpm(struct rte_lpm6 *lpm)
{
	uint64_t next_hop;
	struct rte_lpm6_rule_key *rule_key;
	uint32_t iter = 0;

	while (rte_hash_iterate(lpm->rules_tbl, (void *) &rule_key,
			(void **) &next_hop, &iter) >= 0)
		rte_lpm6_add(lpm, rule_key->ip, rule_key->depth,
			(uint32_t) next_hop);
}
#endif

/*
 * Allocates memory for LPM object
 */
struct rte_lpm6 *
rte_lpm6_create(const char *name, int socket_id,
		const struct rte_lpm6_config *config)
{
	char mem_name[RTE_LPM6_NAMESIZE];
	struct rte_lpm6 *lpm = NULL;
	//struct rte_tailq_entry *te;
	uint64_t mem_size;
	//struct rte_lpm6_list *lpm_list;
	//struct rte_hash *rules_tbl = NULL;
	uint32_t *tbl8_pool = NULL;
	struct rte_lpm_tbl8_hdr *tbl8_hdrs = NULL;

	//lpm_list = RTE_TAILQ_CAST(rte_lpm6_tailq.head, rte_lpm6_list);

	RTE_BUILD_BUG_ON(sizeof(struct rte_lpm6_tbl_entry) != sizeof(uint32_t));

	/* Check user arguments. */
	if ((name == NULL) || (socket_id < -1) || (config == NULL) ||
			config->number_tbl8s > RTE_LPM6_TBL8_MAX_NUM_GROUPS) {
		rte_errno = EINVAL;
		return NULL;
	}

#if 0
	/* create rules hash table */
	snprintf(mem_name, sizeof(mem_name), "LRH_%s", name);
	struct rte_hash_parameters rule_hash_tbl_params = {
		.entries = config->max_rules * 1.2 +
			RULE_HASH_TABLE_EXTRA_SPACE,
		.key_len = sizeof(struct rte_lpm6_rule_key),
		.hash_func = rule_hash,
		.hash_func_init_val = 0,
		.name = mem_name,
		.reserved = 0,
		.socket_id = socket_id,
		.extra_flag = 0
	};

	rules_tbl = rte_hash_create(&rule_hash_tbl_params);
	if (rules_tbl == NULL) {
		RTE_LOG(ERR, LPM, "LPM rules hash table allocation failed: %s (%d)",
				  rte_strerror(rte_errno), rte_errno);
		goto fail_wo_unlock;
	}
#endif

	/* allocate tbl8 indexes pool */
	tbl8_pool = rte_malloc(NULL,
			sizeof(uint32_t) * config->number_tbl8s,
			RTE_CACHE_LINE_SIZE);
	if (tbl8_pool == NULL) {
		RTE_LOG(ERR, LPM, "LPM tbl8 pool allocation failed: %s (%d)",
				  rte_strerror(rte_errno), rte_errno);
		rte_errno = ENOMEM;
		goto fail_wo_unlock;
	}

	/* allocate tbl8 headers */
	tbl8_hdrs = rte_malloc(NULL,
			sizeof(struct rte_lpm_tbl8_hdr) * config->number_tbl8s,
			RTE_CACHE_LINE_SIZE);
	if (tbl8_hdrs == NULL) {
		RTE_LOG(ERR, LPM, "LPM tbl8 headers allocation failed: %s (%d)",
				  rte_strerror(rte_errno), rte_errno);
		rte_errno = ENOMEM;
		goto fail_wo_unlock;
	}

	snprintf(mem_name, sizeof(mem_name), "LPM_%s", name);

	/* Determine the amount of memory to allocate. */
	mem_size = sizeof(*lpm) + (sizeof(lpm->tbl8[0]) *
			RTE_LPM6_TBL8_GROUP_NUM_ENTRIES * config->number_tbl8s);

#if 0
	rte_mcfg_tailq_write_lock();

	/* Guarantee there's no existing */
	TAILQ_FOREACH(te, lpm_list, next) {
		lpm = (struct rte_lpm6 *) te->data;
		if (strncmp(name, lpm->name, RTE_LPM6_NAMESIZE) == 0)
			break;
	}
	lpm = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		goto fail;
	}

	/* allocate tailq entry */
	te = rte_zmalloc("LPM6_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, LPM, "Failed to allocate tailq entry!\n");
		rte_errno = ENOMEM;
		goto fail;
	}
#endif

	/* Allocate memory to store the LPM data structures. */
	lpm = rte_zmalloc_socket(mem_name, (size_t)mem_size,
			RTE_CACHE_LINE_SIZE, socket_id);

	if (lpm == NULL) {
		RTE_LOG(ERR, LPM, "LPM memory allocation failed\n");
		//rte_free(te);
		rte_errno = ENOMEM;
		goto fail;
	}

	/* Save user arguments. */
	//lpm->max_rules = config->max_rules;
	lpm->number_tbl8s = config->number_tbl8s;
	strlcpy(lpm->name, name, sizeof(lpm->name));
	//lpm->rules_tbl = rules_tbl;
	lpm->tbl8_pool = tbl8_pool;
	lpm->tbl8_hdrs = tbl8_hdrs;

	/* init the stack */
	tbl8_pool_init(lpm);

	//te->data = (void *) lpm;

	//TAILQ_INSERT_TAIL(lpm_list, te, next);
	rte_mcfg_tailq_write_unlock();
	return lpm;

fail:
	rte_mcfg_tailq_write_unlock();

fail_wo_unlock:
	rte_free(tbl8_hdrs);
	rte_free(tbl8_pool);
	//rte_hash_free(rules_tbl);

	return NULL;
}

#if 0
/*
 * Find an existing lpm table and return a pointer to it.
 */
struct rte_lpm6 *
rte_lpm6_find_existing(const char *name)
{
	struct rte_lpm6 *l = NULL;
	struct rte_tailq_entry *te;
	struct rte_lpm6_list *lpm_list;

	lpm_list = RTE_TAILQ_CAST(rte_lpm6_tailq.head, rte_lpm6_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, lpm_list, next) {
		l = (struct rte_lpm6 *) te->data;
		if (strncmp(name, l->name, RTE_LPM6_NAMESIZE) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return l;
}
#endif

/*
 * Deallocates memory for given LPM table.
 */
void
rte_lpm6_free(struct rte_lpm6 *lpm)
{
#if 0
	struct rte_lpm6_list *lpm_list;
	struct rte_tailq_entry *te;

	/* Check user arguments. */
	if (lpm == NULL)
		return;

	lpm_list = RTE_TAILQ_CAST(rte_lpm6_tailq.head, rte_lpm6_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, lpm_list, next) {
		if (te->data == (void *) lpm)
			break;
	}

	if (te != NULL)
		TAILQ_REMOVE(lpm_list, te, next);

	rte_mcfg_tailq_write_unlock();
#endif

	rte_free(lpm->tbl8_hdrs);
	rte_free(lpm->tbl8_pool);
	//rte_hash_free(lpm->rules_tbl);
	rte_free(lpm);
	//rte_free(te);
}

#if 0
/* Find a rule */
static inline int
rule_find_with_key(struct rte_lpm6 *lpm,
		  const struct rte_lpm6_rule_key *rule_key,
		  uint32_t *next_hop)
{
	uint64_t hash_val;
	int ret;

	/* lookup for a rule */
	ret = rte_hash_lookup_data(lpm->rules_tbl, (const void *) rule_key,
		(void **) &hash_val);
	if (ret >= 0) {
		*next_hop = (uint32_t) hash_val;
		return 1;
	}

	return 0;
}

/* Find a rule */
static int
rule_find(struct rte_lpm6 *lpm, uint8_t *ip, uint8_t depth,
		  uint32_t *next_hop)
{
	struct rte_lpm6_rule_key rule_key;

	/* init a rule key */
	rule_key_init(&rule_key, ip, depth);

	return rule_find_with_key(lpm, &rule_key, next_hop);
}

/*
 * Checks if a rule already exists in the rules table and updates
 * the nexthop if so. Otherwise it adds a new rule if enough space is available.
 *
 * Returns:
 *    0 - next hop of existed rule is updated
 *    1 - new rule successfully added
 *   <0 - error
 */
static inline int
rule_add(struct rte_lpm6 *lpm, uint8_t *ip, uint8_t depth, uint32_t next_hop)
{
	int ret, rule_exist;
	struct rte_lpm6_rule_key rule_key;
	uint32_t unused;

	/* init a rule key */
	rule_key_init(&rule_key, ip, depth);

	/* Scan through rule list to see if rule already exists. */
	rule_exist = rule_find_with_key(lpm, &rule_key, &unused);

	/*
	 * If rule does not exist check if there is space to add a new rule to
	 * this rule group. If there is no space return error.
	 */
	if (!rule_exist && lpm->used_rules == lpm->max_rules)
		return -ENOSPC;

	/* add the rule or update rules next hop */
	ret = rte_hash_add_key_data(lpm->rules_tbl, &rule_key,
		(void *)(uintptr_t) next_hop);
	if (ret < 0)
		return ret;

	/* Increment the used rules counter for this rule group. */
	if (!rule_exist) {
		lpm->used_rules++;
		return 1;
	}

	return 0;
}
#endif

/*
 * Function that expands a rule across the data structure when a less-generic
 * one has been added before. It assures that every possible combination of bits
 * in the IP address returns a match.
 */
static void
expand_rule(struct rte_lpm6 *lpm, uint32_t tbl8_gindex, uint8_t old_depth,
		uint8_t new_depth, uint32_t next_hop, uint8_t valid)
{
	uint32_t tbl8_group_end, tbl8_gindex_next, j;

	tbl8_group_end = tbl8_gindex + RTE_LPM6_TBL8_GROUP_NUM_ENTRIES;

	struct rte_lpm6_tbl_entry new_tbl8_entry = {
		.valid = valid,
		.valid_group = valid,
		.depth = new_depth,
		.next_hop = next_hop,
		.ext_entry = 0,
	};

	for (j = tbl8_gindex; j < tbl8_group_end; j++) {
		if (!lpm->tbl8[j].valid || (lpm->tbl8[j].ext_entry == 0
				&& lpm->tbl8[j].depth <= old_depth)) {

			lpm->tbl8[j] = new_tbl8_entry;

		} else if (lpm->tbl8[j].ext_entry == 1) {

			tbl8_gindex_next = lpm->tbl8[j].lpm6_tbl8_gindex
					* RTE_LPM6_TBL8_GROUP_NUM_ENTRIES;
			expand_rule(lpm, tbl8_gindex_next, old_depth, new_depth,
					next_hop, valid);
		}
	}
}

/*
 * Init a tbl8 header
 */
static inline void
init_tbl8_header(struct rte_lpm6 *lpm, uint32_t tbl_ind,
		uint32_t owner_tbl_ind, uint32_t owner_entry_ind)
{
	struct rte_lpm_tbl8_hdr *tbl_hdr = &lpm->tbl8_hdrs[tbl_ind];
	tbl_hdr->owner_tbl_ind = owner_tbl_ind;
	tbl_hdr->owner_entry_ind = owner_entry_ind;
	tbl_hdr->ref_cnt = 0;
}

/*
 * Calculate index to the table based on the number and position
 * of the bytes being inspected in this step.
 */
static uint32_t
get_bitshift(const uint8_t *ip, uint8_t first_byte, uint8_t bytes)
{
	uint32_t entry_ind, i;
	int8_t bitshift;

	entry_ind = 0;
	for (i = first_byte; i < (uint32_t)(first_byte + bytes); i++) {
		bitshift = (int8_t)((bytes - i)*BYTE_SIZE);

		if (bitshift < 0)
			bitshift = 0;
		entry_ind = entry_ind | ip[i-1] << bitshift;
	}

	return entry_ind;
}

/*
 * Simulate adding a new route to the LPM counting number
 * of new tables that will be needed
 *
 * It returns 0 on success, or 1 if
 * the process needs to be continued by calling the function again.
 */
static inline int
simulate_add_step(struct rte_lpm6 *lpm, struct rte_lpm6_tbl_entry *tbl,
		struct rte_lpm6_tbl_entry **next_tbl, const uint8_t *ip,
		uint8_t bytes, uint8_t first_byte, uint8_t depth,
		uint32_t *need_tbl_nb)
{
	uint32_t entry_ind;
	uint8_t bits_covered;
	uint32_t next_tbl_ind;

	/*
	 * Calculate index to the table based on the number and position
	 * of the bytes being inspected in this step.
	 */
	entry_ind = get_bitshift(ip, first_byte, bytes);

	/* Number of bits covered in this step */
	bits_covered = (uint8_t)((bytes+first_byte-1)*BYTE_SIZE);

	if (depth <= bits_covered) {
		*need_tbl_nb = 0;
		return 0;
	}

	if (tbl[entry_ind].valid == 0 || tbl[entry_ind].ext_entry == 0) {
		/* from this point on a new table is needed on each level
		 * that is not covered yet
		 */
		depth -= bits_covered;
		uint32_t cnt = depth >> 3; /* depth / BYTE_SIZE */
		if (depth & 7) /* 0b00000111 */
			/* if depth % 8 > 0 then one more table is needed
			 * for those last bits
			 */
			cnt++;

		*need_tbl_nb = cnt;
		return 0;
	}

	next_tbl_ind = tbl[entry_ind].lpm6_tbl8_gindex;
	*next_tbl = &(lpm->tbl8[next_tbl_ind *
		RTE_LPM6_TBL8_GROUP_NUM_ENTRIES]);
	*need_tbl_nb = 0;
	return 1;
}

/*
 * Partially adds a new route to the data structure (tbl24+tbl8s).
 * It returns 0 on success, a negative number on failure, or 1 if
 * the process needs to be continued by calling the function again.
 */
static inline int
add_step(struct rte_lpm6 *lpm, struct rte_lpm6_tbl_entry *tbl,
		uint32_t tbl_ind, struct rte_lpm6_tbl_entry **next_tbl,
		uint32_t *next_tbl_ind, uint8_t *ip, uint8_t bytes,
		uint8_t first_byte, uint8_t depth, uint32_t next_hop,
		uint8_t is_new_rule)
{
	uint32_t entry_ind, tbl_range, tbl8_group_start, tbl8_group_end, i;
	uint32_t tbl8_gindex;
	uint8_t bits_covered;
	int ret;

	/*
	 * Calculate index to the table based on the number and position
	 * of the bytes being inspected in this step.
	 */
	entry_ind = get_bitshift(ip, first_byte, bytes);

	/* Number of bits covered in this step */
	bits_covered = (uint8_t)((bytes+first_byte-1)*BYTE_SIZE);

	/*
	 * If depth if smaller than this number (ie this is the last step)
	 * expand the rule across the relevant positions in the table.
	 */
	if (depth <= bits_covered) {
		tbl_range = 1 << (bits_covered - depth);

		for (i = entry_ind; i < (entry_ind + tbl_range); i++) {
			if (!tbl[i].valid || (tbl[i].ext_entry == 0 &&
					tbl[i].depth <= depth)) {

				struct rte_lpm6_tbl_entry new_tbl_entry = {
					.next_hop = next_hop,
					.depth = depth,
					.valid = VALID,
					.valid_group = VALID,
					.ext_entry = 0,
				};

				tbl[i] = new_tbl_entry;

			} else if (tbl[i].ext_entry == 1) {

				/*
				 * If tbl entry is valid and extended calculate the index
				 * into next tbl8 and expand the rule across the data structure.
				 */
				tbl8_gindex = tbl[i].lpm6_tbl8_gindex *
						RTE_LPM6_TBL8_GROUP_NUM_ENTRIES;
				expand_rule(lpm, tbl8_gindex, depth, depth,
						next_hop, VALID);
			}
		}

		/* update tbl8 rule reference counter */
		if (tbl_ind != TBL24_IND && is_new_rule)
			lpm->tbl8_hdrs[tbl_ind].ref_cnt++;

		return 0;
	}
	/*
	 * If this is not the last step just fill one position
	 * and calculate the index to the next table.
	 */
	else {
		/* If it's invalid a new tbl8 is needed */
		if (!tbl[entry_ind].valid) {
			/* get a new table */
			ret = tbl8_get(lpm, &tbl8_gindex);
			if (ret != 0)
				return -ENOSPC;

			/* invalidate all new tbl8 entries */
			tbl8_group_start = tbl8_gindex *
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES;
			memset(&lpm->tbl8[tbl8_group_start], 0,
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES *
					sizeof(struct rte_lpm6_tbl_entry));

			/* init the new table's header:
			 *   save the reference to the owner table
			 */
			init_tbl8_header(lpm, tbl8_gindex, tbl_ind, entry_ind);

			/* reference to a new tbl8 */
			struct rte_lpm6_tbl_entry new_tbl_entry = {
				.lpm6_tbl8_gindex = tbl8_gindex,
				.depth = 0,
				.valid = VALID,
				.valid_group = VALID,
				.ext_entry = 1,
			};

			tbl[entry_ind] = new_tbl_entry;

			/* update the current table's reference counter */
			if (tbl_ind != TBL24_IND)
				lpm->tbl8_hdrs[tbl_ind].ref_cnt++;
		}
		/*
		 * If it's valid but not extended the rule that was stored
		 * here needs to be moved to the next table.
		 */
		else if (tbl[entry_ind].ext_entry == 0) {
			/* get a new tbl8 index */
			ret = tbl8_get(lpm, &tbl8_gindex);
			if (ret != 0)
				return -ENOSPC;

			tbl8_group_start = tbl8_gindex *
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES;
			tbl8_group_end = tbl8_group_start +
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES;

			struct rte_lpm6_tbl_entry tbl_entry = {
				.next_hop = tbl[entry_ind].next_hop,
				.depth = tbl[entry_ind].depth,
				.valid = VALID,
				.valid_group = VALID,
				.ext_entry = 0
			};

			/* Populate new tbl8 with tbl value. */
			for (i = tbl8_group_start; i < tbl8_group_end; i++)
				lpm->tbl8[i] = tbl_entry;

			/* init the new table's header:
			 *   save the reference to the owner table
			 */
			init_tbl8_header(lpm, tbl8_gindex, tbl_ind, entry_ind);

			/*
			 * Update tbl entry to point to new tbl8 entry. Note: The
			 * ext_flag and tbl8_index need to be updated simultaneously,
			 * so assign whole structure in one go.
			 */
			struct rte_lpm6_tbl_entry new_tbl_entry = {
				.lpm6_tbl8_gindex = tbl8_gindex,
				.depth = 0,
				.valid = VALID,
				.valid_group = VALID,
				.ext_entry = 1,
			};

			tbl[entry_ind] = new_tbl_entry;

			/* update the current table's reference counter */
			if (tbl_ind != TBL24_IND)
				lpm->tbl8_hdrs[tbl_ind].ref_cnt++;
		}

		*next_tbl_ind = tbl[entry_ind].lpm6_tbl8_gindex;
		*next_tbl = &(lpm->tbl8[*next_tbl_ind *
				  RTE_LPM6_TBL8_GROUP_NUM_ENTRIES]);
	}

	return 1;
}

/*
 * Simulate adding a route to LPM
 *
 *	Returns:
 *    0 on success
 *    -ENOSPC not enough tbl8 left
 */
static int
simulate_add(struct rte_lpm6 *lpm, const uint8_t *masked_ip, uint8_t depth)
{
	struct rte_lpm6_tbl_entry *tbl;
	struct rte_lpm6_tbl_entry *tbl_next = NULL;
	int ret, i;

	/* number of new tables needed for a step */
	uint32_t need_tbl_nb;
	/* total number of new tables needed */
	uint32_t total_need_tbl_nb;

	/* Inspect the first three bytes through tbl24 on the first step. */
	ret = simulate_add_step(lpm, lpm->tbl24, &tbl_next, masked_ip,
		ADD_FIRST_BYTE, 1, depth, &need_tbl_nb);
	total_need_tbl_nb = need_tbl_nb;
	/*
	 * Inspect one by one the rest of the bytes until
	 * the process is completed.
	 */
	for (i = ADD_FIRST_BYTE; i < RTE_LPM6_IPV6_ADDR_SIZE && ret == 1; i++) {
		tbl = tbl_next;
		ret = simulate_add_step(lpm, tbl, &tbl_next, masked_ip, 1,
			(uint8_t)(i + 1), depth, &need_tbl_nb);
		total_need_tbl_nb += need_tbl_nb;
	}

	if (tbl8_available(lpm) < total_need_tbl_nb)
		/* not enough tbl8 to add a rule */
		return -ENOSPC;

	return 0;
}

/*
 * Add a route
 */
int
rte_lpm6_add(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
	     uint32_t next_hop, int is_new_rule)
{
	struct rte_lpm6_tbl_entry *tbl;
	struct rte_lpm6_tbl_entry *tbl_next = NULL;
	/* init to avoid compiler warning */
	uint32_t tbl_next_num = 123456;
	int status;
	uint8_t masked_ip[RTE_LPM6_IPV6_ADDR_SIZE];
	int i;

	/* Check user arguments. */
	if ((lpm == NULL) || (depth < 1) || (depth > RTE_LPM6_MAX_DEPTH))
		return -EINVAL;

	/* Copy the IP and mask it to avoid modifying user's input data. */
	ip6_copy_addr(masked_ip, ip);
	ip6_mask_addr(masked_ip, depth);

	/* Simulate adding a new route */
	int ret = simulate_add(lpm, masked_ip, depth);
	if (ret < 0)
		return ret;

#if 0
	/* Add the rule to the rule table. */
	int is_new_rule = rule_add(lpm, masked_ip, depth, next_hop);
	/* If there is no space available for new rule return error. */
	if (is_new_rule < 0)
		return is_new_rule;
#endif

	/* Inspect the first three bytes through tbl24 on the first step. */
	tbl = lpm->tbl24;
	status = add_step(lpm, tbl, TBL24_IND, &tbl_next, &tbl_next_num,
		masked_ip, ADD_FIRST_BYTE, 1, depth, next_hop,
		is_new_rule);
	assert(status >= 0);

	/*
	 * Inspect one by one the rest of the bytes until
	 * the process is completed.
	 */
	for (i = ADD_FIRST_BYTE; i < RTE_LPM6_IPV6_ADDR_SIZE && status == 1; i++) {
		tbl = tbl_next;
		status = add_step(lpm, tbl, tbl_next_num, &tbl_next,
			&tbl_next_num, masked_ip, 1, (uint8_t)(i + 1),
			depth, next_hop, is_new_rule);
		assert(status >= 0);
	}

	return status;
}

/*
 * Takes a pointer to a table entry and inspect one level.
 * The function returns 0 on lookup success, ENOENT if no match was found
 * or 1 if the process needs to be continued by calling the function again.
 */
static inline int
lookup_step(const struct rte_lpm6 *lpm, const struct rte_lpm6_tbl_entry *tbl,
		const struct rte_lpm6_tbl_entry **tbl_next, const uint8_t *ip,
		uint8_t first_byte, uint32_t *next_hop)
{
	uint32_t tbl8_index, tbl_entry;

	/* Take the integer value from the pointer. */
	tbl_entry = *(const uint32_t *)tbl;

	/* If it is valid and extended we calculate the new pointer to return. */
	if ((tbl_entry & RTE_LPM6_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM6_VALID_EXT_ENTRY_BITMASK) {

		tbl8_index = ip[first_byte-1] +
				((tbl_entry & RTE_LPM6_TBL8_BITMASK) *
				RTE_LPM6_TBL8_GROUP_NUM_ENTRIES);

		*tbl_next = &lpm->tbl8[tbl8_index];

		return 1;
	} else {
		/* If not extended then we can have a match. */
		*next_hop = ((uint32_t)tbl_entry & RTE_LPM6_TBL8_BITMASK);
		return (tbl_entry & RTE_LPM6_LOOKUP_SUCCESS) ? 0 : -ENOENT;
	}
}

/*
 * Looks up an IP
 */
int
rte_lpm6_lookup(const struct rte_lpm6 *lpm, const uint8_t *ip,
		uint32_t *next_hop)
{
	const struct rte_lpm6_tbl_entry *tbl;
	const struct rte_lpm6_tbl_entry *tbl_next = NULL;
	int status;
	uint8_t first_byte;
	uint32_t tbl24_index;

	/* DEBUG: Check user input arguments. */
	if ((lpm == NULL) || (ip == NULL) || (next_hop == NULL))
		return -EINVAL;

	first_byte = LOOKUP_FIRST_BYTE;
	tbl24_index = (ip[0] << BYTES2_SIZE) | (ip[1] << BYTE_SIZE) | ip[2];

	/* Calculate pointer to the first entry to be inspected */
	tbl = &lpm->tbl24[tbl24_index];

	do {
		/* Continue inspecting following levels until success or failure */
		status = lookup_step(lpm, tbl, &tbl_next, ip, first_byte++, next_hop);
		tbl = tbl_next;
	} while (status == 1);

	return status;
}

/*
 * Looks up a group of IP addresses
 */
int
rte_lpm6_lookup_bulk_func(const struct rte_lpm6 *lpm,
		uint8_t ips[][RTE_LPM6_IPV6_ADDR_SIZE],
		int32_t *next_hops, unsigned int n)
{
	unsigned int i;
	const struct rte_lpm6_tbl_entry *tbl;
	const struct rte_lpm6_tbl_entry *tbl_next = NULL;
	uint32_t tbl24_index, next_hop;
	uint8_t first_byte;
	int status;

	/* DEBUG: Check user input arguments. */
	if ((lpm == NULL) || (ips == NULL) || (next_hops == NULL))
		return -EINVAL;

	for (i = 0; i < n; i++) {
		first_byte = LOOKUP_FIRST_BYTE;
		tbl24_index = (ips[i][0] << BYTES2_SIZE) |
				(ips[i][1] << BYTE_SIZE) | ips[i][2];

		/* Calculate pointer to the first entry to be inspected */
		tbl = &lpm->tbl24[tbl24_index];

		do {
			/* Continue inspecting following levels
			 * until success or failure
			 */
			status = lookup_step(lpm, tbl, &tbl_next, ips[i],
					first_byte++, &next_hop);
			tbl = tbl_next;
		} while (status == 1);

		if (status < 0)
			next_hops[i] = -1;
		else
			next_hops[i] = (int32_t)next_hop;
	}

	return 0;
}

struct rte_lpm6_rule *
fill_rule6(char *buffer, const uint8_t *ip, uint8_t depth, uint32_t next_hop)
{
	struct rte_lpm6_rule *rule = (struct rte_lpm6_rule *)buffer;

	ip6_copy_addr((uint8_t *)&rule->ip, ip);
	rule->depth = depth;
	rule->next_hop = next_hop;

	return (rule);
}

#if 0
/*
 * Look for a rule in the high-level rules table
 */
int
rte_lpm6_is_rule_present(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
			 uint32_t *next_hop)
{
	uint8_t masked_ip[RTE_LPM6_IPV6_ADDR_SIZE];

	/* Check user arguments. */
	if ((lpm == NULL) || next_hop == NULL || ip == NULL ||
			(depth < 1) || (depth > RTE_LPM6_MAX_DEPTH))
		return -EINVAL;

	/* Copy the IP and mask it to avoid modifying user's input data. */
	ip6_copy_addr(masked_ip, ip);
	ip6_mask_addr(masked_ip, depth);

	return rule_find(lpm, masked_ip, depth, next_hop);
}

/*
 * Delete a rule from the rule table.
 * NOTE: Valid range for depth parameter is 1 .. 128 inclusive.
 * return
 *	  0 on success
 *   <0 on failure
 */
static inline int
rule_delete(struct rte_lpm6 *lpm, uint8_t *ip, uint8_t depth)
{
	int ret;
	struct rte_lpm6_rule_key rule_key;

	/* init rule key */
	rule_key_init(&rule_key, ip, depth);

	/* delete the rule */
	ret = rte_hash_del_key(lpm->rules_tbl, (void *) &rule_key);
	if (ret >= 0)
		lpm->used_rules--;

	return ret;
}

/*
 * Deletes a group of rules
 *
 * Note that the function rebuilds the lpm table,
 * rather than doing incremental updates like
 * the regular delete function
 */
int
rte_lpm6_delete_bulk_func(struct rte_lpm6 *lpm,
		uint8_t ips[][RTE_LPM6_IPV6_ADDR_SIZE], uint8_t *depths,
		unsigned n)
{
	uint8_t masked_ip[RTE_LPM6_IPV6_ADDR_SIZE];
	unsigned i;

	/* Check input arguments. */
	if ((lpm == NULL) || (ips == NULL) || (depths == NULL))
		return -EINVAL;

	for (i = 0; i < n; i++) {
		ip6_copy_addr(masked_ip, ips[i]);
		ip6_mask_addr(masked_ip, depths[i]);
		rule_delete(lpm, masked_ip, depths[i]);
	}

	/*
	 * Set all the table entries to 0 (ie delete every rule
	 * from the data structure.
	 */
	memset(lpm->tbl24, 0, sizeof(lpm->tbl24));
	memset(lpm->tbl8, 0, sizeof(lpm->tbl8[0])
			* RTE_LPM6_TBL8_GROUP_NUM_ENTRIES * lpm->number_tbl8s);
	tbl8_pool_init(lpm);

	/*
	 * Add every rule again (except for the ones that were removed from
	 * the rules table).
	 */
	rebuild_lpm(lpm);

	return 0;
}

/*
 * Delete all rules from the LPM table.
 */
void
rte_lpm6_delete_all(struct rte_lpm6 *lpm)
{
	/* Zero used rules counter. */
	lpm->used_rules = 0;

	/* Zero tbl24. */
	memset(lpm->tbl24, 0, sizeof(lpm->tbl24));

	/* Zero tbl8. */
	memset(lpm->tbl8, 0, sizeof(lpm->tbl8[0]) *
			RTE_LPM6_TBL8_GROUP_NUM_ENTRIES * lpm->number_tbl8s);

	/* init pool of free tbl8 indexes */
	tbl8_pool_init(lpm);

	/* Delete all rules form the rules table. */
	rte_hash_reset(lpm->rules_tbl);
}
#endif

/*
 * Convert a depth to a one byte long mask
 *   Example: 4 will be converted to 0xF0
 */
static uint8_t __attribute__((pure))
depth_to_mask_1b(uint8_t depth)
{
	/* To calculate a mask start with a 1 on the left hand side and right
	 * shift while populating the left hand side with 1's
	 */
	return (signed char)0x80 >> (depth - 1);
}

#if 0
/*
 * Find a less specific rule
 */
static int
rule_find_less_specific(struct rte_lpm6 *lpm, uint8_t *ip, uint8_t depth,
	struct rte_lpm6_rule *rule)
{
	int ret;
	uint32_t next_hop;
	uint8_t mask;
	struct rte_lpm6_rule_key rule_key;

	if (depth == 1)
		return 0;

	rule_key_init(&rule_key, ip, depth);

	while (depth > 1) {
		depth--;

		/* each iteration zero one more bit of the key */
		mask = depth & 7; /* depth % BYTE_SIZE */
		if (mask > 0)
			mask = depth_to_mask_1b(mask);

		rule_key.depth = depth;
		rule_key.ip[depth >> 3] &= mask;

		ret = rule_find_with_key(lpm, &rule_key, &next_hop);
		if (ret) {
			rule->depth = depth;
			ip6_copy_addr(rule->ip, rule_key.ip);
			rule->next_hop = next_hop;
			return 1;
		}
	}

	return 0;
}
#endif

/*
 * Find range of tbl8 cells occupied by a rule
 */
static void
rule_find_range(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
		  struct rte_lpm6_tbl_entry **from,
		  struct rte_lpm6_tbl_entry **to,
		  uint32_t *out_tbl_ind)
{
	uint32_t ind;
	uint32_t first_3bytes = (uint32_t)ip[0] << 16 | ip[1] << 8 | ip[2];

	if (depth <= 24) {
		/* rule is within the top level */
		ind = first_3bytes;
		*from = &lpm->tbl24[ind];
		ind += (1 << (24 - depth)) - 1;
		*to = &lpm->tbl24[ind];
		*out_tbl_ind = TBL24_IND;
	} else {
		/* top level entry */
		struct rte_lpm6_tbl_entry *tbl = &lpm->tbl24[first_3bytes];
		assert(tbl->ext_entry == 1);
		/* first tbl8 */
		uint32_t tbl_ind = tbl->lpm6_tbl8_gindex;
		tbl = &lpm->tbl8[tbl_ind *
				RTE_LPM6_TBL8_GROUP_NUM_ENTRIES];
		/* current ip byte, the top level is already behind */
		uint8_t byte = 3;
		/* minus top level */
		depth -= 24;

		/* iterate through levels (tbl8s)
		 * until we reach the last one
		 */
		while (depth > 8) {
			tbl += ip[byte];
			assert(tbl->ext_entry == 1);
			/* go to the next level/tbl8 */
			tbl_ind = tbl->lpm6_tbl8_gindex;
			tbl = &lpm->tbl8[tbl_ind *
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES];
			byte += 1;
			depth -= 8;
		}

		/* last level/tbl8 */
		ind = ip[byte] & depth_to_mask_1b(depth);
		*from = &tbl[ind];
		ind += (1 << (8 - depth)) - 1;
		*to = &tbl[ind];
		*out_tbl_ind = tbl_ind;
	}
}

/*
 * Remove a table from the LPM tree
 */
static void
remove_tbl(struct rte_lpm6 *lpm, struct rte_lpm_tbl8_hdr *tbl_hdr,
		  uint32_t tbl_ind, struct rte_lpm6_rule *lsp_rule)
{
	struct rte_lpm6_tbl_entry *owner_entry;

	if (tbl_hdr->owner_tbl_ind == TBL24_IND)
		owner_entry = &lpm->tbl24[tbl_hdr->owner_entry_ind];
	else {
		uint32_t owner_tbl_ind = tbl_hdr->owner_tbl_ind;
		owner_entry = &lpm->tbl8[
			owner_tbl_ind * RTE_LPM6_TBL8_GROUP_NUM_ENTRIES +
			tbl_hdr->owner_entry_ind];

		struct rte_lpm_tbl8_hdr *owner_tbl_hdr =
			&lpm->tbl8_hdrs[owner_tbl_ind];
		if (--owner_tbl_hdr->ref_cnt == 0)
			remove_tbl(lpm, owner_tbl_hdr, owner_tbl_ind, lsp_rule);
	}

	assert(owner_entry->ext_entry == 1);

	/* unlink the table */
	if (lsp_rule != NULL) {
		struct rte_lpm6_tbl_entry new_tbl_entry = {
			.next_hop = lsp_rule->next_hop,
			.depth = lsp_rule->depth,
			.valid = VALID,
			.valid_group = VALID,
			.ext_entry = 0
		};

		*owner_entry = new_tbl_entry;
	} else {
		struct rte_lpm6_tbl_entry new_tbl_entry = {
			.next_hop = 0,
			.depth = 0,
			.valid = INVALID,
			.valid_group = INVALID,
			.ext_entry = 0
		};

		*owner_entry = new_tbl_entry;
	}

	/* return the table to the pool */
	tbl8_put(lpm, tbl_ind);
}

/*
 * Deletes a rule
 */
int
rte_lpm6_delete(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
    struct rte_lpm6_rule *lsp_rule)
{
	uint8_t masked_ip[RTE_LPM6_IPV6_ADDR_SIZE];
	//struct rte_lpm6_rule lsp_rule_obj;
	//struct rte_lpm6_rule *lsp_rule;
	//int ret;
	uint32_t tbl_ind;
	struct rte_lpm6_tbl_entry *from, *to;

	/* Check input arguments. */
	if ((lpm == NULL) || (depth < 1) || (depth > RTE_LPM6_MAX_DEPTH))
		return -EINVAL;

	/* Copy the IP and mask it to avoid modifying user's input data. */
	ip6_copy_addr(masked_ip, ip);
	ip6_mask_addr(masked_ip, depth);

#if 0
	/* Delete the rule from the rule table. */
	ret = rule_delete(lpm, masked_ip, depth);
	if (ret < 0)
		return -ENOENT;
#endif

	/* find rule cells */
	rule_find_range(lpm, masked_ip, depth, &from, &to, &tbl_ind);

#if 0
	/* find a less specific rule (a rule with smaller depth)
	 * note: masked_ip will be modified, don't use it anymore
	 */
	ret = rule_find_less_specific(lpm, masked_ip, depth,
			&lsp_rule_obj);
	lsp_rule = ret ? &lsp_rule_obj : NULL;
#endif
	/* decrement the table rule counter,
	 * note that tbl24 doesn't have a header
	 */
	if (tbl_ind != TBL24_IND) {
		struct rte_lpm_tbl8_hdr *tbl_hdr = &lpm->tbl8_hdrs[tbl_ind];
		if (--tbl_hdr->ref_cnt == 0) {
			/* remove the table */
			remove_tbl(lpm, tbl_hdr, tbl_ind, lsp_rule);
			return 0;
		}
	}

	/* iterate rule cells */
	for (; from <= to; from++)
		if (from->ext_entry == 1) {
			/* reference to a more specific space
			 * of the prefix/rule. Entries in a more
			 * specific space that are not used by
			 * a more specific prefix must be occupied
			 * by the prefix
			 */
			if (lsp_rule != NULL)
				expand_rule(lpm,
					from->lpm6_tbl8_gindex *
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES,
					depth, lsp_rule->depth,
					lsp_rule->next_hop, VALID);
			else
				/* since the prefix has no less specific prefix,
				 * its more specific space must be invalidated
				 */
				expand_rule(lpm,
					from->lpm6_tbl8_gindex *
					RTE_LPM6_TBL8_GROUP_NUM_ENTRIES,
					depth, 0, 0, INVALID);
		} else if (from->depth == depth) {
			/* entry is not a reference and belongs to the prefix */
			if (lsp_rule != NULL) {
				struct rte_lpm6_tbl_entry new_tbl_entry = {
					.next_hop = lsp_rule->next_hop,
					.depth = lsp_rule->depth,
					.valid = VALID,
					.valid_group = VALID,
					.ext_entry = 0
				};

				*from = new_tbl_entry;
			} else {
				struct rte_lpm6_tbl_entry new_tbl_entry = {
					.next_hop = 0,
					.depth = 0,
					.valid = INVALID,
					.valid_group = INVALID,
					.ext_entry = 0
				};

				*from = new_tbl_entry;
			}
		}

	return 0;
}
