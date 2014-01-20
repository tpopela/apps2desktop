/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
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

#include <glib/gprintf.h>

#include "headers/npapi.h"
#include "headers/npfunctions.h"
#include "headers/npruntime.h"

#include "a2d-plugin.h"

static NPNetscapeFuncs *npnfuncs = NULL;

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32_t)(x)) & 0xff00) >> 8)
#endif

/**
 * a2d_main_get_value:
 **/
static NPError
a2d_main_get_value (NPP instance, NPPVariable variable, void *value)
{
	NPError err = NPERR_NO_ERROR;

	switch (variable) {
	case NPPVpluginNameString:
		* ((const gchar **)value) = "Apps2Desktop";
		break;
	case NPPVpluginDescriptionString:
		* ((const gchar **)value) = "Plugin for creating .desktop files for Google Chrom(e|ium) Apps";
		break;
  	case NPPVpluginScriptableNPObject:
		if(instance == NULL)
			return NPERR_INVALID_INSTANCE_ERROR;
		A2DPlugin *plugin = A2D_PLUGIN (instance->pdata);
		if(plugin == NULL)
			return NPERR_GENERIC_ERROR;

		a2d_plugin_set_np_netscape_functions (npnfuncs);
		NPObject *new_instance;
		new_instance = a2d_plugin_get_scriptable_object (plugin);
		*(NPObject **) value = new_instance;
		break;
	case NPPVpluginNeedsXEmbed:
		*((char *)value) = 1;
		break;
	default:
		err = NPERR_INVALID_PARAM;
	}
	return err;
}

/**
 * a2d_main_newp:
 **/
static NPError
a2d_main_newp (NPMIMEType pluginType, NPP instance, uint16_t mode,
	int16_t argc, char *argn[], char *argv[], NPSavedData *saved)
{
	bool bWindowed = false;
	A2DPlugin *plugin;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	npnfuncs->setvalue (instance, NPPVpluginWindowBool, (void *) bWindowed);

	plugin = A2D_PLUGIN (a2d_plugin_new ());

	instance->pdata = plugin;

	return NPERR_NO_ERROR;
}

/**
 * a2d_main_destroy:
 **/
static NPError
a2d_main_destroy (NPP instance, NPSavedData **save)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	A2DPlugin *plugin = A2D_PLUGIN (instance->pdata);

	if (plugin != NULL)
		g_object_unref (plugin);

	return NPERR_NO_ERROR;
}

/**
 * a2d_main_set_window:
 **/
static NPError
a2d_main_set_window (NPP instance, NPWindow* pNPWindow)
{
	A2DPlugin *plugin;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	/* find plugin */
	plugin = A2D_PLUGIN (instance->pdata);
	if (plugin == NULL)
		return NPERR_GENERIC_ERROR;

	/* shutdown */
	if (pNPWindow == NULL)
		return NPERR_NO_ERROR;

	return NPERR_NO_ERROR;
}

NPError NP_GetEntryPoints (NPPluginFuncs *nppfuncs);

/**
 * NP_GetEntryPoints:
 **/
NPError
NP_GetEntryPoints (NPPluginFuncs *nppfuncs)
{
	nppfuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	nppfuncs->newp = a2d_main_newp;
	nppfuncs->destroy = a2d_main_destroy;
	nppfuncs->getvalue = a2d_main_get_value;
	nppfuncs->setwindow = a2d_main_set_window;

	return NPERR_NO_ERROR;
}

/**
 * NP_Initialize:
 **/
NPError
NP_Initialize (NPNetscapeFuncs *npnf, NPPluginFuncs *nppfuncs)
{
	if (npnf == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if (HIBYTE (npnf->version) > NP_VERSION_MAJOR)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	npnfuncs = npnf;
	NP_GetEntryPoints (nppfuncs);
	return NPERR_NO_ERROR;
}

/**
 * NP_Shutdown:
 **/
NPError
NP_Shutdown ()
{
	return NPERR_NO_ERROR;
}

/**
 * NP_GetMIMEDescription:
 **/
const char *
NP_GetMIMEDescription (void)
{
	return (const gchar*) "application/basic-plugin:bsp:Application basic plugin";
}

/**
 * NP_GetValue:
 **/
NPError OSCALL
NP_GetValue (void *npp, NPPVariable variable, void *value)
{
	return a2d_main_get_value ((NPP)npp, variable, value);
}

