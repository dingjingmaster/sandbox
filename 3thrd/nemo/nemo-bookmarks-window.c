/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nemo-bookmarks-window.c - implementation of bookmark-editing window.
 */

#include "config.h"
#include "nemo-bookmarks-window.h"
#include "nemo-window.h"

#include "libnemo-private/nemo-undo.h"
#include "libnemo-private/nemo-global-preferences.h"
#include "libnemo-private/nemo-undo-signal-handlers.h"

#include "eel/eel-gtk-extensions.h"
#include "eel/eel-gnome-extensions.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

/* Static variables to keep track of window state. If there were
 * more than one bookmark-editing window, these would be struct or
 * class fields. 
 */
static int		     bookmark_list_changed_signal_id;
static NemoBookmarkList *bookmarks = NULL;
static GtkTreeView	    *bookmark_list_widget = NULL; /* awkward name to distinguish from NemoBookmarkList */
static GtkListStore	    *bookmark_list_store = NULL;
static GtkListStore	    *bookmark_empty_list_store = NULL;
static GtkTreeSelection     *bookmark_selection = NULL;
static int                   selection_changed_id = 0;
static GtkWidget	    *name_field = NULL;
static int		     name_field_changed_signal_id;
static GtkWidget	    *remove_button = NULL;
static GtkWidget            *jump_button = NULL;
static GtkWidget            *sort_button = NULL;
static GtkWidget            *close_button = NULL;
static gboolean		     text_changed = FALSE;
static gboolean		     name_text_changed = FALSE;
static GtkWidget	    *uri_field = NULL;
static int		     uri_field_changed_signal_id;
static int		     row_deleted_signal_id;
static int                   row_activated_signal_id;
static int                   button_pressed_signal_id;
static int                   key_pressed_signal_id;
static int                   jump_button_signal_id;
static int                   sort_button_signal_id;

/* forward declarations */
static guint    get_selected_row                            (void);
static gboolean get_selection_exists                        (void);
static void     name_or_uri_field_activate                  (NemoEntry        *entry);
static void     nemo_bookmarks_window_restore_geometry  (GtkWidget            *window);
static void     on_bookmark_list_changed                    (NemoBookmarkList *list,
							     gpointer              user_data);
static void     on_name_field_changed                       (GtkEditable          *editable,
							     gpointer              user_data);
static void     on_remove_button_clicked                    (GtkButton            *button,
							     gpointer              user_data);
static void     on_jump_button_clicked                      (GtkButton            *button,
							     gpointer              user_data);
static void     on_sort_button_clicked                      (GtkButton            *button,
							     gpointer              user_data);
static void	on_row_deleted				    (GtkListStore	  *store,
							     GtkTreePath	  *path,
							     gpointer		   user_data);
static void	on_row_activated			    (GtkTreeView	  *view,
							     GtkTreePath	  *path,
                                                             GtkTreeViewColumn    *column,
							     gpointer		   user_data);
static gboolean	on_button_pressed                           (GtkTreeView	  *view,
                                                             GdkEventButton       *event,
							     gpointer		   user_data);
static gboolean	on_key_pressed                              (GtkTreeView	  *view,
                                                             GdkEventKey          *event,
							     gpointer		   user_data);
static void     on_selection_changed                        (GtkTreeSelection     *treeselection,
							     gpointer              user_data);

static gboolean on_text_field_focus_out_event               (GtkWidget            *widget,
							     GdkEventFocus        *event,
							     gpointer              user_data);
static void     on_uri_field_changed                        (GtkEditable          *editable,
							     gpointer              user_data);
static gboolean on_window_delete_event                      (GtkWidget            *widget,
							     GdkEvent             *event,
							     gpointer              user_data);
static void     on_window_hide_event                        (GtkWidget            *widget,
							     gpointer              user_data);
static void     on_window_destroy_event                     (GtkWidget            *widget,
							     gpointer              user_data);
static void     repopulate_now                                  (void);
static void     queue_repopulate                                  (void);
static void     set_up_close_accelerator                    (GtkWidget            *window);
static void	open_selected_bookmark 			    (gpointer   user_data, GdkScreen *screen);
static void	update_bookmark_from_text		    (void);

/* We store a pointer to the bookmark in a column so when an item is moved
   with DnD we know which item it is. However we have to be careful to keep
   this in sync with the actual bookmark. Note that
   nemo_bookmark_list_insert_item() makes a copy of the bookmark, so we
   have to fetch the new copy and update our pointer. */
#define BOOKMARK_LIST_COLUMN_ICON		0
#define BOOKMARK_LIST_COLUMN_NAME		1
#define BOOKMARK_LIST_COLUMN_BOOKMARK		2
#define BOOKMARK_LIST_COLUMN_STYLE		3
#define BOOKMARK_LIST_COLUMN_IS_SEPARATOR 4
#define BOOKMARK_LIST_COLUMN_COUNT		5

/* layout constants */

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define BOOKMARKS_WINDOW_MIN_WIDTH	300
#define BOOKMARKS_WINDOW_MIN_HEIGHT	100

/* Larger size initially; user can stretch or shrink (but not shrink below min) */
#define BOOKMARKS_WINDOW_INITIAL_WIDTH	500
#define BOOKMARKS_WINDOW_INITIAL_HEIGHT	300

static void
nemo_bookmarks_window_response_callback (GtkDialog *dialog,
					     int response_id,
					     gpointer callback_data)
{
    if (response_id == GTK_RESPONSE_CLOSE) {
		gtk_widget_hide (GTK_WIDGET (dialog));
    }
}

static GtkListStore *
create_bookmark_store (void)
{
	return gtk_list_store_new (BOOKMARK_LIST_COLUMN_COUNT,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_OBJECT,
				   PANGO_TYPE_STYLE,
                   G_TYPE_BOOLEAN);
}

static void
setup_empty_list (void)
{
	GtkTreeIter iter;

	bookmark_empty_list_store = create_bookmark_store ();
	gtk_list_store_append (bookmark_empty_list_store, &iter);

	gtk_list_store_set (bookmark_empty_list_store, &iter,
			    BOOKMARK_LIST_COLUMN_NAME, _("No bookmarks defined"),
			    BOOKMARK_LIST_COLUMN_STYLE, PANGO_STYLE_ITALIC,
			    -1);
}

static void
bookmarks_set_empty (gboolean empty)
{
	GtkTreeIter iter;

	if (empty) {
		gtk_tree_view_set_model (bookmark_list_widget,
					 GTK_TREE_MODEL (bookmark_empty_list_store));
		gtk_widget_set_sensitive (GTK_WIDGET (bookmark_list_widget), FALSE);
	} else {
		gtk_tree_view_set_model (bookmark_list_widget,
					 GTK_TREE_MODEL (bookmark_list_store));
		gtk_widget_set_sensitive (GTK_WIDGET (bookmark_list_widget), TRUE);

		if (nemo_bookmark_list_length (bookmarks) > 0 &&
		    !get_selection_exists ()) {
			gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (bookmark_list_store),
						       &iter, NULL, 0);
			gtk_tree_selection_select_iter (bookmark_selection, &iter);
		}
	}

	on_selection_changed (bookmark_selection, NULL);
}

static void
edit_bookmarks_dialog_reset_signals (gpointer data,
				     GObject *obj)
{
	g_signal_handler_disconnect (jump_button,
				     jump_button_signal_id);
	g_signal_handler_disconnect (sort_button,
				     sort_button_signal_id);
	g_signal_handler_disconnect (bookmark_list_widget,
				     row_activated_signal_id);
	jump_button_signal_id =
		g_signal_connect (jump_button, "clicked",
				  G_CALLBACK (on_jump_button_clicked), NULL);
	sort_button_signal_id =
		g_signal_connect (sort_button, "clicked",
				  G_CALLBACK (on_sort_button_clicked), NULL);
	row_activated_signal_id =
		g_signal_connect (bookmark_list_widget, "row_activated",
				  G_CALLBACK (on_row_activated), NULL);
}

/**
 * create_bookmarks_window:
 * 
 * Create a new bookmark-editing window. 
 * @list: The NemoBookmarkList that this window will edit.
 *
 * Return value: A pointer to the new window.
 **/
GtkWindow *
create_bookmarks_window (NemoBookmarkList *list, GObject *undo_manager_source)
{
	GtkWidget         *window;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *rend;
	GtkBuilder        *builder;

	bookmarks = list;

	builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	if (!gtk_builder_add_from_resource (builder,
					    "/org/nemo/nemo-bookmarks-window.glade",
					    NULL)) {
		return NULL;
	}

	window = (GtkWidget *)gtk_builder_get_object (builder, "bookmarks_dialog");
	bookmark_list_widget = (GtkTreeView *)gtk_builder_get_object (builder, "bookmark_tree_view");
	remove_button = (GtkWidget *)gtk_builder_get_object (builder, "bookmark_delete_button");
	jump_button = (GtkWidget *)gtk_builder_get_object (builder, "bookmark_jump_button");
    sort_button = (GtkWidget *)gtk_builder_get_object (builder, "bookmark_sort_button");
	close_button = (GtkWidget *)gtk_builder_get_object (builder, "bookmark_close_button");

    gtk_button_set_image (GTK_BUTTON (remove_button), gtk_image_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_image (GTK_BUTTON (jump_button), gtk_image_new_from_icon_name ("go-jump-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_image (GTK_BUTTON (sort_button), gtk_image_new_from_icon_name ("view-sort-ascending-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_image (GTK_BUTTON (close_button), gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON));

	set_up_close_accelerator (window);
	nemo_undo_share_undo_manager (G_OBJECT (window), undo_manager_source);

	gtk_window_set_wmclass (GTK_WINDOW (window), "bookmarks", "Nemo");
	nemo_bookmarks_window_restore_geometry (window);

	g_object_weak_ref (G_OBJECT (undo_manager_source), edit_bookmarks_dialog_reset_signals, 
			   undo_manager_source);
	
	bookmark_list_widget = GTK_TREE_VIEW (gtk_builder_get_object (builder, "bookmark_tree_view"));

	rend = gtk_cell_renderer_pixbuf_new ();
	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"icon-name", 
							BOOKMARK_LIST_COLUMN_ICON,
							NULL);
	gtk_tree_view_append_column (bookmark_list_widget,
				     GTK_TREE_VIEW_COLUMN (col));
	gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (col),
					      NEMO_ICON_SIZE_SMALLER);

	rend = gtk_cell_renderer_text_new ();
	g_object_set (rend,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);

	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"text", 
							BOOKMARK_LIST_COLUMN_NAME,
							"style",
							BOOKMARK_LIST_COLUMN_STYLE,
							NULL);
	gtk_tree_view_append_column (bookmark_list_widget,
				     GTK_TREE_VIEW_COLUMN (col));
	
	bookmark_list_store = create_bookmark_store ();
	setup_empty_list ();
	gtk_tree_view_set_model (bookmark_list_widget,
				 GTK_TREE_MODEL (bookmark_empty_list_store));
	
	bookmark_selection =
		GTK_TREE_SELECTION (gtk_tree_view_get_selection (bookmark_list_widget));

	name_field = nemo_entry_new ();
	
	gtk_widget_show (name_field);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "bookmark_name_placeholder")),
			    name_field, TRUE, TRUE, 0);
	
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (gtk_builder_get_object (builder, "bookmark_name_label")),
		name_field);

	uri_field = nemo_entry_new ();
	gtk_widget_show (uri_field);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "bookmark_location_placeholder")),
			    uri_field, TRUE, TRUE, 0);

	gtk_label_set_mnemonic_widget (
		GTK_LABEL (gtk_builder_get_object (builder, "bookmark_location_label")),
		uri_field);

	bookmark_list_changed_signal_id =
		g_signal_connect (bookmarks, "changed",
				  G_CALLBACK (on_bookmark_list_changed), NULL);
	row_deleted_signal_id =
		g_signal_connect (bookmark_list_store, "row_deleted",
				  G_CALLBACK (on_row_deleted), NULL);
        row_activated_signal_id =
                g_signal_connect (bookmark_list_widget, "row_activated",
                                  G_CALLBACK (on_row_activated), undo_manager_source);
        button_pressed_signal_id =
                g_signal_connect (bookmark_list_widget, "button_press_event",
                                  G_CALLBACK (on_button_pressed), NULL);
        key_pressed_signal_id =
                g_signal_connect (bookmark_list_widget, "key_press_event",
                                  G_CALLBACK (on_key_pressed), NULL);
	selection_changed_id =
		g_signal_connect (bookmark_selection, "changed",
				  G_CALLBACK (on_selection_changed), NULL);	

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (on_window_delete_event), NULL);
	g_signal_connect (window, "hide",
			  G_CALLBACK (on_window_hide_event), NULL);                    	    
	g_signal_connect (window, "destroy",
			  G_CALLBACK (on_window_destroy_event), NULL);
	g_signal_connect (window, "response",
			  G_CALLBACK (nemo_bookmarks_window_response_callback), NULL);

	name_field_changed_signal_id =
		g_signal_connect (name_field, "changed",
				  G_CALLBACK (on_name_field_changed), NULL);
                      		    
	g_signal_connect (name_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), NULL);                            
	g_signal_connect (name_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), NULL);

	uri_field_changed_signal_id = 
		g_signal_connect (uri_field, "changed",
				  G_CALLBACK (on_uri_field_changed), NULL);
                      		    
	g_signal_connect (uri_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), NULL);
	g_signal_connect (uri_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), NULL);
	g_signal_connect (remove_button, "clicked",
			  G_CALLBACK (on_remove_button_clicked), NULL);
	jump_button_signal_id = 
		g_signal_connect (jump_button, "clicked",
				  G_CALLBACK (on_jump_button_clicked), undo_manager_source);
	sort_button_signal_id =
		g_signal_connect (sort_button, "clicked",
				  G_CALLBACK (on_sort_button_clicked), NULL);

	gtk_tree_selection_set_mode (bookmark_selection, GTK_SELECTION_BROWSE);
	
	/* Fill in list widget with bookmarks, must be after signals are wired up. */
	repopulate_now();

	g_object_unref (builder);
	
	return GTK_WINDOW (window);
}

void
edit_bookmarks_dialog_set_signals (GObject *undo_manager_source)
{

	g_signal_handler_disconnect (jump_button,
				     jump_button_signal_id);
	g_signal_handler_disconnect (sort_button,
				     sort_button_signal_id);
	g_signal_handler_disconnect (bookmark_list_widget,
				     row_activated_signal_id);

	jump_button_signal_id =
		g_signal_connect (jump_button, "clicked",
				  G_CALLBACK (on_jump_button_clicked), undo_manager_source);
	sort_button_signal_id =
		g_signal_connect (sort_button, "clicked",
				  G_CALLBACK (on_sort_button_clicked), NULL);
	row_activated_signal_id =
		g_signal_connect (bookmark_list_widget, "row_activated",
				  G_CALLBACK (on_row_activated), undo_manager_source);

	g_object_weak_ref (G_OBJECT (undo_manager_source), edit_bookmarks_dialog_reset_signals,
			   undo_manager_source);
}

static NemoBookmark *
get_selected_bookmark (void)
{
    NemoBookmark *bookmark;
    GtkTreeModel *model;
    GtkTreeIter iter;

	g_return_val_if_fail(NEMO_IS_BOOKMARK_LIST(bookmarks), NULL);

    model = GTK_TREE_MODEL (bookmark_list_store);

    if (!gtk_tree_selection_get_selected (bookmark_selection,
                                          &model,
                                          &iter)) {
        return NULL;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (bookmark_list_store), &iter,
                        BOOKMARK_LIST_COLUMN_BOOKMARK, &bookmark,
                        -1);

    return bookmark;
}

static guint
get_selected_row (void)
{
	GtkTreeIter       iter;
	GtkTreePath      *path;
	GtkTreeModel     *model;
	gint		 *indices, row;
	
	g_assert (get_selection_exists());
	
	model = GTK_TREE_MODEL (bookmark_list_store);

    if (!gtk_tree_selection_get_selected (bookmark_selection,
                                          &model,
                                          &iter)) {
        return -1;
    }

	path = gtk_tree_model_get_path (model, &iter);
	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_path_free (path);
	return row;
}

static gboolean
get_selection_exists (void)
{
	return gtk_tree_selection_get_selected (bookmark_selection, NULL, NULL);
}

static void
nemo_bookmarks_window_restore_geometry (GtkWidget *window)
{
	const char *window_geometry;
	
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));

	window_geometry = nemo_bookmark_list_get_window_geometry (bookmarks);

	if (window_geometry != NULL) {	
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), window_geometry, 
			 BOOKMARKS_WINDOW_MIN_WIDTH, BOOKMARKS_WINDOW_MIN_HEIGHT, FALSE);

	} else {
		/* use default since there was no stored geometry */
		gtk_window_set_default_size (GTK_WINDOW (window), 
					     BOOKMARKS_WINDOW_INITIAL_WIDTH, 
					     BOOKMARKS_WINDOW_INITIAL_HEIGHT);

		/* Let window manager handle default position if no position stored */
	}
}

/**
 * nemo_bookmarks_window_save_geometry:
 * 
 * Save window size & position to disk.
 * @window: The bookmarks window whose geometry should be saved.
 **/
void
nemo_bookmarks_window_save_geometry (GtkWindow *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));

	/* Don't bother if window is already closed */
	if (gtk_widget_get_visible (GTK_WIDGET (window))) {
		char *geometry_string;
		
		geometry_string = eel_gtk_window_get_geometry_string (window);

		nemo_bookmark_list_set_window_geometry (bookmarks, geometry_string);
		g_free (geometry_string);
	}
}

static void
on_bookmark_list_changed (NemoBookmarkList *bmarks, gpointer data)
{
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bmarks));

	/* maybe add logic here or in repopulate to save/restore selection */
	queue_repopulate ();
}

static void
on_name_field_changed (GtkEditable *editable,
		       gpointer     user_data)
{
	GtkTreeIter   iter;
	g_return_if_fail(GTK_IS_TREE_VIEW(bookmark_list_widget));
	g_return_if_fail(GTK_IS_ENTRY(name_field));

	if (!get_selection_exists())
		return;

	/* Update text displayed in list instantly. Also remember that 
	 * user has changed text so we update real bookmark later. 
	 */
	gtk_tree_selection_get_selected (bookmark_selection,
					 NULL,
					 &iter);
	
	gtk_list_store_set (bookmark_list_store, 
			    &iter, BOOKMARK_LIST_COLUMN_NAME, 
			    gtk_entry_get_text (GTK_ENTRY (name_field)),
			    -1);
	text_changed = TRUE;
	name_text_changed = TRUE;
}

static void
open_selected_bookmark (gpointer user_data, GdkScreen *screen)
{
	NemoBookmark *selected;
	NemoWindow *window;
	GFile *location;
	
	selected = get_selected_bookmark ();

	if (!selected) {
		return;
	}

	location = nemo_bookmark_get_location (selected);
	if (location == NULL) { 
		return;
	}

	window = user_data;
	nemo_window_go_to (window, location);

	g_object_unref (location);
}

static void
on_jump_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen (GTK_WIDGET (button));
	open_selected_bookmark (user_data, screen);
}

static void
on_sort_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
    g_assert (NEMO_IS_BOOKMARK_LIST (bookmarks));

    nemo_bookmark_list_sort_ascending (bookmarks);
}

static void
bookmarks_delete_bookmark (void)
{
    NemoBookmark *bookmark;
	GtkTreeIter iter;
	gint i;

	g_assert (GTK_IS_TREE_VIEW (bookmark_list_widget));
	
	if (!gtk_tree_selection_get_selected (bookmark_selection, NULL, &iter))
		return;

    gtk_tree_model_get (GTK_TREE_MODEL (bookmark_list_store), &iter,
                        BOOKMARK_LIST_COLUMN_BOOKMARK, &bookmark,
                        -1);

    for (i = 0; i < nemo_bookmark_list_length (bookmarks); i++) {
        NemoBookmark *bookmark_in_list = nemo_bookmark_list_item_at (bookmarks, i);

        if (bookmark_in_list == bookmark) {
            nemo_bookmark_list_delete_item_at (bookmarks, i);
            break;
        }
    }

    queue_repopulate ();
}

static void
on_remove_button_clicked (GtkButton *button,
                          gpointer   user_data)
{
        bookmarks_delete_bookmark ();
}

static void
on_row_deleted (GtkListStore *store,
        GtkTreePath *path,
        gpointer user_data)
{
	NemoBookmark *bookmark = NULL;

	store = bookmark_list_store;
    GtkTreeIter row_iter;
    gint row = 0, new_index = 0;

    g_signal_handler_block (bookmarks,
                            bookmark_list_changed_signal_id);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &row_iter)) {
        do {
            gtk_tree_model_get (GTK_TREE_MODEL (store), &row_iter,
                                BOOKMARK_LIST_COLUMN_BOOKMARK, &bookmark,
                                -1);
            if (bookmark != NULL) {
                gint i;

                for (i = 0; i < nemo_bookmark_list_length (bookmarks); i++) {
                    NemoBookmark *old_bm = nemo_bookmark_list_item_at (bookmarks, i);

                    if (old_bm == bookmark) {
                        nemo_bookmark_list_move_item (bookmarks, i, new_index++);
                    }
                }
            } else {
                g_settings_set_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT, row);
            }

            row++;
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &row_iter));
    }

    g_signal_handler_unblock (bookmarks,
                              bookmark_list_changed_signal_id);

    queue_repopulate ();
}

/* The update_bookmark_from_text() calls in the
 * on_button_pressed() and on_key_pressed() handlers
 * of the tree view are a hack.
 *
 * The purpose is to track selection changes to the view
 * and write the text fields back before the selection
 * actually changed.
 *
 * Note that the focus-out event of the text entries is emitted
 * after the selection changed, else this would not not be neccessary.
 */

static gboolean
on_button_pressed (GtkTreeView *view,
		   GdkEventButton *event,
		   gpointer user_data)
{
	update_bookmark_from_text ();

	return FALSE;
}

static gboolean
on_key_pressed (GtkTreeView *view,
                GdkEventKey *event,
                gpointer user_data)
{
        if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
                bookmarks_delete_bookmark ();
                return TRUE;
        }

	update_bookmark_from_text ();

        return FALSE;
}

static void
on_row_activated (GtkTreeView       *view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen (GTK_WIDGET (view));
	open_selected_bookmark (user_data, screen);
}

static void
on_selection_changed (GtkTreeSelection *treeselection,
		      gpointer user_data)
{
	NemoBookmark *selected;
	const char *name = NULL;
	char *entry_text = NULL;
	GFile *location;

	g_assert (GTK_IS_ENTRY (name_field));
	g_assert (GTK_IS_ENTRY (uri_field));

	selected = get_selected_bookmark ();

	if (selected) {
		name = nemo_bookmark_get_name (selected);
		location = nemo_bookmark_get_location (selected);
		entry_text = g_file_get_parse_name (location);

		g_object_unref (location);
	}
	
	/* Set the sensitivity of widgets that require a selection */
	gtk_widget_set_sensitive (remove_button, selected != NULL);
    gtk_widget_set_sensitive (jump_button, selected != NULL);
    gtk_widget_set_sensitive (sort_button, selected != NULL);
	gtk_widget_set_sensitive (name_field, selected != NULL);
	gtk_widget_set_sensitive (uri_field, selected != NULL);

	g_signal_handler_block (name_field, name_field_changed_signal_id);
	nemo_entry_set_text (NEMO_ENTRY (name_field),
				 name ? name : "");
	g_signal_handler_unblock (name_field, name_field_changed_signal_id);

	g_signal_handler_block (uri_field, uri_field_changed_signal_id);
	nemo_entry_set_text (NEMO_ENTRY (uri_field),
				 entry_text ? entry_text : "");
	g_signal_handler_unblock (uri_field, uri_field_changed_signal_id);

	text_changed = FALSE;
	name_text_changed = FALSE;

	g_free (entry_text);
}


static void
update_bookmark_from_text (void)
{
	if (text_changed) {
		NemoBookmark *bookmark, *bookmark_in_list;
		const char *name;
		gchar *icon_name;
		guint selected_row;
		GtkTreeIter iter;
		GFile *location;

		g_assert (GTK_IS_ENTRY (name_field));
		g_assert (GTK_IS_ENTRY (uri_field));

		if (gtk_entry_get_text_length (GTK_ENTRY (uri_field)) == 0) {
			return;
		}

		location = g_file_parse_name 
			(gtk_entry_get_text (GTK_ENTRY (uri_field)));
		
		bookmark = nemo_bookmark_new (location,
						  name_text_changed ? gtk_entry_get_text (GTK_ENTRY (name_field)) : NULL,
						  NULL, NULL);
		
		g_object_unref (location);

		selected_row = get_selected_row ();

        if (selected_row < 0) {
            return;
        }

        if (selected_row > g_settings_get_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT)) {
            selected_row--;
        }

		/* turn off list updating 'cuz otherwise the list-reordering code runs
		 * after repopulate(), thus reordering the correctly-ordered list.
		 */
		g_signal_handler_block (bookmarks, 
					bookmark_list_changed_signal_id);
		nemo_bookmark_list_delete_item_at (bookmarks, selected_row);
		nemo_bookmark_list_insert_item (bookmarks, bookmark, selected_row);
		g_signal_handler_unblock (bookmarks, 
					  bookmark_list_changed_signal_id);
		g_object_unref (bookmark);

		/* We also have to update the bookmark pointer in the list
		   store. */
		gtk_tree_selection_get_selected (bookmark_selection,
						 NULL, &iter);

		bookmark_in_list = nemo_bookmark_list_item_at (bookmarks,
								   selected_row);

		name = nemo_bookmark_get_name (bookmark_in_list);
		icon_name = nemo_bookmark_get_icon_name (bookmark_in_list);

		gtk_list_store_set (bookmark_list_store, &iter,
				    BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark_in_list,
				    BOOKMARK_LIST_COLUMN_NAME, name,
				    BOOKMARK_LIST_COLUMN_ICON, icon_name,
				    -1);

		g_free (icon_name);
	}
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	g_assert (NEMO_IS_ENTRY (widget));

	update_bookmark_from_text ();
	return FALSE;
}

static void
name_or_uri_field_activate (NemoEntry *entry)
{
	g_assert (NEMO_IS_ENTRY (entry));

	update_bookmark_from_text ();
	nemo_entry_select_all_at_idle (entry);
}

static void
on_uri_field_changed (GtkEditable *editable,
		      gpointer user_data)
{
	/* Remember that user has changed text so we 
	 * update real bookmark later. 
	 */
	text_changed = TRUE;
}

static gboolean
on_window_delete_event (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_widget_hide (widget);
	return TRUE;
}

static gboolean
restore_geometry (gpointer data)
{
	g_assert (GTK_IS_WINDOW (data));

	nemo_bookmarks_window_restore_geometry (GTK_WIDGET (data));

	/* Don't call this again */
	return FALSE;
}

static void
on_window_hide_event (GtkWidget *widget,
		      gpointer user_data)
{
	nemo_bookmarks_window_save_geometry (GTK_WINDOW (widget));

	/* Disable undo for entry widgets */
	nemo_undo_unregister (G_OBJECT (name_field));
	nemo_undo_unregister (G_OBJECT (uri_field));

	/* restore_geometry only works after window is hidden */
	g_idle_add (restore_geometry, widget);
}

static void
on_window_destroy_event (GtkWidget *widget,
		      	 gpointer user_data)
{
	g_object_unref (bookmark_list_store);
	g_object_unref (bookmark_empty_list_store);
	g_source_remove_by_user_data (widget);
}

static void
add_breakpoint (GtkListStore *store,
                GtkTreeIter *iter)
{
    gtk_list_store_append (store, iter);
    gtk_list_store_set (store, iter,
                        BOOKMARK_LIST_COLUMN_ICON, NULL,
                        BOOKMARK_LIST_COLUMN_NAME, "------------------------",
                        BOOKMARK_LIST_COLUMN_BOOKMARK, NULL,
                        BOOKMARK_LIST_COLUMN_STYLE, PANGO_STYLE_NORMAL,
                        BOOKMARK_LIST_COLUMN_IS_SEPARATOR, TRUE,
                        -1);
}

static void
repopulate_now (void)
{
	NemoBookmark *selected;
	GtkListStore *store;
	GtkTreePath *path;
	GtkTreeRowReference *reference;
	guint index;
    gint breakpoint, bookmarks_length;
    gboolean breakpoint_added;

	g_assert (GTK_IS_TREE_VIEW (bookmark_list_widget));
	g_assert (NEMO_IS_BOOKMARK_LIST (bookmarks));
	
	store = GTK_LIST_STORE (bookmark_list_store);

	selected = get_selected_bookmark ();

	g_signal_handler_block (bookmark_selection,
				selection_changed_id);
	g_signal_handler_block (bookmark_list_store,
				row_deleted_signal_id);
        g_signal_handler_block (bookmark_list_widget,
                                row_activated_signal_id);
        g_signal_handler_block (bookmark_list_widget,
                                key_pressed_signal_id);
        g_signal_handler_block (bookmark_list_widget,
                                button_pressed_signal_id);

	gtk_list_store_clear (store);
	
	g_signal_handler_unblock (bookmark_list_widget,
				  row_activated_signal_id);
        g_signal_handler_unblock (bookmark_list_widget,
                                  key_pressed_signal_id);
        g_signal_handler_unblock (bookmark_list_widget,
                                  button_pressed_signal_id);
	g_signal_handler_unblock (bookmark_list_store,
				  row_deleted_signal_id);
	g_signal_handler_unblock (bookmark_selection,
				  selection_changed_id);

    bookmarks_length = nemo_bookmark_list_length (bookmarks);
    breakpoint = g_settings_get_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT);

    if (breakpoint < 0) {     // Default gsettings value is -1 (which translates to 'not previously set')
        breakpoint = bookmarks_length;
        g_settings_set_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT, breakpoint);
    }

	reference = NULL;
    index = 0;
    breakpoint_added = FALSE;

    while (index < bookmarks_length) {
		NemoBookmark *bookmark;
		const char       *bookmark_name;
		gchar            *bookmark_icon;
		GtkTreeIter       iter;

        if (index == breakpoint && !breakpoint_added) {
            bookmark_icon = NULL;
            bookmark = NULL;

            add_breakpoint (store, &iter);

            breakpoint_added = TRUE;
        } else {
            bookmark = nemo_bookmark_list_item_at (bookmarks, index);
            bookmark_name = nemo_bookmark_get_name (bookmark);
            bookmark_icon = nemo_bookmark_get_icon_name (bookmark);

            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                BOOKMARK_LIST_COLUMN_ICON, bookmark_icon,
                                BOOKMARK_LIST_COLUMN_NAME, bookmark_name,
                                BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark,
                                BOOKMARK_LIST_COLUMN_STYLE, PANGO_STYLE_NORMAL,
                                BOOKMARK_LIST_COLUMN_IS_SEPARATOR, FALSE,
                                -1);
        }

		if (bookmark == selected) {
			/* save old selection */
			GtkTreePath *pth;

			pth = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
			reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), pth);
			gtk_tree_path_free (pth);
		}

		g_free (bookmark_icon);

        if (bookmark) {
            index++;
        }
	}

    if (!breakpoint_added) {
        GtkTreeIter iter;

        add_breakpoint (store, &iter);
    }

	if (reference != NULL) {
		/* restore old selection */

		/* bookmarks_set_empty() will call the selection change handler,
 		 * so we block it here in case of selection change.
 		 */
		g_signal_handler_block (bookmark_selection, selection_changed_id);

		g_assert (index != 0);
		g_assert (gtk_tree_row_reference_valid (reference));

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_selection_select_path (bookmark_selection, path);

        gtk_tree_view_scroll_to_cell (bookmark_list_widget,
                                      path,
                                      NULL,
                                      TRUE, 0.5, 0);

		gtk_tree_row_reference_free (reference);
		gtk_tree_path_free (path);

		g_signal_handler_unblock (bookmark_selection, selection_changed_id);
	}

	bookmarks_set_empty (index == 0);	  
}

static gboolean
idle_repopulate_cb (gpointer data)
{
    repopulate_now ();

    return G_SOURCE_REMOVE;
}

static void
queue_repopulate (void) {
    g_idle_add ((GSourceFunc) idle_repopulate_cb, NULL);
}

static int
handle_close_accelerator (GtkWindow *window, 
			  GdkEventKey *event, 
			  gpointer user_data)
{
	g_assert (GTK_IS_WINDOW (window));
	g_assert (event != NULL);
	g_assert (user_data == NULL);

	if (event->state & GDK_CONTROL_MASK && event->keyval == GDK_KEY_w) {
		gtk_widget_hide (GTK_WIDGET (window));
		return TRUE;
	}

	return FALSE;
}

static void
set_up_close_accelerator (GtkWidget *window)
{
	/* Note that we don't call eel_gtk_window_set_up_close_accelerator
	 * here because we have to handle saving geometry before hiding the
	 * window.
	 */
	g_signal_connect (window, "key_press_event",
			  G_CALLBACK (handle_close_accelerator), NULL);
}
