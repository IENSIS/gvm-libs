/* OpenVAS
 * $Id$
 * Description: Functions related to plugin cache and.
 *
 * Authors:
 * Renaud Deraison <deraison@nessus.org> (Original pre-fork development)
 *
 * Copyright:
 * Based on work Copyright (C) 1998 - 2003 Renaud Deraison
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** @file
 * OpenVAS-Scanner employs a plugin cache to avoid parsing all known nvts at
 * start-up.
 *
 * The cache consists of a .desc file for each script (e.g. cache file of
 * nvts/xyz.nasl is nvts/xyz.nas.desc), which contains a memory dump of the
 * corresponding plugin struct.
 *
 * The cache is used as followed:
 *
 * 1. Init the store with store_init.
 *
 * 2. Add nvts by calling store_plugin or
 *
 * 3. Give the store a file path (store_load_plugin)
 * and receive the plugin as arglist. Under nice conditions the information
 * contained in the cache file can be used. Under not so nice conditions, the
 * store returns NULL (cache is either outdated, contains error or an error
 * occurred).
 *
 * The store is updated at each openvassd start up. There the plugin loader
 * iterates over plugin files and tries to retrieve the cached version.
 * If there is no cached version (or @ref store_load_plugin returns Null for
 * another reason, e.g.because  the script file seems to have been modified in
 * between) the plugin is added to the store (@ref store_plugin).
 */

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/param.h>

#include <glib.h>

#include "share_fd.h"
#include "system.h"
#include "plugutils.h"

#include "nvti.h"

/* for nvticache_get */
#include "nvticache.h"

void
_add_plugin_preference (struct arglist *prefs, const char *p_name,
                        const char *name, const char *type, const char *defaul)
{
  char *pref;
  char *cname;
  int len;

  cname = estrdup (name);
  len = strlen (cname);
  // Terminate string before last trailing space
  while (cname[len - 1] == ' ')
    {
      cname[len - 1] = '\0';
      len--;
    }
  if (!prefs || !p_name)
    {
      efree (&cname);
      return;
    }


  pref = emalloc (strlen (p_name) + 10 + strlen (type) + strlen (cname));
  // RATS: ignore
  snprintf (pref, strlen (p_name) + 10 + strlen (type) + strlen (cname),
            "%s[%s]:%s", p_name, type, cname);
  if (arg_get_value (prefs, pref) == NULL)
    arg_add_value (prefs, pref, ARG_STRING, strlen (defaul), estrdup (defaul));

  efree (&cname);
  efree (&pref);
}

/**
 * @brief Returns a (plugin) arglist assembled from the cached description file
 *
 * @param file Filename of the plugin (e.g. "scriptname1.nasl"
 *             or "subdir1/subdir2/scriptname2.nasl" ).
 *
 * @param prefs Plugin preference arglist.
 *
 * NULL is returned in either of these cases:
 * 1) The .NVT definition or .desc file does not exist.
 * 2) NVT definition file (e.g. xyz.nasl) or nvt signature (xyz.asc) file is
 *    newer than the .desc file.
 * 3) The NVT definition files (e.g. xyz.nasl) or nvt signature (xyz.asc) files
 *    timestamp is in the future.
 * 4) The magic number test failed (other file format expected).
 * 5) An internal error occured.
 *
 * Point 4) is necessary because the cache will not create .desc files with
 * timestamps in the future. Thus, when creating a new cache file for the given
 * NVT, it would not be able to become loaded from the cache (point 2)).
 *
 * @return Pointer to plugin as arglist or NULL.
 */
struct arglist *
store_load_plugin (const char *file, struct arglist *prefs)
{
  struct arglist *ret;
  int i;

  nvti_t *n = nvticache_get (arg_get_value(prefs, "nvticache"), file);
  if (!n)
    return NULL;

  ret = emalloc (sizeof (struct arglist));

  arg_add_value (ret, "NVTI", ARG_PTR, -1, n);
  arg_add_value (ret, "preferences", ARG_ARGLIST, -1, prefs);

  for (i = 0; i < nvti_pref_len (n); i++)
    {
      nvtpref_t *np = nvti_pref (n, i);
      _add_plugin_preference (prefs, nvti_name (n), nvtpref_name (np),
                              nvtpref_type (np), nvtpref_default (np));
    }

  return ret;
}
