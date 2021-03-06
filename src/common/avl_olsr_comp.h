/*
 * avl_olsr_comp.h
 *
 *  Created on: 10.07.2010
 *      Author: henning
 */

#ifndef AVL_OLSR_COMP_H_
#define AVL_OLSR_COMP_H_

#include <common/avl.h>

extern avl_tree_comp avl_comp_default;
extern avl_tree_comp avl_comp_addr_origin_default;
extern avl_tree_comp avl_comp_prefix_default;
extern avl_tree_comp avl_comp_prefix_origin_default;

extern int avl_comp_ipv4(const void *, const void *, void *);
extern int avl_comp_ipv6(const void *, const void *, void *);
extern int avl_comp_mac(const void *, const void *, void *);
extern int avl_comp_int(const void *, const void *, void *);
extern int avl_comp_interface_id(const void *, const void *, void *);

int avl_comp_ipv4_prefix(const void *, const void *, void *);
int avl_comp_ipv6_prefix(const void *, const void *, void *);
int avl_comp_ipv4_prefix_origin(const void *, const void *, void *);
int avl_comp_ipv6_prefix_origin(const void *, const void *, void *);
int avl_comp_ipv4_addr_origin(const void *, const void *, void *);
int avl_comp_ipv6_addr_origin(const void *, const void *, void *);

#endif /* AVL_OLSR_COMP_H_ */
