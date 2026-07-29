/* ns-pango
 * ns-pango-cache.h: Control over the caches this fork adds.
 *
 * Copyright (C) 2026 Northstar contributors
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __NS_PANGO_CACHE_H__
#define __NS_PANGO_CACHE_H__

#include <glib.h>

G_BEGIN_DECLS

void ns_pango_cache_clear     (void);

void ns_pango_cache_get_stats (guint64 *hits,
                               guint64 *misses,
                               guint64 *skipped,
                               guint64 *entries);

G_END_DECLS

#endif /* __NS_PANGO_CACHE_H__ */
