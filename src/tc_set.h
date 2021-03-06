
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
#ifndef _OLSR_TOP_SET
#define _OLSR_TOP_SET

#include "defs.h"
#include "common/avl.h"
#include "common/list.h"
#include "olsr_protocol.h"
#include "lq_packet.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "olsr_memcookie.h"
#include "duplicate_set.h"

/*
 * This file holds the definitions for the link state database.
 * The lsdb consists of nodes and edges.
 * During input parsing all nodes and edges are extracted and synthesized.
 * The SPF calculation operates on these datasets.
 */

struct tc_edge_entry {
  struct avl_node edge_node;           /* edge_tree node in tc_entry */
  union olsr_ip_addr T_dest_addr;      /* edge_node key */
  struct tc_edge_entry *edge_inv;      /* shortcut, used during SPF calculation */
  struct tc_entry *tc;                 /* backpointer to owning tc entry */
  struct nbr_entry *neighbor;          /* back pointer for edges to neighbor */
  olsr_linkcost cost;                  /* metric for this edge received by the last tc */
  olsr_linkcost common_cost;           /* metric for this edge or the reverse one received by the last tc */
  bool virtual;                        /* 1 if this is a virtual edge created by the neighbors TC ? */
  uint16_t ansn;                       /* ansn of this edge, used for multipart msgs */
};

struct tc_entry {
  struct avl_node vertex_node;         /* node keyed by ip address */
  union olsr_ip_addr addr;             /* vertex_node key */
  struct avl_node cand_tree_node;      /* SPF candidate heap, node keyed by path_etx */
  olsr_linkcost path_cost;             /* SPF calculated distance, cand_tree_node key */
  struct list_entity path_list_node;   /* SPF result list */
  struct avl_tree edge_tree;           /* subtree for edges */
  struct avl_tree prefix_tree;         /* subtree for prefixes */
  struct avl_tree mid_tree;            /* subtree for MID entries */
  struct avl_tree hna_tree;            /* subtree for HNA entries */
  struct link_entry *next_hop;         /* SPF calculated link to the 1st hop neighbor */
  struct olsr_timer_entry *edge_gc_timer;   /* used for edge garbage collection */
  struct olsr_timer_entry *validity_timer;  /* tc validity time */
  bool virtual;                        /* true if node is virtual */
  int tc_seq;                          /* sequence number of the tc message */
  int mid_seq;                         /* sequence number of the mid message */
  int hna_seq;                         /* sequence number of the hna message */
  uint8_t msg_hops;                    /* hopcount as per the tc message */
  uint8_t hops;                        /* SPF calculated hopcount */
  uint16_t ansn;                       /* ANSN number of the tc message */
};

/*
 * Garbage collection time for edges.
 * This is used for multipart messages.
 */
#define OLSR_TC_EDGE_GC_TIME (2*1000)   /* milliseconds */
#define OLSR_TC_EDGE_GC_JITTER 5        /* percent */

#define OLSR_TC_VTIME_JITTER 5 /* percent */

/*
 * macros for traversing vertices, edges and prefixes in the link state database.
 * it is recommended to use this because it hides all the internal
 * datastructure from the callers.
 *
 * the loop prefetches the next node in order to not loose context if
 * for example the caller wants to delete the current entry.
 */
#define OLSR_FOR_ALL_TC_ENTRIES(tc, iterator) avl_for_each_element_safe(&tc_tree, tc, vertex_node, iterator)
#define OLSR_FOR_ALL_TC_EDGE_ENTRIES(tc, tc_edge, iterator) avl_for_each_element_safe(&tc->edge_tree, tc_edge, edge_node, iterator)
#define OLSR_FOR_ALL_PREFIX_ENTRIES(tc, rtp, iterator) avl_for_each_element_safe(&tc->prefix_tree, rtp, rtp_prefix_tree_node, iterator)

extern struct avl_tree EXPORT(tc_tree);
extern struct tc_entry *tc_myself;

extern struct olsr_memcookie_info *EXPORT(tc_mem_cookie);

void olsr_init_tc(void);
void olsr_change_myself_tc(void);
void olsr_print_tc_table(void);
void olsr_time_out_tc_set(void);

/* tc msg input parser */
void olsr_input_tc(struct olsr_message *, struct interface *, union olsr_ip_addr *, enum duplicate_status);

/* tc_entry manipulation */
struct tc_entry *EXPORT(olsr_lookup_tc_entry) (const union olsr_ip_addr *);
struct tc_entry *EXPORT(olsr_locate_tc_entry)(const union olsr_ip_addr *);

/* tc_edge_entry manipulation */
struct tc_edge_entry *EXPORT(olsr_lookup_tc_edge) (struct tc_entry *, const union olsr_ip_addr *);
struct tc_edge_entry *olsr_add_tc_edge_entry(struct tc_entry *, const union olsr_ip_addr *, uint16_t);
void olsr_delete_tc_entry(struct tc_entry *);
void olsr_delete_tc_edge_entry(struct tc_edge_entry *);
bool olsr_calc_tc_edge_entry_etx(struct tc_edge_entry *);
void olsr_set_tc_edge_timer(struct tc_edge_entry *, unsigned int);
void olsr_delete_all_tc_entries(void);
uint32_t EXPORT(getRelevantTcCount) (void);
uint16_t EXPORT(get_local_ansn_number)(void);
void increase_local_ansn_number(void);
void olsr_output_lq_tc(void *ifp);
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
