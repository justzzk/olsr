
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

#ifndef _OLSR_ROUTING_TABLE
#define _OLSR_ROUTING_TABLE

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include "hna_set.h"
#include "link_set.h"
#include "olsr_memcookie.h"
#include "common/avl.h"
#include "common/list.h"

#define NETMASK_HOST 0xffffffff
#define NETMASK_DEFAULT 0x0

/*
 * the kernel FIB does not need to know the metric of a route.
 * this saves us from enqueuing/dequeueing hopcount only changes.
 */
#define RT_METRIC_DEFAULT 2

/* a composite metric is used for path selection */
struct rt_metric {
  olsr_linkcost cost;
  uint32_t hops;
};

/* a nexthop is a pointer to a gateway router plus an interface */
struct rt_nexthop {
  union olsr_ip_addr gateway;          /* gateway router */
  struct interface *interface;         /* outgoing interface */
};

/*
 * Every prefix in our RIB needs a route entry that contains
 * the nexthop of the best path as installed in the kernel FIB.
 * The route entry is the root of a rt_path tree of equal prefixes
 * originated by different routers. It also contains a shortcut
 * for accessing the best route among all contributing routes.
 */
struct rt_entry {
  struct olsr_ip_prefix rt_dst;
  struct avl_node rt_tree_node;
  struct rt_path *rt_best;             /* shortcut to the best path */
  struct rt_nexthop rt_nexthop;        /* nexthop of FIB route */
  struct rt_metric rt_metric;          /* metric of FIB route */
  struct avl_tree rt_path_tree;
  struct list_entity rt_change_node;     /* queue for kernel FIB add/chg/del */
  int failure_count;
};

#define OLSR_FOR_ALL_RTLIST_ENTRIES(head, rt_entry, iterator) list_for_each_element_safe(head, rt_entry, rt_change_node, iterator)

/*
 * For every received route a rt_path is added to the RIB.
 * Depending on the results of the SPF calculation we perform a
 * best_path calculation and pick the one with the lowest etx/metric.
 * The rt_path gets first inserted into the per tc_entry prefix tree.
 * If during the SPF calculation the tc_entry becomes reachable via
 * a local nexthop it is inserted into the global RIB tree.
 */
struct rt_path {
  struct rt_entry *rtp_rt;             /* backpointer to owning route head */
  struct tc_entry *rtp_tc;             /* backpointer to owning tc entry */
  struct rt_nexthop rtp_nexthop;
  struct rt_metric rtp_metric;
  struct avl_node rtp_tree_node;       /* global rtp node */
  struct olsr_ip_prefix rtp_originator; /* originator of the route */
  struct avl_node rtp_prefix_tree_node; /* tc entry rtp node */
  struct olsr_ip_prefix rtp_dst;       /* the prefix */
  uint32_t rtp_version;                /* for detection of outdated rt_paths */
};

#define OLSR_FOR_ALL_RT_PATH_ENTRIES(rt, rtp, iterator) avl_for_each_element_safe(&rt->rt_path_tree, rtp, rtp_tree_node, iterator)

/*
 * Different routes types used in olsrd.
 * Used to track which component did install a route.
 */
enum olsr_rt_origin {
  OLSR_RT_ORIGIN_MIN,
  OLSR_RT_ORIGIN_TC,                   /* TC main-addr route */
  OLSR_RT_ORIGIN_MID,                  /* MID alias route */
  OLSR_RT_ORIGIN_HNA,                  /* HNA route */
  OLSR_RT_ORIGIN_LINK,                 /* 1-hop LINK route */
  OLSR_RT_ORIGIN_MAX
};

/*
 * OLSR_FOR_ALL_RT_ENTRIES
 *
 * macro for traversing the entire routing table.
 * it is recommended to use this because it hides all the internal
 * datastructure from the callers.
 *
 * the loop prefetches the next node in order to not loose context if
 * for example the caller wants to delete the current rt_entry.
 */
#define OLSR_FOR_ALL_RT_ENTRIES(rt, iterator) avl_for_each_element_safe(&routingtree, rt, rt_tree_node, iterator)

/**
 * IPv4 <-> IPv6 wrapper
 */
union olsr_kernel_route {
  struct {
    struct sockaddr rt_dst;
    struct sockaddr rt_gateway;
    uint32_t metric;
  } v4;

  struct {
    struct in6_addr rtmsg_dst;
    struct in6_addr rtmsg_gateway;
    uint32_t rtmsg_metric;
  } v6;
};

extern struct avl_tree EXPORT(routingtree);
extern unsigned int routingtree_version;
extern struct olsr_memcookie_info *rt_mem_cookie;

void olsr_init_routing_table(void);

/**
 * Bump the version number of the routing tree.
 *
 * After route-insertion compare the version number of the routes
 * against the version number of the table.
 * This is a lightweight detection if a node or prefix went away,
 * rather than brute force old vs. new rt_entry comparision.
 */
static INLINE void
olsr_bump_routingtree_version(void)
{
  routingtree_version++;
}


void olsr_rt_best(struct rt_entry *);

/**
 * Check if there is an interface or gateway change.
 */
static INLINE bool
olsr_nh_change(const struct rt_nexthop *nh1, const struct rt_nexthop *nh2)
{
  return olsr_ipcmp(&nh1->gateway, &nh2->gateway) != 0 || nh1->interface != nh2->interface;
}

/**
 * Check if there is a hopcount change.
 */
static INLINE bool
olsr_hopcount_change(const struct rt_metric *met1, const struct rt_metric *met2)
{
  return met1->hops != met2->hops;
}

bool EXPORT(olsr_cmp_rt) (const struct rt_entry *, const struct rt_entry *);

#if defined WIN32

/**
 * Depending if flat_metric is configured and the kernel fib operation
 * return the hopcount metric of a route.
 * For adds this is the metric of best rour after olsr_rt_best() election,
 * for deletes this is the metric of the route that got stored in the rt_entry,
 * during route installation.
 */
static INLINE uint8_t
olsr_fib_metric(const struct rt_metric *met)
{
  return FIBM_CORRECT == olsr_cnf->fib_metric ? met->hops : RT_METRIC_DEFAULT;
}
#endif

char *olsr_rt_to_string(const struct rt_entry *);
char *olsr_rtp_to_string(const struct rt_path *);
void olsr_print_routing_table(void);

/**
 * depending on the operation (add/chg/del) the nexthop
 * field from the route entry or best route path shall be used.
 */
static INLINE const struct rt_nexthop *
olsr_get_nh(const struct rt_entry *rt)
{
  return rt->rt_best != NULL
    /* this is a route add/chg - grab nexthop from the best route. */
    ? &rt->rt_best->rtp_nexthop
    /* this is a route deletion - all routes are gone. */
    : &rt->rt_nexthop;
}


/* rt_path manipulation */
struct rt_path *olsr_insert_routing_table(const union olsr_ip_addr *, const int, const union olsr_ip_addr *, const int);
void olsr_delete_routing_table(union olsr_ip_addr *, int, union olsr_ip_addr *, int);
void olsr_insert_rt_path(struct rt_path *, struct tc_entry *, struct link_entry *);
void olsr_update_rt_path(struct rt_path *, struct tc_entry *, struct link_entry *);
void olsr_delete_rt_path(struct rt_path *);

struct rt_entry *EXPORT(olsr_lookup_routing_table) (const union olsr_ip_addr *);


#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
