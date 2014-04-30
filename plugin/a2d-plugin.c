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

#include <glib.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <string.h>

#include "a2d-plugin.h"

#define A2D_PLUGIN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), A2D_TYPE_PLUGIN, A2DPluginPrivate))

struct A2DPluginPrivate
{
    NPP			pNPInstance;
    NPObject		*pScriptableObject;
};

G_DEFINE_TYPE (A2DPlugin, a2d_plugin, G_TYPE_OBJECT)

#define METHOD_DISABLE "disable"
#define METHOD_ENABLE "enable"
#define METHOD_ADD "add"
#define METHOD_REMOVE "remove"

#define CHROME_EXTENSIONS_PATH "/google-chrome/Default/Extensions/"
#define CHROMIUM_EXTENSIONS_PATH "/chromium/Default/Extensions/"
#define USER_DATA_DIR_APPLICATIONS "/applications/"
#define USER_DATA_DIR_ICONS "/icons/hicolor/"
#define MANIFEST_FILE "manifest.json"

#define CHROME "Chrome"
#define CHROMIUM "Chromium"

static NPNetscapeFuncs *npnfuncs = NULL;
static gboolean running_chromium = FALSE;
static gchar* app_prefix = NULL;
static const gchar* executable = NULL;

/*
 * NPClass
 * https://developer.mozilla.org/en-US/docs/NPClass
 */
static NPClass plugin_ref_obj = {
    NP_CLASS_STRUCT_VERSION,
    np_class_allocate,
    np_class_deallocate,
    NULL,
    np_class_has_method,
    np_class_invoke,
    np_class_invoke_default,
    np_class_has_property,
    np_class_get_property,
    NULL,
    NULL,
    NULL,
    NULL
};

/*
 * update_modification_date:
 *
 * Updates modification date of given file or directory.
 */
static void
update_modification_date (const char *filename)
{
    GStatBuf stat_buf;
    struct utimbuf utim_buf;

    g_stat (filename, &stat_buf);
    utim_buf.actime = stat_buf.st_atime;
    utim_buf.modtime = time (NULL);
    g_utime (filename, &utim_buf);
}

/*
 * set_running_executable:
 *
 * Saves the executable, that we are run from (e.g. Chrome or Chromium).
 *
 */
static void
set_running_executable ()
{
    executable = g_environ_getenv (g_get_environ (), "CHROME_WRAPPER");

    if (executable) {
        if (strstr (executable, "chromium"))
                running_chromium = TRUE;
    } else {
        executable = g_file_read_link ("/proc/self/exe", NULL);
        running_chromium = TRUE;
    }
}

/*
 * get_generated_app_name:
 *
 * Returns .desktop file name of given app.
 */
static gchar *
get_generated_app_name (const gchar *app_id)
{
    gchar *app_name;
    gchar *prefix;

    prefix = app_prefix ? g_strconcat ("-", app_prefix, NULL) : g_strdup ("");

    app_name = g_strconcat ("a2d-", app_id, prefix, NULL);

    g_free (prefix);

    return app_name;
}

/*
 * get_desktop_filename:
 *
 * Returns .desktop file name of given app.
 */
static gchar *
get_desktop_filename (const gchar *app_id)
{
    gchar *desktop_filename;
    gchar *generated_app_name = get_generated_app_name (app_id);

    desktop_filename = g_strconcat (generated_app_name, ".desktop", NULL);

    g_free (generated_app_name);

    return desktop_filename;
}

/*
 * get_desktop_filename_path:
 *
 * Returns .desktop file path of given app.
 */
static gchar *
get_desktop_filename_path (const gchar *app_id)
{
    gchar *desktop_filename_path;
    gchar *desktop_filename = get_desktop_filename (app_id);

    desktop_filename_path = g_strconcat (
        g_get_user_data_dir (), USER_DATA_DIR_APPLICATIONS, desktop_filename, NULL);

    g_free (desktop_filename);

    return desktop_filename_path;
}

/*
 * get_extension_directory_path:
 *
 * Returns path to Chrom(e|ium) extenstion directory.
 */
static gchar *
get_extension_directory_path (const gchar *app_id)
{
    GDir *dir;
    gchar *extension_root;
    gchar *extension_path = NULL;

    extension_root = g_strconcat (
        g_get_user_config_dir (),
        running_chromium ? CHROMIUM_EXTENSIONS_PATH : CHROME_EXTENSIONS_PATH,
        app_id, "/", NULL);

    dir = g_dir_open (extension_root, 0, NULL);

    if (dir) {
        extension_path = g_strconcat (extension_root, g_dir_read_name (dir), "/", NULL);

        g_dir_close (dir);
    }

    g_free (extension_root);

    return extension_path;
}

/*
 * get_app_wm_class:
 *
 * Returns applications WMClass.
 */
static gchar *
get_app_wm_class (const gchar *app_launch_url)
{
        gchar *wm_class, *without_query;
        const gchar *arguments = strstr (app_launch_url, "?");

        if (arguments) {
            without_query = g_strndup (app_launch_url, arguments - app_launch_url - 1);
        } else {
            without_query = g_strdup (app_launch_url);
        }

        arguments = strstr (without_query, "://");

        if (arguments) {
            wm_class = g_strdup (arguments + 3);
        } else {
            wm_class = g_strdup (without_query);
        }

        g_free (without_query);

        arguments = strstr (wm_class, "/");
        if (arguments) {
            gchar *new_wm_class, *beg = g_strndup (wm_class, arguments - wm_class);

            if (g_str_has_suffix (wm_class, "/")) {
                gint length = strlen (arguments) - 2;
                length = length >= 0 ? length : 0;
                gchar *end = g_strndup (arguments + 1, length);
                if (*end)
                    new_wm_class = g_strconcat (beg, "__", end, NULL);
                else
                    new_wm_class = g_strdup (beg);
                g_free (end);
            } else {
                new_wm_class = g_strconcat (beg, "__", arguments + 1, NULL);
            }

            g_free (wm_class);
            g_free (beg);
            wm_class = new_wm_class;
        }

        gint ii = 0;
        while (ii < strlen(wm_class)) {
            unsigned char character = wm_class[ii];

            if (character == ':' || character == '/')
                wm_class[ii] = '_';

            ii++;
        }

        return wm_class;
}

static gboolean
app_updated (const gchar *desktop_file_filename, const gchar *app_version)
{
        gchar *content;
        gboolean ret_val = FALSE;

        if (!g_file_get_contents (desktop_file_filename, &content, NULL, NULL))
            goto out;

        if (strstr (content, "app_version"))
            ret_val = TRUE;

 out:
        g_free (content);

        return ret_val;
}

/*
 * check_if_prefix_needed:
 *
 * Checks if we need prefix before app name. Prefix is required when we have Chrome
 * and Chromium installed at the same time.
 */
static void
check_if_prefix_needed ()
{
    GDir *dir;
    gchar *desktop_file_directory;
    const gchar *desktop_file;
    gboolean already_found_something = FALSE;

    desktop_file_directory = g_strconcat (
        g_get_user_data_dir (), USER_DATA_DIR_APPLICATIONS, NULL);

    dir = g_dir_open (desktop_file_directory, 0, NULL);
    desktop_file = g_dir_read_name (dir);

    while (desktop_file) {
        gchar *content;
        char *desktop_file_path =
            g_strconcat (desktop_file_directory, desktop_file, NULL);

        if (!g_file_get_contents (desktop_file_path, &content, NULL, NULL))
            goto next;

        if (strstr (desktop_file, "a2d-"))
            already_found_something = TRUE;

        if (strstr (content, "xdg-open"))
            goto next;

        if (running_chromium && strstr (content, executable)) {
            if (strstr (desktop_file, CHROMIUM))
                app_prefix = g_strdup (CHROMIUM);

            goto out;
        }

        if (!running_chromium && strstr (content, executable)) {
            if (strstr (desktop_file, CHROME))
                app_prefix = g_strdup (CHROME);

            goto out;
        }

 next:
        g_free (content);
        g_free (desktop_file_path);
        desktop_file = g_dir_read_name (dir);

        continue;
 out:
        g_free (content);
        g_free (desktop_file_path);
        g_free (desktop_file_directory);
        g_dir_close(dir);

        return;
    }

    if (already_found_something)
        app_prefix = g_strdup (running_chromium ? CHROMIUM : CHROME);
    else
        app_prefix = NULL;

    g_free (desktop_file_directory);
    g_dir_close (dir);
}

/*
 * enable_app:
 *
 * Enables / Disables app (hides the .desktop file from app menu) with
 * given id.
 */
static gboolean
enable_app (const gchar* app_id, gboolean enable)
{
    GKeyFile *desktop_file = NULL;
    gboolean ret_val = FALSE;
    gchar* desktop_file_filename = get_desktop_filename_path (app_id);

    if (!g_file_test (desktop_file_filename, G_FILE_TEST_EXISTS))
        goto out;

    desktop_file = g_key_file_new ();

    if (g_key_file_load_from_file (desktop_file,
                                   desktop_file_filename,
                                   G_KEY_FILE_KEEP_TRANSLATIONS,
                                   NULL)) {

        g_key_file_set_boolean (desktop_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_HIDDEN,
                                !enable);
    } else
        goto out;

    g_file_set_contents (desktop_file_filename,
                         g_key_file_to_data (desktop_file, NULL, NULL),
                        -1,
                        NULL);

    ret_val = TRUE;
 out:
    g_key_file_free (desktop_file);
    g_free (desktop_file_filename);

    return ret_val;
}

/*
 * remove_app_icons:
 *
 * Removes icons symlinks.
 */
static void
remove_app_icons (const char* app_id)
{
    GDir *dir;
    gchar *icon_path_root;
    const gchar *icon_size_directory_name;
    gchar *icon_filename;

    icon_path_root = g_strconcat (
        g_get_user_data_dir (), USER_DATA_DIR_ICONS, NULL);

    dir = g_dir_open (icon_path_root, 0, NULL);
    icon_size_directory_name = g_dir_read_name (dir);

    while (icon_size_directory_name) {
        icon_filename = g_strconcat (
                icon_path_root,
                icon_size_directory_name, "/apps/",
                app_id, ".png", NULL);

        g_remove (icon_filename);

        icon_size_directory_name = g_dir_read_name (dir);
        g_free (icon_filename);
    }

    g_free (icon_path_root);
    g_dir_close (dir);
}

/*
 * remove_app:
 *
 * Removes app .desktop file.
 */
static gboolean
remove_app (const char* app_id)
{
    GDir *dir;
    gchar *desktop_file_directory;
    gchar *desktop_file_path;
    const gchar *desktop_file;
    gchar* desktop_file_filename = get_desktop_filename (app_id);

    desktop_file_directory = g_strconcat (
        g_get_user_data_dir (), USER_DATA_DIR_APPLICATIONS, NULL);

    dir = g_dir_open (desktop_file_directory, 0, NULL);
    desktop_file = g_dir_read_name (dir);

    while (desktop_file) {
        if (g_strcmp0 (desktop_file, desktop_file_filename) != 0) {
            desktop_file = g_dir_read_name (dir);

            continue;
        }

        desktop_file_path = g_strconcat (
                desktop_file_directory,
                desktop_file, NULL);

        if (!g_remove (desktop_file_path)) {
            gchar *generated_app_name = get_generated_app_name (app_id);
            remove_app_icons (generated_app_name);
            g_free (desktop_file_path);
            g_free (generated_app_name);
            break;
        }

        g_free (desktop_file_path);
    }

    g_free (desktop_file_directory);
    g_free (desktop_file_filename);
    g_dir_close (dir);

    return FALSE;
}

/*
 * save_localizations:
 *
 * Parses and saves localization data from app to .desktop file.
 */
static void
save_localizations (GKeyFile *desktop_file, const gchar *app_id)
{
    GDir *dir;
    JsonParser *parser;
    JsonReader *reader;
    GError *error = NULL;
    gchar *localization_directory;
    const gchar *locale;
    gchar *extension_directory = get_extension_directory_path(app_id);

    localization_directory = g_strconcat (extension_directory, "_locales/", NULL);

    parser = json_parser_new ();

    dir = g_dir_open (localization_directory, 0, NULL);
    locale = g_dir_read_name (dir);

    while (locale) {
        gchar *json_locale_file =
            g_strconcat (localization_directory, locale, "/messages.json", NULL);

        json_parser_load_from_file (parser, json_locale_file, &error);

        g_free (json_locale_file);

        if (error) {
            g_error_free (error);
            locale = g_dir_read_name (dir);

            continue;
        }

        reader = json_reader_new (json_parser_get_root (parser));

        if (json_reader_read_member (reader, "appName")) {
            json_reader_read_member (reader, "message");
            g_key_file_set_locale_string (desktop_file,
                                          G_KEY_FILE_DESKTOP_GROUP,
                                          G_KEY_FILE_DESKTOP_KEY_NAME,
                                          locale,
                                          json_reader_get_string_value (reader));
            json_reader_end_element (reader);
            json_reader_end_element (reader);
        }

        if (json_reader_read_member (reader, "appDesc")) {
            json_reader_read_member (reader, "message");
            g_key_file_set_locale_string (desktop_file,
                                          G_KEY_FILE_DESKTOP_GROUP,
                                          G_KEY_FILE_DESKTOP_KEY_COMMENT,
                                          locale,
                                          json_reader_get_string_value (reader));
        }
        locale = g_dir_read_name (dir);
    }

    g_dir_close (dir);
    g_object_unref (parser);
    g_object_unref (reader);
    g_free (localization_directory);
    g_free (extension_directory);
}

/*
 * add_app:
 *
 * Parses app informations, creates .desktop file and symlinks app icons.
 */
static gboolean
add_app (const gchar* app_name, const gchar* app_id, const char* app_version,
         const gchar* app_launch_url, gboolean app_enabled)
{
    JsonParser *parser;
    JsonReader *reader;
    GError *error = NULL;
    GKeyFile *desktop_file;
    gboolean offline_enabled = FALSE;
    gboolean ret_val = TRUE;
    gchar *icon_directory, *launch_sequence, *desktop_name, *wm_class, *tmp_prefix;
    gchar *extension_directory = get_extension_directory_path (app_id);
    gchar *desktop_file_filename = get_desktop_filename_path (app_id);
    gchar *generated_app_name = get_generated_app_name (app_id);
    gchar *manifest_file_path = g_strconcat (extension_directory, MANIFEST_FILE, NULL);

    if (g_file_test (desktop_file_filename, G_FILE_TEST_EXISTS)) {
        if (!app_updated (desktop_file_filename, app_version))
            goto out;
        else
            remove_app (app_id);
    }

    parser = json_parser_new ();

    json_parser_load_from_file (parser, manifest_file_path, &error);
    if (error) {
        g_error_free (error);
        g_object_unref (parser);

        ret_val = FALSE;
        goto out;
    }

    g_free (manifest_file_path);

    reader = json_reader_new (json_parser_get_root (parser));
    desktop_file = g_key_file_new ();

    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_TYPE,
                          G_KEY_FILE_DESKTOP_TYPE_APPLICATION);

    tmp_prefix = app_prefix ? g_strconcat (app_prefix, " - ", NULL) : g_strdup ("");
    desktop_name = g_strconcat (tmp_prefix, app_name, NULL);
    g_free (tmp_prefix);

    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_NAME,
                          desktop_name);

    if (*app_launch_url)
        launch_sequence = g_strconcat (executable, " --app=", app_launch_url, NULL);
    else
        launch_sequence = g_strconcat (executable, " --app-id=", app_id, NULL);

    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_EXEC,
                          launch_sequence);

    g_free (launch_sequence);

    if (*app_launch_url) {
        wm_class = get_app_wm_class (app_launch_url);
    } else
        wm_class = g_strconcat ("crx_", app_id, NULL);

    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS,
                          wm_class);
    g_free (wm_class);

    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_CATEGORIES,
                          "NETWORK");
    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_ICON,
                          generated_app_name);
    g_key_file_set_boolean (desktop_file,
                            G_KEY_FILE_DESKTOP_GROUP,
                            G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                            FALSE);

    if (json_reader_read_member (reader, "offline_enabled"))
        offline_enabled = json_reader_get_boolean_value (reader);

    json_reader_end_element (reader);
    g_key_file_set_boolean (desktop_file,
                            G_KEY_FILE_DESKTOP_GROUP,
                            "X-Offline-Enabled",
                            offline_enabled);
    g_key_file_set_boolean (desktop_file,
                            G_KEY_FILE_DESKTOP_GROUP,
                            G_KEY_FILE_DESKTOP_KEY_HIDDEN,
                            !app_enabled);
    g_key_file_set_value (desktop_file,
                          G_KEY_FILE_DESKTOP_GROUP,
                          "X-App-Version",
                          app_version);

//    save_localizations (desktop_file, app_id);

    g_file_set_contents (
            desktop_file_filename,
            g_key_file_to_data (desktop_file, NULL, NULL),
            -1,
            NULL);

    g_key_file_free (desktop_file);

    icon_directory = g_strconcat (
        g_get_user_data_dir (), USER_DATA_DIR_ICONS, NULL);

    if (json_reader_read_member (reader, "icons")) {
        gint ii;
        for (ii = 0; ii < json_reader_count_members (reader); ii++) {
            json_reader_read_element (reader, ii);

            const char *icon_size = json_reader_get_member_name (reader);
            const char *icon_filename = json_reader_get_string_value (reader);

            gchar *dest_icon_path =
                g_strconcat (icon_directory, icon_size, "x", icon_size, "/apps/", NULL);

            gchar *dest_icon_path_icon =
                g_strconcat (dest_icon_path, generated_app_name, ".png", NULL);

            gchar *src_icon_path =
                g_strconcat (extension_directory, icon_filename, NULL);

            g_mkdir_with_parents (dest_icon_path, 0775);
            symlink (src_icon_path, dest_icon_path_icon);

            g_free (dest_icon_path);
            g_free (dest_icon_path_icon);
            g_free (src_icon_path);
        }
    }

    /* When icons are installed we need to update modification time of
     * icon's parent directory to get the icon's cache be rebuilded */
    update_modification_date (icon_directory);

    g_object_unref (reader);
    g_object_unref (parser);
    g_free (icon_directory);
    g_free (desktop_name);

 out:
    g_free (generated_app_name);
    g_free (extension_directory);
    g_free (desktop_file_filename);

    return ret_val;
}

/*
 * a2d_plugin_set_no_netscape_functions:
 *
 */
void
a2d_plugin_set_np_netscape_functions (NPNetscapeFuncs *npnfunctions)
{
    npnfuncs = npnfunctions;
}

NPObject*
np_class_allocate(NPP instance, NPClass* npclass)
{
    return g_new0 (NPObject, 1);
}

void
np_class_deallocate(NPObject* obj)
{
    g_free (obj);
}

bool
np_class_has_method(NPObject* obj, NPIdentifier method_name)
{
    NPIdentifier add_id = npnfuncs->getstringidentifier(METHOD_ADD);
    NPIdentifier remove_id = npnfuncs->getstringidentifier(METHOD_REMOVE);
    NPIdentifier enable_id = npnfuncs->getstringidentifier(METHOD_ENABLE);
    NPIdentifier disable_id = npnfuncs->getstringidentifier(METHOD_DISABLE);

    return (method_name == add_id || method_name == remove_id ||
            method_name == enable_id || method_name == disable_id);
}

bool
np_class_invoke_default(NPObject* obj, const NPVariant* args,
                            uint32_t argCount, NPVariant* result)
{
    return true;
}

bool
np_class_invoke(NPObject* obj, NPIdentifier method_name, const NPVariant* args,
                uint32_t arg_count, NPVariant* result)
{
    NPIdentifier add_id = npnfuncs->getstringidentifier(METHOD_ADD);
    NPIdentifier remove_id = npnfuncs->getstringidentifier(METHOD_REMOVE);
    NPIdentifier enable_id = npnfuncs->getstringidentifier(METHOD_ENABLE);
    NPIdentifier disable_id = npnfuncs->getstringidentifier(METHOD_DISABLE);

    if (!executable) {
        set_running_executable ();
        check_if_prefix_needed ();
    }

    if (method_name == add_id) {
        gboolean ret_val = FALSE;
        gchar *app_name, *app_id, *app_version, *app_launch_url;

        /* 5 arguments */
        /* 1 - app name */
        /* 2 - app id */
        /* 3 - app version */
        /* 4 - launch url */
        /* 5 - enabled ? */
        if (arg_count != 5)
            return false;

        if (!(NPVARIANT_IS_STRING (args[0]) && NPVARIANT_IS_STRING (args[1]) && NPVARIANT_IS_STRING (args[2]) &&
                    NPVARIANT_IS_STRING (args[3]) && NPVARIANT_IS_BOOLEAN (args[4])))
            return false;

        NPString np_app_name = NPVARIANT_TO_STRING(args[0]);
        app_name = g_strndup (np_app_name.UTF8Characters, np_app_name.UTF8Length);
        NPString np_app_id = NPVARIANT_TO_STRING(args[1]);
        app_id = g_strndup (np_app_id.UTF8Characters, np_app_id.UTF8Length);
        NPString np_app_version = NPVARIANT_TO_STRING(args[2]);
        app_version = g_strndup (np_app_version.UTF8Characters, np_app_version.UTF8Length);
        NPString np_app_launch_url = NPVARIANT_TO_STRING(args[3]);
        app_launch_url = g_strndup (np_app_launch_url.UTF8Characters, np_app_launch_url.UTF8Length);
        NPBool app_enabled = NPVARIANT_TO_BOOLEAN(args[4]);

        ret_val = add_app (app_name, app_id, app_version, app_launch_url, app_enabled);

        g_free (app_name);
        g_free (app_id);
        g_free (app_version);
        g_free (app_launch_url);

        return ret_val;
    } else if (method_name == remove_id) {
        gboolean ret_val = FALSE;
        gchar *app_id;

        /* 1 argument */
        /* 1 - app id */
        if (arg_count != 1)
            return false;

        if (!NPVARIANT_IS_STRING (args[0]))
            return false;

        NPString np_app_id = NPVARIANT_TO_STRING(args[0]);
        app_id = g_strndup (np_app_id.UTF8Characters, np_app_id.UTF8Length);

        ret_val = remove_app (app_id);
        g_free (app_id);

        return ret_val;
    } else if (method_name == enable_id || method_name == disable_id) {
        gboolean ret_val = FALSE;
        gchar *app_id;

        /* 1 argument */
        /* 1 - app id */
        if (arg_count != 1)
            return false;

        if (!NPVARIANT_IS_STRING (args[0]))
            return false;

        NPString np_app_id = NPVARIANT_TO_STRING(args[0]);
        app_id = g_strndup (np_app_id.UTF8Characters, np_app_id.UTF8Length);

        ret_val = enable_app (app_id, method_name == enable_id);
        g_free (app_id);

        return ret_val;
    } else {
        npnfuncs->setexception(obj, "Unknown method");
        return false;
    }
    return true;
}

bool
np_class_has_property(NPObject* obj, NPIdentifier property_name)
{
    return false;
}

bool
np_class_get_property(NPObject* obj, NPIdentifier property_name, NPVariant* result)
{
    return false;
}

NPObject *
a2d_plugin_get_scriptable_object (A2DPlugin *plugin)
{
    g_return_val_if_fail (A2D_IS_PLUGIN (plugin), NULL);

    if (!plugin->priv->pScriptableObject) {
        NPObject *scriptableObject;

        scriptableObject = npnfuncs->createobject (plugin->priv->pNPInstance, &plugin_ref_obj);
        npnfuncs->retainobject (scriptableObject);
        plugin->priv->pScriptableObject = scriptableObject;
    }

    return plugin->priv->pScriptableObject;
}

/**
 * a2d_plugin_finalize:
 **/
static void
a2d_plugin_finalize (GObject *object)
{
    A2DPlugin *plugin;
    g_return_if_fail (A2D_IS_PLUGIN (object));
    plugin = A2D_PLUGIN (object);
    g_free (app_prefix);

    if (plugin->priv->pScriptableObject)
        npnfuncs->releaseobject ((NPObject *) plugin->priv->pScriptableObject);

    G_OBJECT_CLASS (a2d_plugin_parent_class)->finalize (object);
}

/**
 * a2d_plugin_class_init:
 **/
static void
a2d_plugin_class_init (A2DPluginClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = a2d_plugin_finalize;

    g_type_class_add_private (klass, sizeof (A2DPluginPrivate));
}

/**
 * a2d_plugin_init:
 **/
static void
a2d_plugin_init (A2DPlugin *plugin)
{
    plugin->priv = A2D_PLUGIN_GET_PRIVATE (plugin);
    plugin->priv->pNPInstance = 0;
    plugin->priv->pScriptableObject = NULL;
}

/**
 * a2d_plugin_new:
 * Return value: A new plugin_install class instance.
 **/
A2DPlugin *
a2d_plugin_new (void)
{
    A2DPlugin *plugin;
    plugin = g_object_new (A2D_TYPE_PLUGIN, NULL);
    return A2D_PLUGIN (plugin);
}

