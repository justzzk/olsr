
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2007, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2007, Hannes Gredler <hannes-at-gredler.at>
 * Copyright (c) 2007, Sven-Ola Tuecke <sven-ola-at-gmx.de>
 * Copyright (c) 2008, Bernd Petrovitsch <bernd-at-firmix.at>
 * Copyright (c) 2008, Hannes Gredler <hannes-at-gredler.at>
 * Copyright (c) 2008, Sven-Ola Tuecke <sven-ola-at-gmx.de>
 * Copyright (c) 2009, Henning Rogge <rogge-at-fgan.de>
 * Copyright (c) 2009, Sven-Ola Tuecke <sven-ola-at-gmx.de>
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

#ifndef _IPCALC
#define _IPCALC

#include "olsr_types.h"
#include "defs.h"
#include "olsr_cfg.h"
#include "common/string.h"

#include <limits.h>
#include <string.h>
#include <arpa/inet.h>

extern const union olsr_ip_addr all_zero;

extern const struct olsr_ip_prefix ipv4_internet_route;
extern const struct olsr_ip_prefix ipv6_mappedv4_route;
extern const struct olsr_ip_prefix ipv6_internet_route;

struct ipaddr_str {
  char buf[MAX(INET6_ADDRSTRLEN, INET_ADDRSTRLEN)];
};

struct ipprefix_str {
  /* We need for IPv6 an IP address + '/' + prefix and for IPv4 an IP address + '/' + a netmask */
  char buf[MAX(INET6_ADDRSTRLEN + 1 + 3, INET_ADDRSTRLEN + 1 + 2)];
};

/*
 * Macros for comparing and copying IP addresses
 */
static INLINE int
ip4cmp(const struct in_addr *a, const struct in_addr *b)
{
  return a->s_addr > b->s_addr ? +1 : a->s_addr < b->s_addr ? -1 : 0;
}

static INLINE int
ip6cmp(const struct in6_addr *a, const struct in6_addr *b)
{
  return memcmp(a, b, sizeof(*a));
}

static INLINE int
ipcmp(int ip_version, const union olsr_ip_addr *a, const union olsr_ip_addr *b)
{
  return ip_version == AF_INET ? ip4cmp(&a->v4, &b->v4) : ip6cmp(&a->v6, &b->v6);
}

static INLINE int
olsr_ipcmp(const union olsr_ip_addr *a, const union olsr_ip_addr *b)
{
  return ipcmp(olsr_cnf->ip_version, a, b);
}

/* Do not use this - this is as evil as the COPY_IP() macro was and only used in
 * source which also needs cleanups.
 */
static INLINE void
genipcopy(void *dst, const void *src)
{
  memcpy(dst, src, olsr_cnf->ipsize);
}

int ip_in_net(const union olsr_ip_addr *ipaddr, const struct olsr_ip_prefix *net, int ip_version);


int EXPORT(prefix_to_netmask) (uint8_t *, int, uint8_t);

static INLINE int
olsr_prefix_to_netmask(union olsr_ip_addr *adr, uint8_t prefixlen)
{
  return prefix_to_netmask(adr->v6.s6_addr, olsr_cnf->ipsize, prefixlen);
}

uint8_t EXPORT(netmask_to_prefix) (const uint8_t *, int);

static INLINE uint8_t
olsr_netmask_to_prefix(const union olsr_ip_addr *adr)
{
  return netmask_to_prefix(adr->v6.s6_addr, olsr_cnf->ipsize);
}

static INLINE uint8_t
olsr_netmask4_to_prefix(const uint32_t * a)
{
  return netmask_to_prefix((const uint8_t *)a, sizeof(*a));
}
static INLINE uint8_t
olsr_netmask6_to_prefix(const struct in6_addr *a)
{
  return netmask_to_prefix((const uint8_t *)a, sizeof(*a));
}

static INLINE const char *
ip4_to_string(struct ipaddr_str *const buf, const struct in_addr addr4)
{
  return inet_ntop(AF_INET, &addr4, buf->buf, sizeof(buf->buf));
}

static INLINE const char *
ip6_to_string(struct ipaddr_str *const buf, const struct in6_addr *const addr6)
{
  return inet_ntop(AF_INET6, addr6, buf->buf, sizeof(buf->buf));
}

static INLINE const char *
olsr_sockaddr_to_string(struct ipaddr_str *const buf, const union olsr_sockaddr *s) {
  if (s->storage.ss_family == AF_INET) {
    return inet_ntop(AF_INET, &s->v4.sin_addr, buf->buf, sizeof(buf->buf));
  }
  else if (s->storage.ss_family == AF_INET6) {
    return inet_ntop(AF_INET6, &s->v6.sin6_addr, buf->buf, sizeof(buf->buf));
  }
  else {
    strscpy(buf->buf, "unkown ip family", sizeof(buf->buf));
    return buf->buf;
  }
}

static INLINE const char *
ip_to_string(int af, struct ipaddr_str *const buf, const union olsr_ip_addr *addr)
{
  return inet_ntop(af, addr, buf->buf, sizeof(buf->buf));
}

static INLINE const char *
olsr_ip_to_string(struct ipaddr_str *const buf, const union olsr_ip_addr *addr)
{
  return ip_to_string(olsr_cnf->ip_version, buf, addr);
}

const char *EXPORT(ip_prefix_to_string) (int af, struct ipprefix_str * const buf, const struct olsr_ip_prefix * prefix);

static INLINE const char *
olsr_ip_prefix_to_string(struct ipprefix_str *const buf, const struct olsr_ip_prefix *prefix)
{
  return ip_prefix_to_string(olsr_cnf->ip_version, buf, prefix);
}

const char *sockaddr_to_string(char *buf, int bufsize, const struct sockaddr *const addr, unsigned int addrsize);

/* we need to handle one value specifically since shifting 32 bits of a 32 bit integer is the same as shifting 0 bits.
 * The result is in host-byte-order.
 */
static INLINE uint32_t
prefix_to_netmask4(uint8_t prefixlen)
{
  return prefixlen == 0 ? 0 : (~0U << (32 - prefixlen));
}

extern const struct in6_addr EXPORT(in6addr_v4mapped_loopback);

void EXPORT(ip_map_4to6) (union olsr_ip_addr *ip);

static INLINE bool
is_prefix_niit_ipv6(const struct olsr_ip_prefix *p) {
  return olsr_cnf->ip_version == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&p->prefix.v6)
      && p->prefix_len >= ipv6_mappedv4_route.prefix_len;
}

static INLINE struct olsr_ip_prefix *
prefix_mappedv4_to_v4(struct olsr_ip_prefix *v4, const struct olsr_ip_prefix *v6) {
  memcpy(&v4->prefix.v4, &v6->prefix.v6.s6_addr[12], sizeof(struct in_addr));
      v4->prefix_len = v6->prefix_len - 96;
  return v4;
}

static INLINE bool
ip_is_linklocal(const union olsr_ip_addr *ip) {
  return olsr_cnf->ip_version == AF_INET6
      && ip->v6.s6_addr[0] == 0xfe && (ip->v6.s6_addr[1] & 0xc0) == 0x80;
}

static INLINE bool
ip_prefix_is_mappedv4(const struct olsr_ip_prefix *prefix) {
  return prefix->prefix_len >= ipv6_mappedv4_route.prefix_len
      && memcmp(prefix, &ipv6_mappedv4_route, ipv6_mappedv4_route.prefix_len / 8) == 0;
}

static INLINE bool
ip_prefix_is_mappedv4_inetgw(const struct olsr_ip_prefix *prefix) {
  return prefix->prefix_len == ipv6_mappedv4_route.prefix_len
      && memcmp(prefix, &ipv6_mappedv4_route, ipv6_mappedv4_route.prefix_len / 8) == 0;
}

static INLINE bool
ip_prefix_is_v4_inetgw(const struct olsr_ip_prefix *prefix) {
  return prefix->prefix_len == ipv4_internet_route.prefix_len
      && prefix->prefix.v4.s_addr == ipv4_internet_route.prefix.v4.s_addr;
}

static INLINE bool
ip_prefix_is_v6_inetgw(const struct olsr_ip_prefix *prefix) {
  return prefix->prefix_len == ipv6_internet_route.prefix_len
      && memcmp(prefix, &ipv6_internet_route, ipv6_internet_route.prefix_len/8) == 0;
}

extern bool is_prefix_inetgw(const struct olsr_ip_prefix *prefix);

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
