/**
 * Copyright (c) 2011-2013 Vadim Ushakov
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
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <waterline/global.h>
#include "plugin_internal.h"
#include "plugin_private.h"
#include <waterline/paths.h>
#include <waterline/panel.h>
#include "panel_internal.h"
#include "panel_private.h"
#include <waterline/misc.h>
#include "bg.h"
#include <waterline/x11_utils.h>
#include <waterline/gtkcompat.h>

/******************************************************************************/

static gint
panel_popupmenu_configure(GtkWidget *widget, gpointer user_data)
{
    panel_configure( (Panel*)user_data, 0 );
    return TRUE;
}

static void panel_popupmenu_expand_plugin(GtkMenuItem * item, Plugin * plugin)
{
    plugin_set_expand(plugin, !plugin_get_expand(plugin));
}

static void panel_popupmenu_config_plugin( GtkMenuItem* item, Plugin* plugin )
{
    plugin->class->show_properties(plugin, GTK_WINDOW(plugin->panel->topgwin));

    /* FIXME: this should be more elegant */
    plugin->panel->config_changed = TRUE;
}

static void set_icon_size(Plugin * plugin, int value)
{
    plugin->icon_size = value;
    plugin->panel->config_changed = TRUE;
    panel_preferences_changed(plugin->panel, 0);

    if (plugin->icon_size_dialog)
    {
        gtk_widget_destroy(plugin->icon_size_dialog);
        plugin->icon_size_dialog = NULL;
    }
}

static void panel_popupmenu_plugin_icon_size_default(GtkCheckMenuItem * item, Plugin * plugin)
{
    set_icon_size(plugin, 0);
}

static void panel_popupmenu_plugin_icon_size_n(GtkCheckMenuItem * item, Plugin * plugin)
{
    const char * v = gtk_menu_item_get_label(GTK_MENU_ITEM(item));
    if (*v == '_')
        v++;

    set_icon_size(plugin, atoi(v));
}

static void set_icon_size_callback(char * value, gpointer p)
{
    Plugin * plugin = (Plugin *) p;

    if (value)
        set_icon_size(plugin, atoi(value));
    else
        set_icon_size(plugin, plugin->icon_size);
}

static void panel_popupmenu_plugin_icon_size_custom(GtkCheckMenuItem * item, Plugin * plugin)
{
    gchar * value = g_strdup_printf("%d", plugin->icon_size);

    if (plugin->icon_size_dialog)
    {
        gtk_widget_destroy(plugin->icon_size_dialog);
        plugin->icon_size_dialog = NULL;
    }

    plugin->icon_size_dialog = wtl_create_entry_dialog(_("Set icon size"), _("Icon size:"), value, set_icon_size_callback, plugin);
}


static void panel_popupmenu_add_item(GtkMenuItem* item, Panel* panel)
{
    /* panel_add_plugin( panel, panel->topgwin ); */
    panel_configure(panel, 3);
}

static void panel_popupmenu_remove_item( GtkMenuItem* item, Plugin* plugin )
{
    Panel* panel = plugin->panel;

    GtkWidget* dlg = gtk_message_dialog_new_with_markup(GTK_WINDOW(panel->topgwin),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_QUESTION,
                                             GTK_BUTTONS_OK_CANCEL,
                                             _("Really delete plugin \"%s\" from the panel?"),
                                             _(plugin->class->name));

    panel_apply_icon(GTK_WINDOW(dlg));
    gtk_window_set_title(GTK_WINDOW(dlg), _("Confirm") );
    gboolean ok = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK;
    gtk_widget_destroy( dlg );

    if (!ok)
        return;


    if (panel->pref_dialog.pref_dialog != NULL)
    {
        configurator_remove_plugin_from_list(panel, plugin);
    }

    panel->plugins = g_list_remove( panel->plugins, plugin );
    plugin_delete(plugin);
    panel_save_configuration(panel);
}

static void panel_popupmenu_create_panel( GtkMenuItem* item, Panel* panel )
{
    create_empty_panel();
}

static void panel_popupmenu_delete_panel( GtkMenuItem* item, Panel* panel )
{
    gboolean ok = TRUE;

    if (panel->plugins)
    {
        GtkWidget* dlg;

        dlg = gtk_message_dialog_new_with_markup( GTK_WINDOW(panel->topgwin),
                                                  GTK_DIALOG_MODAL,
                                                  GTK_MESSAGE_QUESTION,
                                                  GTK_BUTTONS_OK_CANCEL,
                                                  _("Really delete this panel?\n<b>Warning: This can not be recovered.</b>") );
        panel_apply_icon(GTK_WINDOW(dlg));
        gtk_window_set_title( (GtkWindow*)dlg, _("Confirm") );
        ok = ( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK );
        gtk_widget_destroy( dlg );
    }

    if (ok)
        delete_panel(panel);
}

static void panel_popupmenu_about( GtkMenuItem* item, Panel* panel )
{
    GtkWidget *about;
    const gchar* authors[] = {
        "Vadim Ushakov (geekless) <igeekless@gmail.com>",
        "Hong Jen Yee (PCMan) <pcman.tw@gmail.com>",
        "Jim Huang <jserv.tw@gmail.com>",
        "Greg McNew <gmcnew@gmail.com> (battery plugin)",
        "Fred Chien <cfsghost@gmail.com>",
        "Daniel Kesler <kesler.daniel@gmail.com>",
        "Juergen Hoetzel <juergen@archlinux.org>",
        "Marty Jack <martyj19@comcast.net>",
        NULL
    };
    /* TRANSLATORS: Replace this string with your names, one name per line. */
    gchar *translators = _("translator-credits");

    gchar * logo_path = wtl_resolve_own_resource("", "images", "my-computer.png", 0);

    about = gtk_about_dialog_new();
    panel_apply_icon(GTK_WINDOW(about));
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), _("Waterline"));
    gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about), gdk_pixbuf_new_from_file(logo_path, NULL));
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about), _("Copyright (C) 2008-2013"));
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), _("A lightweight framework for desktop widgets and applets"));
    gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about), wtl_license);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), wtl_website);
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about), translators);
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_destroy(about);

    g_free(logo_path);
}

static void panel_popupmenu_quit( GtkMenuItem* item, Panel* panel )
{
    gtk_main_quit();
}

GtkMenu * panel_get_panel_menu(Panel * panel, Plugin * plugin)
{
    GtkMenuShell * panel_submenu = GTK_MENU_SHELL(gtk_menu_new());

    if (!wtl_is_in_kiosk_mode())
    {
        {
            GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("Panel _Settings..."));
            char * tooltip = g_strdup_printf( _("Edit settings of the panel \"%s\""), panel->name);
            gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tooltip);
            g_free(tooltip);
            gtk_menu_shell_append(panel_submenu, menu_item);
            g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(panel_popupmenu_configure), panel );
        }

        {
            GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Add Applet..."));
            gtk_menu_shell_append(panel_submenu, menu_item);
            g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_add_item), panel );
        }

        gtk_menu_shell_append(panel_submenu, gtk_separator_menu_item_new());

        {
            GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Delete Panel"));
            char * tooltip = g_strdup_printf( _("Delete panel \"%s\""), panel->name);
            gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tooltip);
            g_free(tooltip);
            gtk_menu_shell_append(panel_submenu, menu_item);
            g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_delete_panel), panel);
            gtk_widget_set_sensitive(menu_item, panel_count() > 1);
        }

        {
            GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Create New Panel"));
            gtk_menu_shell_append(panel_submenu, menu_item);
            g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_create_panel), panel);
        }

        gtk_menu_shell_append(panel_submenu, gtk_separator_menu_item_new());

        if (quit_in_menu)
        {
            {
                GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Quit"));
                gtk_menu_shell_append(panel_submenu, menu_item);
                g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_quit), panel );
            }

            gtk_menu_shell_append(panel_submenu, gtk_separator_menu_item_new());
        }
    }

    {
        GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("A_bout"));
        gtk_menu_shell_append(panel_submenu, menu_item);
        g_signal_connect( menu_item, "activate", G_CALLBACK(panel_popupmenu_about), panel);
    }

    /*****************************************************/

    GtkMenuShell * applet_submenu = NULL;

    if (plugin && !wtl_is_in_kiosk_mode())
    {
        applet_submenu = GTK_MENU_SHELL(gtk_menu_new());

        {
            GtkWidget * menu_item = gtk_check_menu_item_new_with_mnemonic(_("_Stretch to Available Space"));
            gtk_widget_set_sensitive(menu_item, plugin_get_expandable(plugin));
            if (plugin_get_expandable(plugin))
            {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), plugin_get_expand(plugin));
                g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_expand_plugin), plugin);
            }
            gtk_menu_shell_append(applet_submenu, menu_item);
        }

        if (plugin->icon_size_used)
        {
            GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Icon Size"));
            gtk_menu_shell_append(applet_submenu, menu_item);

            GtkMenuShell * icon_size_submenu = GTK_MENU_SHELL(gtk_menu_new());
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(icon_size_submenu));

            {
                GtkWidget * menu_item = gtk_check_menu_item_new_with_mnemonic(_("_Default"));
                gtk_menu_shell_append(icon_size_submenu, menu_item);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), plugin->icon_size < 1);
                g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_plugin_icon_size_default), plugin);
            }

            gboolean activate_custom = plugin->icon_size > 0;

            #define DEF_SIZE(name, size) \
            {\
                GtkWidget * menu_item = gtk_check_menu_item_new_with_mnemonic(_(name));\
                gtk_menu_shell_append(icon_size_submenu, menu_item);\
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), plugin->icon_size == (size));\
                if (plugin->icon_size == (size))\
                    activate_custom = FALSE;\
                g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_plugin_icon_size_n), plugin);\
            }

            DEF_SIZE("_16 px", 16);
            DEF_SIZE("_24 px", 24);
            DEF_SIZE("_32 px", 32);
            DEF_SIZE("_48 px", 48);
            DEF_SIZE("_56 px", 56);
            DEF_SIZE("_64 px", 64);

            #undef DEF_SIZE

            {
                gchar * name;
                if (activate_custom)
                    name = g_strdup_printf(_("_Custom: %d px"), plugin->icon_size);
                else
                    name = g_strdup(_("_Custom..."));
                GtkWidget * menu_item = gtk_check_menu_item_new_with_mnemonic(name);
                gtk_menu_shell_append(icon_size_submenu, menu_item);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), activate_custom);
                g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_plugin_icon_size_custom), plugin);
                g_free(name);
            }
        }

        gtk_menu_shell_append(applet_submenu, gtk_separator_menu_item_new());

        {
            GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("D_elete Applet"));
            gtk_menu_shell_append(applet_submenu, menu_item);
            g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_remove_item), plugin);

            gchar * tooltip = g_strdup_printf(_("Delete \"%s\""), _(plugin_class(plugin)->name));
            gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tooltip);
            g_free(tooltip);
        }
    }

    /*****************************************************/

    GtkMenuShell * menu = GTK_MENU_SHELL(gtk_menu_new());

    {
        GtkWidget * menu_item = gtk_menu_item_new_with_mnemonic(_("Pa_nel"));
        gtk_menu_shell_append(menu, menu_item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(panel_submenu));
    }

    if (applet_submenu)
    {
        GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("Apple_t"));
        gtk_menu_shell_append(menu, menu_item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(applet_submenu));

        gchar * tooltip = g_strdup_printf(_("Applet \"%s\""), _(plugin_class(plugin)->name));
        gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tooltip);
        g_free(tooltip);
    }

    if (plugin && !wtl_is_in_kiosk_mode())
    {
        GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Applet Properties"));

        gchar * tooltip = g_strdup_printf(_("Properties of the applet \"%s\""), _(plugin_class(plugin)->name));
        gtk_widget_set_tooltip_text(GTK_WIDGET(menu_item), tooltip);
        g_free(tooltip);

        gtk_menu_shell_append(menu, menu_item);
        if (plugin && plugin_class(plugin)->show_properties && !wtl_is_in_kiosk_mode())
            g_signal_connect(menu_item, "activate", G_CALLBACK(panel_popupmenu_config_plugin), plugin);
        else
            gtk_widget_set_sensitive(menu_item, FALSE);
    }

    gtk_widget_show_all(GTK_WIDGET(menu));

    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

    if (plugin && plugin_class(plugin)->popup_menu_hook)
        plugin_class(plugin)->popup_menu_hook(plugin, GTK_MENU(menu));

    return GTK_MENU(menu);
}

void panel_show_panel_menu(Panel * panel, Plugin * plugin, GdkEventButton * event)
{
    GtkMenu* popup = panel_get_panel_menu(panel, plugin);
    if (popup)
        gtk_menu_popup(popup, NULL, NULL, NULL, NULL, event->button, event->time);
}
