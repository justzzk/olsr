
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

#ifndef _OLSR_PLUGIN_LOADER
#define _OLSR_PLUGIN_LOADER

#include "plugin.h"
#include "olsr_types.h"

#include "common/avl.h"
#include "common/list.h"

/*
 * declare a plugin without parameters (NoParameter)
 *
 * OLSR_PLUGIN6_NP() {
 *   .descr = "<description of the plugin",
 *   .author = "<author of the plugin",
 *   .init = <pointer_to_plugin_initialization_callback>,
 *   .....
 * };
 */
#define OLSR_PLUGIN6_NP()   static struct olsr_plugin olsr_internal_plugin_definition; \
static void hookup_plugin_definition (void) __attribute__ ((constructor)); \
static void hookup_plugin_definition (void) { \
  static const char *plname = PLUGIN_FULLNAME; \
  olsr_internal_plugin_definition.name = plname; \
  olsr_hookup_plugin(&olsr_internal_plugin_definition); \
} \
static struct olsr_plugin olsr_internal_plugin_definition =

/*
 * declare a plugin with parameters
 *
 * static const struct olsrd_plugin_parameters plugin_parameters[] = {
 *   { ...},
 * };
 *
 * OLSR_PLUGIN6(plugin_parameters) {
 *   .descr = "<description of the plugin",
 *   .author = "<author of the plugin",
 *   .init = <pointer_to_plugin_initialization_callback>,
 *   .....
 * };
 */
#define OLSR_PLUGIN6(param) static struct olsr_plugin olsr_internal_plugin_definition; \
static void hookup_plugin_definition (void) __attribute__ ((constructor)); \
static void hookup_plugin_definition (void) { \
  static const char *plname = PLUGIN_FULLNAME; \
  olsr_internal_plugin_definition.name = plname; \
  olsr_internal_plugin_definition.internal_param = param; \
  olsr_internal_plugin_definition.internal_param_cnt = ARRAYSIZE(param); \
  olsr_hookup_plugin(&olsr_internal_plugin_definition); \
} \
static struct olsr_plugin olsr_internal_plugin_definition =

/* different types of plugins must be initialized at different times during startup */
enum plugin_type {
  PLUGIN_TYPE_ALL = -1,
  PLUGIN_TYPE_DEFAULT = 0,
  PLUGIN_TYPE_LQ = 1
};

/* plugin definition functions (version 5) */
typedef int (*plugin_init_func) (void);
typedef int (*get_interface_version_func) (void);
typedef void (*get_plugin_parameters_func) (const struct olsrd_plugin_parameters ** internal_params, unsigned int *size);

struct olsr_plugin {
  struct avl_node p_node;

  /* plugin information */
  const char *name;
  const char *descr;
  const char *author;

  /* true if the plugin can be (de)activated during runtime */
  bool deactivate;

  /* type of the plugin, normally 0 */
  enum plugin_type type;

  /* plugin callbacks for (de)initialization */
  int (*init) (void);
  int (*enable) (void);
  int (*disable) (void);
  int (*exit) (void);

  /*
   * internal callback pointer for legacy plugins, will be initialized
   * by the plugin loader
   */
  int (*internal_legacy_init) (void);

  /* plugin interface version */
  int internal_version;

  /* plugin list of possible arguments */
  const struct olsrd_plugin_parameters *internal_param;

  /* number of arguments */
  unsigned int internal_param_cnt;

  /* internal olsr data */
  void *internal_dlhandle;
  struct plugin_param *internal_params;
  bool internal_active;
};

#define OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin, iterator) avl_for_each_element_safe(&plugin_tree, plugin, p_node, iterator)

struct olsr_plugin *EXPORT(olsr_get_plugin)(const char *libname);

void EXPORT(olsr_hookup_plugin) (struct olsr_plugin *plugin);
void EXPORT(olsr_unhookup_plugin) (struct olsr_plugin *plugin);

void olsr_init_pluginsystem(void);
void olsr_plugins_init(bool);
void olsr_plugins_enable(enum plugin_type type, bool fail_fast);
void olsr_destroy_pluginsystem(void);

struct olsr_plugin *EXPORT(olsr_init_plugin)(const char *);
int EXPORT(olsr_exit_plugin)(struct olsr_plugin *);

int EXPORT(olsr_enable_plugin)(struct olsr_plugin *);
int EXPORT(olsr_disable_plugin)(struct olsr_plugin *);

extern struct avl_tree EXPORT(plugin_tree);

#endif

/*
 * Local Variables:
 * mode: c
 * style: linux
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
