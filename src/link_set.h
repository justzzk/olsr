
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

/*
 * Link sensing database for the OLSR routing daemon
 */

#ifndef _LINK_SET_H
#define _LINK_SET_H

#include "lq_packet.h"
#include "lq_plugin.h"
#include "common/list.h"
#include "olsr_clock.h"

#define MID_ALIAS_HACK_VTIME  10.0

#define LINK_LOSS_MULTIPLIER (1<<16)

struct link_entry {
  union olsr_ip_addr local_iface_addr;
  union olsr_ip_addr neighbor_iface_addr;
  struct interface *inter;
  char *if_name;
  struct olsr_timer_entry *link_timer;
  struct olsr_timer_entry *link_sym_timer;
  uint32_t ASYM_time;
  uint32_t vtime;
  struct nbr_entry *neighbor;
  uint8_t status;

  bool is_mpr, is_mprs;

  /* for faster hello packet generation */
  uint8_t iflocal_link_status, iflocal_neigh_status;

  /*
   * packet loss
   */
  uint32_t loss_helloint;
  struct olsr_timer_entry *link_loss_timer;

  /* user defined multiplies for link quality, multiplied with 65536 */
  uint32_t loss_link_multiplier;

  /* cost of this link */
  olsr_linkcost linkcost;

  struct list_entity link_list;          /* double linked list of all link entries */
};

#define OLSR_LINK_JITTER       5        /* percent */
#define OLSR_LINK_HELLO_JITTER 0        /* percent jitter */
#define OLSR_LINK_SYM_JITTER   0        /* percent jitter */
#define OLSR_LINK_LOSS_JITTER  0        /* percent jitter */

/* deletion safe macro for link entry traversal */
#define OLSR_FOR_ALL_LINK_ENTRIES(link, iterator) list_for_each_element_safe(&link_entry_head, link, link_list, iterator)

/* Externals */
extern struct list_entity EXPORT(link_entry_head);
extern bool link_changes;

/* Function prototypes */

void olsr_init_link_set(void);
void olsr_delete_link_entry_by_if(const struct interface *);
void olsr_expire_link_hello_timer(void *);
void olsr_lq_hello_handler(struct link_entry *, bool);
void signal_link_changes(bool);        /* XXX ugly */


struct link_entry *EXPORT(get_best_link_to_neighbor)(struct nbr_entry *nbr);

struct link_entry *EXPORT(get_best_link_to_neighbor_ip) (const union olsr_ip_addr *);

struct link_entry *EXPORT(lookup_link_entry) (const union olsr_ip_addr *,
                                              const union olsr_ip_addr * remote_main, const struct interface *);

struct link_entry *update_link_entry(const union olsr_ip_addr *,
                                     const union olsr_ip_addr *, struct lq_hello_message *, struct interface *);

int EXPORT(check_neighbor_link) (const union olsr_ip_addr *);
int replace_neighbor_link_set(const struct nbr_entry *, struct nbr_entry *);
int lookup_link_status(const struct link_entry *);
void olsr_update_packet_loss_hello_int(struct link_entry *, uint32_t);
void olsr_update_packet_loss(struct link_entry *entry);
void olsr_print_link_set(void);
void generate_hello(void *);
#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
