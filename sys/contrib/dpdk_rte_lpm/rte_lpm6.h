/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#ifndef _RTE_LPM6_H_
#define _RTE_LPM6_H_

/**
 * @file
 * RTE Longest Prefix Match for IPv6 (LPM6)
 */

#ifdef __cplusplus
extern "C" {
#endif


#define RTE_LPM6_MAX_DEPTH               128
#define RTE_LPM6_IPV6_ADDR_SIZE           16
/** Max number of characters in LPM name. */
#define RTE_LPM6_NAMESIZE                 32

/** LPM structure. */
struct rte_lpm6;

struct nhop_object;
struct rte_lpm6_external {
	struct nhop_object	**nh_idx;	/**< # -> idx mappings  */
	uint32_t		default_idx;	/* nhop index of default route */
	uint32_t		fibnum;		/* fib index */
};

/** LPM configuration structure. */
struct rte_lpm6_config {
	uint32_t max_rules;      /**< Max number of rules. */
	uint32_t number_tbl8s;   /**< Number of tbl8s to allocate. */
	int flags;               /**< This field is currently unused. */
};

#define	RTE_LPM6_RULE_SIZE	32
struct rte_lpm6_rule *fill_rule6(char *buffer, const uint8_t *ip,
				uint8_t depth, uint32_t next_hop);
/**
 * Create an LPM object.
 *
 * @param name
 *   LPM object name
 * @param socket_id
 *   NUMA socket ID for LPM table memory allocation
 * @param config
 *   Structure containing the configuration
 * @return
 *   Handle to LPM object on success, NULL otherwise with rte_errno set
 *   to an appropriate values. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - invalid parameter passed to function
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_lpm6 *
rte_lpm6_create(const char *name, int socket_id,
		const struct rte_lpm6_config *config);

/**
 * Find an existing LPM object and return a pointer to it.
 *
 * @param name
 *   Name of the lpm object as passed to rte_lpm6_create()
 * @return
 *   Pointer to lpm object or NULL if object not found with rte_errno
 *   set appropriately. Possible rte_errno values include:
 *    - ENOENT - required entry not available to return.
 */
struct rte_lpm6 *
rte_lpm6_find_existing(const char *name);

/**
 * Free an LPM object.
 *
 * @param lpm
 *   LPM object handle
 * @return
 *   None
 */
void
rte_lpm6_free(struct rte_lpm6 *lpm);

/**
 * Add a rule to the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP of the rule to be added to the LPM table
 * @param depth
 *   Depth of the rule to be added to the LPM table
 * @param next_hop
 *   Next hop of the rule to be added to the LPM table
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_lpm6_add(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
	     uint32_t next_hop, int is_new_rule);

/**
 * Check if a rule is present in the LPM table,
 * and provide its next hop if it is.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP of the rule to be searched
 * @param depth
 *   Depth of the rule to searched
 * @param next_hop
 *   Next hop of the rule (valid only if it is found)
 * @return
 *   1 if the rule exists, 0 if it does not, a negative value on failure
 */
int
rte_lpm6_is_rule_present(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
			 uint32_t *next_hop);

/**
 * Delete a rule from the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP of the rule to be deleted from the LPM table
 * @param depth
 *   Depth of the rule to be deleted from the LPM table
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_lpm6_delete(struct rte_lpm6 *lpm, const uint8_t *ip, uint8_t depth,
			struct rte_lpm6_rule *lsp_rule);

/**
 * Delete a rule from the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ips
 *   Array of IPs to be deleted from the LPM table
 * @param depths
 *   Array of depths of the rules to be deleted from the LPM table
 * @param n
 *   Number of rules to be deleted from the LPM table
 * @return
 *   0 on success, negative value otherwise.
 */
int
rte_lpm6_delete_bulk_func(struct rte_lpm6 *lpm,
		uint8_t ips[][RTE_LPM6_IPV6_ADDR_SIZE], uint8_t *depths, unsigned n);

/**
 * Delete all rules from the LPM table.
 *
 * @param lpm
 *   LPM object handle
 */
void
rte_lpm6_delete_all(struct rte_lpm6 *lpm);

/**
 * Lookup an IP into the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP to be looked up in the LPM table
 * @param next_hop
 *   Next hop of the most specific rule found for IP (valid on lookup hit only)
 * @return
 *   -EINVAL for incorrect arguments, -ENOENT on lookup miss, 0 on lookup hit
 */
int
rte_lpm6_lookup(const struct rte_lpm6 *lpm, const uint8_t *ip, uint32_t *next_hop);

/**
 * Lookup multiple IP addresses in an LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ips
 *   Array of IPs to be looked up in the LPM table
 * @param next_hops
 *   Next hop of the most specific rule found for IP (valid on lookup hit only).
 *   This is an array of two byte values. The next hop will be stored on
 *   each position on success; otherwise the position will be set to -1.
 * @param n
 *   Number of elements in ips (and next_hops) array to lookup.
 *  @return
 *   -EINVAL for incorrect arguments, otherwise 0
 */
int
rte_lpm6_lookup_bulk_func(const struct rte_lpm6 *lpm,
		uint8_t ips[][RTE_LPM6_IPV6_ADDR_SIZE],
		int32_t *next_hops, unsigned int n);

#ifdef __cplusplus
}
#endif

#endif
