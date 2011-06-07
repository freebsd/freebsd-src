/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2008 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2007      Simula Research Laboratory. All rights reserved.
 * Copyright (c) 2007      Silicon Graphics Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *      Implementation of LASH algorithm Calculation functions
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <complib/cl_debug.h>
#include <complib/cl_qmap.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_log.h>

/* //////////////////////////// */
/*  Local types                 */
/* //////////////////////////// */

enum {
	UNQUEUED,
	Q_MEMBER,
	MST_MEMBER,
	MAX_INT = 9999,
	NONE = MAX_INT
};

typedef struct _cdg_vertex {
	int num_dependencies;
	struct _cdg_vertex **dependency;
	int from;
	int to;
	int seen;
	int temp;
	int visiting_number;
	struct _cdg_vertex *next;
	int num_temp_depend;
	int num_using_vertex;
	int *num_using_this_depend;
} cdg_vertex_t;

typedef struct _reachable_dest {
	int switch_id;
	struct _reachable_dest *next;
} reachable_dest_t;

typedef struct _switch {
	osm_switch_t *p_sw;
	int *dij_channels;
	int id;
	int used_channels;
	int q_state;
	struct routing_table {
		unsigned out_link;
		unsigned lane;
	} *routing_table;
	unsigned int num_connections;
	int *virtual_physical_port_table;
	int *phys_connections;
} switch_t;

typedef struct _lash {
	osm_opensm_t *p_osm;
	int num_switches;
	uint8_t vl_min;
	int balance_limit;
	switch_t **switches;
	cdg_vertex_t ****cdg_vertex_matrix;
	int *num_mst_in_lane;
	int ***virtual_location;
} lash_t;

static cdg_vertex_t *create_cdg_vertex(unsigned num_switches)
{
	cdg_vertex_t *cdg_vertex = (cdg_vertex_t *) malloc(sizeof(cdg_vertex_t));

	cdg_vertex->dependency = malloc((num_switches - 1) * sizeof(cdg_vertex_t *));
	cdg_vertex->num_using_this_depend = (int *)malloc((num_switches - 1) * sizeof(int));
	return cdg_vertex;
}

static void connect_switches(lash_t * p_lash, int sw1, int sw2, int phy_port_1)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	unsigned num = p_lash->switches[sw1]->num_connections;

	p_lash->switches[sw1]->phys_connections[num] = sw2;
	p_lash->switches[sw1]->virtual_physical_port_table[num] = phy_port_1;
	p_lash->switches[sw1]->num_connections++;

	OSM_LOG(p_log, OSM_LOG_VERBOSE,
		"LASH connect: %d, %d, %d\n", sw1, sw2,
		phy_port_1);

}

static osm_switch_t *get_osm_switch_from_port(osm_port_t * port)
{
	osm_physp_t *p = port->p_physp;
	if (p->p_node->sw)
		return p->p_node->sw;
	else if (p->p_remote_physp->p_node->sw)
		return p->p_remote_physp->p_node->sw;
	return NULL;
}

#if 0
static int randint(int high)
{
	int r;

	if (high == 0)
		return 0;
	r = rand();
	high++;
	return (r % high);
}
#endif

static int cycle_exists(cdg_vertex_t * start, cdg_vertex_t * current,
			cdg_vertex_t * prev, int visit_num)
{
	cdg_vertex_t *h;
	int i, new_visit_num;
	int cycle_found = 0;

	if (current != NULL && current->visiting_number > 0) {
		if (visit_num > current->visiting_number && current->seen == 0) {
			h = start;
			cycle_found = 1;
		}
	} else {
		if (current == NULL) {
			current = start;
			CL_ASSERT(prev == NULL);
		}

		current->visiting_number = visit_num;

		if (prev != NULL) {
			prev->next = current;
			CL_ASSERT(prev->to == current->from);
			CL_ASSERT(prev->visiting_number > 0);
		}

		new_visit_num = visit_num + 1;

		for (i = 0; i < current->num_dependencies; i++) {
			cycle_found =
			    cycle_exists(start, current->dependency[i], current,
					 new_visit_num);
			if (cycle_found == 1)
				i = current->num_dependencies;
		}

		current->seen = 1;
		if (prev != NULL)
			prev->next = NULL;
	}

	return cycle_found;
}

static void remove_semipermanent_depend_for_sp(lash_t * p_lash, int sw,
					       int dest_switch, int lane)
{
	switch_t **switches = p_lash->switches;
	cdg_vertex_t ****cdg_vertex_matrix = p_lash->cdg_vertex_matrix;
	int i_next_switch, output_link, i, next_link, i_next_next_switch,
	    depend = 0;
	cdg_vertex_t *v;
	int found;

	output_link = switches[sw]->routing_table[dest_switch].out_link;
	i_next_switch = switches[sw]->phys_connections[output_link];

	while (sw != dest_switch) {
		v = cdg_vertex_matrix[lane][sw][i_next_switch];
		CL_ASSERT(v != NULL);

		if (v->num_using_vertex == 1) {

			cdg_vertex_matrix[lane][sw][i_next_switch] = NULL;

			free(v);
		} else {
			v->num_using_vertex--;
			if (i_next_switch != dest_switch) {
				next_link =
				    switches[i_next_switch]->routing_table[dest_switch].out_link;
				i_next_next_switch =
				    switches[i_next_switch]->phys_connections[next_link];
				found = 0;

				for (i = 0; i < v->num_dependencies; i++)
					if (v->dependency[i] ==
					    cdg_vertex_matrix[lane][i_next_switch]
					    [i_next_next_switch]) {
						found = 1;
						depend = i;
					}

				CL_ASSERT(found);

				if (v->num_using_this_depend[depend] == 1) {
					for (i = depend;
					     i < v->num_dependencies - 1; i++) {
						v->dependency[i] =
						    v->dependency[i + 1];
						v->num_using_this_depend[i] =
						    v->num_using_this_depend[i +
									     1];
					}

					v->num_dependencies--;
				} else
					v->num_using_this_depend[depend]--;
			}
		}

		sw = i_next_switch;
		output_link = switches[sw]->routing_table[dest_switch].out_link;

		if (sw != dest_switch)
			i_next_switch =
			    switches[sw]->phys_connections[output_link];
	}
}

inline static void enqueue(cl_list_t * bfsq, switch_t * sw)
{
	CL_ASSERT(sw->q_state == UNQUEUED);
	sw->q_state = Q_MEMBER;
	cl_list_insert_tail(bfsq, sw);
}

inline static void dequeue(cl_list_t * bfsq, switch_t ** sw)
{
	*sw = (switch_t *) cl_list_remove_head(bfsq);
	CL_ASSERT((*sw)->q_state == Q_MEMBER);
	(*sw)->q_state = MST_MEMBER;
}

static int get_phys_connection(switch_t *sw, int switch_to)
{
	unsigned int i = 0;

	for (i = 0; i < sw->num_connections; i++)
		if (sw->phys_connections[i] == switch_to)
			return i;
	return i;
}

static void shortest_path(lash_t * p_lash, int ir)
{
	switch_t **switches = p_lash->switches, *sw, *swi;
	unsigned int i;
	cl_list_t bfsq;

	cl_list_construct(&bfsq);
	cl_list_init(&bfsq, 20);

	enqueue(&bfsq, switches[ir]);

	while (!cl_is_list_empty(&bfsq)) {
		dequeue(&bfsq, &sw);
		for (i = 0; i < sw->num_connections; i++) {
			swi = switches[sw->phys_connections[i]];
			if (swi->q_state == UNQUEUED) {
				enqueue(&bfsq, swi);
				sw->dij_channels[sw->used_channels++] = swi->id;
			}
		}
	}

	cl_list_destroy(&bfsq);
}

static void generate_routing_func_for_mst(lash_t * p_lash, int sw_id,
					  reachable_dest_t ** destinations)
{
	int i, next_switch;
	switch_t *sw = p_lash->switches[sw_id];
	int num_channels = sw->used_channels;
	reachable_dest_t *dest, *i_dest, *concat_dest = NULL, *prev;

	for (i = 0; i < num_channels; i++) {
		next_switch = sw->dij_channels[i];
		generate_routing_func_for_mst(p_lash, next_switch, &dest);

		i_dest = dest;
		prev = i_dest;

		while (i_dest != NULL) {
			if (sw->routing_table[i_dest->switch_id].out_link ==
			    NONE) {
				sw->routing_table[i_dest->switch_id].out_link =
				    get_phys_connection(sw, next_switch);
			}

			prev = i_dest;
			i_dest = i_dest->next;
		}

		CL_ASSERT(prev->next == NULL);
		prev->next = concat_dest;
		concat_dest = dest;
	}

	i_dest = (reachable_dest_t *) malloc(sizeof(reachable_dest_t));
	i_dest->switch_id = sw->id;
	i_dest->next = concat_dest;
	*destinations = i_dest;
}

static void generate_cdg_for_sp(lash_t * p_lash, int sw, int dest_switch,
				int lane)
{
	unsigned num_switches = p_lash->num_switches;
	switch_t **switches = p_lash->switches;
	cdg_vertex_t ****cdg_vertex_matrix = p_lash->cdg_vertex_matrix;
	int next_switch, output_link, j, exists;
	cdg_vertex_t *v, *prev = NULL;

	output_link = switches[sw]->routing_table[dest_switch].out_link;
	next_switch = switches[sw]->phys_connections[output_link];

	while (sw != dest_switch) {

		if (cdg_vertex_matrix[lane][sw][next_switch] == NULL) {
			unsigned i;
			v = create_cdg_vertex(num_switches);

			for (i = 0; i < num_switches - 1; i++) {
				v->dependency[i] = NULL;
				v->num_using_this_depend[i] = 0;
			}

			v->num_using_vertex = 0;
			v->num_dependencies = 0;
			v->from = sw;
			v->to = next_switch;
			v->seen = 0;
			v->visiting_number = 0;
			v->next = NULL;
			v->temp = 1;
			v->num_temp_depend = 0;

			cdg_vertex_matrix[lane][sw][next_switch] = v;
		} else
			v = cdg_vertex_matrix[lane][sw][next_switch];

		v->num_using_vertex++;

		if (prev != NULL) {
			exists = 0;

			for (j = 0; j < prev->num_dependencies; j++)
				if (prev->dependency[j] == v) {
					exists = 1;
					prev->num_using_this_depend[j]++;
				}

			if (exists == 0) {
				prev->dependency[prev->num_dependencies] = v;
				prev->num_using_this_depend[prev->num_dependencies]++;
				prev->num_dependencies++;

				CL_ASSERT(prev->num_dependencies < (int)num_switches);

				if (prev->temp == 0)
					prev->num_temp_depend++;

			}
		}

		sw = next_switch;
		output_link = switches[sw]->routing_table[dest_switch].out_link;

		if (sw != dest_switch) {
			CL_ASSERT(output_link != NONE);
			next_switch = switches[sw]->phys_connections[output_link];
		}

		prev = v;
	}
}

static void set_temp_depend_to_permanent_for_sp(lash_t * p_lash, int sw,
						int dest_switch, int lane)
{
	switch_t **switches = p_lash->switches;
	cdg_vertex_t ****cdg_vertex_matrix = p_lash->cdg_vertex_matrix;
	int next_switch, output_link;
	cdg_vertex_t *v;

	output_link = switches[sw]->routing_table[dest_switch].out_link;
	next_switch = switches[sw]->phys_connections[output_link];

	while (sw != dest_switch) {
		v = cdg_vertex_matrix[lane][sw][next_switch];
		CL_ASSERT(v != NULL);

		if (v->temp == 1)
			v->temp = 0;
		else
			v->num_temp_depend = 0;

		sw = next_switch;
		output_link = switches[sw]->routing_table[dest_switch].out_link;

		if (sw != dest_switch)
			next_switch =
			    switches[sw]->phys_connections[output_link];
	}

}

static void remove_temp_depend_for_sp(lash_t * p_lash, int sw, int dest_switch,
				      int lane)
{
	switch_t **switches = p_lash->switches;
	cdg_vertex_t ****cdg_vertex_matrix = p_lash->cdg_vertex_matrix;
	int next_switch, output_link, i;
	cdg_vertex_t *v;

	output_link = switches[sw]->routing_table[dest_switch].out_link;
	next_switch = switches[sw]->phys_connections[output_link];

	while (sw != dest_switch) {
		v = cdg_vertex_matrix[lane][sw][next_switch];
		CL_ASSERT(v != NULL);

		if (v->temp == 1) {
			cdg_vertex_matrix[lane][sw][next_switch] = NULL;
			free(v);
		} else {
			CL_ASSERT(v->num_temp_depend <= v->num_dependencies);
			v->num_dependencies =
			    v->num_dependencies - v->num_temp_depend;
			v->num_temp_depend = 0;
			v->num_using_vertex--;

			for (i = v->num_dependencies;
			     i < p_lash->num_switches - 1; i++)
				v->num_using_this_depend[i] = 0;
		}

		sw = next_switch;
		output_link = switches[sw]->routing_table[dest_switch].out_link;

		if (sw != dest_switch)
			next_switch =
			    switches[sw]->phys_connections[output_link];

	}
}

static void balance_virtual_lanes(lash_t * p_lash, unsigned lanes_needed)
{
	unsigned num_switches = p_lash->num_switches;
	cdg_vertex_t ****cdg_vertex_matrix = p_lash->cdg_vertex_matrix;
	int *num_mst_in_lane = p_lash->num_mst_in_lane;
	int ***virtual_location = p_lash->virtual_location;
	int min_filled_lane, max_filled_lane, medium_filled_lane, trials;
	int old_min_filled_lane, old_max_filled_lane, new_num_min_lane,
	    new_num_max_lane;
	unsigned int i, j;
	int src, dest, start, next_switch, output_link;
	int next_switch2, output_link2;
	int stop = 0, cycle_found;
	int cycle_found2;

	max_filled_lane = 0;
	min_filled_lane = lanes_needed - 1;

	if (max_filled_lane > 1)
		medium_filled_lane = max_filled_lane - 1;

	trials = num_mst_in_lane[max_filled_lane];
	if (lanes_needed == 1)
		stop = 1;

	while (stop == 0) {
		src = abs(rand()) % (num_switches);
		dest = abs(rand()) % (num_switches);

		while (virtual_location[src][dest][max_filled_lane] != 1) {
			start = dest;
			if (dest == num_switches - 1)
				dest = 0;
			else
				dest++;

			while (dest != start
			       && virtual_location[src][dest][max_filled_lane]
			       != 1) {
				if (dest == num_switches - 1)
					dest = 0;
				else
					dest++;
			}

			if (virtual_location[src][dest][max_filled_lane] != 1) {
				if (src == num_switches - 1)
					src = 0;
				else
					src++;
			}
		}

		generate_cdg_for_sp(p_lash, src, dest, min_filled_lane);
		generate_cdg_for_sp(p_lash, dest, src, min_filled_lane);

		output_link = p_lash->switches[src]->routing_table[dest].out_link;
		next_switch = p_lash->switches[src]->phys_connections[output_link];

		output_link2 = p_lash->switches[dest]->routing_table[src].out_link;
		next_switch2 = p_lash->switches[dest]->phys_connections[output_link2];

		CL_ASSERT(cdg_vertex_matrix[min_filled_lane][src][next_switch] != NULL);
		CL_ASSERT(cdg_vertex_matrix[min_filled_lane][dest][next_switch2] != NULL);

		cycle_found =
		    cycle_exists(cdg_vertex_matrix[min_filled_lane][src][next_switch], NULL, NULL,
				 1);
		cycle_found2 =
		    cycle_exists(cdg_vertex_matrix[min_filled_lane][dest][next_switch2], NULL, NULL,
				 1);

		for (i = 0; i < num_switches; i++)
			for (j = 0; j < num_switches; j++)
				if (cdg_vertex_matrix[min_filled_lane][i][j] != NULL) {
					cdg_vertex_matrix[min_filled_lane][i][j]->visiting_number =
					    0;
					cdg_vertex_matrix[min_filled_lane][i][j]->seen = 0;
				}

		if (cycle_found == 1 || cycle_found2 == 1) {
			remove_temp_depend_for_sp(p_lash, src, dest, min_filled_lane);
			remove_temp_depend_for_sp(p_lash, dest, src, min_filled_lane);

			virtual_location[src][dest][max_filled_lane] = 2;
			virtual_location[dest][src][max_filled_lane] = 2;
			trials--;
			trials--;
		} else {
			set_temp_depend_to_permanent_for_sp(p_lash, src, dest, min_filled_lane);
			set_temp_depend_to_permanent_for_sp(p_lash, dest, src, min_filled_lane);

			num_mst_in_lane[max_filled_lane]--;
			num_mst_in_lane[max_filled_lane]--;
			num_mst_in_lane[min_filled_lane]++;
			num_mst_in_lane[min_filled_lane]++;

			remove_semipermanent_depend_for_sp(p_lash, src, dest, max_filled_lane);
			remove_semipermanent_depend_for_sp(p_lash, dest, src, max_filled_lane);
			virtual_location[src][dest][max_filled_lane] = 0;
			virtual_location[dest][src][max_filled_lane] = 0;
			virtual_location[src][dest][min_filled_lane] = 1;
			virtual_location[dest][src][min_filled_lane] = 1;
			p_lash->switches[src]->routing_table[dest].lane = min_filled_lane;
			p_lash->switches[dest]->routing_table[src].lane = min_filled_lane;
		}

		if (trials == 0)
			stop = 1;
		else {
			if (num_mst_in_lane[max_filled_lane] - num_mst_in_lane[min_filled_lane] <
			    p_lash->balance_limit)
				stop = 1;
		}

		old_min_filled_lane = min_filled_lane;
		old_max_filled_lane = max_filled_lane;

		new_num_min_lane = MAX_INT;
		new_num_max_lane = 0;

		for (i = 0; i < lanes_needed; i++) {

			if (num_mst_in_lane[i] < new_num_min_lane) {
				new_num_min_lane = num_mst_in_lane[i];
				min_filled_lane = i;
			}

			if (num_mst_in_lane[i] > new_num_max_lane) {
				new_num_max_lane = num_mst_in_lane[i];
				max_filled_lane = i;
			}
		}

		if (old_min_filled_lane != min_filled_lane) {
			trials = num_mst_in_lane[max_filled_lane];
			for (i = 0; i < num_switches; i++)
				for (j = 0; j < num_switches; j++)
					if (virtual_location[i][j][max_filled_lane] == 2)
						virtual_location[i][j][max_filled_lane] = 1;
		}

		if (old_max_filled_lane != max_filled_lane) {
			trials = num_mst_in_lane[max_filled_lane];
			for (i = 0; i < num_switches; i++)
				for (j = 0; j < num_switches; j++)
					if (virtual_location[i][j][old_max_filled_lane] == 2)
						virtual_location[i][j][old_max_filled_lane] = 1;
		}
	}
}

static switch_t *switch_create(lash_t * p_lash, unsigned id, osm_switch_t * p_sw)
{
	unsigned num_switches = p_lash->num_switches;
	unsigned num_ports = p_sw->num_ports;
	switch_t *sw;
	unsigned int i;

	sw = malloc(sizeof(*sw));
	if (!sw)
		return NULL;

	memset(sw, 0, sizeof(*sw));

	sw->id = id;
	sw->dij_channels = malloc(num_ports * sizeof(int));
	if (!sw->dij_channels) {
		free(sw);
		return NULL;
	}

	sw->virtual_physical_port_table = malloc(num_ports * sizeof(int));
	if (!sw->virtual_physical_port_table) {
		free(sw->dij_channels);
		free(sw);
		return NULL;
	}

	sw->phys_connections = malloc(num_ports * sizeof(int));
	if (!sw->phys_connections) {
		free(sw->virtual_physical_port_table);
		free(sw->dij_channels);
		free(sw);
		return NULL;
	}

	sw->routing_table = malloc(num_switches * sizeof(sw->routing_table[0]));
	if (!sw->routing_table) {
		free(sw->phys_connections);
		free(sw->virtual_physical_port_table);
		free(sw->dij_channels);
		free(sw);
		return NULL;
	}

	for (i = 0; i < num_switches; i++) {
		sw->routing_table[i].out_link = NONE;
		sw->routing_table[i].lane = NONE;
	}

	for (i = 0; i < num_ports; i++) {
		sw->virtual_physical_port_table[i] = -1;
		sw->phys_connections[i] = NONE;
	}

	sw->p_sw = p_sw;
	if (p_sw)
		p_sw->priv = sw;

	return sw;
}

static void switch_delete(switch_t * sw)
{
	if (sw->dij_channels)
		free(sw->dij_channels);
	if (sw->virtual_physical_port_table)
		free(sw->virtual_physical_port_table);
	if (sw->phys_connections)
		free(sw->phys_connections);
	if (sw->routing_table)
		free(sw->routing_table);
	free(sw);
}

static void free_lash_structures(lash_t * p_lash)
{
	unsigned int i, j, k;
	unsigned num_switches = p_lash->num_switches;
	osm_log_t *p_log = &p_lash->p_osm->log;

	OSM_LOG_ENTER(p_log);

	// free cdg_vertex_matrix
	for (i = 0; i < p_lash->vl_min; i++) {
		for (j = 0; j < num_switches; j++) {
			for (k = 0; k < num_switches; k++) {
				if (p_lash->cdg_vertex_matrix[i][j][k]) {

					if (p_lash->cdg_vertex_matrix[i][j][k]->dependency)
						free(p_lash->cdg_vertex_matrix[i][j][k]->
						     dependency);

					if (p_lash->cdg_vertex_matrix[i][j][k]->
					    num_using_this_depend)
						free(p_lash->cdg_vertex_matrix[i][j][k]->
						     num_using_this_depend);

					free(p_lash->cdg_vertex_matrix[i][j][k]);
				}
			}
			if (p_lash->cdg_vertex_matrix[i][j])
				free(p_lash->cdg_vertex_matrix[i][j]);
		}
		if (p_lash->cdg_vertex_matrix[i])
			free(p_lash->cdg_vertex_matrix[i]);
	}

	if (p_lash->cdg_vertex_matrix)
		free(p_lash->cdg_vertex_matrix);

	// free virtual_location
	for (i = 0; i < num_switches; i++) {
		for (j = 0; j < num_switches; j++) {
			if (p_lash->virtual_location[i][j])
				free(p_lash->virtual_location[i][j]);
		}
		if (p_lash->virtual_location[i])
			free(p_lash->virtual_location[i]);
	}
	if (p_lash->virtual_location)
		free(p_lash->virtual_location);

	if (p_lash->num_mst_in_lane)
		free(p_lash->num_mst_in_lane);

	OSM_LOG_EXIT(p_log);
}

static int init_lash_structures(lash_t * p_lash)
{
	unsigned vl_min = p_lash->vl_min;
	unsigned num_switches = p_lash->num_switches;
	osm_log_t *p_log = &p_lash->p_osm->log;
	int status = 0;
	unsigned int i, j, k;

	OSM_LOG_ENTER(p_log);

	// initialise cdg_vertex_matrix[num_switches][num_switches][num_switches]
	p_lash->cdg_vertex_matrix =
	    (cdg_vertex_t ****) malloc(vl_min * sizeof(cdg_vertex_t ****));
	for (i = 0; i < vl_min; i++) {
		p_lash->cdg_vertex_matrix[i] =
		    (cdg_vertex_t ***) malloc(num_switches *
					      sizeof(cdg_vertex_t ***));

		if (p_lash->cdg_vertex_matrix[i] == NULL)
			goto Exit_Mem_Error;
	}

	for (i = 0; i < vl_min; i++) {
		for (j = 0; j < num_switches; j++) {
			p_lash->cdg_vertex_matrix[i][j] =
			    (cdg_vertex_t **) malloc(num_switches *
						     sizeof(cdg_vertex_t **));
			if (p_lash->cdg_vertex_matrix[i][j] == NULL)
				goto Exit_Mem_Error;

			for (k = 0; k < num_switches; k++) {
				p_lash->cdg_vertex_matrix[i][j][k] = NULL;
			}
		}
	}

	// initialise virtual_location[num_switches][num_switches][num_layers],
	// default value = 0
	p_lash->virtual_location =
	    (int ***)malloc(num_switches * sizeof(int ***));
	if (p_lash->virtual_location == NULL)
		goto Exit_Mem_Error;

	for (i = 0; i < num_switches; i++) {
		p_lash->virtual_location[i] =
		    (int **)malloc(num_switches * sizeof(int **));
		if (p_lash->virtual_location[i] == NULL)
			goto Exit_Mem_Error;
	}

	for (i = 0; i < num_switches; i++) {
		for (j = 0; j < num_switches; j++) {
			p_lash->virtual_location[i][j] =
			    (int *)malloc(vl_min * sizeof(int *));
			if (p_lash->virtual_location[i][j] == NULL)
				goto Exit_Mem_Error;
			for (k = 0; k < vl_min; k++) {
				p_lash->virtual_location[i][j][k] = 0;
			}
		}
	}

	// initialise num_mst_in_lane[num_switches], default 0
	p_lash->num_mst_in_lane = (int *)malloc(num_switches * sizeof(int));
	if (p_lash->num_mst_in_lane == NULL)
		goto Exit_Mem_Error;
	memset(p_lash->num_mst_in_lane, 0,
	       num_switches * sizeof(p_lash->num_mst_in_lane[0]));

	goto Exit;

Exit_Mem_Error:
	status = -1;
	OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 4D01: "
		"Could not allocate required memory for LASH errno %d, errno %d for lack of memory\n",
		errno, ENOMEM);

Exit:
	OSM_LOG_EXIT(p_log);
	return status;
}

static int lash_core(lash_t * p_lash)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	unsigned num_switches = p_lash->num_switches;
	switch_t **switches = p_lash->switches;
	unsigned lanes_needed = 1;
	unsigned int i, j, k, dest_switch = 0;
	reachable_dest_t *dests, *idest;
	int cycle_found = 0;
	unsigned v_lane;
	int stop = 0, output_link, i_next_switch;
	int output_link2, i_next_switch2;
	int cycle_found2 = 0;
	int status = 0;
	int *switch_bitmap;	/* Bitmap to check if we have processed this pair */

	OSM_LOG_ENTER(p_log);

	for (i = 0; i < num_switches; i++) {

		shortest_path(p_lash, i);
		generate_routing_func_for_mst(p_lash, i, &dests);

		idest = dests;
		while (idest != NULL) {
			dests = dests->next;
			free(idest);
			idest = dests;
		}

		for (j = 0; j < num_switches; j++) {
			switches[j]->used_channels = 0;
			switches[j]->q_state = UNQUEUED;
		}
	}

	switch_bitmap = malloc(num_switches * num_switches * sizeof(int));
	if (!switch_bitmap) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 4D04: "
			"Failed allocating switch_bitmap - out of memory\n");
		goto Exit;
	}
	memset(switch_bitmap, 0, num_switches * num_switches * sizeof(int));

	for (i = 0; i < num_switches; i++) {
		for (dest_switch = 0; dest_switch < num_switches; dest_switch++)
			if (dest_switch != i && switch_bitmap[i * num_switches + dest_switch] == 0) {
				v_lane = 0;
				stop = 0;
				while (v_lane < lanes_needed && stop == 0) {
					generate_cdg_for_sp(p_lash, i, dest_switch, v_lane);
					generate_cdg_for_sp(p_lash, dest_switch, i, v_lane);

					output_link =
					    switches[i]->routing_table[dest_switch].out_link;
					output_link2 =
					    switches[dest_switch]->routing_table[i].out_link;

					i_next_switch = switches[i]->phys_connections[output_link];
					i_next_switch2 =
					    switches[dest_switch]->phys_connections[output_link2];

					CL_ASSERT(p_lash->
						  cdg_vertex_matrix[v_lane][i][i_next_switch] !=
						  NULL);
					CL_ASSERT(p_lash->
						  cdg_vertex_matrix[v_lane][dest_switch]
						  [i_next_switch2] != NULL);

					cycle_found =
					    cycle_exists(p_lash->
							 cdg_vertex_matrix[v_lane][i]
							 [i_next_switch], NULL, NULL, 1);
					cycle_found2 =
					    cycle_exists(p_lash->
							 cdg_vertex_matrix[v_lane][dest_switch]
							 [i_next_switch2], NULL, NULL, 1);

					for (j = 0; j < num_switches; j++)
						for (k = 0; k < num_switches; k++)
							if (p_lash->
							    cdg_vertex_matrix[v_lane][j][k] !=
							    NULL) {
								p_lash->
								    cdg_vertex_matrix[v_lane][j]
								    [k]->visiting_number = 0;
								p_lash->
								    cdg_vertex_matrix[v_lane][j]
								    [k]->seen = 0;
							}

					if (cycle_found == 1 || cycle_found2 == 1) {
						remove_temp_depend_for_sp(p_lash, i, dest_switch,
									  v_lane);
						remove_temp_depend_for_sp(p_lash, dest_switch, i,
									  v_lane);
						v_lane++;
					} else {
						set_temp_depend_to_permanent_for_sp(p_lash, i,
										    dest_switch,
										    v_lane);
						set_temp_depend_to_permanent_for_sp(p_lash,
										    dest_switch, i,
										    v_lane);
						stop = 1;
						p_lash->num_mst_in_lane[v_lane]++;
						p_lash->num_mst_in_lane[v_lane]++;
					}
				}

				switches[i]->routing_table[dest_switch].lane = v_lane;
				switches[dest_switch]->routing_table[i].lane = v_lane;

				if (cycle_found == 1 || cycle_found2 == 1) {
					if (++lanes_needed > p_lash->vl_min)
						goto Error_Not_Enough_Lanes;

					generate_cdg_for_sp(p_lash, i, dest_switch, v_lane);
					generate_cdg_for_sp(p_lash, dest_switch, i, v_lane);

					set_temp_depend_to_permanent_for_sp(p_lash, i, dest_switch,
									    v_lane);
					set_temp_depend_to_permanent_for_sp(p_lash, dest_switch, i,
									    v_lane);

					p_lash->num_mst_in_lane[v_lane]++;
					p_lash->num_mst_in_lane[v_lane]++;
				}
				p_lash->virtual_location[i][dest_switch][v_lane] = 1;
				p_lash->virtual_location[dest_switch][i][v_lane] = 1;

				switch_bitmap[i * num_switches + dest_switch] = 1;
				switch_bitmap[dest_switch * num_switches + i] = 1;
			}
	}

	OSM_LOG(p_log, OSM_LOG_INFO,
		"Lanes needed: %d, Balancing\n", lanes_needed);

	for (i = 0; i < lanes_needed; i++) {
		OSM_LOG(p_log, OSM_LOG_INFO, "Lanes in layer %d: %d\n",
			i, p_lash->num_mst_in_lane[i]);
	}

	balance_virtual_lanes(p_lash, lanes_needed);

	for (i = 0; i < lanes_needed; i++) {
		OSM_LOG(p_log, OSM_LOG_INFO, "Lanes in layer %d: %d\n",
			i, p_lash->num_mst_in_lane[i]);
	}

	goto Exit;

Error_Not_Enough_Lanes:
	status = -1;
	OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 4D02: "
		"Lane requirements (%d) exceed available lanes (%d)\n",
		p_lash->vl_min, lanes_needed);
Exit:
	if (switch_bitmap)
		free(switch_bitmap);
	OSM_LOG_EXIT(p_log);
	return status;
}

static unsigned get_lash_id(osm_switch_t * p_sw)
{
	return ((switch_t *) p_sw->priv)->id;
}

static void populate_fwd_tbls(lash_t * p_lash)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	osm_subn_t *p_subn = &p_lash->p_osm->subn;
	osm_opensm_t *p_osm = p_lash->p_osm;
	osm_switch_t *p_sw, *p_next_sw, *p_dst_sw;
	osm_port_t *port;
	uint16_t max_lid_ho, lid;

	OSM_LOG_ENTER(p_log);

	p_next_sw = (osm_switch_t *) cl_qmap_head(&p_subn->sw_guid_tbl);

	// Go through each swtich individually
	while (p_next_sw != (osm_switch_t *) cl_qmap_end(&p_subn->sw_guid_tbl)) {
		uint64_t current_guid;
		switch_t *sw;
		p_sw = p_next_sw;
		p_next_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);

		max_lid_ho = p_sw->max_lid_ho;
		current_guid = p_sw->p_node->node_info.port_guid;
		sw = p_sw->priv;

		memset(p_sw->new_lft, OSM_NO_PATH, IB_LID_UCAST_END_HO + 1);

		for (lid = 1; lid <= max_lid_ho; lid++) {
			port = cl_ptr_vector_get(&p_subn->port_lid_tbl, lid);
			if (!port)
				continue;

			p_dst_sw = get_osm_switch_from_port(port);
			if (p_dst_sw == p_sw) {
				uint8_t egress_port = port->p_node->sw ? 0 :
					port->p_physp->p_remote_physp->port_num;
				p_sw->new_lft[lid] = egress_port;
				OSM_LOG(p_log, OSM_LOG_VERBOSE,
					"LASH fwd MY SRC SRC GUID 0x%016" PRIx64
					" src lash id (%d), src lid no (%u) src lash port (%d) "
					"DST GUID 0x%016" PRIx64
					" src lash id (%d), src lash port (%d)\n",
					cl_ntoh64(current_guid), -1, lid,
					egress_port, cl_ntoh64(current_guid),
					-1, egress_port);
			} else if (p_dst_sw) {
				unsigned dst_lash_switch_id =
				    get_lash_id(p_dst_sw);
				uint8_t lash_egress_port =
				    (uint8_t) sw->
				    routing_table[dst_lash_switch_id].out_link;
				uint8_t physical_egress_port =
				    (uint8_t) sw->
				    virtual_physical_port_table
				    [lash_egress_port];

				p_sw->new_lft[lid] = physical_egress_port;
				OSM_LOG(p_log, OSM_LOG_VERBOSE,
					"LASH fwd SRC GUID 0x%016" PRIx64
					" src lash id (%d), "
					"src lid no (%u) src lash port (%d) "
					"DST GUID 0x%016" PRIx64
					" src lash id (%d), src lash port (%d)\n",
					cl_ntoh64(current_guid), sw->id, lid,
					lash_egress_port,
					cl_ntoh64(p_dst_sw->p_node->node_info.
						  port_guid),
					dst_lash_switch_id,
					physical_egress_port);
			}
		}		// for
		osm_ucast_mgr_set_fwd_table(&p_osm->sm.ucast_mgr, p_sw);
	}
	OSM_LOG_EXIT(p_log);
}

static void osm_lash_process_switch(lash_t * p_lash, osm_switch_t * p_sw)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	int i, port_count;
	osm_physp_t *p_current_physp, *p_remote_physp;
	unsigned switch_a_lash_id, switch_b_lash_id;

	OSM_LOG_ENTER(p_log);

	switch_a_lash_id = get_lash_id(p_sw);
	port_count = osm_node_get_num_physp(p_sw->p_node);

	// starting at port 1, ignoring management port on switch
	for (i = 1; i < port_count; i++) {

		p_current_physp = osm_node_get_physp_ptr(p_sw->p_node, i);
		if (p_current_physp) {
			p_remote_physp = p_current_physp->p_remote_physp;
			if (p_remote_physp && p_remote_physp->p_node->sw) {
				int physical_port_a_num =
				    osm_physp_get_port_num(p_current_physp);
				int physical_port_b_num =
				    osm_physp_get_port_num(p_remote_physp);
				switch_b_lash_id =
				    get_lash_id(p_remote_physp->p_node->sw);

				connect_switches(p_lash, switch_a_lash_id,
						 switch_b_lash_id,
						 physical_port_a_num);
				OSM_LOG(p_log, OSM_LOG_VERBOSE,
					"LASH SUCCESS connected G 0x%016" PRIx64
					" , lash_id(%u), P(%u) " " to G 0x%016"
					PRIx64 " , lash_id(%u) , P(%u)\n",
					cl_ntoh64(osm_physp_get_port_guid
						  (p_current_physp)),
					switch_a_lash_id, physical_port_a_num,
					cl_ntoh64(osm_physp_get_port_guid
						  (p_remote_physp)),
					switch_b_lash_id, physical_port_b_num);
			}
		}
	}

	OSM_LOG_EXIT(p_log);
}

static void lash_cleanup(lash_t * p_lash)
{
	osm_subn_t *p_subn = &p_lash->p_osm->subn;
	osm_switch_t *p_next_sw, *p_sw;

	/* drop any existing references to old lash switches */
	p_next_sw = (osm_switch_t *) cl_qmap_head(&p_subn->sw_guid_tbl);
	while (p_next_sw != (osm_switch_t *) cl_qmap_end(&p_subn->sw_guid_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);
		p_sw->priv = NULL;
	}

	if (p_lash->switches) {
		unsigned id;
		for (id = 0; ((int)id) < p_lash->num_switches; id++)
			if (p_lash->switches[id])
				switch_delete(p_lash->switches[id]);
		free(p_lash->switches);
	}
	p_lash->switches = NULL;
}

/*
  static int  discover_network_properties()
  Traverse the topology of the network in order to determine
   - the maximum number of switches,
   - the minimum number of virtual layers
*/

static int discover_network_properties(lash_t * p_lash)
{
	int i = 0, id = 0;
	uint8_t vl_min;
	osm_subn_t *p_subn = &p_lash->p_osm->subn;
	osm_switch_t *p_next_sw, *p_sw;
	osm_log_t *p_log = &p_lash->p_osm->log;

	p_lash->num_switches = cl_qmap_count(&p_subn->sw_guid_tbl);

	p_lash->switches = malloc(p_lash->num_switches * sizeof(switch_t *));
	if (!p_lash->switches)
		return -1;
	memset(p_lash->switches, 0, p_lash->num_switches * sizeof(switch_t *));

	vl_min = 5;		// set to a high value

	p_next_sw = (osm_switch_t *) cl_qmap_head(&p_subn->sw_guid_tbl);
	while (p_next_sw != (osm_switch_t *) cl_qmap_end(&p_subn->sw_guid_tbl)) {
		uint16_t port_count;
		p_sw = p_next_sw;
		p_next_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);

		p_lash->switches[id] = switch_create(p_lash, id, p_sw);
		if (!p_lash->switches[id])
			return -1;
		id++;

		port_count = osm_node_get_num_physp(p_sw->p_node);

		// Note, ignoring port 0. management port
		for (i = 1; i < port_count; i++) {
			osm_physp_t *p_current_physp =
			    osm_node_get_physp_ptr(p_sw->p_node, i);

			if (p_current_physp
			    && p_current_physp->p_remote_physp) {

				ib_port_info_t *p_port_info =
				    &p_current_physp->port_info;
				uint8_t port_vl_min =
				    ib_port_info_get_op_vls(p_port_info);
				if (port_vl_min && port_vl_min < vl_min)
					vl_min = port_vl_min;
			}
		}		// for
	}			// while

	vl_min = 1 << (vl_min - 1);
	if (vl_min > 15)
		vl_min = 15;

	p_lash->vl_min = vl_min;

	OSM_LOG(p_log, OSM_LOG_INFO,
		"min operational vl(%d) max_switches(%d)\n", p_lash->vl_min,
		p_lash->num_switches);
	return 0;
}

static void process_switches(lash_t * p_lash)
{
	osm_switch_t *p_sw, *p_next_sw;
	osm_subn_t *p_subn = &p_lash->p_osm->subn;

	/* Go through each swithc and process it. i.e build the connection
	   structure required by LASH */
	p_next_sw = (osm_switch_t *) cl_qmap_head(&p_subn->sw_guid_tbl);
	while (p_next_sw != (osm_switch_t *) cl_qmap_end(&p_subn->sw_guid_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);

		osm_lash_process_switch(p_lash, p_sw);
	}
}

static int lash_process(void *context)
{
	lash_t *p_lash = context;
	osm_log_t *p_log = &p_lash->p_osm->log;
	int return_status = IB_SUCCESS;

	OSM_LOG_ENTER(p_log);

	p_lash->balance_limit = 6;

	// everything starts here
	lash_cleanup(p_lash);

	discover_network_properties(p_lash);

	return_status = init_lash_structures(p_lash);
	if (return_status != IB_SUCCESS)
		goto Exit;

	process_switches(p_lash);

	return_status = lash_core(p_lash);
	if (return_status != IB_SUCCESS)
		goto Exit;

	populate_fwd_tbls(p_lash);

Exit:
	free_lash_structures(p_lash);
	OSM_LOG_EXIT(p_log);

	return return_status;
}

static lash_t *lash_create(osm_opensm_t * p_osm)
{
	lash_t *p_lash;

	p_lash = malloc(sizeof(lash_t));
	if (!p_lash)
		return NULL;

	memset(p_lash, 0, sizeof(lash_t));
	p_lash->p_osm = p_osm;

	return (p_lash);
}

static void lash_delete(void *context)
{
	lash_t *p_lash = context;
	if (p_lash->switches) {
		unsigned id;
		for (id = 0; ((int)id) < p_lash->num_switches; id++)
			if (p_lash->switches[id])
				switch_delete(p_lash->switches[id]);
		free(p_lash->switches);
	}
	free(p_lash);
}

uint8_t osm_get_lash_sl(osm_opensm_t * p_osm, osm_port_t * p_src_port,
			osm_port_t * p_dst_port)
{
	unsigned dst_id;
	unsigned src_id;
	osm_switch_t *p_sw;

	if (p_osm->routing_engine_used != OSM_ROUTING_ENGINE_TYPE_LASH)
		return OSM_DEFAULT_SL;

	p_sw = get_osm_switch_from_port(p_dst_port);
	if (!p_sw || !p_sw->priv)
		return OSM_DEFAULT_SL;
	dst_id = get_lash_id(p_sw);

	p_sw = get_osm_switch_from_port(p_src_port);
	if (!p_sw || !p_sw->priv)
		return OSM_DEFAULT_SL;

	src_id = get_lash_id(p_sw);
	if (src_id == dst_id)
		return OSM_DEFAULT_SL;

	return (uint8_t) ((switch_t *) p_sw->priv)->routing_table[dst_id].lane;
}

int osm_ucast_lash_setup(struct osm_routing_engine *r, osm_opensm_t *p_osm)
{
	lash_t *p_lash = lash_create(p_osm);
	if (!p_lash)
		return -1;

	r->context = p_lash;
	r->ucast_build_fwd_tables = lash_process;
	r->delete = lash_delete;

	return 0;
}
