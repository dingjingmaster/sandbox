/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 2004, 2011 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *           Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "config.h"

#include "nemo-navigation-action.h"

#include "nemo-window.h"

#include <gtk/gtk.h>
#include "eel/eel-gtk-extensions.h"

G_DEFINE_TYPE (NemoNavigationAction, nemo_navigation_action, GTK_TYPE_ACTION);
#define NEMO_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NEMO_TYPE_NAVIGATION_ACTION, NemoNavigationActionPrivate))

struct NemoNavigationActionPrivate
{
	NemoWindow *window;
	NemoNavigationDirection direction;
	char *arrow_tooltip;

        guint popup_timeout_id;
};

enum
{
	PROP_0,
	PROP_ARROW_TOOLTIP,
	PROP_DIRECTION,
	PROP_WINDOW
};

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NemoWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

	nemo_window_back_or_forward (window, back, index, nemo_event_get_window_open_flags ());
}

static void
activate_back_menu_item_callback (GtkMenuItem *menu_item,
                                  NemoWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem *menu_item, NemoWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static void
fill_menu (NemoWindow *window,
	   GtkWidget *menu,
	   gboolean back)
{
	NemoWindowSlot *slot;
	GtkWidget *menu_item;
	int index;
	GList *list;

	slot = nemo_window_get_active_slot (window);
	
	list = back ? slot->back_list : slot->forward_list;
	index = 0;
	while (list != NULL) {
		menu_item = nemo_bookmark_menu_item_new (NEMO_BOOKMARK (list->data));
		g_object_set_data (G_OBJECT (menu_item), "user_data", GINT_TO_POINTER (index));
		gtk_widget_show (GTK_WIDGET (menu_item));
  		g_signal_connect_object (menu_item, "activate",
					 back
					 ? G_CALLBACK (activate_back_menu_item_callback)
					 : G_CALLBACK (activate_forward_menu_item_callback),
					 window, 0);
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		list = g_list_next (list);
		++index;
	}
}

static void
show_menu (NemoNavigationAction *self,
           guint button,
           guint32 event_time)
{
	NemoWindow *window;
	GtkWidget *menu;

	window = self->priv->window;
	
	menu = gtk_menu_new ();

	switch (self->priv->direction) {
	case NEMO_NAVIGATION_DIRECTION_FORWARD:
		fill_menu (window, menu, FALSE);
		break;
	case NEMO_NAVIGATION_DIRECTION_BACK:
		fill_menu (window, menu, TRUE);
		break;
 	case NEMO_NAVIGATION_DIRECTION_UP:
 		return;
 	case NEMO_NAVIGATION_DIRECTION_RELOAD:
 		return;
 	case NEMO_NAVIGATION_DIRECTION_HOME:
 		return;
 	case NEMO_NAVIGATION_DIRECTION_COMPUTER:
 		return;
 	case NEMO_NAVIGATION_DIRECTION_EDIT:
 		return;
	default:
		g_assert_not_reached ();
		break;
	}

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        button, event_time);
}

#define MENU_POPUP_TIMEOUT 1200

static gboolean
popup_menu_timeout_cb (gpointer data)
{
        NemoNavigationAction *self = data;

        show_menu (self, 1, gtk_get_current_event_time ());

        return FALSE;
}

static void
unschedule_menu_popup_timeout (NemoNavigationAction *self)
{
        if (self->priv->popup_timeout_id != 0) {
                g_source_remove (self->priv->popup_timeout_id);
                self->priv->popup_timeout_id = 0;
        }
}

static void
schedule_menu_popup_timeout (NemoNavigationAction *self)
{
        /* unschedule any previous timeouts */
        unschedule_menu_popup_timeout (self);

        self->priv->popup_timeout_id =
                g_timeout_add (MENU_POPUP_TIMEOUT,
                               popup_menu_timeout_cb,
                               self);
}

/* Performs the given action using a new tab, if the selected action
   supports this. In this case TRUE is returned. */
static gboolean
new_tab_action (NemoNavigationAction *self)
{
	NemoWindow *window;

	window = self->priv->window;

	switch (self->priv->direction) {
	case NEMO_NAVIGATION_DIRECTION_FORWARD:
		nemo_window_back_or_forward (window, FALSE, 0,
					     NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
		break;
	case NEMO_NAVIGATION_DIRECTION_BACK:
		nemo_window_back_or_forward (window, TRUE, 0,
					     NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
		break;
	case NEMO_NAVIGATION_DIRECTION_UP:
		nemo_window_slot_go_up (nemo_window_get_active_slot (window),
					NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
		break;
	/* remaining actions are not supported */
        case NEMO_NAVIGATION_DIRECTION_RELOAD:
        case NEMO_NAVIGATION_DIRECTION_HOME:
        case NEMO_NAVIGATION_DIRECTION_COMPUTER:
        case NEMO_NAVIGATION_DIRECTION_EDIT:
                return FALSE;
                break;
	default:
		return FALSE;
	}
	

	return TRUE;
}

/* Checks if, given the event, the action should be performed in a new tab. */
static gboolean
is_new_tab_event(GdkEventButton *event) {
	/* When double clicking with the middle bouse button, tool_button_press_cb
	   is called three instead of two times. Handle the current event
	   as new_tab_action only if it's an actual button press to prevent
	   calling new_tab_action the third time. */
	return (event->button == 2
		|| (event->button == 1 && event->state & GDK_CONTROL_MASK))
	       && event->type == GDK_BUTTON_PRESS;
}

static gboolean
tool_button_press_cb (GtkButton *button,
                      GdkEventButton *event,
                      gpointer user_data)
{
        NemoNavigationAction *self = user_data;

        if (event->button == 3) {
                /* right click */
                show_menu (self, event->button, event->time);
                return TRUE;
        }

	if (is_new_tab_event (event) && new_tab_action (self)) {
		/* it was a valid new_tab_action -> event is handled */
		return TRUE;
	}

        if (event->button == 1) {
                schedule_menu_popup_timeout (self);
        }

	return FALSE;
}

static gboolean
tool_button_release_cb (GtkButton *button,
                        GdkEventButton *event,
                        gpointer user_data)
{
        NemoNavigationAction *self = user_data;

        unschedule_menu_popup_timeout (self);
        
        return FALSE;
}

static void
connect_proxy (GtkAction *action,
               GtkWidget *proxy)
{
    if (GTK_IS_BUTTON (proxy)) {
        g_signal_connect (proxy, "button-press-event",
                          G_CALLBACK (tool_button_press_cb), action);
        g_signal_connect (proxy, "button-release-event",
                          G_CALLBACK (tool_button_release_cb), action);
    }

	(* GTK_ACTION_CLASS (nemo_navigation_action_parent_class)->connect_proxy) (action, proxy);
}

static void
disconnect_proxy (GtkAction *action,
                  GtkWidget *proxy)
{
	if (GTK_IS_BUTTON (proxy)) {
        /* remove any possible timeout going on */
        unschedule_menu_popup_timeout (NEMO_NAVIGATION_ACTION (action));

		g_signal_handlers_disconnect_by_func (proxy,
                                              G_CALLBACK (tool_button_press_cb), action);
		g_signal_handlers_disconnect_by_func (proxy,
                                              G_CALLBACK (tool_button_release_cb), action);
	}

	(* GTK_ACTION_CLASS (nemo_navigation_action_parent_class)->disconnect_proxy) (action, proxy);
}

static void
nemo_navigation_action_finalize (GObject *object)
{
	NemoNavigationAction *action = NEMO_NAVIGATION_ACTION (object);

        /* remove any possible timeout going on */
        unschedule_menu_popup_timeout (action);

	g_free (action->priv->arrow_tooltip);

	(* G_OBJECT_CLASS (nemo_navigation_action_parent_class)->finalize) (object);
}

static void
nemo_navigation_action_set_property (GObject *object,
					 guint prop_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
	NemoNavigationAction *nav;

	nav = NEMO_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_free (nav->priv->arrow_tooltip);
			nav->priv->arrow_tooltip = g_value_dup_string (value);
			break;
		case PROP_DIRECTION:
			nav->priv->direction = g_value_get_int (value);
			break;
		case PROP_WINDOW:
			nav->priv->window = g_value_get_object (value);
			break;
                default:
                        break;
	}
}

static void
nemo_navigation_action_get_property (GObject *object,
					 guint prop_id,
					 GValue *value,
					 GParamSpec *pspec)
{
	NemoNavigationAction *nav;

	nav = NEMO_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_value_set_string (value, nav->priv->arrow_tooltip);
			break;
		case PROP_DIRECTION:
			g_value_set_int (value, nav->priv->direction);
			break;
		case PROP_WINDOW:
			g_value_set_object (value, nav->priv->window);
			break;
                default:
                        break;
	}
}

static void
nemo_navigation_action_class_init (NemoNavigationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = nemo_navigation_action_finalize;
	object_class->set_property = nemo_navigation_action_set_property;
	object_class->get_property = nemo_navigation_action_get_property;

	action_class->toolbar_item_type = GTK_TYPE_TOOL_BUTTON;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;

	g_object_class_install_property (object_class,
                                         PROP_ARROW_TOOLTIP,
                                         g_param_spec_string ("arrow-tooltip",
                                                              "Arrow Tooltip",
                                                              "Arrow Tooltip",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_DIRECTION,
                                         g_param_spec_int ("direction",
                                                           "Direction",
                                                           "Direction",
                                                           0,
							   G_MAXINT,
							   0,
                                                           G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "The navigation window",
                                                              NEMO_TYPE_WINDOW,
                                                              G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(NemoNavigationActionPrivate));
}

static void
nemo_navigation_action_init (NemoNavigationAction *action)
{
        action->priv = NEMO_NAVIGATION_ACTION_GET_PRIVATE (action);
}
