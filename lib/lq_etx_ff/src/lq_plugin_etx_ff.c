
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

#include <stdio.h>

#include "tc_set.h"
#include "link_set.h"
#include "lq_plugin.h"
#include "olsr_spf.h"
#include "lq_packet.h"
#include "olsr.h"
#include "parser.h"
#include "mid_set.h"
#include "olsr_timer.h"
#include "olsr_socket.h"
#include "olsr_logging.h"
#include "common/string.h"
#include "neighbor_table.h"
#include "lq_plugin_etx_ff.h"

#define PLUGIN_DESCR "Freifunk ETX metric based on the original design of Elektra and Thomas Lopatic"
#define PLUGIN_AUTHOR "Henning Rogge"

#define LQ_ALGORITHM_ETX_FF_NAME "etx_ff"

#define LQ_FF_QUICKSTART_INIT 4

#define LQ_PLUGIN_RELEVANT_COSTCHANGE_FF 16

static int lq_etxff_enable(void);

static void lq_etxff_initialize(void);
static void lq_etxff_deinitialize(void);

static olsr_linkcost lq_etxff_calc_link_entry_cost(struct link_entry *);
static olsr_linkcost lq_etxff_calc_lq_hello_neighbor_cost(struct lq_hello_neighbor *);
static olsr_linkcost lq_etxff_calc_tc_edge_entry_cost(struct tc_edge_entry *);

static void lq_etxff_hello_handler(struct link_entry *, bool);

static void lq_etxff_memorize_foreign_hello(struct link_entry *, struct lq_hello_neighbor *);
static void lq_etxff_copy_link_entry_lq_into_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source);

static void lq_etxff_clear_link_entry(struct link_entry *);
static void lq_etxff_clear_tc_edge_entry(struct tc_edge_entry *);

static void lq_etxff_serialize_hello_lq(uint8_t **curr, struct link_entry *link);
static void lq_etxff_serialize_tc_lq(uint8_t **curr, struct link_entry *link);
static void lq_etxff_deserialize_hello_lq(uint8_t const **curr, struct lq_hello_neighbor *lq);
static void lq_etxff_deserialize_tc_lq(uint8_t const **curr, struct tc_edge_entry *lq);

static int lq_etxff_get_linkentry_data(struct link_entry *, int);
static const char *lq_etxff_print_cost(olsr_linkcost cost, char *buffer, size_t bufsize);
static const char *lq_etxff_print_link_entry_lq(struct link_entry *entry, int index, char *buffer, size_t bufsize);

static struct olsr_timer_info *default_lq_ff_timer_info = NULL;

OLSR_PLUGIN6_NP() {
  .descr = PLUGIN_DESCR,
  .author = PLUGIN_AUTHOR,
  .enable = lq_etxff_enable,
  .type = PLUGIN_TYPE_LQ
};

/* etx lq plugin (freifunk fpm version) settings */
struct lq_linkdata_type lq_etxff_linktypes[] = {
  { "ETX", 5, 65536, 65536*2, 65536*4, INT32_MAX },
  { "LQ", 5, 255, 240, 192, 0 },
  { "NLQ", 5, 255, 240, 192, 0 }
};

struct lq_handler lq_etxff_handler = {
  "etx (freifunk)",

  &lq_etxff_initialize,
  &lq_etxff_deinitialize,

  &lq_etxff_calc_link_entry_cost,
  &lq_etxff_calc_lq_hello_neighbor_cost,
  &lq_etxff_calc_tc_edge_entry_cost,

  &lq_etxff_hello_handler,

  &lq_etxff_memorize_foreign_hello,
  &lq_etxff_copy_link_entry_lq_into_tc_edge_entry,

  &lq_etxff_clear_link_entry,
  NULL,
  &lq_etxff_clear_tc_edge_entry,

  &lq_etxff_serialize_hello_lq,
  &lq_etxff_serialize_tc_lq,
  &lq_etxff_deserialize_hello_lq,
  &lq_etxff_deserialize_tc_lq,

  &lq_etxff_get_linkentry_data,
  &lq_etxff_print_cost,
  &lq_etxff_print_link_entry_lq,

  lq_etxff_linktypes,
  ARRAYSIZE(lq_etxff_linktypes),

  sizeof(struct lq_etxff_tc_edge),
  sizeof(struct lq_etxff_lq_hello_neighbor),
  sizeof(struct lq_etxff_link_entry),

  LQ_HELLO_MESSAGE,
  LQ_TC_MESSAGE,

  4,4
};

static int
lq_etxff_enable(void) {
  active_lq_handler = &lq_etxff_handler;
  return false;
}

static void
lq_etxff_packet_parser(struct olsr_packet *pkt, uint8_t *binary __attribute__ ((unused)),
    struct interface *in_if, union olsr_ip_addr *from_addr)
{
  const union olsr_ip_addr *main_addr;
  struct lq_etxff_link_entry *lnk;
  uint32_t seq_diff;
#if !defined(REMOVE_LOG_WARN) || !defined(REMOVE_LOG_DEBUG)
    struct ipaddr_str buf;
#endif

  /* Find main address */
  main_addr = olsr_lookup_main_addr_by_alias(from_addr);

  /* Loopup link entry */
  lnk = (struct lq_etxff_link_entry *)lookup_link_entry(from_addr, main_addr, in_if);
  if (lnk == NULL) {
    return;
  }

  if (lnk->last_seq_nr == pkt->seqno) {
    OLSR_WARN(LOG_LQ_PLUGINS, "Got package with same sequence number from %s, if=%s, seq=0x%04x\n",
        olsr_ip_to_string(&buf, from_addr), in_if->int_name, pkt->seqno);
    return;
  }
  if (lnk->last_seq_nr > pkt->seqno) {
    seq_diff = (uint32_t) pkt->seqno + 65536 - lnk->last_seq_nr;
  } else {
    seq_diff = pkt->seqno - lnk->last_seq_nr;
  }

  OLSR_DEBUG(LOG_LQ_PLUGINS, "Got package from %s, if=%s, seq=0x%04x, diff=0x%04x\n",
      olsr_ip_to_string(&buf, from_addr), in_if->int_name, pkt->seqno, seq_diff);

  /* Jump in sequence numbers ? */
  if (seq_diff > 256) {
    seq_diff = 1;
  }

  lnk->received[lnk->activePtr]++;
  lnk->total[lnk->activePtr] += seq_diff;

  lnk->last_seq_nr = pkt->seqno;
  lnk->missed_seconds = 0;
}

static void
lq_etxff_timer(void __attribute__ ((unused)) * context)
{
  struct link_entry *link, *link_iterator;
  struct nbr_entry *nbr, *nbr_iterator;

  OLSR_FOR_ALL_LINK_ENTRIES(link, link_iterator) {
    struct lq_etxff_link_entry *lq_link;
    uint32_t ratio;
    uint16_t i, received, total;

#ifndef REMOVE_LOG_DEBUG
    struct ipaddr_str buf;
    // struct lqtextbuffer lqbuffer;
#endif
    lq_link = (struct lq_etxff_link_entry *)link;

    OLSR_DEBUG(LOG_LQ_PLUGINS, "LQ-FF new entry for %s: rec: %d total: %d",
               olsr_ip_to_string(&buf, &link->neighbor_iface_addr),
               lq_link->received[lq_link->activePtr], lq_link->total[lq_link->activePtr]);

    received = 0;
    total = 0;

    /* enlarge window if still in quickstart phase */
    if (lq_link->windowSize < LQ_FF_WINDOW) {
      lq_link->windowSize++;
    }
    for (i = 0; i < lq_link->windowSize; i++) {
      received += lq_link->received[i];
      total += lq_link->total[i];
    }

    OLSR_DEBUG(LOG_LQ_PLUGINS, " total-rec: %d total: %d", received, total);

    /* calculate link quality */
    if (total == 0) {
      lq_link->lq.valueLq = 0;
    } else {

      /* keep missed hello periods in mind (round up hello interval to seconds) */
      uint32_t missed_hello_cost = lq_link->missed_seconds / ((lq_link->core.link_loss_timer->timer_period + 999) / 1000);
      total += missed_hello_cost * missed_hello_cost;

      // start with link-loss-factor
      ratio = link->loss_link_multiplier;

      // calculate received/(received + loss) factor
      ratio = ratio * received;
      ratio = ratio / total;
      ratio = (ratio * 255) >> 16;

      lq_link->lq.valueLq = (uint8_t) (ratio);
    }

    // shift buffer
    lq_link->activePtr = (lq_link->activePtr + 1) % LQ_FF_WINDOW;
    lq_link->total[lq_link->activePtr] = 0;
    lq_link->received[lq_link->activePtr] = 0;
    lq_link->missed_seconds++;

    /* update linkcost */
    link->linkcost = lq_etxff_calc_link_entry_cost(link);
  }

  OLSR_FOR_ALL_NBR_ENTRIES(nbr, nbr_iterator) {
    olsr_neighbor_cost_may_changed(nbr);
  }
}

static struct olsr_timer_entry *lq_etxff_timer_struct = NULL;

static void
lq_etxff_initialize(void)
{
  /* Some cookies for stats keeping */
  olsr_packetparser_add_function(&lq_etxff_packet_parser);
  default_lq_ff_timer_info = olsr_timer_add("Default Freifunk LQ",  &lq_etxff_timer, true);
  lq_etxff_timer_struct = olsr_timer_start(1000, 0, NULL, default_lq_ff_timer_info);
}

static void
lq_etxff_deinitialize(void)
{
  olsr_timer_stop(lq_etxff_timer_struct);
  olsr_packetparser_remove_function(&lq_etxff_packet_parser);
}

static olsr_linkcost
lq_etxff_calc_linkcost(struct lq_etxff_linkquality *lq)
{
  olsr_linkcost cost;

  if (lq->valueLq < (unsigned int)(255 * MINIMAL_USEFUL_LQ) || lq->valueNlq < (unsigned int)(255 * MINIMAL_USEFUL_LQ)) {
    return LINK_COST_BROKEN;
  }

  cost = 65536 * 255 / (int)lq->valueLq * 255 / (int)lq->valueNlq;

  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0)
    return 1;
  return cost;
}

static olsr_linkcost
lq_etxff_calc_link_entry_cost(struct link_entry *link)
{
  struct lq_etxff_link_entry *lq_link = (struct lq_etxff_link_entry *)link;

  return lq_etxff_calc_linkcost(&lq_link->lq);
}

static olsr_linkcost
lq_etxff_calc_lq_hello_neighbor_cost(struct lq_hello_neighbor *neigh)
{
  struct lq_etxff_lq_hello_neighbor *lq_neigh = (struct lq_etxff_lq_hello_neighbor *)neigh;

  return lq_etxff_calc_linkcost(&lq_neigh->lq);
}

static olsr_linkcost
lq_etxff_calc_tc_edge_entry_cost(struct tc_edge_entry *edge)
{
  struct lq_etxff_tc_edge *lq_edge = (struct lq_etxff_tc_edge *)edge;

  return lq_etxff_calc_linkcost(&lq_edge->lq);
}

static void
lq_etxff_hello_handler(struct link_entry *link __attribute__ ((unused)), bool loss __attribute__ ((unused)))
{
}

static void
lq_etxff_memorize_foreign_hello(struct link_entry *target, struct lq_hello_neighbor *source)
{
  struct lq_etxff_link_entry *lq_target = (struct lq_etxff_link_entry *)target;
  struct lq_etxff_lq_hello_neighbor *lq_source = (struct lq_etxff_lq_hello_neighbor *)source;

  if (source) {
    lq_target->lq.valueNlq = lq_source->lq.valueLq;
  } else {
    lq_target->lq.valueNlq = 0;
  }

}

static void
lq_etxff_copy_link_entry_lq_into_tc_edge_entry(struct tc_edge_entry *target, struct link_entry *source)
{
  struct lq_etxff_tc_edge *lq_target = (struct lq_etxff_tc_edge *)target;
  struct lq_etxff_link_entry *lq_source = (struct lq_etxff_link_entry *)source;

  lq_target->lq = lq_source->lq;
}

static void
lq_etxff_clear_link_entry(struct link_entry *link)
{
  struct lq_etxff_link_entry *lq_link = (struct lq_etxff_link_entry *)link;
  int i;

  lq_link->windowSize = LQ_FF_QUICKSTART_INIT;
  for (i = 0; i < LQ_FF_WINDOW; i++) {
    lq_link->total[i] = 3;
  }
}

static void
lq_etxff_clear_tc_edge_entry(struct tc_edge_entry *edge) {
  struct lq_etxff_tc_edge *lq_edge = (struct lq_etxff_tc_edge *)edge;

  memset (&lq_edge->lq, 0, sizeof(lq_edge->lq));
}

static void
lq_etxff_serialize_hello_lq(uint8_t **curr, struct link_entry *link)
{
  struct lq_etxff_link_entry *lq_link = (struct lq_etxff_link_entry *)link;

  pkt_put_u8(curr, lq_link->lq.valueLq);
  pkt_put_u8(curr, lq_link->lq.valueNlq);
  pkt_put_u16(curr, 0);
}
static void
lq_etxff_serialize_tc_lq(uint8_t **curr, struct link_entry *link)
{
  struct lq_etxff_link_entry *lq_link = (struct lq_etxff_link_entry *)link;

  pkt_put_u8(curr, lq_link->lq.valueLq);
  pkt_put_u8(curr, lq_link->lq.valueNlq);
  pkt_put_u16(curr, 0);
}

static void
lq_etxff_deserialize_hello_lq(uint8_t const **curr, struct lq_hello_neighbor *neigh)
{
  struct lq_etxff_lq_hello_neighbor *lq_neigh = (struct lq_etxff_lq_hello_neighbor *)neigh;

  pkt_get_u8(curr, &lq_neigh->lq.valueLq);
  pkt_get_u8(curr, &lq_neigh->lq.valueNlq);
  pkt_ignore_u16(curr);

}
static void
lq_etxff_deserialize_tc_lq(uint8_t const **curr, struct tc_edge_entry *edge)
{
  struct lq_etxff_tc_edge *lq_edge = (struct lq_etxff_tc_edge *)edge;

  pkt_get_u8(curr, &lq_edge->lq.valueLq);
  pkt_get_u8(curr, &lq_edge->lq.valueNlq);
  pkt_ignore_u16(curr);
}

static int
lq_etxff_get_linkentry_data(struct link_entry *link, int idx) {
  struct lq_etxff_link_entry *lq = (struct lq_etxff_link_entry *)link;
  return idx == 1 ? lq->lq.valueLq : lq->lq.valueNlq;
}

static const char *
lq_etxff_print_link_entry_lq(struct link_entry *link, int idx, char *buffer, size_t bufsize)
{
  struct lq_etxff_link_entry *lq = (struct lq_etxff_link_entry *)link;
  uint8_t value;

  if (idx == 1) {
    value = lq->lq.valueLq;
  }
  else {
    value = lq->lq.valueNlq;
  }

  if (value == 255) {
    strscpy(buffer, "1.000", bufsize);
  } else {
    snprintf(buffer, bufsize, "0.%03d", (value * 1000) / 255);
  }
  return buffer;
}

static const char *
lq_etxff_print_cost(olsr_linkcost cost, char *buffer, size_t bufsize)
{
  // must calculate
  uint32_t roundDown = cost >> 16;
  uint32_t fraction = ((cost & 0xffff) * 1000) >> 16;

  snprintf(buffer, bufsize, "%u.%03u", roundDown, fraction);
  return buffer;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
