
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
 * Dynamic linked library for UniK OLSRd
 */

#ifndef _NAMESERVICE_PLUGIN
#define _NAMESERVICE_PLUGIN

#include <regex.h>

#include "olsr_types.h"
#include "interfaces.h"
#include "olsr_protocol.h"
#include "common/list.h"
#include "duplicate_set.h"
#include "plugin.h"
#include "nameservice_msg.h"
#include "hashing.h"
#include "mapwrite.h"
#include "olsr_clock.h"

#define PLUGIN_NAME	"OLSRD nameservice plugin"
#define PLUGIN_VERSION	"0.3"
#define PLUGIN_AUTHOR   "Bruno Randolf, Jens Nachtigall, Sven-Ola Tuecke"

#define MESSAGE_TYPE		130
#define PARSER_TYPE		MESSAGE_TYPE
#define EMISSION_INTERVAL	120     /* seconds */
#define EMISSION_JITTER         25      /* percent */
#define NAME_VALID_TIME		1800    /* seconds */
#define NAMESERVER_COUNT        3

#define NAME_PROTOCOL_VERSION	1

#define MAX_NAME 127
#define MAX_FILE 255
#define MAX_SUFFIX 63

#define MID_ENTRIES 1
#define MID_MAXLEN 16
#define MID_PREFIX "mid%i."

/**
 * a linked list of name_entry
 * if type is NAME_HOST, name is a hostname and ip its IP addr
 * if type is NAME_FORWARDER, then ip is a dns-server (and name is irrelevant)
 * if type is NAME_SERVICE, then name is a service-line (and the ip is irrelevant)
 * if type is NAME_LATLON, then name has 2 floats with lat and lon (and the ip is irrelevant)
 */
struct name_entry {
  union olsr_ip_addr ip;
  uint16_t type;
  uint16_t len;
  char *name;
  struct name_entry *next;             /* linked list */
};

/* *
 * linked list of db_entries for each originator with
 * originator being its router_id
 *
 * names points to the name_entry with its hostname, dns-server or
 * service-line entry
 *
 * all the db_entries are hashed in nameservice.c to avoid a too long list
 * for many nodes in a net
 *
 * */
struct db_entry {
  union olsr_ip_addr originator;       /* IP address of the node this entry describes */
  struct olsr_timer_entry *db_timer;        /* Validity time */
  struct name_entry *names;            /* list of names this originator declares */
  struct list_entity db_list;            /* linked list of db entries per hash container */
};


#define OLSR_NAMESVC_DB_JITTER 5        /* percent */

extern struct name_entry *my_names;
extern struct list_entity latlon_list[HASHSIZE];
extern float my_lat, my_lon;
extern struct olsr_memcookie_info *map_poll_timer_cookie;

void olsr_expire_write_file_timer(void *);
void olsr_namesvc_delete_db_entry(struct db_entry *);

/* Parser function to register with the sceduler */
void olsr_parser(struct olsr_message *, struct interface *, union olsr_ip_addr *, enum duplicate_status);

/* callback for periodic timer */
void olsr_namesvc_gen(void *);

int
  encap_namemsg(struct namemsg *);

struct name_entry *add_name_to_list(struct name_entry *my_list, const char *value, int type, const union olsr_ip_addr *ip);

struct name_entry *remove_nonvalid_names_from_list(struct name_entry *my_list, int type);

void
  free_all_listold_entries(struct list_entity *);

void
  decap_namemsg(const struct name *from_packet, struct name_entry **to, bool * this_table_changed);

void
  insert_new_name_in_list(union olsr_ip_addr *, struct list_entity *, const struct name *, bool *, uint32_t);

bool allowed_hostname_or_ip_in_service(const char *service_line, const regmatch_t * hostname_or_ip);

void
  update_name_entry(union olsr_ip_addr *, const struct namemsg *, const uint8_t *, uint32_t);

void
  write_hosts_file(void);

void
  write_services_file(bool writemacs);

void
  write_resolv_file(void);

int
  register_olsr_param(char *key, char *value);

void
  free_name_entry_list(struct name_entry **list);

bool allowed_ip(const union olsr_ip_addr *addr);

bool allowed_service(const char *service_line);

bool is_name_wellformed(const char *service_line);

bool is_service_wellformed(const char *service_line);

bool is_mac_wellformed(const char *service_line);

bool is_latlon_wellformed(const char *latlon_line);

bool get_isdefhna_latlon(void);

void
  lookup_defhna_latlon(union olsr_ip_addr *ip);

const char *lookup_name_latlon(union olsr_ip_addr *ip);

void
  write_latlon_file(void);

char *create_packet(struct name *to, struct name_entry *from);

void
  name_constructor(void);

void
  name_destructor(void);

int
  name_init(void);

#endif


/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
