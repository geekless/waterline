/**
 * Copyright (c) 2006 LxDE Developers, see the file AUTHORS for details.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plugin_internal.h"
#include "plugin_private.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
#include <gdk/gdk.h>
#include <string.h>
#include <stdlib.h>

#include <waterline/paths.h>
#include <waterline/misc.h>
#include "bg.h"
#include "panel_internal.h"
#include "panel_private.h"

#include <glib/gi18n.h>
#include <glib-object.h>

static GList * pcl = NULL; /* List of PluginClass structures */

static PluginClass * register_plugin_class(PluginClass * pc, gboolean dynamic);
static void init_plugin_class_list(void);
static PluginClass * plugin_find_class(const char * type);
static PluginClass * plugin_load_dynamic(const char * type, const gchar * path);
static void plugin_class_unref(PluginClass * pc);

/* Dynamic parameter for static (built-in) plugins must be FALSE so we will not try to unload them */
#define REGISTER_STATIC_PLUGIN_CLASS(pc) \
do {\
    extern PluginClass pc;\
    register_plugin_class(&pc, FALSE);\
} while (0)

/* Register a PluginClass. */
static PluginClass * register_plugin_class(PluginClass * pc, gboolean dynamic)
{
    size_t structure_size = sizeof(PluginClass);
    if (structure_size < pc->structure_actual_size)
        structure_size = pc->structure_actual_size;

    PluginClass * pc1 = (PluginClass *) g_malloc0(structure_size);
    memcpy(pc1, pc, pc->structure_actual_size);
    pc = pc1;

    pcl = g_list_append(pcl, pc);
    pc->dynamic = dynamic;
    pc->internal = g_new0(PluginClassInternal, 1);

    return pc;
}

/* Initialize the static plugins. */
static void init_plugin_class_list(void)
{
#ifndef DISABLE_MENU
#ifdef STATIC_LAUNCHBAR
    REGISTER_STATIC_PLUGIN_CLASS(launchbar_plugin_class);
#endif
#endif

#ifdef STATIC_LAUNCHBUTTON
    REGISTER_STATIC_PLUGIN_CLASS(launchbutton_plugin_class);
#endif

#ifdef STATIC_DCLOCK
    REGISTER_STATIC_PLUGIN_CLASS(dclock_plugin_class);
#endif

#ifdef STATIC_DIRMENU
    REGISTER_STATIC_PLUGIN_CLASS(dirmenu_plugin_class);
#endif

#ifndef DISABLE_MENU
#ifdef STATIC_MENU
    REGISTER_STATIC_PLUGIN_CLASS(menu_plugin_class);
#endif
#endif

#ifdef STATIC_SEPARATOR
    REGISTER_STATIC_PLUGIN_CLASS(separator_plugin_class);
#endif
}

/* Look up a plugin class by name. */
static PluginClass * plugin_find_class(const char * type)
{
    GList * tmp;
    for (tmp = pcl; tmp != NULL; tmp = g_list_next(tmp))
    {
        PluginClass * pc = (PluginClass *) tmp->data;
        if (g_ascii_strcasecmp(type, pc->type) == 0)
            return pc;
    }
    return NULL;
}

/* Load a dynamic plugin. */
static PluginClass * plugin_load_dynamic(const char * type, const gchar * path)
{
    PluginClass * pc = NULL;

    GModule * m = NULL;
    gchar * class_name = NULL;

    /* Load the external module. */
    m = g_module_open(path, 0);

    if (m == NULL)
    {
        su_print_error_message("%s: %s\n", path, g_module_error());
        goto err;
    }

    /* Formulate the name of the expected external variable of type PluginClass. */
    class_name = g_strdup_printf("%s_plugin_class", type);

    gpointer tmpsym;
    if (( ! g_module_symbol(m, class_name, &tmpsym))
    || ((pc = tmpsym) == NULL))
    {
        su_print_error_message("%s.so is not a waterline plugin: symbol %s not found\n", type, class_name);
        goto err;
    }

    if (pc->structure_magic != PLUGINCLASS_MAGIC)
    {
        su_print_error_message("%s.so is not a waterline plugin: invalid magic value 0x%lx (should be 0x%lx)\n", type, pc->structure_magic, PLUGINCLASS_MAGIC);
        goto err;
    }

    if (pc->structure_version != PLUGINCLASS_VERSION)
    {
        su_print_error_message("%s.so: invalid plugin class version: %d\n", type, pc->structure_version);
        goto err;
    }

    if (pc->structure_subversion != 0)
    {
        su_print_error_message("%s.so: unknown plugin class subversion: %d\n", type, pc->structure_subversion);
        /* not fatal error */
    }

    if (pc->structure_base_size != PLUGINCLASS_BASE_SIZE)
    {
        su_print_error_message("%s.so: invalid plugin class base size: %d (should be %d)\n", type, pc->structure_base_size, PLUGINCLASS_BASE_SIZE);
        goto err;
    }

    if (pc->structure_actual_size < pc->structure_base_size)
    {
        su_print_error_message("%s.so: invalid plugin class actual size: %d (should be not less than %d)\n", type, pc->structure_actual_size, pc->structure_base_size);
        goto err;
    }

    if (strcmp(type, pc->type) != 0)
    {
        su_print_error_message("%s.so: invalid plugin class type: %s\n", type, pc->type);
        goto err;
    }

    /* Register the newly loaded and valid plugin. */
    pc = register_plugin_class(pc, TRUE);
    pc->internal->gmodule = m;

    g_free(class_name);

    return pc;

err:
    if (m)
        g_module_close(m);
    g_free(class_name);
    return NULL;
}

/* Unreference a dynamic plugin. */
static void plugin_class_unref(PluginClass * pc)
{
    pc->internal->count -= 1;

    /* If the reference count drops to zero, unload the plugin if it is dynamic and has declared itself unloadable. */
    if ((pc->internal->count == 0)
    && (pc->dynamic)
    && ( ! pc->not_unloadable))
    {
        pcl = g_list_remove(pcl, pc);
        PluginClassInternal * internal = pc->internal;
        g_module_close(internal->gmodule);
        g_free(internal);
        g_free(pc);
    }
}


/* Create an instance of a plugin with a specified name, loading it if external. */
Plugin * plugin_load(const char * type)
{
    /* Initialize static plugins on first call. */
    if (pcl == NULL)
        init_plugin_class_list();

    /* Look up the PluginClass. */
    PluginClass * pc = plugin_find_class(type);

#ifndef DISABLE_PLUGINS_LOADING
    /* If not found and dynamic loading is available, try to locate an external plugin. */
    if ((pc == NULL) && (g_module_supported()))
    {
        gchar * soname = g_strdup_printf("%s.so", type);
        gchar * path = wtl_resolve_own_resource("lib", "plugins", soname, 0);
        pc = plugin_load_dynamic(type, path);
        g_free(path);
        g_free(soname);
    }
#endif  /* DISABLE_PLUGINS_LOADING */

    /* If not found, return failure. */
    if (pc == NULL)
        return NULL;

    /* Instantiate the plugin */
    Plugin * plug = g_new0(Plugin, 1);
    plug->class = pc;
    pc->internal->count += 1;

    plug->json = json_object();

    return plug;
}

static void plugin_size_allocate(GtkWidget * widget, GtkAllocation * allocation, Plugin * pl)
{
    pl->pwid_allocation = *allocation;
}


/* Configure and start a plugin by calling its constructor. */
int plugin_start(Plugin * pl)
{
    /* Call the constructor.
     * It is responsible for parsing the parameters, and setting "pwid" to the top level widget. */
    if ( ! pl->class->constructor(pl))
        return 0;

    /* If this plugin can only be instantiated once, count the instantiation.
     * This causes the configuration system to avoid displaying the plugin as one that can be added. */
    if (pl->class->one_per_system)
        pl->class->one_per_system_instantiated = TRUE;

    /* If the plugin has a top level widget, add it to the panel's container. */
    if (pl->pwid != NULL)
    {
        g_signal_connect(G_OBJECT(pl->pwid), "size-allocate", G_CALLBACK(plugin_size_allocate), (gpointer) pl);
        gtk_widget_set_name(pl->pwid, pl->class->type);
        gtk_box_pack_start(GTK_BOX(pl->panel->plugin_box), pl->pwid, pl->expand, TRUE, pl->padding);
        gtk_container_set_border_width(GTK_CONTAINER(pl->pwid), pl->border);
        gtk_widget_show(pl->pwid);
    }
    return 1;
}

/* Unload a plugin if initialization fails. */
void plugin_unload(Plugin * plugin)
{
    plugin_class_unref(plugin->class);
    json_decref(plugin->json);
    g_free(plugin);
}

/* Delete a plugin. */
void plugin_delete(Plugin * plugin)
{
    Panel * p = plugin->panel;
    PluginClass * pc = plugin->class;

    /* If a plugin configuration dialog is open, close it. */
    if (p->pref_dialog.plugin_pref_dialog != NULL)
    {
        gtk_widget_destroy(p->pref_dialog.plugin_pref_dialog);
        p->pref_dialog.plugin_pref_dialog = NULL;
    }

    if (plugin->icon_size_dialog)
    {
        gtk_widget_destroy(plugin->icon_size_dialog);
        plugin->icon_size_dialog = NULL;
    }

    /* Run the destructor and then destroy the top level widget.
     * This prevents problems with the plugin destroying child widgets. */
    pc->destructor(plugin);
    if (plugin->pwid != NULL)
        gtk_widget_destroy(plugin->pwid);

    /* Data structure bookkeeping. */
    pc->one_per_system_instantiated = FALSE;
    plugin_class_unref(pc);

    json_decref(plugin->json);

    /* Free the Plugin structure. */
    g_free(plugin);
}

/* Get a list of all available plugin classes.
 * Returns a newly allocated GList which should be freed with plugin_class_list_free(list). */
GList * plugin_get_available_classes(void)
{
    /* Initialize static plugins on first call. */
    if (pcl == NULL)
        init_plugin_class_list();

    /* Loop over all classes to formulate the result.
     * Increase the reference count; it will be decreased in plugin_class_list_free. */
    GList * classes = NULL;
    GList * l;
    for (l = pcl; l != NULL; l = l->next)
    {
        PluginClass * pc = (PluginClass *) l->data;
        classes = g_list_prepend(classes, pc);
        pc->internal->count += 1;
    }

#ifndef DISABLE_PLUGINS_LOADING
    gchar * plugin_dir = wtl_resolve_own_resource("lib", "plugins", 0);
    GDir * dir = g_dir_open(plugin_dir, 0, NULL);
    g_free(plugin_dir);

    if (dir != NULL)
    {
        const char * file;
        while ((file = g_dir_read_name(dir)) != NULL)
        {
            if (g_str_has_suffix(file, ".so"))
            {
                char * type = g_strndup(file, strlen(file) - 3);
                if (plugin_find_class(type) == NULL)
                {
                    /* If it has not been loaded, do it.  If successful, add it to the result. */
                    char * path = wtl_resolve_own_resource("lib", "plugins", file, 0);
                    PluginClass * pc = plugin_load_dynamic(type, path);
                    if (pc != NULL)
                    {
                        pc->internal->count += 1;
                        classes = g_list_prepend(classes, pc);
                    }
                    g_free(path);
                }
                g_free(type);
            }
        }
        g_dir_close(dir);
    }
#endif
    return classes;
}

/* Free the list allocated by plugin_get_available_classes. */
void plugin_class_list_free(GList * list)
{
   g_list_foreach(list, (GFunc) plugin_class_unref, NULL);
   g_list_free(list);
}

/* Handler for "button_press_event" signal with Plugin as parameter.
 * External so can be used from a plugin. */
gboolean plugin_button_press_event(GtkWidget *widget, GdkEventButton *event, Plugin *plugin)
{
    panel_button_press_hack(plugin->panel);

    if (event->button == 3) /* right button */
    {
        plugin_show_menu(plugin, event);
        return TRUE;
    }
    return panel_handle_drag_move(plugin->panel, event);
}

/* Helper for position-calculation callback for popup menus. */
void plugin_popup_set_position_helper(Plugin * p, GtkWidget * near, GtkWidget * popup, GtkRequisition * popup_req, gint * px, gint * py)
{
    plugin_popup_set_position_helper2(p, near, popup, popup_req, 0, 0, px, py);
}

void plugin_popup_set_position_helper2(Plugin * p, GtkWidget * near, GtkWidget * popup, GtkRequisition * popup_req, int offset, float alignment, gint * px, gint * py)
{
    int popop_width  = 0;
    int popop_height = 0;
    if (popup_req)
    {
        popop_width  = popup_req->width;
        popop_height = popup_req->height;
    }
    else
    {
        popop_width  = popup->allocation.width;
        popop_height = popup->allocation.height;
    }

    /* Get the origin of the requested-near widget in screen coordinates. */
    gint x, y;
    gdk_window_get_origin(GDK_WINDOW(near->window), &x, &y);
    if (x != near->allocation.x) x += near->allocation.x; /* Doesn't seem to be working according to spec; the allocation.x sometimes has the window origin in it */
    if (y != near->allocation.y) y += near->allocation.y;

    /* Dispatch on edge to lay out the popup menu with respect to the button.
     * Also set "push-in" to avoid any case where it might flow off screen. */
    switch (p->panel->edge)
    {
        case EDGE_TOP:
             y += near->allocation.height + offset;
             x += near->allocation.width * alignment - (popop_width * alignment);
             break;
        case EDGE_BOTTOM:
             y -= popop_height + offset;
             x += near->allocation.width * alignment - (popop_width * alignment);
             break;
        case EDGE_LEFT:
             x += near->allocation.width + offset;
             y += near->allocation.height * alignment - (popop_height * alignment);
             break;
        case EDGE_RIGHT:
             x -= popop_width + offset;
             y += near->allocation.height * alignment - (popop_height * alignment);
             break;
    }

    switch (p->panel->edge)
    {
        case EDGE_TOP:
        case EDGE_BOTTOM:
        {
             int screen_width = gdk_screen_width();
             while (x < 0 && x + popop_width < screen_width) x++;
             while (x > 0 && x + popop_width > screen_width) x--;
             break;
        }
        case EDGE_LEFT:
        case EDGE_RIGHT:
        {
             int screen_height = gdk_screen_height();
             while (y < 0 && y + popop_height < screen_height) y++;
             while (y > 0 && y + popop_height > screen_height) y--;
             break;
        }
    }


    *px = x;
    *py = y;
}

/* Adjust the position of a popup window. */
void plugin_adjust_popup_position(GtkWidget * popup, Plugin * plugin)
{
    /* Initialize. */
    Panel * p = plugin->panel;
    GtkWidget * parent = plugin->pwid;

    /* Get the coordinates of the plugin top level widget. */
    int x, y;
    int offset = 2;

    switch (p->edge)
    {
        case EDGE_TOP:
        case EDGE_BOTTOM:
            x = p->cx + parent->allocation.x + (parent->allocation.width - popup->allocation.width) / 2;
            break;
        case EDGE_LEFT:
        case EDGE_RIGHT:
            y = p->cy + parent->allocation.y + (parent->allocation.height - popup->allocation.height) / 2;
            break;
    }

    switch (p->edge)
    {
        case EDGE_TOP:
            y = p->cy + p->ch + offset;
            break;
        case EDGE_BOTTOM:
            y = p->cy - popup->allocation.height - offset;
            break;
        case EDGE_LEFT:
            x = p->cx + p->cw + offset;
            break;
        case EDGE_RIGHT:
            x = p->cx - popup->allocation.width - offset;
            break;
    }

    /* Clip the coordinates to ensure that the popup remains on screen. */
    int screen_width = gdk_screen_width();
    int screen_height = gdk_screen_height();
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if ((x + popup->allocation.width) > screen_width) x = screen_width - popup->allocation.width;
    if ((y + popup->allocation.height) > screen_height) y = screen_height - popup->allocation.height;

    /* Move the popup to position. */
    gtk_window_move(GTK_WINDOW(popup), x, y);
}

void plugin_lock_visible(Plugin * plugin)
{
    plugin->lock_visible++;
    panel_autohide_conditions_changed(plugin->panel);
}

void plugin_unlock_visible(Plugin * plugin)
{
    plugin->lock_visible--;
    if (plugin->lock_visible < 0)
        plugin->lock_visible = 0;
    panel_autohide_conditions_changed(plugin->panel);
}

void plugin_run_command(Plugin * plugin, char ** argv, int argc)
{
    if (plugin && plugin->class->run_command)
        plugin->class->run_command(plugin, argv, argc);
}

gchar * plugin_get_config_path(Plugin * plugin, const char * file_name, SU_PATH_CONFIG_TYPE config_type)
{
    gchar * path = g_build_filename("plugins", plugin->class->type, file_name, NULL);
    gchar * result = wtl_get_config_path(path, config_type);
    g_free(path);
    return result;
}

Panel * plugin_panel(Plugin * plugin)
{
    return plugin->panel;
}

PluginClass * plugin_class(Plugin * plugin)
{
    return plugin->class;
}

GtkWidget * plugin_widget(Plugin * plugin)
{
    return plugin->pwid;
}

void plugin_set_widget(Plugin * plugin, GtkWidget * widget)
{
    plugin->pwid = widget;
}

void * plugin_priv(Plugin * plugin)
{
    return plugin->priv;
}

void plugin_set_priv(Plugin * plugin, void * priv)
{
    plugin->priv = priv;
}

void plugin_set_has_system_menu(Plugin * plugin, gboolean v)
{
    plugin->has_system_menu = v;
}

int plugin_get_icon_size(Plugin * plugin)
{
    if (!plugin)
        return 0;

    plugin->icon_size_used = TRUE;
    if (plugin->icon_size > 0)
        return plugin->icon_size;

    return panel_get_icon_size(plugin_panel(plugin));
}

int plugin_get_orientation(Plugin * plugin)
{
    return panel_get_orientation(plugin_panel(plugin));
}

GtkMenu * plugin_get_menu(Plugin * plugin)
{
    return panel_get_panel_menu(plugin_panel(plugin), plugin);
}

void plugin_show_menu(Plugin * plugin, GdkEventButton * event)
{
    panel_show_panel_menu(plugin_panel(plugin), plugin, event);
}

void plugin_save_configuration(Plugin * plugin)
{
    panel_save_configuration(plugin_panel(plugin));
}

json_t * plugin_inner_json(Plugin * plugin)
{
    json_t * json_inner = json_object_get(plugin->json, "settings");
    if (!json_is_object(json_inner))
    {
        json_inner = json_object();
        json_object_set_nocheck(plugin->json, "settings", json_inner);
        json_decref(json_inner);
    }

    return json_inner;
}

gchar * plugin_get_display_name(Plugin * plugin)
{
    const char * name1 = _(plugin->class->name);
    gchar * name2 = plugin->class->get_name ? plugin->class->get_name(plugin) : NULL;
    gchar * name = su_str_empty(name2) ? g_strdup(name1) : g_strdup_printf(_("%s (%s)"), name1, name2);
    g_free(name2);
    return name;
}

gboolean plugin_get_expandable(Plugin * plugin)
{
    return plugin && plugin->class && plugin->class->expand_available;
}

gboolean plugin_get_expand(Plugin * plugin)
{
    return plugin && plugin->expand;
}

void plugin_set_expand(Plugin * plugin, gboolean value)
{
    if (!plugin)
        return;

    if (plugin->expand == value)
        return;

    plugin->expand = value;

    if (plugin->panel->plugin_box && plugin->pwid)
    {
        gboolean old_expand, fill;
        guint padding;
        GtkPackType pack_type;

        gtk_box_query_child_packing(
            GTK_BOX(plugin->panel->plugin_box), plugin->pwid, &old_expand, &fill, &padding, &pack_type);
        gtk_box_set_child_packing(
            GTK_BOX(plugin->panel->plugin_box), plugin->pwid, plugin->expand, fill, padding, pack_type);
    }

    plugin->panel->config_changed = TRUE;
}
