
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2009, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */


#ifndef _OLSR_NEIGH_TBL
#define _OLSR_NEIGH_TBL

#include "defs.h"
#include "olsr_clock.h"
#include "olsr_types.h"
#include "common/avl.h"
#include "tc_set.h"

#define NB2S_COVERED  0x1     /* node has been covered by a MPR */

/*
 * This is a connector between a neighbor and a two-hop neighbor
 */
struct nbr_con {
  struct avl_node nbr_tree_node;
  struct avl_node nbr2_tree_node;

  struct nbr_entry *nbr;
  struct nbr2_entry *nbr2;

  struct olsr_timer_entry *nbr2_con_timer;

  olsr_linkcost second_hop_linkcost;
  olsr_linkcost path_linkcost;
};


#define OLSR_NBR2_LIST_JITTER 5 /* percent */

struct nbr_entry {
  struct avl_node nbr_node;            /* nbr keyed by ip address */
  union olsr_ip_addr nbr_addr;
  struct tc_edge_entry *tc_edge;
  unsigned int willingness:3;
  unsigned int is_sym:1;
  unsigned int is_mpr:1;
  unsigned int was_mpr:1;              /* Used to detect changes in MPR */
  unsigned int skip:1; 
  unsigned int mprs_count:16;          /* >0 if we are choosen as an MPR by this neighbor */
  unsigned int linkcount;
  struct avl_tree con_tree;            /* subtree for connectors to nbr2 */
};


struct nbr2_entry {
  struct avl_node nbr2_node;
  union olsr_ip_addr nbr2_addr;
  unsigned int mpr_covered_count;      /* Used in mpr calculation */
  unsigned int processed:1;            /* Used in mpr calculation */
  struct avl_tree con_tree;            /* subtree for connectors to nbr */
};


/*
 * macros for traversing neighbors, neighbor2 and connectors.
 * it is recommended to use this because it hides all the internal
 * datastructure from the callers.
 *
 * the loop prefetches the next node in order to not loose context if
 * for example the caller wants to delete the current entry.
 */
#define OLSR_FOR_ALL_NBR_ENTRIES(nbr, iterator) avl_for_each_element_safe(&nbr_tree, nbr, nbr_node, iterator)

#define OLSR_FOR_ALL_NBR_CON_ENTRIES(nbr, con, iterator) avl_for_each_element_safe(&nbr->con_tree, con, nbr_tree_node, iterator)

/*
 * macros for traversing two-hop neighbors and neighbor ref lists.
 * it is recommended to use this because it hides all the internal
 * datastructure from the callers.
 *
 * the loop prefetches the next node in order to not loose context if
 * for example the caller wants to delete the current entry.
 */
#define OLSR_FOR_ALL_NBR2_ENTRIES(nbr2, iterator) avl_for_each_element_safe(&nbr2_tree, nbr2, nbr2_node, iterator)

#define OLSR_FOR_ALL_NBR2_CON_ENTRIES(nbr2, con, iterator) avl_for_each_element_safe(&nbr2->con_tree, con, nbr2_tree_node, iterator)

/*
 * The one hop neighbor and two hop neighbor trees.
 */
extern struct avl_tree EXPORT(nbr_tree);
extern struct avl_tree EXPORT(nbr2_tree);

void olsr_init_neighbor_table(void);

/* work with 1-hop neighbors */
struct nbr_entry *olsr_add_nbr_entry(const union olsr_ip_addr *);
void olsr_delete_nbr_entry(struct nbr_entry *);
struct nbr_entry *EXPORT(olsr_lookup_nbr_entry) (const union olsr_ip_addr *, bool aliaslookup);

void olsr_update_nbr_status(struct nbr_entry *);

/* work with 2-hop neighbors */
struct nbr2_entry *olsr_add_nbr2_entry(const union olsr_ip_addr *);
void olsr_delete_nbr2_entry(struct nbr2_entry *);
struct nbr2_entry *EXPORT(olsr_lookup_nbr2_entry)(const union olsr_ip_addr *, bool aliaslookup);

/* work with connectors */
struct nbr_con *olsr_link_nbr_nbr2(struct nbr_entry *, const union olsr_ip_addr *, uint32_t);
void olsr_delete_nbr_con(struct nbr_con *);
struct nbr_con *EXPORT(olsr_lookup_nbr_con_entry)(struct nbr_entry *, const union olsr_ip_addr *);
struct nbr_con *EXPORT(olsr_lookup_nbr2_con_entry)(struct nbr2_entry *, const union olsr_ip_addr *);

void olsr_print_neighbor_table(void);

#endif /* OLSR_NEIGH_TBL */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
