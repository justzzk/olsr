
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

#ifndef OLSR_TIMER_H_
#define OLSR_TIMER_H_

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

#define TIMER_WHEEL_SLOTS 1024
#define TIMER_WHEEL_MASK (TIMER_WHEEL_SLOTS - 1)

/* prototype for timer callback */
typedef void (*timer_cb_func) (void *);

/*
 * This struct defines a class of timers which have the same
 * type (periodic/non-periodic) and callback.
 */
struct olsr_timer_info {
  /* node of timerinfo list */
  struct list_entity node;

  /* name of this timer class */
  char *name;

  /* callback function */
  timer_cb_func callback;

  /* true if this is a class of periodic timers */
  bool periodic;

  /* Stats, resource usage */
  uint32_t usage;

  /* Stats, resource churn */
  uint32_t changes;
};


/*
 * Our timer implementation is a based on individual timers arranged in
 * a double linked list hanging of hash containers called a timer wheel slot.
 * For every timer a olsr_timer_entry is created and attached to the timer wheel slot.
 * When the timer fires, the timer_cb function is called with the
 * context pointer.
 */
struct olsr_timer_entry {
  /* Wheel membership */
  struct list_entity timer_list;

  /* backpointer to timer info */
  struct olsr_timer_info *timer_info;

  /* when timer shall fire (absolute internal timerstamp) */
  uint32_t timer_clock;

  /* timeperiod between two timer events for periodical timers */
  uint32_t timer_period;

  /* the jitter expressed in percent */
  uint8_t timer_jitter_pct;

  /* true if timer is running at the moment */
  bool timer_running;

  /* true if timer is in callback at the moment */
  bool timer_in_callback;

  /* cache random() result for performance reasons */
  unsigned int timer_random;

  /* context pointer */
  void *timer_cb_context;
};

/* Timers */
extern struct list_entity EXPORT(timerinfo_list);
#define OLSR_FOR_ALL_TIMERS(ti, iterator) list_for_each_element_safe(&timerinfo_list, ti, node, iterator)

void olsr_timer_init(void);
void olsr_timer_cleanup(void);
void olsr_timer_walk(void);

struct olsr_timer_info *EXPORT(olsr_timer_add)(
    const char *name, timer_cb_func callback, bool periodic) __attribute__((warn_unused_result));
void olsr_timer_remove(struct olsr_timer_info *);

void EXPORT(olsr_timer_set) (struct olsr_timer_entry **, uint32_t, uint8_t,
    void *, struct olsr_timer_info *);
struct olsr_timer_entry *EXPORT(olsr_timer_start) (uint32_t, uint8_t,
    void *, struct olsr_timer_info *);
void EXPORT(olsr_timer_change)(struct olsr_timer_entry *, uint32_t, uint8_t);
void EXPORT(olsr_timer_stop) (struct olsr_timer_entry *);

#endif /* OLSR_TIMER_H_ */
