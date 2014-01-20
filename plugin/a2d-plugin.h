/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Tomas Popela <tpopela@redhat.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __A2D_PLUGIN_H
#define __A2D_PLUGIN_H

#include <glib-object.h>

#include "headers/npapi.h"
#include "headers/npfunctions.h"

G_BEGIN_DECLS

#define A2D_TYPE_PLUGIN		(a2d_plugin_get_type ())
#define A2D_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), A2D_TYPE_PLUGIN, A2DPlugin))
#define A2D_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), A2D_TYPE_PLUGIN, A2DPluginClass))
#define A2D_IS_PLUGIN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), A2D_TYPE_PLUGIN))
#define A2D_IS_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), A2D_TYPE_PLUGIN))
#define A2D_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), A2D_TYPE_PLUGIN, A2DPluginClass))

typedef struct A2DPluginPrivate A2DPluginPrivate;

typedef struct
{
	GObject		 parent;
	A2DPluginPrivate	*priv;
} A2DPlugin;

typedef struct
{
	GObjectClass	 parent_class;
} A2DPluginClass;

GType		a2d_plugin_get_type			(void);
A2DPlugin *	a2d_plugin_new				(void);
void		a2d_plugin_set_np_netscape_functions	(NPNetscapeFuncs *npnfunctions);
NPObject *	a2d_plugin_get_scriptable_object	(A2DPlugin *plugin);

/*
 * NPClass methods
 */
NPObject *	np_class_allocate			(NPP instance,
							 NPClass* npclass);
void		np_class_deallocate			(NPObject* obj);
bool		np_class_has_method			(NPObject* obj,
							 NPIdentifier method_name);
bool		np_class_invoke_default 		(NPObject* obj,
							 const NPVariant* args,
							 uint32_t arg_count,
							 NPVariant* result);
bool		np_class_invoke				(NPObject* obj,
							 NPIdentifier method_name,
							 const NPVariant* args,
							 uint32_t arg_count,
							 NPVariant* result);
bool		np_class_has_property			(NPObject* obj,
							 NPIdentifier property_mame);
bool		np_class_get_property			(NPObject* obj,
							 NPIdentifier property_Name,
							 NPVariant* result);
G_END_DECLS

#endif /* __A2D_PLUGIN_H */
