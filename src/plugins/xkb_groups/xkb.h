/*
//====================================================================
//  xfce4-xkb-plugin - XFCE4 Xkb Layout Indicator panel plugin
// -------------------------------------------------------------------
//  Alexander Iliev <sasoiliev@mamul.org>
//  20-Feb-04
// -------------------------------------------------------------------
//  Parts of this code belong to Michael Glickman <wmalms@yahooo.com>
//  and his program wmxkb.
//  WARNING: DO NOT BOTHER Michael Glickman WITH QUESTIONS ABOUT THIS
//           PROGRAM!!! SEND INSTEAD EMAILS TO <sasoiliev@mamul.org>
//====================================================================
*/

/* Modified by Hong Jen Yee (PCMan) <pcman.tw@gmail.com> on 2008-04-06 for lxpanel */

#ifndef _XKB_PLUGIN_H_
#define _XKB_PLUGIN_H_

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <gtk/gtk.h>
#include <glib.h>

#define PLUGIN_PRIV_TYPE xkb_groups_t

#include <waterline/global.h>
#include <waterline/plugin.h>
#include <waterline/symbol_visibility.h>
#include <waterline/paths.h>
#include <waterline/panel.h>
#include <waterline/ev.h>

typedef struct {

    /* Plugin interface. */
    Plugin * plugin;                /* Back pointer to Plugin */
    GtkWidget * btn;                /* Top level button */
    GtkWidget * label;              /* Label containing country name */
    GtkWidget * image;              /* Image containing country flag */
    gboolean display_as_text;       /* Display layout as image or text */
    gboolean per_window_layout;     /* Enable per application (window) layout) */
    gint default_group;             /* Default group for "locale per process" */
    guint source_id;                /* Source ID for channel listening to XKB events */
    GtkWidget * config_dlg;         /* Configuration dialog */
    GtkWidget * per_app_default_layout_menu; /* Combo box of all available layouts */

    /* Mechanism. */
    int base_event_code;            /* Result of initializing Xkb extension */
    int base_error_code;
    int current_group_xkb_no;       /* Current layout */
    int group_count;                /* Count of groups as returned by Xkb */
    char * group_names[XkbNumKbdGroups];  /* Group names as returned by Xkb */
    char * symbol_names[XkbNumKbdGroups]; /* Symbol names as returned by Xkb */
    GHashTable * group_hash_table;  /* Hash table to correlate application with layout */

} xkb_groups_t;

extern SYMBOL_HIDDEN void xkb_groups_update(xkb_groups_t * xkb);

extern SYMBOL_HIDDEN int xkb_groups_get_current_group_xkb_no(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN int xkb_groups_get_group_count(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN const char * xkb_groups_get_symbol_name_by_res_no(xkb_groups_t * xkb, int group_res_no);
extern SYMBOL_HIDDEN const char * xkb_groups_get_current_group_name(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN const char * xkb_groups_get_current_symbol_name(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN const char * xkb_groups_get_current_symbol_name_lowercase(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN void xkb_groups_mechanism_constructor(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN void xkb_groups_mechanism_destructor(xkb_groups_t * xkb);
extern SYMBOL_HIDDEN int xkb_groups_change_group(xkb_groups_t * xkb, int increment);
extern SYMBOL_HIDDEN void xkb_groups_active_window_changed(xkb_groups_t * xkb, Window window);

#endif

