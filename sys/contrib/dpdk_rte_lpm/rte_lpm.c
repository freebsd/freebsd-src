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

int errno = 0, rte_errno = 0;

#if 0
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_memory.h>        /* for definition of RTE_CACHE_LINE_SIZE */
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_tailq.h>
#endif

#include "rte_shim.h"
#include "rte_lpm.h"

#if 0
TAILQ_HEAD(rte_lpm_list, rte_tailq_entry);

static struct rte_tailq_elem rte_lpm_tailq = {
	.name = "RTE_LPM",
};
EAL_REGISTER_TAILQ(rte_lpm_tailq)
#endif

#define MAX_DEPTH_TBL24 24

enum valid_flag {
	INVALID = 0,
	VALID
};

/* Macro to enable/disable run-time checks. */
#if defined(RTE_LIBRTE_LPM_DEBUG)
#include <rte_debug.h>
#define VERIFY_DEPTH(depth) do {                                \
	if ((depth == 0) || (depth > RTE_LPM_MAX_DEPTH))        \
		rte_panic("LPM: Invalid depth (%u) at line %d", \
				(unsigned)(depth), __LINE__);   \
} while (0)
#else
#define VERIFY_DEPTH(depth)
#endif

/*
 * Converts a given depth value to its corresponding mask value.
 *
 * depth  (IN)		: range = 1 - 32
 * mask   (OUT)		: 32bit mask
 */
static uint32_t __attribute__((pure))
depth_to_mask(uint8_t depth)
{
	VERIFY_DEPTH(depth);

	/* To calculate a mask start with a 1 on the left hand side and right
	 * shift while populating the left hand side with 1's
	 */
	return (int)0x80000000 >> (depth - 1);
}

/*
 * Converts given depth value to its corresponding range value.
 */
static uint32_t __attribute__((pure))
depth_to_range(uint8_t depth)
{
	VERIFY_DEPTH(depth);

	/*
	 * Calculate tbl24 range. (Note: 2^depth = 1 << depth)
	 */
	if (depth <= MAX_DEPTH_TBL24)
		return 1 << (MAX_DEPTH_TBL24 - depth);

	/* Else if depth is greater than 24 */
	return 1 << (RTE_LPM_MAX_DEPTH - depth);
}

#if 0
/*
 * Find an existing lpm table and return a pointer to it.
 */
struct rte_lpm *
rte_lpm_find_existing(const char *name)
{
	struct rte_lpm *l = NULL;
	struct rte_tailq_entry *te;
	struct rte_lpm_list *lpm_list;

	lpm_list = RTE_TAILQ_CAST(rte_lpm_tailq.head, rte_lpm_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, lpm_list, next) {
		l = te->data;
		if (strncmp(name, l->name, RTE_LPM_NAMESIZE) == 0)
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
 * Allocates memory for LPM object
 */
struct rte_lpm *
rte_lpm_create(const char *name, int socket_id,
		const struct rte_lpm_config *config)
{
	char mem_name[RTE_LPM_NAMESIZE];
	struct rte_lpm *lpm = NULL;
	//struct rte_tailq_entry *te;
	uint32_t mem_size, rules_size, tbl8s_size;
	//struct rte_lpm_list *lpm_list;

	//lpm_list = RTE_TAILQ_CAST(rte_lpm_tailq.head, rte_lpm_list);

	RTE_BUILD_BUG_ON(sizeof(struct rte_lpm_tbl_entry) != 4);

	/* Check user arguments. */
	if ((name == NULL) || (socket_id < -1)
			|| config->number_tbl8s > RTE_LPM_MAX_TBL8_NUM_GROUPS) {
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "LPM_%s", name);

	/* Determine the amount of memory to allocate. */
	mem_size = sizeof(*lpm);
	rules_size = sizeof(struct rte_lpm_rule) * config->max_rules;
	tbl8s_size = (sizeof(struct rte_lpm_tbl_entry) *
			RTE_LPM_TBL8_GROUP_NUM_ENTRIES * config->number_tbl8s);

#if 0
	rte_mcfg_tailq_write_lock();

	/* guarantee there's no existing */
	TAILQ_FOREACH(te, lpm_list, next) {
		lpm = te->data;
		if (strncmp(name, lpm->name, RTE_LPM_NAMESIZE) == 0)
			break;
	}

	if (te != NULL) {
		lpm = NULL;
		rte_errno = EEXIST;
		goto exit;
	}

	/* allocate tailq entry */
	te = rte_zmalloc("LPM_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, LPM, "Failed to allocate tailq entry\n");
		rte_errno = ENOMEM;
		goto exit;
	}
#endif

	/* Allocate memory to store the LPM data structures. */
	lpm = rte_zmalloc_socket(mem_name, mem_size,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (lpm == NULL) {
		RTE_LOG(ERR, LPM, "LPM memory allocation failed\n");
		//rte_free(te);
		rte_errno = ENOMEM;
		goto exit;
	}

	lpm->rules_tbl = rte_zmalloc_socket(NULL,
			(size_t)rules_size, RTE_CACHE_LINE_SIZE, socket_id);

	if (lpm->rules_tbl == NULL) {
		RTE_LOG(ERR, LPM, "LPM rules_tbl memory allocation failed\n");
		rte_free(lpm);
		lpm = NULL;
		//rte_free(te);
		rte_errno = ENOMEM;
		goto exit;
	}

	lpm->tbl8 = rte_zmalloc_socket(NULL,
			(size_t)tbl8s_size, RTE_CACHE_LINE_SIZE, socket_id);

	if (lpm->tbl8 == NULL) {
		RTE_LOG(ERR, LPM, "LPM tbl8 memory allocation failed\n");
		rte_free(lpm->rules_tbl);
		rte_free(lpm);
		lpm = NULL;
		//rte_free(te);
		rte_errno = ENOMEM;
		goto exit;
	}

	/* Save user arguments. */
	lpm->max_rules = config->max_rules;
	lpm->number_tbl8s = config->number_tbl8s;
	strlcpy(lpm->name, name, sizeof(lpm->name));

	//te->data = lpm;

	//TAILQ_INSERT_TAIL(lpm_list, te, next);

exit:
	rte_mcfg_tailq_write_unlock();

	return lpm;
}

/*
 * Deallocates memory for given LPM table.
 */
void
rte_lpm_free(struct rte_lpm *lpm)
{
#if 0
	struct rte_lpm_list *lpm_list;
	struct rte_tailq_entry *te;

	/* Check user arguments. */
	if (lpm == NULL)
		return;

	lpm_list = RTE_TAILQ_CAST(rte_lpm_tailq.head, rte_lpm_list);

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

	rte_free(lpm->tbl8);
	rte_free(lpm->rules_tbl);
	rte_free(lpm);
	//rte_free(te);
}

#if 0
/*
 * Adds a rule to the rule table.
 *
 * NOTE: The rule table is split into 32 groups. Each group contains rules that
 * apply to a specific prefix depth (i.e. group 1 contains rules that apply to
 * prefixes with a depth of 1 etc.). In the following code (depth - 1) is used
 * to refer to depth 1 because even though the depth range is 1 - 32, depths
 * are stored in the rule table from 0 - 31.
 * NOTE: Valid range for depth parameter is 1 .. 32 inclusive.
 */
static int32_t
rule_add(struct rte_lpm *lpm, uint32_t ip_masked, uint8_t depth,
	uint32_t next_hop)
{
	uint32_t rule_gindex, rule_index, last_rule;
	int i;

	VERIFY_DEPTH(depth);

	/* Scan through rule group to see if rule already exists. */
	if (lpm->rule_info[depth - 1].used_rules > 0) {

		/* rule_gindex stands for rule group index. */
		rule_gindex = lpm->rule_info[depth - 1].first_rule;
		/* Initialise rule_index to point to start of rule group. */
		rule_index = rule_gindex;
		/* Last rule = Last used rule in this rule group. */
		last_rule = rule_gindex + lpm->rule_info[depth - 1].used_rules;

		for (; rule_index < last_rule; rule_index++) {

			/* If rule already exists update next hop and return. */
			if (lpm->rules_tbl[rule_index].ip == ip_masked) {

				if (lpm->rules_tbl[rule_index].next_hop
						== next_hop)
					return -EEXIST;
				lpm->rules_tbl[rule_index].next_hop = next_hop;

				return rule_index;
			}
		}

		if (rule_index == lpm->max_rules)
			return -ENOSPC;
	} else {
		/* Calculate the position in which the rule will be stored. */
		rule_index = 0;

		for (i = depth - 1; i > 0; i--) {
			if (lpm->rule_info[i - 1].used_rules > 0) {
				rule_index = lpm->rule_info[i - 1].first_rule
						+ lpm->rule_info[i - 1].used_rules;
				break;
			}
		}
		if (rule_index == lpm->max_rules)
			return -ENOSPC;

		lpm->rule_info[depth - 1].first_rule = rule_index;
	}

	/* Make room for the new rule in the array. */
	for (i = RTE_LPM_MAX_DEPTH; i > depth; i--) {
		if (lpm->rule_info[i - 1].first_rule
				+ lpm->rule_info[i - 1].used_rules == lpm->max_rules)
			return -ENOSPC;

		if (lpm->rule_info[i - 1].used_rules > 0) {
			lpm->rules_tbl[lpm->rule_info[i - 1].first_rule
				+ lpm->rule_info[i - 1].used_rules]
					= lpm->rules_tbl[lpm->rule_info[i - 1].first_rule];
			lpm->rule_info[i - 1].first_rule++;
		}
	}

	/* Add the new rule. */
	lpm->rules_tbl[rule_index].ip = ip_masked;
	lpm->rules_tbl[rule_index].next_hop = next_hop;

	/* Increment the used rules counter for this rule group. */
	lpm->rule_info[depth - 1].used_rules++;

	return rule_index;
}

/*
 * Delete a rule from the rule table.
 * NOTE: Valid range for depth parameter is 1 .. 32 inclusive.
 */
static void
rule_delete(struct rte_lpm *lpm, int32_t rule_index, uint8_t depth)
{
	int i;

	VERIFY_DEPTH(depth);

	lpm->rules_tbl[rule_index] =
			lpm->rules_tbl[lpm->rule_info[depth - 1].first_rule
			+ lpm->rule_info[depth - 1].used_rules - 1];

	for (i = depth; i < RTE_LPM_MAX_DEPTH; i++) {
		if (lpm->rule_info[i].used_rules > 0) {
			lpm->rules_tbl[lpm->rule_info[i].first_rule - 1] =
					lpm->rules_tbl[lpm->rule_info[i].first_rule
						+ lpm->rule_info[i].used_rules - 1];
			lpm->rule_info[i].first_rule--;
		}
	}

	lpm->rule_info[depth - 1].used_rules--;
}

/*
 * Finds a rule in rule table.
 * NOTE: Valid range for depth parameter is 1 .. 32 inclusive.
 */
static int32_t
rule_find(struct rte_lpm *lpm, uint32_t ip_masked, uint8_t depth)
{
	uint32_t rule_gindex, last_rule, rule_index;

	VERIFY_DEPTH(depth);

	rule_gindex = lpm->rule_info[depth - 1].first_rule;
	last_rule = rule_gindex + lpm->rule_info[depth - 1].used_rules;

	/* Scan used rules at given depth to find rule. */
	for (rule_index = rule_gindex; rule_index < last_rule; rule_index++) {
		/* If rule is found return the rule index. */
		if (lpm->rules_tbl[rule_index].ip == ip_masked)
			return rule_index;
	}

	/* If rule is not found return -EINVAL. */
	return -EINVAL;
}
#endif

/*
 * Find, clean and allocate a tbl8.
 */
static int32_t
tbl8_alloc(struct rte_lpm_tbl_entry *tbl8, uint32_t number_tbl8s)
{
	uint32_t group_idx; /* tbl8 group index. */
	struct rte_lpm_tbl_entry *tbl8_entry;

	/* Scan through tbl8 to find a free (i.e. INVALID) tbl8 group. */
	for (group_idx = 0; group_idx < number_tbl8s; group_idx++) {
		tbl8_entry = &tbl8[group_idx * RTE_LPM_TBL8_GROUP_NUM_ENTRIES];
		/* If a free tbl8 group is found clean it and set as VALID. */
		if (!tbl8_entry->valid_group) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.next_hop = 0,
				.valid = INVALID,
				.depth = 0,
				.valid_group = VALID,
			};

			memset(&tbl8_entry[0], 0,
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES *
					sizeof(tbl8_entry[0]));

			__atomic_store(tbl8_entry, &new_tbl8_entry,
					__ATOMIC_RELAXED);

			/* Return group index for allocated tbl8 group. */
			return group_idx;
		}
	}

	/* If there are no tbl8 groups free then return error. */
	return -ENOSPC;
}

static void
tbl8_free(struct rte_lpm_tbl_entry *tbl8, uint32_t tbl8_group_start)
{
	/* Set tbl8 group invalid*/
	struct rte_lpm_tbl_entry zero_tbl8_entry = {0};

	__atomic_store(&tbl8[tbl8_group_start], &zero_tbl8_entry,
			__ATOMIC_RELAXED);
}

static __rte_noinline int32_t
add_depth_small(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
		uint32_t next_hop)
{
#define group_idx next_hop
	uint32_t tbl24_index, tbl24_range, tbl8_index, tbl8_group_end, i, j;

	/* Calculate the index into Table24. */
	tbl24_index = ip >> 8;
	tbl24_range = depth_to_range(depth);

	for (i = tbl24_index; i < (tbl24_index + tbl24_range); i++) {
		/*
		 * For invalid OR valid and non-extended tbl 24 entries set
		 * entry.
		 */
		if (!lpm->tbl24[i].valid || (lpm->tbl24[i].valid_group == 0 &&
				lpm->tbl24[i].depth <= depth)) {

			struct rte_lpm_tbl_entry new_tbl24_entry = {
				.next_hop = next_hop,
				.valid = VALID,
				.valid_group = 0,
				.depth = depth,
			};

			/* Setting tbl24 entry in one go to avoid race
			 * conditions
			 */
			__atomic_store(&lpm->tbl24[i], &new_tbl24_entry,
					__ATOMIC_RELEASE);

			continue;
		}

		if (lpm->tbl24[i].valid_group == 1) {
			/* If tbl24 entry is valid and extended calculate the
			 *  index into tbl8.
			 */
			tbl8_index = lpm->tbl24[i].group_idx *
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
			tbl8_group_end = tbl8_index +
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

			for (j = tbl8_index; j < tbl8_group_end; j++) {
				if (!lpm->tbl8[j].valid ||
						lpm->tbl8[j].depth <= depth) {
					struct rte_lpm_tbl_entry
						new_tbl8_entry = {
						.valid = VALID,
						.valid_group = VALID,
						.depth = depth,
						.next_hop = next_hop,
					};

					/*
					 * Setting tbl8 entry in one go to avoid
					 * race conditions
					 */
					__atomic_store(&lpm->tbl8[j],
						&new_tbl8_entry,
						__ATOMIC_RELAXED);

					continue;
				}
			}
		}
	}
#undef group_idx
	return 0;
}

static __rte_noinline int32_t
add_depth_big(struct rte_lpm *lpm, uint32_t ip_masked, uint8_t depth,
		uint32_t next_hop)
{
#define group_idx next_hop
	uint32_t tbl24_index;
	int32_t tbl8_group_index, tbl8_group_start, tbl8_group_end, tbl8_index,
		tbl8_range, i;

	tbl24_index = (ip_masked >> 8);
	tbl8_range = depth_to_range(depth);

	if (!lpm->tbl24[tbl24_index].valid) {
		/* Search for a free tbl8 group. */
		tbl8_group_index = tbl8_alloc(lpm->tbl8, lpm->number_tbl8s);

		/* Check tbl8 allocation was successful. */
		if (tbl8_group_index < 0) {
			return tbl8_group_index;
		}

		/* Find index into tbl8 and range. */
		tbl8_index = (tbl8_group_index *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES) +
				(ip_masked & 0xFF);

		/* Set tbl8 entry. */
		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.valid = VALID,
				.depth = depth,
				.valid_group = lpm->tbl8[i].valid_group,
				.next_hop = next_hop,
			};
			__atomic_store(&lpm->tbl8[i], &new_tbl8_entry,
					__ATOMIC_RELAXED);
		}

		/*
		 * Update tbl24 entry to point to new tbl8 entry. Note: The
		 * ext_flag and tbl8_index need to be updated simultaneously,
		 * so assign whole structure in one go
		 */

		struct rte_lpm_tbl_entry new_tbl24_entry = {
			.group_idx = tbl8_group_index,
			.valid = VALID,
			.valid_group = 1,
			.depth = 0,
		};

		/* The tbl24 entry must be written only after the
		 * tbl8 entries are written.
		 */
		__atomic_store(&lpm->tbl24[tbl24_index], &new_tbl24_entry,
				__ATOMIC_RELEASE);

	} /* If valid entry but not extended calculate the index into Table8. */
	else if (lpm->tbl24[tbl24_index].valid_group == 0) {
		/* Search for free tbl8 group. */
		tbl8_group_index = tbl8_alloc(lpm->tbl8, lpm->number_tbl8s);

		if (tbl8_group_index < 0) {
			return tbl8_group_index;
		}

		tbl8_group_start = tbl8_group_index *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl8_group_end = tbl8_group_start +
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

		/* Populate new tbl8 with tbl24 value. */
		for (i = tbl8_group_start; i < tbl8_group_end; i++) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.valid = VALID,
				.depth = lpm->tbl24[tbl24_index].depth,
				.valid_group = lpm->tbl8[i].valid_group,
				.next_hop = lpm->tbl24[tbl24_index].next_hop,
			};
			__atomic_store(&lpm->tbl8[i], &new_tbl8_entry,
					__ATOMIC_RELAXED);
		}

		tbl8_index = tbl8_group_start + (ip_masked & 0xFF);

		/* Insert new rule into the tbl8 entry. */
		for (i = tbl8_index; i < tbl8_index + tbl8_range; i++) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.valid = VALID,
				.depth = depth,
				.valid_group = lpm->tbl8[i].valid_group,
				.next_hop = next_hop,
			};
			__atomic_store(&lpm->tbl8[i], &new_tbl8_entry,
					__ATOMIC_RELAXED);
		}

		/*
		 * Update tbl24 entry to point to new tbl8 entry. Note: The
		 * ext_flag and tbl8_index need to be updated simultaneously,
		 * so assign whole structure in one go.
		 */

		struct rte_lpm_tbl_entry new_tbl24_entry = {
				.group_idx = tbl8_group_index,
				.valid = VALID,
				.valid_group = 1,
				.depth = 0,
		};

		/* The tbl24 entry must be written only after the
		 * tbl8 entries are written.
		 */
		__atomic_store(&lpm->tbl24[tbl24_index], &new_tbl24_entry,
				__ATOMIC_RELEASE);

	} else { /*
		* If it is valid, extended entry calculate the index into tbl8.
		*/
		tbl8_group_index = lpm->tbl24[tbl24_index].group_idx;
		tbl8_group_start = tbl8_group_index *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl8_index = tbl8_group_start + (ip_masked & 0xFF);

		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {

			if (!lpm->tbl8[i].valid ||
					lpm->tbl8[i].depth <= depth) {
				struct rte_lpm_tbl_entry new_tbl8_entry = {
					.valid = VALID,
					.depth = depth,
					.next_hop = next_hop,
					.valid_group = lpm->tbl8[i].valid_group,
				};

				/*
				 * Setting tbl8 entry in one go to avoid race
				 * condition
				 */
				__atomic_store(&lpm->tbl8[i], &new_tbl8_entry,
						__ATOMIC_RELAXED);

				continue;
			}
		}
	}
#undef group_idx
	return 0;
}

/*
 * Add a route
 */
int
rte_lpm_add(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
		uint32_t next_hop)
{
	int32_t status = 0;
	uint32_t ip_masked;

	/* Check user arguments. */
	if ((lpm == NULL) || (depth < 1) || (depth > RTE_LPM_MAX_DEPTH))
		return -EINVAL;

	ip_masked = ip & depth_to_mask(depth);

#if 0
	/* Add the rule to the rule table. */
	rule_index = rule_add(lpm, ip_masked, depth, next_hop);

	/* Skip table entries update if The rule is the same as
	 * the rule in the rules table.
	 */
	if (rule_index == -EEXIST)
		return 0;

	/* If the is no space available for new rule return error. */
	if (rule_index < 0) {
		return rule_index;
	}
#endif

	if (depth <= MAX_DEPTH_TBL24) {
		status = add_depth_small(lpm, ip_masked, depth, next_hop);
	} else { /* If depth > RTE_LPM_MAX_DEPTH_TBL24 */
		status = add_depth_big(lpm, ip_masked, depth, next_hop);

		/*
		 * If add fails due to exhaustion of tbl8 extensions delete
		 * rule that was added to rule table.
		 */
		if (status < 0) {
			//rule_delete(lpm, rule_index, depth);

			return status;
		}
	}

	return 0;
}

#if 0
/*
 * Look for a rule in the high-level rules table
 */
int
rte_lpm_is_rule_present(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
uint32_t *next_hop)
{
	uint32_t ip_masked;
	int32_t rule_index;

	/* Check user arguments. */
	if ((lpm == NULL) ||
		(next_hop == NULL) ||
		(depth < 1) || (depth > RTE_LPM_MAX_DEPTH))
		return -EINVAL;

	/* Look for the rule using rule_find. */
	ip_masked = ip & depth_to_mask(depth);
	rule_index = rule_find(lpm, ip_masked, depth);

	if (rule_index >= 0) {
		*next_hop = lpm->rules_tbl[rule_index].next_hop;
		return 1;
	}

	/* If rule is not found return 0. */
	return 0;
}

static int32_t
find_previous_rule(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
		uint8_t *sub_rule_depth)
{
	int32_t rule_index;
	uint32_t ip_masked;
	uint8_t prev_depth;

	for (prev_depth = (uint8_t)(depth - 1); prev_depth > 0; prev_depth--) {
		ip_masked = ip & depth_to_mask(prev_depth);

		rule_index = rule_find(lpm, ip_masked, prev_depth);

		if (rule_index >= 0) {
			*sub_rule_depth = prev_depth;
			return rule_index;
		}
	}

	return -1;
}
#endif

static int32_t
delete_depth_small(struct rte_lpm *lpm, uint32_t ip_masked,
	uint8_t depth, uint32_t sub_rule_nhop, uint8_t sub_rule_depth)
{
#define group_idx next_hop
	uint32_t tbl24_range, tbl24_index, tbl8_group_index, tbl8_index, i, j;

	/* Calculate the range and index into Table24. */
	tbl24_range = depth_to_range(depth);
	tbl24_index = (ip_masked >> 8);
	struct rte_lpm_tbl_entry zero_tbl24_entry = {0};

	/*
	 * Firstly check the sub_rule_index. A -1 indicates no replacement rule
	 * and a positive number indicates a sub_rule_index.
	 */
	if (sub_rule_nhop == 0) {
		/*
		 * If no replacement rule exists then invalidate entries
		 * associated with this rule.
		 */
		for (i = tbl24_index; i < (tbl24_index + tbl24_range); i++) {

			if (lpm->tbl24[i].valid_group == 0 &&
					lpm->tbl24[i].depth <= depth) {
				__atomic_store(&lpm->tbl24[i],
					&zero_tbl24_entry, __ATOMIC_RELEASE);
			} else if (lpm->tbl24[i].valid_group == 1) {
				/*
				 * If TBL24 entry is extended, then there has
				 * to be a rule with depth >= 25 in the
				 * associated TBL8 group.
				 */

				tbl8_group_index = lpm->tbl24[i].group_idx;
				tbl8_index = tbl8_group_index *
						RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

				for (j = tbl8_index; j < (tbl8_index +
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES); j++) {

					if (lpm->tbl8[j].depth <= depth)
						lpm->tbl8[j].valid = INVALID;
				}
			}
		}
	} else {
		/*
		 * If a replacement rule exists then modify entries
		 * associated with this rule.
		 */

		struct rte_lpm_tbl_entry new_tbl24_entry = {
			.next_hop = sub_rule_nhop,
			.valid = VALID,
			.valid_group = 0,
			.depth = sub_rule_depth,
		};

		struct rte_lpm_tbl_entry new_tbl8_entry = {
			.valid = VALID,
			.valid_group = VALID,
			.depth = sub_rule_depth,
			.next_hop = sub_rule_nhop,
		};

		for (i = tbl24_index; i < (tbl24_index + tbl24_range); i++) {

			if (lpm->tbl24[i].valid_group == 0 &&
					lpm->tbl24[i].depth <= depth) {
				__atomic_store(&lpm->tbl24[i], &new_tbl24_entry,
						__ATOMIC_RELEASE);
			} else  if (lpm->tbl24[i].valid_group == 1) {
				/*
				 * If TBL24 entry is extended, then there has
				 * to be a rule with depth >= 25 in the
				 * associated TBL8 group.
				 */

				tbl8_group_index = lpm->tbl24[i].group_idx;
				tbl8_index = tbl8_group_index *
						RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

				for (j = tbl8_index; j < (tbl8_index +
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES); j++) {

					if (lpm->tbl8[j].depth <= depth)
						__atomic_store(&lpm->tbl8[j],
							&new_tbl8_entry,
							__ATOMIC_RELAXED);
				}
			}
		}
	}
#undef group_idx
	return 0;
}

/*
 * Checks if table 8 group can be recycled.
 *
 * Return of -EEXIST means tbl8 is in use and thus can not be recycled.
 * Return of -EINVAL means tbl8 is empty and thus can be recycled
 * Return of value > -1 means tbl8 is in use but has all the same values and
 * thus can be recycled
 */
static int32_t
tbl8_recycle_check(struct rte_lpm_tbl_entry *tbl8,
		uint32_t tbl8_group_start)
{
	uint32_t tbl8_group_end, i;
	tbl8_group_end = tbl8_group_start + RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

	/*
	 * Check the first entry of the given tbl8. If it is invalid we know
	 * this tbl8 does not contain any rule with a depth < RTE_LPM_MAX_DEPTH
	 *  (As they would affect all entries in a tbl8) and thus this table
	 *  can not be recycled.
	 */
	if (tbl8[tbl8_group_start].valid) {
		/*
		 * If first entry is valid check if the depth is less than 24
		 * and if so check the rest of the entries to verify that they
		 * are all of this depth.
		 */
		if (tbl8[tbl8_group_start].depth <= MAX_DEPTH_TBL24) {
			for (i = (tbl8_group_start + 1); i < tbl8_group_end;
					i++) {

				if (tbl8[i].depth !=
						tbl8[tbl8_group_start].depth) {

					return -EEXIST;
				}
			}
			/* If all entries are the same return the tb8 index */
			return tbl8_group_start;
		}

		return -EEXIST;
	}
	/*
	 * If the first entry is invalid check if the rest of the entries in
	 * the tbl8 are invalid.
	 */
	for (i = (tbl8_group_start + 1); i < tbl8_group_end; i++) {
		if (tbl8[i].valid)
			return -EEXIST;
	}
	/* If no valid entries are found then return -EINVAL. */
	return -EINVAL;
}

static int32_t
delete_depth_big(struct rte_lpm *lpm, uint32_t ip_masked,
	uint8_t depth, uint32_t sub_rule_nhop, uint8_t sub_rule_depth)
{
#define group_idx next_hop
	uint32_t tbl24_index, tbl8_group_index, tbl8_group_start, tbl8_index,
			tbl8_range, i;
	int32_t tbl8_recycle_index;

	/*
	 * Calculate the index into tbl24 and range. Note: All depths larger
	 * than MAX_DEPTH_TBL24 are associated with only one tbl24 entry.
	 */
	tbl24_index = ip_masked >> 8;

	/* Calculate the index into tbl8 and range. */
	tbl8_group_index = lpm->tbl24[tbl24_index].group_idx;
	tbl8_group_start = tbl8_group_index * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
	tbl8_index = tbl8_group_start + (ip_masked & 0xFF);
	tbl8_range = depth_to_range(depth);

	if (sub_rule_nhop == 0) {
		/*
		 * Loop through the range of entries on tbl8 for which the
		 * rule_to_delete must be removed or modified.
		 */
		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {
			if (lpm->tbl8[i].depth <= depth)
				lpm->tbl8[i].valid = INVALID;
		}
	} else {
		/* Set new tbl8 entry. */
		struct rte_lpm_tbl_entry new_tbl8_entry = {
			.valid = VALID,
			.depth = sub_rule_depth,
			.valid_group = lpm->tbl8[tbl8_group_start].valid_group,
			.next_hop = sub_rule_nhop,
		};

		/*
		 * Loop through the range of entries on tbl8 for which the
		 * rule_to_delete must be modified.
		 */
		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {
			if (lpm->tbl8[i].depth <= depth)
				__atomic_store(&lpm->tbl8[i], &new_tbl8_entry,
						__ATOMIC_RELAXED);
		}
	}

	/*
	 * Check if there are any valid entries in this tbl8 group. If all
	 * tbl8 entries are invalid we can free the tbl8 and invalidate the
	 * associated tbl24 entry.
	 */

	tbl8_recycle_index = tbl8_recycle_check(lpm->tbl8, tbl8_group_start);

	if (tbl8_recycle_index == -EINVAL) {
		/* Set tbl24 before freeing tbl8 to avoid race condition.
		 * Prevent the free of the tbl8 group from hoisting.
		 */
		lpm->tbl24[tbl24_index].valid = 0;
		__atomic_thread_fence(__ATOMIC_RELEASE);
		tbl8_free(lpm->tbl8, tbl8_group_start);
	} else if (tbl8_recycle_index > -1) {
		/* Update tbl24 entry. */
		struct rte_lpm_tbl_entry new_tbl24_entry = {
			.next_hop = lpm->tbl8[tbl8_recycle_index].next_hop,
			.valid = VALID,
			.valid_group = 0,
			.depth = lpm->tbl8[tbl8_recycle_index].depth,
		};

		/* Set tbl24 before freeing tbl8 to avoid race condition.
		 * Prevent the free of the tbl8 group from hoisting.
		 */
		__atomic_store(&lpm->tbl24[tbl24_index], &new_tbl24_entry,
				__ATOMIC_RELAXED);
		__atomic_thread_fence(__ATOMIC_RELEASE);
		tbl8_free(lpm->tbl8, tbl8_group_start);
	}
#undef group_idx
	return 0;
}

/*
 * Deletes a rule
 */
int
rte_lpm_delete(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
	uint8_t sub_rule_depth, uint32_t sub_rule_nhop)
{
	//int32_t rule_to_delete_index;
	uint32_t ip_masked;
	//uint8_t sub_rule_depth;
	/*
	 * Check input arguments. Note: IP must be a positive integer of 32
	 * bits in length therefore it need not be checked.
	 */
	if ((lpm == NULL) || (depth < 1) || (depth > RTE_LPM_MAX_DEPTH)) {
		return -EINVAL;
	}

	ip_masked = ip & depth_to_mask(depth);

#if 0
	/*
	 * Find the index of the input rule, that needs to be deleted, in the
	 * rule table.
	 */
	rule_to_delete_index = rule_find(lpm, ip_masked, depth);

	/*
	 * Check if rule_to_delete_index was found. If no rule was found the
	 * function rule_find returns -EINVAL.
	 */
	if (rule_to_delete_index < 0)
		return -EINVAL;

	/* Delete the rule from the rule table. */
	rule_delete(lpm, rule_to_delete_index, depth);
#endif

	/*
	 * Find rule to replace the rule_to_delete. If there is no rule to
	 * replace the rule_to_delete we return -1 and invalidate the table
	 * entries associated with this rule.
	 */
	//sub_rule_depth = *psub_rule_depth;
	//sub_rule_index = find_previous_rule(lpm, ip, depth, &sub_rule_depth);

	/*
	 * If the input depth value is less than 25 use function
	 * delete_depth_small otherwise use delete_depth_big.
	 */
	if (depth <= MAX_DEPTH_TBL24) {
		return delete_depth_small(lpm, ip_masked, depth,
				sub_rule_nhop, sub_rule_depth);
	} else { /* If depth > MAX_DEPTH_TBL24 */
		return delete_depth_big(lpm, ip_masked, depth, sub_rule_nhop,
				sub_rule_depth);
	}
}

/*
 * Delete all rules from the LPM table.
 */
void
rte_lpm_delete_all(struct rte_lpm *lpm)
{
	/* Zero rule information. */
	memset(lpm->rule_info, 0, sizeof(lpm->rule_info));

	/* Zero tbl24. */
	memset(lpm->tbl24, 0, sizeof(lpm->tbl24));

	/* Zero tbl8. */
	memset(lpm->tbl8, 0, sizeof(lpm->tbl8[0])
			* RTE_LPM_TBL8_GROUP_NUM_ENTRIES * lpm->number_tbl8s);

	/* Delete all rules form the rules table. */
	memset(lpm->rules_tbl, 0, sizeof(lpm->rules_tbl[0]) * lpm->max_rules);
}
