
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

#include "parser.h"
#include "ipcalc.h"
#include "defs.h"
#include "process_package.h"
#include "olsr_clock.h"
#include "duplicate_set.h"
#include "mid_set.h"
#include "olsr.h"
#include "os_net.h"
#include "olsr_logging.h"
#include "net_olsr.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#ifdef WIN32
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

static void parse_packet(uint8_t *binary, int size, struct interface *in_if, union olsr_ip_addr *from_addr);

static struct parse_function_entry *parse_functions = NULL;
static struct preprocessor_function_entry *preprocessor_functions = NULL;
static struct packetparser_function_entry *packetparser_functions = NULL;

static int olsr_forward_message(struct olsr_message *msg,
    uint8_t *binary, struct interface *in_if, union olsr_ip_addr *from_addr);

/**
 *Initialize the parser.
 *
 *@return nada
 */
void
olsr_init_parser(void)
{
  OLSR_INFO(LOG_PACKET_PARSING, "Initializing parser...\n");

  /* Initialize the packet functions */
  olsr_init_package_process();
}

void
olsr_deinit_parser(void)
{
  OLSR_INFO(LOG_PACKET_PARSING, "Deinitializing parser...\n");
  olsr_deinit_package_process();
}

void
olsr_parser_add_function(parse_function * function, uint32_t type)
{
  struct parse_function_entry *new_entry;

  new_entry = olsr_malloc(sizeof(*new_entry), "Register parse function");

  new_entry->function = function;
  new_entry->type = type;

  /* Queue */
  new_entry->next = parse_functions;
  parse_functions = new_entry;

  OLSR_INFO(LOG_PACKET_PARSING, "Register parse function: Added function for type %u\n", type);
}

int
olsr_parser_remove_function(parse_function * function)
{
  struct parse_function_entry *entry, *prev;

  for (entry = parse_functions, prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
    if ((entry->function == function)) {
      if (entry == parse_functions) {
        parse_functions = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry);
      return 1;
    }
  }
  return 0;
}

void
olsr_preprocessor_add_function(preprocessor_function * function)
{
  struct preprocessor_function_entry *new_entry;

  new_entry = olsr_malloc(sizeof(*new_entry), "Register preprocessor function");
  new_entry->function = function;

  /* Queue */
  new_entry->next = preprocessor_functions;
  preprocessor_functions = new_entry;

  OLSR_INFO(LOG_PACKET_PARSING, "Registered preprocessor function\n");
}

int
olsr_preprocessor_remove_function(preprocessor_function * function)
{
  struct preprocessor_function_entry *entry, *prev;

  for (entry = preprocessor_functions, prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
    if (entry->function == function) {
      if (entry == preprocessor_functions) {
        preprocessor_functions = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry);
      return 1;
    }
  }
  return 0;
}

void
olsr_packetparser_add_function(packetparser_function * function)
{
  struct packetparser_function_entry *new_entry;

  new_entry = olsr_malloc(sizeof(struct packetparser_function_entry), "Register packetparser function");

  new_entry->function = function;

  /* Queue */
  new_entry->next = packetparser_functions;
  packetparser_functions = new_entry;

  OLSR_INFO(LOG_PACKET_PARSING, "Registered packetparser  function\n");
}

int
olsr_packetparser_remove_function(packetparser_function * function)
{
  struct packetparser_function_entry *entry, *prev;

  for (entry = packetparser_functions, prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
    if (entry->function == function) {
      if (entry == packetparser_functions) {
        packetparser_functions = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry);
      return 1;
    }
  }
  return 0;
}

static void
olsr_parse_msg_hdr(const uint8_t **curr, struct olsr_message *msg)
{
  assert(curr);
  assert(msg);

  msg->header = *curr;

  pkt_get_u8(curr, &msg->type);
  pkt_get_reltime(curr, &msg->vtime);
  pkt_get_u16(curr, &msg->size);
  pkt_get_ipaddress(curr, &msg->originator);
  pkt_get_u8(curr, &msg->ttl);
  pkt_get_u8(curr, &msg->hopcnt);
  pkt_get_u16(curr, &msg->seqno);

  msg->payload = *curr;
  msg->end = msg->header + msg->size;
}

/**
 *Process a newly received OLSR packet. Checks the type
 *and to the neccessary convertions and call the
 *corresponding functions to handle the information.
 *@param from the sockaddr struct describing the sender
 *@param olsr the olsr struct containing the message
 *@param size the size of the message
 *@return nada
 */
static void
parse_packet(uint8_t *binary, int size, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  struct olsr_packet pkt;
  struct olsr_message msg;
  struct parse_function_entry *entry;
  struct packetparser_function_entry *packetparser;
  enum duplicate_status dup_status = 0;
  uint8_t *curr, *end;
  const uint8_t *packet;

#if !defined(REMOVE_LOG_INFO) || !defined(REMOVE_LOG_WARN)
  struct ipaddr_str buf;
#endif

  curr = binary;
  packet = binary;
  end = binary + size;

  /* packet smaller than minimal olsr packet ? */
  if (size < 4) {
    OLSR_WARN(LOG_PACKET_PARSING, "Received too small packet (%u bytes) from %s\n",
        size, olsr_ip_to_string(&buf, from_addr));
    return;
  }

  pkt_get_u16(&packet, &pkt.size);
  pkt_get_u16(&packet, &pkt.seqno);

  if (pkt.size != (size_t) size) {
    OLSR_WARN(LOG_PACKET_PARSING, "Received packet from %s (%u bytes) has bad size field: %u bytes\n",
              olsr_ip_to_string(&buf, from_addr), size, pkt.size);
    return;
  }

  // call packetparser
  for (packetparser = packetparser_functions; packetparser != NULL; packetparser = packetparser->next) {
    packetparser->function(&pkt, binary, in_if, from_addr);
  }

  /* advance pointer to messages */
  curr += 4;

  for (;curr <= end - MIN_MESSAGE_SIZE(); curr += msg.size) {
    const uint8_t *msg_payload = curr;
    olsr_parse_msg_hdr(&msg_payload, &msg);

    /* Check size of message */
    if (curr + msg.size > end) {
      OLSR_WARN(LOG_PACKET_PARSING, "Packet received from %s is too short (%u bytes) for message %u (%u bytes)!",
          olsr_ip_to_string(&buf, from_addr), size, msg.type, msg.size);
      break;
    }
    else if (msg.size == 0) {
      OLSR_WARN(LOG_PACKET_PARSING, "Received a zero lengthed message from %s (typde %d), ignoring all further content of the packet!",
          olsr_ip_to_string(&buf, from_addr), msg.type);
      return;
    }

    /*RFC 3626 section 3.4:
     *  2    If the time to live of the message is less than or equal to
     *  '0' (zero), or if the message was sent by the receiving node
     *  (i.e., the Originator Address of the message is the main
     *  address of the receiving node): the message MUST silently be
     *  dropped.
     */

    /* Should be the same for IPv4 and IPv6 */
    if (olsr_ipcmp(&msg.originator, &olsr_cnf->router_id) == 0
        || !olsr_validate_address(&msg.originator)) {
      OLSR_INFO(LOG_PACKET_PARSING, "Skip processing our own message coming from %s!\n",
                olsr_ip_to_string(&buf, from_addr));
      continue;
    }

    if (msg.ttl == 0 || (int)msg.ttl + (int)msg.hopcnt > 255) {
#if !defined(REMOVE_LOG_WARN)
      struct ipaddr_str buf2;
#endif
      OLSR_WARN(LOG_PACKET_PARSING, "Malformed incoming message type %u from %s with originator %s: ttl=%u and hopcount=%u\n",
          msg.type, olsr_ip_to_string(&buf, from_addr), olsr_ip_to_string(&buf2, &msg.originator), msg.ttl, msg.hopcnt);
      continue;
    }
    if (olsr_is_duplicate_message(&msg, false, &dup_status)) {
      OLSR_INFO(LOG_PACKET_PARSING, "Not processing message duplicate from %s (seqnr %u)!\n",
          olsr_ip_to_string(&buf, &msg.originator), msg.seqno);
    }
    else {
      OLSR_DEBUG(LOG_PACKET_PARSING, "Processing message type %u (seqno %u) from %s\n",
          msg.type, msg.seqno, olsr_ip_to_string(&buf, &msg.originator));
      for (entry = parse_functions; entry != NULL; entry = entry->next) {
        /* Should be the same for IPv4 and IPv6 */
        /* Promiscuous or exact match */
        if ((entry->type == PROMISCUOUS) || (entry->type == msg.type)) {
          entry->function(&msg, in_if, from_addr, dup_status);
        }
      }
    }
    olsr_forward_message(&msg, curr, in_if, from_addr);
  }                             /* for olsr_msg */
}

/**
 *Processing OLSR data from socket. Reading data, setting
 *wich interface recieved the message, Sends IPC(if used)
 *and passes the packet on to parse_packet().
 *
 *@param fd the filedescriptor that data should be read from.
 *@return nada
 */
void
olsr_input(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  unsigned int cpu_overload_exit = 0;
#ifndef REMOVE_LOG_DEBUG
  char addrbuf[128];
#endif
#if !defined(REMOVE_LOG_INFO) || !defined(REMOVE_LOG_WARN)
  struct ipaddr_str buf;
#endif

  for (;;) {
    struct interface *olsr_in_if;
    union olsr_ip_addr from_addr;
    struct preprocessor_function_entry *entry;
    uint8_t *packet;
    /* sockaddr_in6 is bigger than sockaddr !!!! */
    union olsr_sockaddr from;
    socklen_t fromlen;
    int size;
    uint8_t inbuf[MAXMESSAGESIZE] __attribute__ ((aligned));

    if (32 < ++cpu_overload_exit) {
      OLSR_WARN(LOG_PACKET_PARSING, "CPU overload detected, ending olsr_input() loop\n");
      break;
    }

    fromlen = sizeof(from);
    size = os_recvfrom(fd, inbuf, sizeof(inbuf), 0, &from, &fromlen);

    if (size <= 0) {
      if (size < 0 && errno != EWOULDBLOCK) {
        OLSR_WARN(LOG_PACKET_PARSING, "error recvfrom: %s", strerror(errno));
      }
      break;
    }

    OLSR_DEBUG(LOG_PACKET_PARSING, "Recieved a packet from %s (fd=%d)\n",
               sockaddr_to_string(addrbuf, sizeof(addrbuf), (struct sockaddr *)&from, fromlen),
               fd);

    if (olsr_cnf->ip_version == AF_INET) {
      /* IPv4 sender address */
      if (fromlen != sizeof(struct sockaddr_in)) {
        OLSR_WARN(LOG_PACKET_PARSING, "Got wrong ip size from recv()\n");
        break;
      }
      from_addr.v4 = ((struct sockaddr_in *)&from)->sin_addr;
    } else {
      /* IPv6 sender address */
      if (fromlen != sizeof(struct sockaddr_in6)) {
        OLSR_WARN(LOG_PACKET_PARSING, "Got wrong ip size from recv()\n");
        break;
      }
      from_addr.v6 = ((struct sockaddr_in6 *)&from)->sin6_addr;
    }

    /* are we talking to ourselves? */
    if (if_ifwithaddr(&from_addr) != NULL) {
      OLSR_INFO(LOG_PACKET_PARSING, "Ignore packet from ourself (%s).\n",
          olsr_ip_to_string(&buf, &from_addr));
      return;
    }
    olsr_in_if = if_ifwithsock(fd);
    if (olsr_in_if == NULL) {
      OLSR_WARN(LOG_PACKET_PARSING, "Could not find input interface for message from %s size %d\n",
                olsr_ip_to_string(&buf, &from_addr), size);
      return;
    }
    // call preprocessors
    packet = &inbuf[0];
    for (entry = preprocessor_functions; entry != NULL; entry = entry->next) {
      packet = entry->function(packet, olsr_in_if, &from_addr, &size);
      // discard package ?
      if (packet == NULL) {
        OLSR_INFO(LOG_PACKET_PARSING, "Discard package because of preprocessor\n");
        return;
      }
    }

    /*
     * &from - sender
     * &inbuf.olsr
     * size - bytes read
     */
    parse_packet(packet, size, olsr_in_if, &from_addr);
  }
}

/**
 *Check if a message is to be forwarded and forward
 *it if necessary.
 *
 *@param m the OLSR message recieved
 *
 *@returns positive if forwarded
 */
static int
olsr_forward_message(struct olsr_message *msg, uint8_t *binary, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  union olsr_ip_addr *src;
  struct nbr_entry *neighbor;
  struct interface *ifn, *iterator;
  uint8_t *tmp;
#if !defined REMOVE_LOG_DEBUG
  struct ipaddr_str buf;
#endif

  /* Lookup sender address */
  src = olsr_lookup_main_addr_by_alias(from_addr);
  if (!src)
    src = from_addr;

  neighbor = olsr_lookup_nbr_entry(src, true);
  if (!neighbor) {
    OLSR_DEBUG(LOG_PACKET_PARSING, "Not forwarding message type %d because no nbr entry found for %s\n",
        msg->type, olsr_ip_to_string(&buf, src));
    return 0;
  }
  if (!neighbor->is_sym) {
    OLSR_DEBUG(LOG_PACKET_PARSING, "Not forwarding message type %d because received by non-symmetric neighbor %s\n",
        msg->type, olsr_ip_to_string(&buf, src));
    return 0;
  }

  /* Check MPR */
  if (neighbor->mprs_count == 0) {
    OLSR_DEBUG(LOG_PACKET_PARSING, "Not forwarding message type %d because we are no MPR for %s\n",
        msg->type, olsr_ip_to_string(&buf, src));
    /* don't forward packages if not a MPR */
    return 0;
  }

  /* check if we already forwarded this message */
  if (olsr_is_duplicate_message(msg, true, NULL)) {
    OLSR_DEBUG(LOG_PACKET_PARSING, "Not forwarding message type %d from %s because we already forwarded it.\n",
        msg->type, olsr_ip_to_string(&buf, src));
    return 0;                   /* it's a duplicate, forget about it */
  }

  /* Treat TTL hopcnt */
  msg->hopcnt++;
  msg->ttl--;
  tmp = binary;
  olsr_put_msg_hdr(&tmp, msg);

  if (msg->ttl == 0) {
    OLSR_DEBUG(LOG_PACKET_PARSING, "Not forwarding message type %d from %s because TTL is 0.\n",
        msg->type, olsr_ip_to_string(&buf, src));
    return 0;                   /* TTL 0, forget about it */
  }
  OLSR_DEBUG(LOG_PACKET_PARSING, "Forwarding message type %d from %s.\n",
      msg->type, olsr_ip_to_string(&buf, src));

  /* looping trough interfaces */
  OLSR_FOR_ALL_INTERFACES(ifn, iterator) {
    if (net_output_pending(ifn)) {
      /* dont forward to incoming interface if interface is mode ether */
      if (in_if->mode == IF_MODE_ETHER && ifn == in_if)
        continue;

      /*
       * Check if message is to big to be piggybacked
       */
      if (net_outbuffer_push(ifn, binary, msg->size) != msg->size) {
        /* Send */
        net_output(ifn);
        /* Buffer message */
        set_buffer_timer(ifn);

        if (net_outbuffer_push(ifn, binary, msg->size) != msg->size) {
          OLSR_WARN(LOG_NETWORKING, "Received message to big to be forwarded in %s(%d bytes)!", ifn->int_name, msg->size);
        }
      }
    } else {
      /* No forwarding pending */
      set_buffer_timer(ifn);

      if (net_outbuffer_push(ifn, binary, msg->size) != msg->size) {
        OLSR_WARN(LOG_NETWORKING, "Received message to big to be forwarded in %s(%d bytes)!", ifn->int_name, msg->size);
      }
    }
  }

  return 1;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
