/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* Nemo - Icon canvas item class for icon container.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "../config.h"
#include <math.h>
#include "nemo-icon-canvas-item.h"

#include <glib/gi18n.h>

#include "nemo-file-utilities.h"
#include "nemo-global-preferences.h"
#include "nemo-icon-private.h"
#include "../eel/eel-art-extensions.h"
#include "../eel/eel-gdk-extensions.h"
#include "../eel/eel-glib-extensions.h"
#include "../eel/eel-graphic-effects.h"
#include "../eel/eel-string.h"
#include "../eel/eel-accessibility.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <atk/atkimage.h>
#include <atk/atkcomponent.h>
#include <atk/atknoopobject.h>
#include <stdio.h>
#include <string.h>

/* gap between bottom of icon and start of text box */
#define LABEL_OFFSET_BESIDES 3
#define LABEL_LINE_SPACING 0


/* special text height handling
 * each item has three text height variables:
 *  + text_height: actual height of the displayed (i.e. on-screen) PangoLayout.
 *  + text_height_for_layout: height used in icon grid layout algorithms.
 *       		      “sane amount” of text.
 *   “sane amount“ as of
 *      + hard-coded to three lines in text-below-icon mode.
 *      + unlimited in text-besides-icon mode (see VOODOO-TODO)
 *
 *  This layout height is used by grid layout algorithms, even
 *  though the actually displayed and/or requested text size may be larger
 *  and overlap adjacent icons, if an icon is selected.
 *
 *  + text_height_for_entire_text: height needed to display the entire PangoLayout,
 *    if it wasn't ellipsized.
 */

/* Private part of the NemoIconCanvasItem structure. */
struct NemoIconCanvasItemDetails {
	/* The image, text, font. */
	double x, y;
	GdkPixbuf *pixbuf;
    cairo_surface_t *rendered_surface;
	char *editable_text;		/* Text that can be modified by a renaming function */
	char *additional_text;		/* Text that cannot be modifed, such as file size, etc. */

	/* Size of the text at current font. */
	int text_dx;
	int text_width;

	/* actual size required for rendering the text to display */
	int text_height;
	/* actual size that would be required for rendering the entire text if it wasn't ellipsized */
	int text_height_for_entire_text;
	/* actual size needed for rendering a “sane amount” of text */
	int text_height_for_layout;

	int editable_text_height;

	/* whether the entire text must always be visible. In that case,
	 * text_height_for_layout will always be equal to text_height.
	 * Used for the last line of a line-wise icon layout. */
	guint entire_text : 1;

    	/* Highlight state. */
   	guint is_highlighted_for_selection : 1;
	guint is_highlighted_as_keyboard_focus: 1;
   	guint is_highlighted_for_drop : 1;
	guint is_highlighted_for_clipboard : 1;
	guint show_stretch_handles : 1;
	guint is_prelit : 1;

	guint rendered_is_highlighted_for_selection : 1;
	guint rendered_is_highlighted_for_drop : 1;
	guint rendered_is_highlighted_for_clipboard : 1;
	guint rendered_is_prelit : 1;
	guint rendered_is_focused : 1;

	guint is_renaming : 1;

	guint bounds_cached : 1;

	guint is_visible : 1;

    guint is_pinned : 1;
    guint fav_unavailable : 1;

	/* Cached PangoLayouts. Only used if the icon is visible */
	PangoLayout *editable_text_layout;
	PangoLayout *additional_text_layout;

	/* Cached rectangle in canvas coordinates */
	EelIRect canvas_rect;
	EelIRect text_rect;

	EelIRect bounds_cache;
	EelIRect bounds_cache_for_layout;
	EelIRect bounds_cache_for_entire_item;

	GdkWindow *cursor_window;

	/* Accessibility bits */
	GailTextUtil *text_util;
};

/* Object argument IDs. */
enum {
	PROP_0,
	PROP_EDITABLE_TEXT,
	PROP_ADDITIONAL_TEXT,
    	PROP_HIGHLIGHTED_FOR_SELECTION,
    	PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
    	PROP_HIGHLIGHTED_FOR_DROP,
	PROP_HIGHLIGHTED_FOR_CLIPBOARD,
    PROP_PINNED,
    PROP_FAV_UNAVAILABLE
};

typedef enum {
	RIGHT_SIDE,
	BOTTOM_SIDE,
	LEFT_SIDE,
	TOP_SIDE
} RectangleSide;

static void nemo_icon_canvas_item_text_interface_init (EelAccessibleTextIface *iface);
static GType nemo_icon_canvas_item_accessible_factory_get_type (void);

G_DEFINE_TYPE_WITH_CODE (NemoIconCanvasItem, nemo_icon_canvas_item, EEL_TYPE_CANVAS_ITEM,
			 G_IMPLEMENT_INTERFACE (EEL_TYPE_ACCESSIBLE_TEXT,
						nemo_icon_canvas_item_text_interface_init));

/* private */
static void     draw_label_text                      (NemoIconCanvasItem        *item,
						      cairo_t                       *cr,
						      EelIRect                       icon_rect);
static void     measure_label_text                   (NemoIconCanvasItem        *item);
static void     draw_pixbuf                          (GdkPixbuf                     *pixbuf,
						      cairo_t                       *cr,
						      int                            x,
						      int                            y);
static PangoLayout *get_label_layout                 (PangoLayout                  **layout,
						      NemoIconCanvasItem        *item,
						      const char                    *text);
static gboolean hit_test_stretch_handle              (NemoIconCanvasItem        *item,
						      EelIRect                       canvas_rect,
						      GtkCornerType *corner);

static void       nemo_icon_canvas_item_ensure_bounds_up_to_date (NemoIconCanvasItem *icon_item);

/* Object initialization function for the icon item. */
static void
nemo_icon_canvas_item_init (NemoIconCanvasItem *icon_item)
{
	icon_item->details = G_TYPE_INSTANCE_GET_PRIVATE ((icon_item), NEMO_TYPE_ICON_CANVAS_ITEM, NemoIconCanvasItemDetails);
	nemo_icon_canvas_item_invalidate_label_size (icon_item);
}

static void
nemo_icon_canvas_item_finalize (GObject *object)
{
	NemoIconCanvasItemDetails *details;

	g_assert (NEMO_IS_ICON_CANVAS_ITEM (object));

	details = NEMO_ICON_CANVAS_ITEM (object)->details;

	if (details->cursor_window != NULL) {
		gdk_window_set_cursor (details->cursor_window, NULL);
		g_object_unref (details->cursor_window);
	}

	if (details->pixbuf != NULL) {
		g_object_unref (details->pixbuf);
	}

	if (details->text_util != NULL) {
		g_object_unref (details->text_util);
	}

	g_free (details->editable_text);
	g_free (details->additional_text);

    if (details->rendered_surface != NULL) {
        cairo_surface_destroy (details->rendered_surface);
    }

	if (details->editable_text_layout != NULL) {
		g_object_unref (details->editable_text_layout);
	}

	if (details->additional_text_layout != NULL) {
		g_object_unref (details->additional_text_layout);
	}

	G_OBJECT_CLASS (nemo_icon_canvas_item_parent_class)->finalize (object);
}

/* Currently we require pixbufs in this format (for hit testing).
 * Perhaps gdk-pixbuf will be changed so it can do the hit testing
 * and we won't have this requirement any more.
 */
static gboolean
pixbuf_is_acceptable (GdkPixbuf *pixbuf)
{
	return gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB
		&& ((!gdk_pixbuf_get_has_alpha (pixbuf)
		     && gdk_pixbuf_get_n_channels (pixbuf) == 3)
		    || (gdk_pixbuf_get_has_alpha (pixbuf)
			&& gdk_pixbuf_get_n_channels (pixbuf) == 4))
		&& gdk_pixbuf_get_bits_per_sample (pixbuf) == 8;
}

static void
nemo_icon_canvas_item_invalidate_bounds_cache (NemoIconCanvasItem *item)
{
	item->details->bounds_cached = FALSE;
}

/* invalidate the text width and height cached in the item details. */
void
nemo_icon_canvas_item_invalidate_label_size (NemoIconCanvasItem *item)
{
	if (item->details->editable_text_layout != NULL) {
		pango_layout_context_changed (item->details->editable_text_layout);
	}
	if (item->details->additional_text_layout != NULL) {
		pango_layout_context_changed (item->details->additional_text_layout);
	}

	nemo_icon_canvas_item_invalidate_bounds_cache (item);
	item->details->text_width = -1;
	item->details->text_height = -1;
	item->details->text_height_for_layout = -1;
	item->details->text_height_for_entire_text = -1;
	item->details->editable_text_height = -1;
}

/* Set property handler for the icon item. */
static void
nemo_icon_canvas_item_set_property (GObject        *object,
					guint           property_id,
					const GValue   *value,
					GParamSpec     *pspec)
{
	NemoIconCanvasItem *item;
	NemoIconCanvasItemDetails *details;
	AtkObject *accessible;

	item = NEMO_ICON_CANVAS_ITEM (object);
	accessible = atk_gobject_accessible_for_object (G_OBJECT (item));
	details = item->details;

	switch (property_id) {

	case PROP_EDITABLE_TEXT:
		if (g_strcmp0 (details->editable_text,
				g_value_get_string (value)) == 0) {
			return;
		}

		g_free (details->editable_text);
		details->editable_text = g_strdup (g_value_get_string (value));
		if (details->text_util) {
			gail_text_util_text_setup (details->text_util,
						   details->editable_text);
			g_object_notify (G_OBJECT(accessible), "accessible-name");
		}

		nemo_icon_canvas_item_invalidate_label_size (item);
		if (details->editable_text_layout) {
			g_object_unref (details->editable_text_layout);
			details->editable_text_layout = NULL;
		}
		break;

	case PROP_ADDITIONAL_TEXT:
		if (g_strcmp0 (details->additional_text,
			       g_value_get_string (value)) == 0) {
			return;
		}

		g_free (details->additional_text);
		details->additional_text = g_strdup (g_value_get_string (value));

		nemo_icon_canvas_item_invalidate_label_size (item);
		if (details->additional_text_layout) {
			g_object_unref (details->additional_text_layout);
			details->additional_text_layout = NULL;
		}
		break;

	case PROP_HIGHLIGHTED_FOR_SELECTION:
		if (!details->is_highlighted_for_selection == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_for_selection = g_value_get_boolean (value);
		nemo_icon_canvas_item_invalidate_label_size (item);

		atk_object_notify_state_change (accessible, ATK_STATE_SELECTED,
						details->is_highlighted_for_selection);

		break;

        case PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
		if (!details->is_highlighted_as_keyboard_focus == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_as_keyboard_focus = g_value_get_boolean (value);

		if (details->is_highlighted_as_keyboard_focus) {
			atk_focus_tracker_notify (accessible);
		}
		break;

        case PROP_HIGHLIGHTED_FOR_DROP:
		if (!details->is_highlighted_for_drop == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_for_drop = g_value_get_boolean (value);
        nemo_icon_canvas_item_invalidate_label_size (item);

		break;

	case PROP_HIGHLIGHTED_FOR_CLIPBOARD:
		if (!details->is_highlighted_for_clipboard == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_for_clipboard = g_value_get_boolean (value);
		break;

    case PROP_PINNED:
        if (!details->is_pinned == !g_value_get_boolean (value)) {
            return;
        }
        details->is_pinned = g_value_get_boolean (value);
        nemo_icon_canvas_item_invalidate_label (item);

        break;
    case PROP_FAV_UNAVAILABLE:
        if (!details->fav_unavailable == !g_value_get_boolean (value)) {
            return;
        }
        details->fav_unavailable = g_value_get_boolean (value);
        nemo_icon_canvas_item_invalidate_label (item);

        break;

	default:
		g_warning ("nemo_icons_view_item_item_set_arg on unknown argument");
		return;
	}

	eel_canvas_item_request_update (EEL_CANVAS_ITEM (object));
}

/* Get property handler for the icon item */
static void
nemo_icon_canvas_item_get_property (GObject        *object,
					guint           property_id,
					GValue         *value,
					GParamSpec     *pspec)
{
	NemoIconCanvasItemDetails *details;

	details = NEMO_ICON_CANVAS_ITEM (object)->details;

	switch (property_id) {

	case PROP_EDITABLE_TEXT:
		g_value_set_string (value, details->editable_text);
		break;

	case PROP_ADDITIONAL_TEXT:
		g_value_set_string (value, details->additional_text);
		break;

        case PROP_HIGHLIGHTED_FOR_SELECTION:
		g_value_set_boolean (value, details->is_highlighted_for_selection);
                break;

        case PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
		g_value_set_boolean (value, details->is_highlighted_as_keyboard_focus);
                break;

        case PROP_HIGHLIGHTED_FOR_DROP:
		g_value_set_boolean (value, details->is_highlighted_for_drop);
                break;

	case PROP_HIGHLIGHTED_FOR_CLIPBOARD:
		g_value_set_boolean (value, details->is_highlighted_for_clipboard);
                break;

        default:
		g_warning ("invalid property %d", property_id);
		break;
	}
}

static void
get_scaled_icon_size (NemoIconCanvasItem *item,
             gint *width,
             gint *height)
{
   EelCanvas *canvas;
   GdkPixbuf *pixbuf = NULL;
   gint scale;

   scale = 1.0;

   if (item != NULL) {
       canvas = EEL_CANVAS_ITEM (item)->canvas;
       scale = gtk_widget_get_scale_factor (GTK_WIDGET (canvas));
       pixbuf = item->details->pixbuf;
   }

   if (width)
       *width = (pixbuf == NULL) ? 0 : (gdk_pixbuf_get_width (pixbuf) / scale);
   if (height)
       *height = (pixbuf == NULL) ? 0 : (gdk_pixbuf_get_height (pixbuf) / scale);
}

cairo_surface_t *
nemo_icon_canvas_item_get_drag_surface (NemoIconCanvasItem *item)
{
	cairo_surface_t *surface;
	EelCanvas *canvas;
	int width, height;
    int pix_width, pix_height;
	int item_offset_x, item_offset_y;
	EelIRect icon_rect;
	double item_x, item_y;
	cairo_t *cr;
	GtkStyleContext *context;
    cairo_surface_t *drag_surface;

	g_return_val_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item), NULL);

	canvas = EEL_CANVAS_ITEM (item)->canvas;
	context = gtk_widget_get_style_context (GTK_WIDGET (canvas));

	gtk_style_context_save (context);

        gtk_style_context_add_class (context, "nemo-canvas-item");

	/* Assume we're updated so canvas item data is right */

	/* Calculate the offset from the top-left corner of the
	   new image to the item position (where the pixmap is placed) */
	eel_canvas_world_to_window (canvas,
				    item->details->x, item->details->y,
				    &item_x, &item_y);

	item_offset_x = item_x - EEL_CANVAS_ITEM (item)->x1;
	item_offset_y = item_y - EEL_CANVAS_ITEM (item)->y1;

	/* Calculate the width of the item */
	width = EEL_CANVAS_ITEM (item)->x2 - EEL_CANVAS_ITEM (item)->x1;
	height = EEL_CANVAS_ITEM (item)->y2 - EEL_CANVAS_ITEM (item)->y1;

        surface = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (canvas)),
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     width, height);

	cr = cairo_create (surface);

    drag_surface = gdk_cairo_surface_create_from_pixbuf (item->details->pixbuf,
                                                         gtk_widget_get_scale_factor (GTK_WIDGET (canvas)),
                                                         gtk_widget_get_window (GTK_WIDGET (canvas)));
    gtk_render_icon_surface (context, cr, drag_surface,
                            item_offset_x, item_offset_y);
    cairo_surface_destroy (drag_surface);

    get_scaled_icon_size (item, &pix_width, &pix_height);

	icon_rect.x0 = item_offset_x;
	icon_rect.y0 = item_offset_y;
	icon_rect.x1 = item_offset_x + pix_width;
	icon_rect.y1 = item_offset_y + pix_height;

	draw_label_text (item, cr, icon_rect);
	cairo_destroy (cr);

	gtk_style_context_restore (context);

	return surface;
}

void
nemo_icon_canvas_item_set_image (NemoIconCanvasItem *item,
				     GdkPixbuf *image)
{
	NemoIconCanvasItemDetails *details;

	g_return_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (image == NULL || pixbuf_is_acceptable (image));

	details = item->details;
	if (details->pixbuf == image) {
		return;
	}

	if (image != NULL) {
		g_object_ref (image);
	}
	if (details->pixbuf != NULL) {
		g_object_unref (details->pixbuf);
	}
    if (details->rendered_surface != NULL) {
        cairo_surface_destroy (details->rendered_surface);
        details->rendered_surface = NULL;
    }

	details->pixbuf = image;

	nemo_icon_canvas_item_invalidate_bounds_cache (item);
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

/* Recomputes the bounding box of a icon canvas item.
 * This is a generic implementation that could be used for any canvas item
 * class, it has no assumptions about how the item is used.
 */
static void
recompute_bounding_box (NemoIconCanvasItem *icon_item,
			double i2w_dx, double i2w_dy)
{
	/* The bounds stored in the item is the same as what get_bounds
	 * returns, except it's in canvas coordinates instead of the item's
	 * parent's coordinates.
	 */

	EelCanvasItem *item;
	EelDRect bounds_rect;

	item = EEL_CANVAS_ITEM (icon_item);

	eel_canvas_item_get_bounds (item,
				    &bounds_rect.x0, &bounds_rect.y0,
				    &bounds_rect.x1, &bounds_rect.y1);

	bounds_rect.x0 += i2w_dx;
	bounds_rect.y0 += i2w_dy;
	bounds_rect.x1 += i2w_dx;
	bounds_rect.y1 += i2w_dy;
	eel_canvas_w2c_d (item->canvas,
			  bounds_rect.x0, bounds_rect.y0,
			  &item->x1, &item->y1);
	eel_canvas_w2c_d (item->canvas,
			  bounds_rect.x1, bounds_rect.y1,
			  &item->x2, &item->y2);
}

static EelIRect
compute_text_rectangle (const NemoIconCanvasItem *item,
			EelIRect icon_rectangle,
			gboolean canvas_coords,
			NemoIconCanvasItemBoundsUsage usage)
{
	EelIRect text_rectangle;
	double pixels_per_unit;
	double text_width, text_height, text_height_for_layout, text_height_for_entire_text, real_text_height, text_dx;

	pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
	if (canvas_coords) {
		text_width = item->details->text_width;
		text_height = item->details->text_height;
		text_height_for_layout = item->details->text_height_for_layout;
		text_height_for_entire_text = item->details->text_height_for_entire_text;
		text_dx = item->details->text_dx;
	} else {
		text_width = item->details->text_width / pixels_per_unit;
		text_height = item->details->text_height / pixels_per_unit;
		text_height_for_layout = item->details->text_height_for_layout / pixels_per_unit;
		text_height_for_entire_text = item->details->text_height_for_entire_text / pixels_per_unit;
		text_dx = item->details->text_dx / pixels_per_unit;
	}

	if (NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas)->details->label_position == NEMO_ICON_LABEL_POSITION_BESIDE) {
		if (!nemo_icon_container_is_layout_rtl (NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas))) {
                	text_rectangle.x0 = icon_rectangle.x1 + LABEL_OFFSET_BESIDES;
                	text_rectangle.x1 = text_rectangle.x0 + text_dx + text_width;
		} else {
                	text_rectangle.x1 = icon_rectangle.x0;
                	text_rectangle.x0 = text_rectangle.x1 - text_dx - text_width;
		}

		/* VOODOO-TODO */
#if 0
		if (for_layout) {
			/* in this case, we should be more smart and calculate the size according to the maximum
			 * number of lines fitting next to the icon. However, this requires a more complex layout logic.
			 * It would mean that when measuring the label, the icon dimensions must be known already,
			 * and we
			 *   1. start with an unlimited layout
			 *   2. measure how many lines of this layout fit next to the icon
			 *   3. limit the number of lines to the given number of fitting lines
			 */
			real_text_height = VOODOO();
		} else {
#endif
        if (usage == BOUNDS_USAGE_FOR_LAYOUT) {
            real_text_height = text_height_for_layout;
        } else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM) {
            real_text_height = text_height_for_entire_text;
        } else if (usage == BOUNDS_USAGE_FOR_DISPLAY) {
            real_text_height = text_height_for_layout;
        } else {
            g_assert_not_reached ();
        }
#if 0
		}
#endif

        text_rectangle.y0 = (icon_rectangle.y0 + icon_rectangle.y1) / 2- (int) real_text_height / 2;
        text_rectangle.y1 = text_rectangle.y0 + real_text_height;
	} else {
        text_rectangle.x0 = (icon_rectangle.x0 + icon_rectangle.x1) / 2 - (int) text_width / 2;
        text_rectangle.y0 = icon_rectangle.y1;
        text_rectangle.x1 = text_rectangle.x0 + text_width;

		if (usage == BOUNDS_USAGE_FOR_LAYOUT) {
			real_text_height = text_height_for_layout;
		} else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM) {
			real_text_height = text_height_for_entire_text;
		} else if (usage == BOUNDS_USAGE_FOR_DISPLAY) {
			real_text_height = text_height + 6; /* Extra bottom highlight padding. */
		} else {
			g_assert_not_reached ();
		}

		text_rectangle.y1 = text_rectangle.y0 + real_text_height;
        }

	return text_rectangle;
}

static EelIRect
get_current_canvas_bounds (EelCanvasItem *item)
{
	EelIRect bounds;

	g_assert (EEL_IS_CANVAS_ITEM (item));

	bounds.x0 = item->x1;
	bounds.y0 = item->y1;
	bounds.x1 = item->x2;
	bounds.y1 = item->y2;

	return bounds;
}

void
nemo_icon_canvas_item_update_bounds (NemoIconCanvasItem *item,
					 double i2w_dx, double i2w_dy)
{
	EelIRect before, after;
	EelCanvasItem *canvas_item;

	canvas_item = EEL_CANVAS_ITEM (item);

	/* Compute new bounds. */
	before = get_current_canvas_bounds (canvas_item);
	recompute_bounding_box (item, i2w_dx, i2w_dy);
	after = get_current_canvas_bounds (canvas_item);

	/* If the bounds didn't change, we are done. */
	if (eel_irect_equal (before, after)) {
		return;
	}

	/* Update canvas and text rect cache */
	nemo_icon_canvas_item_get_icon_canvas_rectangle (item, &item->details->canvas_rect);
	item->details->text_rect = compute_text_rectangle (item, item->details->canvas_rect,
							   TRUE, BOUNDS_USAGE_FOR_DISPLAY);

	/* queue a redraw. */
	eel_canvas_request_redraw (canvas_item->canvas,
				   before.x0, before.y0,
				   before.x1 + 5, before.y1 + 5);
}

/* Update handler for the icon canvas item. */
static void
nemo_icon_canvas_item_update (EelCanvasItem *item,
				  double i2w_dx, double i2w_dy,
				  gint flags)
{
	nemo_icon_canvas_item_update_bounds (NEMO_ICON_CANVAS_ITEM (item), i2w_dx, i2w_dy);

	eel_canvas_item_request_redraw (EEL_CANVAS_ITEM (item));

	EEL_CANVAS_ITEM_CLASS (nemo_icon_canvas_item_parent_class)->update (item, i2w_dx, i2w_dy, flags);
}

/* Rendering */
static gboolean
in_single_click_mode (void)
{
	int click_policy;

	click_policy = g_settings_get_enum (nemo_preferences,
					    NEMO_PREFERENCES_CLICK_POLICY);

	return click_policy == NEMO_CLICK_POLICY_SINGLE;
}


/* Keep these for a bit while we work on performance of draw_or_measure_label_text. */
/*
  #define PERFORMANCE_TEST_DRAW_DISABLE
  #define PERFORMANCE_TEST_MEASURE_DISABLE
*/

/* This gets the size of the layout from the position of the layout.
 * This means that if the layout is right aligned we get the full width
 * of the layout, not just the width of the text snippet on the right side
 */
static void
layout_get_full_size (PangoLayout *layout,
		      int         *width,
		      int         *height,
		      int         *dx)
{
	PangoRectangle logical_rect;
	int the_width, total_width;

	pango_layout_get_extents (layout, NULL, &logical_rect);
	the_width = (logical_rect.width + PANGO_SCALE / 2) / PANGO_SCALE;
	total_width = (logical_rect.x + logical_rect.width + PANGO_SCALE / 2) / PANGO_SCALE;

	if (width != NULL) {
		*width = the_width;
	}

	if (height != NULL) {
		*height = (logical_rect.height + PANGO_SCALE / 2) / PANGO_SCALE;
	}

	if (dx != NULL) {
		*dx = total_width - the_width;
	}
}

static void
layout_get_size_for_layout (PangoLayout *layout,
			    int          max_layout_line_count,
			    int          height_for_entire_text,
			    int         *height_for_layout)
{
	PangoLayoutIter *iter;
	PangoRectangle logical_rect;
	int i;

	/* only use the first max_layout_line_count lines for the gridded auto layout */
	if (pango_layout_get_line_count (layout) <= max_layout_line_count) {
		*height_for_layout = height_for_entire_text;
	} else {
		*height_for_layout = 0;
		iter = pango_layout_get_iter (layout);
		/* VOODOO-TODO, determine number of lines based on the icon size for text besides icon.
		 * cf. compute_text_rectangle() */
		for (i = 0; i < max_layout_line_count; i++) {
			pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
			*height_for_layout += (logical_rect.height + PANGO_SCALE / 2) / PANGO_SCALE;

			if (!pango_layout_iter_next_line (iter)) {
				break;
			}

			*height_for_layout += pango_layout_get_spacing (layout);
		}
		pango_layout_iter_free (iter);
	}
}

#define IS_COMPACT_VIEW(container) \
        ((container->details->layout_mode == NEMO_ICON_LAYOUT_T_B_L_R || \
	  container->details->layout_mode == NEMO_ICON_LAYOUT_T_B_R_L) && \
	 container->details->label_position == NEMO_ICON_LABEL_POSITION_BESIDE)

#define TEXT_BACK_PADDING_X 4
#define TEXT_BACK_PADDING_Y 1
#define TEXT_TOP_GAP 3

static void
prepare_pango_layout_width (NemoIconCanvasItem *item,
			    PangoLayout *layout)
{
	if (nemo_icon_canvas_item_get_max_text_width (item) < 0) {
		pango_layout_set_width (layout, -1);
	} else {
		pango_layout_set_width (layout, floor (nemo_icon_canvas_item_get_max_text_width (item)) * PANGO_SCALE);
		pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
	}
}

static void
prepare_pango_layout_for_measure_entire_text (NemoIconCanvasItem *item,
					      PangoLayout *layout)
{
	NemoIconContainer *container;

	prepare_pango_layout_width (item, layout);

	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);

	if (IS_COMPACT_VIEW (container)) {
		pango_layout_set_height (layout, -1);
	} else {
		pango_layout_set_height (layout, G_MININT);
	}
}

static void
prepare_pango_layout_for_draw (NemoIconCanvasItem *item,
			       PangoLayout *layout)
{
	NemoIconCanvasItemDetails *details;
	NemoIconContainer *container;

	prepare_pango_layout_width (item, layout);

	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	details = item->details;

	if (IS_COMPACT_VIEW (container)) {
		pango_layout_set_height (layout, -1);
	} else if (details->is_prelit ||
		   details->entire_text) {
		/* VOODOO-TODO, cf. compute_text_rectangle() */
		pango_layout_set_height (layout, G_MININT);
	} else {
		/* TODO? we might save some resources, when the re-layout is not neccessary in case
		 * the layout height already fits into max. layout lines. But pango should figure this
		 * out itself (which it doesn't ATM).
		 */
		pango_layout_set_height (layout,
					 nemo_icon_container_get_max_layout_lines_for_pango (container));
	}
}

static void
measure_label_text (NemoIconCanvasItem *item)
{
	NemoIconCanvasItemDetails *details;
	NemoIconContainer *container;
	gint editable_height, editable_height_for_layout, editable_height_for_entire_text, editable_width, editable_dx;
	gint additional_height, additional_width, additional_dx;
	PangoLayout *editable_layout;
	PangoLayout *additional_layout;
	gboolean have_editable, have_additional;

	/* check to see if the cached values are still valid; if so, there's
	 * no work necessary
	 */

	if (item->details->text_width >= 0 && item->details->text_height >= 0) {
		return;
	}

	details = item->details;

	have_editable = details->editable_text != NULL && details->editable_text[0] != '\0';
	have_additional = details->additional_text != NULL && details->additional_text[0] != '\0';

	/* No font or no text, then do no work. */
	if (!have_editable && !have_additional) {
		details->text_height = 0;
		details->text_height_for_layout = 0;
		details->text_height_for_entire_text = 0;
		details->text_width = 0;
		return;
	}

#ifdef PERFORMANCE_TEST_MEASURE_DISABLE
	/* fake out the width */
	details->text_width = 80;
	details->text_height = 20;
	details->text_height_for_layout = 20;
	details->text_height_for_entire_text = 20;
	return;
#endif

	editable_width = 0;
	editable_height = 0;
	editable_height_for_layout = 0;
	editable_height_for_entire_text = 0;
	editable_dx = 0;
	additional_width = 0;
	additional_height = 0;
	additional_dx = 0;

	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	editable_layout = NULL;
	additional_layout = NULL;

	if (have_editable) {
		/* first, measure required text height: editable_height_for_entire_text
		 * then, measure text height applicable for layout: editable_height_for_layout
		 * next, measure actually displayed height: editable_height
		 */
		editable_layout = get_label_layout (&details->editable_text_layout, item, details->editable_text);

		prepare_pango_layout_for_measure_entire_text (item, editable_layout);
		layout_get_full_size (editable_layout,
				      NULL,
				      &editable_height_for_entire_text,
				      NULL);
		layout_get_size_for_layout (editable_layout,
					    nemo_icon_container_get_max_layout_lines (container),
					    editable_height_for_entire_text,
					    &editable_height_for_layout);

		prepare_pango_layout_for_draw (item, editable_layout);
		layout_get_full_size (editable_layout,
				      &editable_width,
				      &editable_height,
				      &editable_dx);
	}

	if (have_additional) {
		additional_layout = get_label_layout (&details->additional_text_layout, item, details->additional_text);
		prepare_pango_layout_for_draw (item, additional_layout);
		layout_get_full_size (additional_layout,
				      &additional_width, &additional_height, &additional_dx);
	}

	details->editable_text_height = editable_height;

	if (editable_width > additional_width) {
		details->text_width = editable_width;
		details->text_dx = editable_dx;
	} else {
		details->text_width = additional_width;
		details->text_dx = additional_dx;
	}

	if (have_additional) {
		details->text_height = editable_height + LABEL_LINE_SPACING + additional_height;
		details->text_height_for_layout = editable_height_for_layout + LABEL_LINE_SPACING + additional_height;
		details->text_height_for_entire_text = editable_height_for_entire_text + LABEL_LINE_SPACING + additional_height;
	} else {
		details->text_height = editable_height;
		details->text_height_for_layout = editable_height_for_layout;
		details->text_height_for_entire_text = editable_height_for_entire_text;
	}

	/* add some extra space for highlighting even when we don't highlight so things won't move */

    if (IS_COMPACT_VIEW (container)) {
        details->text_width += TEXT_BACK_PADDING_X;
        details->text_height_for_layout += TEXT_BACK_PADDING_Y*2;
    } else {
        /* extra slop for nicer highlighting */
        details->text_height += TEXT_BACK_PADDING_Y*2;
        details->text_height_for_layout += TEXT_BACK_PADDING_Y*2;
        details->text_height_for_entire_text += TEXT_BACK_PADDING_Y*2;
        details->editable_text_height += TEXT_BACK_PADDING_Y*2;
        /* extra to make it look nicer */
        details->text_width += TEXT_BACK_PADDING_X*2;
    }

	if (editable_layout) {
		g_object_unref (editable_layout);
	}

	if (additional_layout) {
		g_object_unref (additional_layout);
	}
}

static void
draw_label_text (NemoIconCanvasItem *item,
                 cairo_t *cr,
		 EelIRect icon_rect)
{
	NemoIconCanvasItemDetails *details;
	NemoIconContainer *container;
	PangoLayout *editable_layout;
	PangoLayout *additional_layout;
	GtkStyleContext *context;
	GtkStateFlags state, base_state;
	gboolean have_editable, have_additional;
	gboolean needs_highlight, prelight_label, is_rtl_label_beside;
	EelIRect text_rect;
	int x, y;
	int max_text_width;
	gdouble frame_w, frame_h, frame_x, frame_y;
	gboolean draw_frame = TRUE;

#ifdef PERFORMANCE_TEST_DRAW_DISABLE
	return;
#endif

	details = item->details;

	measure_label_text (item);
	if (details->text_height == 0 ||
	    details->text_width == 0) {
		return;
	}

	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	context = gtk_widget_get_style_context (GTK_WIDGET (container));

	text_rect = compute_text_rectangle (item, icon_rect, TRUE, BOUNDS_USAGE_FOR_DISPLAY);

	needs_highlight = details->is_highlighted_for_selection || details->is_highlighted_for_drop;
	is_rtl_label_beside = nemo_icon_container_is_layout_rtl (container) &&
			      container->details->label_position == NEMO_ICON_LABEL_POSITION_BESIDE;

	editable_layout = NULL;
	additional_layout = NULL;

	have_editable = details->editable_text != NULL && details->editable_text[0] != '\0';
	have_additional = details->additional_text != NULL && details->additional_text[0] != '\0';
	g_assert (have_editable || have_additional);

	max_text_width = floor (nemo_icon_canvas_item_get_max_text_width (item));

	base_state = gtk_widget_get_state_flags (GTK_WIDGET (container));
	base_state &= ~(GTK_STATE_FLAG_SELECTED |
			GTK_STATE_FLAG_PRELIGHT);
	state = base_state;

	gtk_widget_style_get (GTK_WIDGET (container),
			      "activate_prelight_icon_label", &prelight_label,
			      NULL);

	/* if the icon is highlighted, do some set-up */
	if (needs_highlight &&
	    !details->is_renaming) {
		state |= GTK_STATE_FLAG_SELECTED;

		frame_x = is_rtl_label_beside ? text_rect.x0 + item->details->text_dx : text_rect.x0;
		frame_y = text_rect.y0;
		frame_w = is_rtl_label_beside ? text_rect.x1 - text_rect.x0 - item->details->text_dx : text_rect.x1 - text_rect.x0;
		frame_h = text_rect.y1 - text_rect.y0;
	} else if (!needs_highlight && have_editable &&
		   details->text_width > 0 && details->text_height > 0 &&
		   prelight_label && item->details->is_prelit) {
		state |= GTK_STATE_FLAG_PRELIGHT;

		frame_x = text_rect.x0;
		frame_y = text_rect.y0;
		frame_w = text_rect.x1 - text_rect.x0;
		frame_h = text_rect.y1 - text_rect.y0;
	} else {
		draw_frame = FALSE;
	}

	if (draw_frame) {
		gtk_style_context_save (context);
		gtk_style_context_set_state (context, state);

		gtk_render_frame (context, cr,
				  frame_x, frame_y,
				  frame_w, frame_h);
		gtk_render_background (context, cr,
				       frame_x, frame_y,
				       frame_w, frame_h);

		gtk_style_context_restore (context);
	}

	if (container->details->label_position == NEMO_ICON_LABEL_POSITION_BESIDE) {
		x = text_rect.x0 + 2;
        y = text_rect.y0;
	} else {
		x = text_rect.x0 + ((text_rect.x1 - text_rect.x0) - max_text_width) / 2;
        y = text_rect.y0 + TEXT_TOP_GAP;
	}

	if (have_editable &&
	    !details->is_renaming) {
		state = base_state;

		if (prelight_label && item->details->is_prelit) {
			state |= GTK_STATE_FLAG_PRELIGHT;
		}

		if (needs_highlight) {
			state |= GTK_STATE_FLAG_SELECTED;
		}

		editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
		prepare_pango_layout_for_draw (item, editable_layout);

		gtk_style_context_save (context);
		gtk_style_context_set_state (context, state);

		gtk_render_layout (context, cr,
				   x, y,
				   editable_layout);

		gtk_style_context_restore (context);
	}

	if (have_additional &&
	    !details->is_renaming) {
		state = base_state;

		if (needs_highlight) {
			state |= GTK_STATE_FLAG_SELECTED;
		}

		additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
		prepare_pango_layout_for_draw (item, additional_layout);

		gtk_style_context_save (context);
		gtk_style_context_set_state (context, state);
		gtk_style_context_add_class (context, "dim-label");

		gtk_render_layout (context, cr,
				   x, y + details->editable_text_height + LABEL_LINE_SPACING,
				   additional_layout);
        gtk_style_context_restore (context);
	}

	if (item->details->is_highlighted_as_keyboard_focus) {
		if (needs_highlight) {
			state = GTK_STATE_FLAG_SELECTED;
		}

		gtk_style_context_save (context);
		gtk_style_context_set_state (context, state);

		gtk_render_focus (context,
				  cr,
				  text_rect.x0,
				  text_rect.y0,
				  text_rect.x1 - text_rect.x0,
				  text_rect.y1 - text_rect.y0);

		gtk_style_context_restore (context);
	}

	if (editable_layout != NULL) {
		g_object_unref (editable_layout);
	}

	if (additional_layout != NULL) {
		g_object_unref (additional_layout);
	}
}

void
nemo_icon_canvas_item_set_is_visible (NemoIconCanvasItem       *item,
					  gboolean                      visible)
{
	if (item->details->is_visible == visible)
		return;

	item->details->is_visible = visible;

	if (!visible) {
		nemo_icon_canvas_item_invalidate_label (item);
	}
}

void
nemo_icon_canvas_item_invalidate_label (NemoIconCanvasItem     *item)
{
	nemo_icon_canvas_item_invalidate_label_size (item);

	if (item->details->editable_text_layout) {
		g_object_unref (item->details->editable_text_layout);
		item->details->editable_text_layout = NULL;
	}

	if (item->details->additional_text_layout) {
		g_object_unref (item->details->additional_text_layout);
		item->details->additional_text_layout = NULL;
	}
}


static GdkPixbuf *
get_knob_pixbuf (void)
{
	GdkPixbuf *knob_pixbuf;

	knob_pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						"stock-nemo-knob",
						8, 0, NULL);
	if (!knob_pixbuf) {
		GInputStream *stream = g_resources_open_stream ("/org/nemo/icons/knob.png", 0, NULL);
		if (stream != NULL) {
			knob_pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
			g_object_unref (stream);
		}
	}

	return knob_pixbuf;
}

static void
draw_stretch_handles (NemoIconCanvasItem *item,
                      cairo_t *cr,
		      const EelIRect *rect)
{
	GtkWidget *widget;
	GdkPixbuf *knob_pixbuf;
	int knob_width, knob_height;
	double dash = { 2.0 };
	GtkStyleContext *style;
	GdkRGBA color;

	if (!item->details->show_stretch_handles) {
		return;
	}

	widget = GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas);
	style = gtk_widget_get_style_context (widget);

        cairo_save (cr);
	knob_pixbuf = get_knob_pixbuf ();
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);

	/* first draw the box */
	gtk_style_context_get_color (style, GTK_STATE_FLAG_SELECTED, &color);
	gdk_cairo_set_source_rgba (cr, &color);
	cairo_set_dash (cr, &dash, 1, 0);
	cairo_set_line_width (cr, 1.0);
	cairo_rectangle (cr,
			 rect->x0 + 0.5,
			 rect->y0 + 0.5,
			 rect->x1 - rect->x0 - 1,
			 rect->y1 - rect->y0 - 1);
	cairo_stroke (cr);

        cairo_restore (cr);

	/* draw the stretch handles themselves */
	draw_pixbuf (knob_pixbuf, cr, rect->x0, rect->y0);
	draw_pixbuf (knob_pixbuf, cr, rect->x0, rect->y1 - knob_height);
	draw_pixbuf (knob_pixbuf, cr, rect->x1 - knob_width, rect->y0);
	draw_pixbuf (knob_pixbuf, cr, rect->x1 - knob_width, rect->y1 - knob_height);

	g_object_unref (knob_pixbuf);
}

static void
draw_pixbuf (GdkPixbuf *pixbuf,
             cairo_t *cr,
             int x, int y)
{
        cairo_save (cr);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
	cairo_paint (cr);
        cairo_restore (cr);
}

/* shared code to highlight or dim the passed-in pixbuf */
static cairo_surface_t *
real_map_surface (NemoIconCanvasItem *icon_item)
{
	EelCanvas *canvas;
	GdkPixbuf *temp_pixbuf, *old_pixbuf;
    cairo_surface_t *surface;

	temp_pixbuf = icon_item->details->pixbuf;
	canvas = EEL_CANVAS_ITEM(icon_item)->canvas;

	g_object_ref (temp_pixbuf);

	if (icon_item->details->is_prelit ||
	    icon_item->details->is_highlighted_for_clipboard) {
		old_pixbuf = temp_pixbuf;

		temp_pixbuf = eel_create_spotlight_pixbuf (temp_pixbuf);
		g_object_unref (old_pixbuf);
	}

    surface = gdk_cairo_surface_create_from_pixbuf (temp_pixbuf,
                                                    gtk_widget_get_scale_factor (GTK_WIDGET (canvas)),
                                                    gtk_widget_get_window (GTK_WIDGET (canvas)));
    g_object_unref (temp_pixbuf);

    return surface;
}

static cairo_surface_t *
map_surface (NemoIconCanvasItem *icon_item)
{
#if 0
    g_printerr ("%p prelit: %d->%d, high sel: %d->%d, high drop: %d->%d, high clip: %d->%d, focus:%d+%d->%d\n",
            icon_item->details->rendered_surface,
            icon_item->details->rendered_is_prelit, icon_item->details->is_prelit,
            icon_item->details->rendered_is_highlighted_for_selection, icon_item->details->is_highlighted_for_selection,
            icon_item->details->rendered_is_highlighted_for_drop, icon_item->details->is_highlighted_for_drop,
            icon_item->details->rendered_is_highlighted_for_clipboard, icon_item->details->is_highlighted_for_clipboard,
            icon_item->details->is_highlighted_for_selection, icon_item->details->rendered_is_focused, gtk_widget_has_focus (GTK_WIDGET (EEL_CANVAS_ITEM (icon_item)->canvas)));
#endif

	if (!(icon_item->details->rendered_surface != NULL
	      && icon_item->details->rendered_is_prelit == icon_item->details->is_prelit
	      && icon_item->details->rendered_is_highlighted_for_selection == icon_item->details->is_highlighted_for_selection
	      && icon_item->details->rendered_is_highlighted_for_drop == icon_item->details->is_highlighted_for_drop
	      && icon_item->details->rendered_is_highlighted_for_clipboard == icon_item->details->is_highlighted_for_clipboard
	      && icon_item->details->rendered_is_focused == gtk_widget_has_focus (GTK_WIDGET (EEL_CANVAS_ITEM (icon_item)->canvas)))) {
		if (icon_item->details->rendered_surface != NULL) {
            cairo_surface_destroy (icon_item->details->rendered_surface);
		}
        icon_item->details->rendered_surface = real_map_surface (icon_item);
		icon_item->details->rendered_is_prelit = icon_item->details->is_prelit;
		icon_item->details->rendered_is_highlighted_for_selection = icon_item->details->is_highlighted_for_selection;
		icon_item->details->rendered_is_highlighted_for_drop = icon_item->details->is_highlighted_for_drop;
	        icon_item->details->rendered_is_highlighted_for_clipboard = icon_item->details->is_highlighted_for_clipboard;
		icon_item->details->rendered_is_focused = gtk_widget_has_focus (GTK_WIDGET (EEL_CANVAS_ITEM (icon_item)->canvas));
	}

	cairo_surface_reference (icon_item->details->rendered_surface);

	return icon_item->details->rendered_surface;
}

/* Draw the icon item for non-anti-aliased mode. */
static void
nemo_icon_canvas_item_draw (EelCanvasItem *item,
                                cairo_t *cr,
                                cairo_region_t *region)
{
	NemoIconContainer *container;
	NemoIconCanvasItem *icon_item;
	NemoIconCanvasItemDetails *details;
	EelIRect icon_rect;
    cairo_surface_t *temp_surface;
	GtkStyleContext *context;

	container = NEMO_ICON_CONTAINER (item->canvas);
	icon_item = NEMO_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

        /* Draw the pixbuf. */
     	if (details->pixbuf == NULL) {
		return;
	}

	context = gtk_widget_get_style_context (GTK_WIDGET (container));
	gtk_style_context_save (context);

	gtk_style_context_add_class (context, "nemo-canvas-item");

	icon_rect = icon_item->details->canvas_rect;
	temp_surface = map_surface (icon_item);

    gtk_render_icon_surface (context, cr,
                             temp_surface,
                             icon_rect.x0, icon_rect.y0);
    cairo_surface_destroy (temp_surface);

	/* Draw stretching handles (if necessary). */
	draw_stretch_handles (icon_item, cr, &icon_rect);

	/* Draw the label text. */
	draw_label_text (icon_item, cr, icon_rect);

	gtk_style_context_restore (context);
}

#define ZERO_WIDTH_SPACE "\xE2\x80\x8B"


static PangoLayout *
create_label_layout (NemoIconCanvasItem *item,
		     const char *text)
{
	PangoLayout *layout;
	PangoContext *context;
	PangoFontDescription *desc;
	NemoIconContainer *container;
	EelCanvasItem *canvas_item;
	GString *str;
	char *zeroified_text;
	const char *p;
#ifdef HAVE_PANGO_144
 	PangoAttrList *attr_list;
#endif

	canvas_item = EEL_CANVAS_ITEM (item);

	container = NEMO_ICON_CONTAINER (canvas_item->canvas);
	context = gtk_widget_get_pango_context (GTK_WIDGET (canvas_item->canvas));
	layout = pango_layout_new (context);
#ifdef HAVE_PANGO_144
 	attr_list = pango_attr_list_new ();
#endif

	zeroified_text = NULL;

	if (text != NULL) {
		str = g_string_new (NULL);

		for (p = text; *p != '\0'; p++) {
			str = g_string_append_c (str, *p);

			if (*p == '_' || *p == '-' || (*p == '.' && !g_ascii_isdigit(*(p+1)))) {
				/* Ensure that we allow to break after '_' or '.' characters,
				 * if they are not followed by a number */
				str = g_string_append (str, ZERO_WIDTH_SPACE);
			}
		}

		zeroified_text = g_string_free (str, FALSE);
	}

	pango_layout_set_text (layout, zeroified_text, -1);
	pango_layout_set_auto_dir (layout, FALSE);

	if (container->details->label_position == NEMO_ICON_LABEL_POSITION_BESIDE) {
		if (!nemo_icon_container_is_layout_rtl (container)) {
			pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
		} else {
			pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
		}
	} else {
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	}

	pango_layout_set_spacing (layout, LABEL_LINE_SPACING);
	pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

#ifdef HAVE_PANGO_144
 	pango_attr_list_insert (attr_list, pango_attr_insert_hyphens_new (FALSE));
 	pango_layout_set_attributes (layout, attr_list);
#endif

	/* Create a font description */
	if (container->details->font && g_strcmp0 (container->details->font, "") != 0) {
		desc = pango_font_description_from_string (container->details->font);
	} else {
		desc = pango_font_description_copy (pango_context_get_font_description (context));
	}

    if (pango_font_description_get_size (desc) > 0) {
        pango_font_description_set_size (desc,
                                         pango_font_description_get_size (desc) +
                                         container->details->font_size_table [container->details->zoom_level]);
    }

    if (item->details->fav_unavailable) {
        pango_font_description_set_weight (desc, UNAVAILABLE_TEXT_WEIGHT);
    }
    else
    if (item->details->is_pinned) {
        pango_font_description_set_weight (desc, PINNED_TEXT_WEIGHT);
    }

	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
	g_free (zeroified_text);
#ifdef HAVE_PANGO_144
 	pango_attr_list_unref (attr_list);
#endif

	return layout;
}

static PangoLayout *
get_label_layout (PangoLayout **layout_cache,
		  NemoIconCanvasItem *item,
		  const char *text)
{
	PangoLayout *layout;

	if (*layout_cache != NULL) {
		return g_object_ref (*layout_cache);
	}

	layout = create_label_layout (item, text);

	if (item->details->is_visible) {
		*layout_cache = g_object_ref (layout);
	}

	return layout;
}

/* handle events */

static int
nemo_icon_canvas_item_event (EelCanvasItem *item, GdkEvent *event)
{
	NemoIconCanvasItem *icon_item;
	GdkCursor *cursor;
	GdkWindow *cursor_window;

	icon_item = NEMO_ICON_CANVAS_ITEM (item);
	cursor_window = ((GdkEventAny *)event)->window;


    if (event->type == GDK_ENTER_NOTIFY) {
        nemo_icon_container_update_tooltip_text (NEMO_ICON_CONTAINER (item->canvas), icon_item);
		if (!icon_item->details->is_prelit) {
			icon_item->details->is_prelit = TRUE;
			nemo_icon_canvas_item_invalidate_label_size (icon_item);
			eel_canvas_item_request_update (item);
			eel_canvas_item_send_behind (item,
						     NEMO_ICON_CONTAINER (item->canvas)->details->rubberband_info.selection_rectangle);

			/* show a hand cursor */
			if (in_single_click_mode ()) {
				cursor = gdk_cursor_new_for_display (gdk_display_get_default(),
								     GDK_HAND2);
				gdk_window_set_cursor (cursor_window, cursor);
				g_object_unref (cursor);

				icon_item->details->cursor_window = g_object_ref (cursor_window);
			}
		}
		return TRUE;
    } else if (event->type == GDK_LEAVE_NOTIFY) {
        nemo_icon_container_update_tooltip_text (NEMO_ICON_CONTAINER (item->canvas), NULL);
		if (icon_item->details->is_prelit
		    || icon_item->details->is_highlighted_for_drop) {
			/* When leaving, turn of the prelight state and the
			 * higlighted for drop. The latter gets turned on
			 * by the drag&drop motion callback.
			 */
			icon_item->details->is_prelit = FALSE;
			icon_item->details->is_highlighted_for_drop = FALSE;
			nemo_icon_canvas_item_invalidate_label_size (icon_item);
			eel_canvas_item_request_update (item);

			/* show default cursor */
			gdk_window_set_cursor (cursor_window, NULL);
			g_clear_object (&icon_item->details->cursor_window);
		}
		return TRUE;
    }

	return FALSE;
}

static gboolean
hit_test (NemoIconCanvasItem *icon_item, EelIRect canvas_rect)
{
	NemoIconCanvasItemDetails *details;

	details = icon_item->details;

	/* Quick check to see if the rect hits the icon or text at all. */
	if (!eel_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect)
	    && (!eel_irect_hits_irect (details->text_rect, canvas_rect))) {
		return FALSE;
	}

	/* Check for hits in the stretch handles. */
	if (hit_test_stretch_handle (icon_item, canvas_rect, NULL)) {
		return TRUE;
	}

	/* Check for hit in the icon. */
	if (eel_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect)) {
		return TRUE;
	}

	/* Check for hit in the text. */
	if (eel_irect_hits_irect (details->text_rect, canvas_rect)
	    && !icon_item->details->is_renaming) {
		return TRUE;
	}

	return FALSE;
}

/* Point handler for the icon canvas item. */
static double
nemo_icon_canvas_item_point (EelCanvasItem *item, double x, double y, int cx, int cy,
				 EelCanvasItem **actual_item)
{
	EelIRect canvas_rect;

	*actual_item = item;
	canvas_rect.x0 = cx;
	canvas_rect.y0 = cy;
	canvas_rect.x1 = cx + 1;
	canvas_rect.y1 = cy + 1;
	if (hit_test (NEMO_ICON_CANVAS_ITEM (item), canvas_rect)) {
		return 0.0;
	} else {
		/* This value means not hit.
		 * It's kind of arbitrary. Can we do better?
		 */
		return item->canvas->pixels_per_unit * 2 + 10;
	}
}

static void
nemo_icon_canvas_item_translate (EelCanvasItem *item, double dx, double dy)
{
	NemoIconCanvasItem *icon_item;
	NemoIconCanvasItemDetails *details;

	icon_item = NEMO_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

	details->x += dx;
	details->y += dy;
}

void
nemo_icon_canvas_item_get_bounds_for_layout (NemoIconCanvasItem *icon_item,
						 double *x1, double *y1, double *x2, double *y2)
{
	NemoIconCanvasItemDetails *details;
	EelIRect *total_rect;

	details = icon_item->details;

	nemo_icon_canvas_item_ensure_bounds_up_to_date (icon_item);
	g_assert (details->bounds_cached);

	total_rect = &details->bounds_cache_for_layout;

	/* Return the result. */
	if (x1 != NULL) {
		*x1 = (int)details->x + total_rect->x0;
	}
	if (y1 != NULL) {
		*y1 = (int)details->y + total_rect->y0;
	}
	if (x2 != NULL) {
		*x2 = (int)details->x + total_rect->x1 + 1;
	}
	if (y2 != NULL) {
		*y2 = (int)details->y + total_rect->y1 + 1;
	}
}

void
nemo_icon_canvas_item_get_bounds_for_entire_item (NemoIconCanvasItem *icon_item,
						      double *x1, double *y1, double *x2, double *y2)
{
	NemoIconCanvasItemDetails *details;
	EelIRect *total_rect;

	details = icon_item->details;

	nemo_icon_canvas_item_ensure_bounds_up_to_date (icon_item);
	g_assert (details->bounds_cached);

	total_rect = &details->bounds_cache_for_entire_item;

	/* Return the result. */
	if (x1 != NULL) {
		*x1 = (int)details->x + total_rect->x0;
	}
	if (y1 != NULL) {
		*y1 = (int)details->y + total_rect->y0;
	}
	if (x2 != NULL) {
		*x2 = (int)details->x + total_rect->x1 + 1;
	}
	if (y2 != NULL) {
		*y2 = (int)details->y + total_rect->y1 + 1;
	}
}

/* Bounds handler for the icon canvas item. */
static void
nemo_icon_canvas_item_bounds (EelCanvasItem *item,
				  double *x1, double *y1, double *x2, double *y2)
{
	NemoIconCanvasItem *icon_item;
	NemoIconCanvasItemDetails *details;
	EelIRect *total_rect;

	icon_item = NEMO_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

	g_assert (x1 != NULL);
	g_assert (y1 != NULL);
	g_assert (x2 != NULL);
	g_assert (y2 != NULL);

	nemo_icon_canvas_item_ensure_bounds_up_to_date (icon_item);
	g_assert (details->bounds_cached);

	total_rect = &details->bounds_cache;

	/* Return the result. */
	*x1 = (int)details->x + total_rect->x0;
	*y1 = (int)details->y + total_rect->y0;
	*x2 = (int)details->x + total_rect->x1 + 1;
	*y2 = (int)details->y + total_rect->y1 + 1;
}

static void
nemo_icon_canvas_item_ensure_bounds_up_to_date (NemoIconCanvasItem *icon_item)
{
	NemoIconCanvasItemDetails *details;
	EelIRect icon_rect, icon_rect_raw;
	EelIRect text_rect, text_rect_for_layout, text_rect_for_entire_text;
	EelIRect total_rect, total_rect_for_layout, total_rect_for_entire_text;
	EelCanvasItem *item;
	double pixels_per_unit;
    gint width, height;

	details = icon_item->details;
	item = EEL_CANVAS_ITEM (icon_item);

	if (!details->bounds_cached) {
		measure_label_text (icon_item);

		pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;

		/* Compute raw and scaled icon rectangle. */
		icon_rect.x0 = 0;
		icon_rect.y0 = 0;
		icon_rect_raw.x0 = 0;
		icon_rect_raw.y0 = 0;

        get_scaled_icon_size (icon_item, &width, &height);

        icon_rect_raw.x1 = icon_rect_raw.x0 + width;
        icon_rect_raw.y1 = icon_rect_raw.y0 + height;
        icon_rect.x1 = icon_rect_raw.x1 / pixels_per_unit;
        icon_rect.y1 = icon_rect_raw.y1 / pixels_per_unit;

		/* Compute text rectangle. */
		text_rect = compute_text_rectangle (icon_item, icon_rect, FALSE, BOUNDS_USAGE_FOR_DISPLAY);
		text_rect_for_layout = compute_text_rectangle (icon_item, icon_rect, FALSE, BOUNDS_USAGE_FOR_LAYOUT);
		text_rect_for_entire_text = compute_text_rectangle (icon_item, icon_rect, FALSE, BOUNDS_USAGE_FOR_ENTIRE_ITEM);

		/* Compute total rectangle */
		eel_irect_union (&total_rect, &icon_rect, &text_rect);
		eel_irect_union (&total_rect_for_layout, &icon_rect, &text_rect_for_layout);
		eel_irect_union (&total_rect_for_entire_text, &icon_rect, &text_rect_for_entire_text);

		details->bounds_cache = total_rect;
		details->bounds_cache_for_layout = total_rect_for_layout;
		details->bounds_cache_for_entire_item = total_rect_for_entire_text;
		details->bounds_cached = TRUE;
	}
}

/* Get the rectangle of the icon only, in world coordinates. */
EelDRect
nemo_icon_canvas_item_get_icon_rectangle (const NemoIconCanvasItem *item)
{
	EelDRect rectangle;
	double pixels_per_unit;
    gint width, height;

	g_return_val_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item), eel_drect_empty);

	rectangle.x0 = item->details->x;
	rectangle.y0 = item->details->y;

	pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
    get_scaled_icon_size (NEMO_ICON_CANVAS_ITEM (item), &width, &height);
    rectangle.x1 = rectangle.x0 + width / pixels_per_unit;
    rectangle.y1 = rectangle.y0 + height / pixels_per_unit;

	eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
			     &rectangle.x0,
			     &rectangle.y0);
	eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
			     &rectangle.x1,
			     &rectangle.y1);

	return rectangle;
}

EelDRect
nemo_icon_canvas_item_get_text_rectangle (NemoIconCanvasItem *item,
					      gboolean for_layout)
{
	/* FIXME */
	EelIRect icon_rectangle;
	EelIRect text_rectangle;
	EelDRect ret;
	double pixels_per_unit;
    gint width, height;

	g_return_val_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item), eel_drect_empty);

	icon_rectangle.x0 = item->details->x;
	icon_rectangle.y0 = item->details->y;

	pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
    get_scaled_icon_size (item, &width, &height);
    icon_rectangle.x1 = icon_rectangle.x0 + width / pixels_per_unit;
    icon_rectangle.y1 = icon_rectangle.y0 + height / pixels_per_unit;

	measure_label_text (item);

	text_rectangle = compute_text_rectangle (item, icon_rectangle, FALSE,
						 for_layout ? BOUNDS_USAGE_FOR_LAYOUT : BOUNDS_USAGE_FOR_DISPLAY);

	ret.x0 = text_rectangle.x0;
	ret.y0 = text_rectangle.y0;
	ret.x1 = text_rectangle.x1;
	ret.y1 = text_rectangle.y1;

        eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                             &ret.x0,
                             &ret.y0);
        eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                             &ret.x1,
                             &ret.y1);

        return ret;
}


/* Get the rectangle of the icon only, in canvas coordinates. */
void
nemo_icon_canvas_item_get_icon_canvas_rectangle (NemoIconCanvasItem *item,
                                                 EelIRect *rect)
{
    gint width, height;

	g_assert (NEMO_IS_ICON_CANVAS_ITEM (item));
	g_assert (rect != NULL);

	eel_canvas_w2c (EEL_CANVAS_ITEM (item)->canvas,
			item->details->x,
			item->details->y,
			&rect->x0,
			&rect->y0);

    get_scaled_icon_size (item, &width, &height);

	rect->x1 = rect->x0 + width;
    rect->y1 = rect->y0 + height;
}

void
nemo_icon_canvas_item_set_show_stretch_handles (NemoIconCanvasItem *item,
						    gboolean show_stretch_handles)
{
	g_return_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (show_stretch_handles == FALSE || show_stretch_handles == TRUE);

	if (!item->details->show_stretch_handles == !show_stretch_handles) {
		return;
	}

	item->details->show_stretch_handles = show_stretch_handles;
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

/* Check if one of the stretch handles was hit. */
static gboolean
hit_test_stretch_handle (NemoIconCanvasItem *item,
			 EelIRect probe_canvas_rect,
			 GtkCornerType *corner)
{
	EelIRect icon_rect;
	GdkPixbuf *knob_pixbuf;
	int knob_width, knob_height;
	int hit_corner;

	g_assert (NEMO_IS_ICON_CANVAS_ITEM (item));

	/* Make sure there are handles to hit. */
	if (!item->details->show_stretch_handles) {
		return FALSE;
	}

	/* Quick check to see if the rect hits the icon at all. */
	icon_rect = item->details->canvas_rect;
	if (!eel_irect_hits_irect (probe_canvas_rect, icon_rect)) {
		return FALSE;
	}

	knob_pixbuf = get_knob_pixbuf ();
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);
	g_object_unref (knob_pixbuf);

	/* Check for hits in the stretch handles. */
	hit_corner = -1;
	if (probe_canvas_rect.x0 < icon_rect.x0 + knob_width) {
		if (probe_canvas_rect.y0 < icon_rect.y0 + knob_height)
			hit_corner = GTK_CORNER_TOP_LEFT;
		else if (probe_canvas_rect.y1 >= icon_rect.y1 - knob_height)
			hit_corner = GTK_CORNER_BOTTOM_LEFT;
	}
	else if (probe_canvas_rect.x1 >= icon_rect.x1 - knob_width) {
		if (probe_canvas_rect.y0 < icon_rect.y0 + knob_height)
			hit_corner = GTK_CORNER_TOP_RIGHT;
		else if (probe_canvas_rect.y1 >= icon_rect.y1 - knob_height)
			hit_corner = GTK_CORNER_BOTTOM_RIGHT;
	}
	if (corner)
		*corner = hit_corner;

	return hit_corner != -1;
}

gboolean
nemo_icon_canvas_item_hit_test_stretch_handles (NemoIconCanvasItem *item,
						    gdouble world_x,
						    gdouble world_y,
						    GtkCornerType *corner)
{
	EelIRect canvas_rect;

	g_return_val_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item), FALSE);

	eel_canvas_w2c (EEL_CANVAS_ITEM (item)->canvas,
			  world_x,
			  world_y,
			  &canvas_rect.x0,
			  &canvas_rect.y0);
	canvas_rect.x1 = canvas_rect.x0 + 1;
	canvas_rect.y1 = canvas_rect.y0 + 1;
	return hit_test_stretch_handle (item, canvas_rect, corner);
}

/* nemo_icon_canvas_item_hit_test_rectangle
 *
 * Check and see if there is an intersection between the item and the
 * canvas rect.
 */
gboolean
nemo_icon_canvas_item_hit_test_rectangle (NemoIconCanvasItem *item, EelIRect canvas_rect)
{
	g_return_val_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item), FALSE);

	return hit_test (item, canvas_rect);
}

const char *
nemo_icon_canvas_item_get_editable_text (NemoIconCanvasItem *icon_item)
{
	g_return_val_if_fail (NEMO_IS_ICON_CANVAS_ITEM (icon_item), NULL);

	return icon_item->details->editable_text;
}

void
nemo_icon_canvas_item_set_renaming (NemoIconCanvasItem *item, gboolean state)
{
	g_return_if_fail (NEMO_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (state == FALSE || state == TRUE);

	if (!item->details->is_renaming == !state) {
		return;
	}

	item->details->is_renaming = state;
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

double
nemo_icon_canvas_item_get_max_text_width (NemoIconCanvasItem *item)
{
    EelCanvasItem *canvas_item;
    NemoIconContainer *container;

    canvas_item = EEL_CANVAS_ITEM (item);
    container = NEMO_ICON_CONTAINER (canvas_item->canvas);

    if (container->details->label_position == NEMO_ICON_LABEL_POSITION_BESIDE) {
        if (container->details->layout_mode == NEMO_ICON_LAYOUT_T_B_L_R ||
            container->details->layout_mode == NEMO_ICON_LAYOUT_T_B_R_L) {
            /* compact view */
            if (container->details->all_columns_same_width) {
                return GET_VIEW_CONSTANT (container, max_text_width_beside_top_to_bottom) * canvas_item->canvas->pixels_per_unit;
            } else {
                return -1;
            }
        } else {
            /* normal icon view with labels-beside-icons */
            return GET_VIEW_CONSTANT (container, max_text_width_beside) * canvas_item->canvas->pixels_per_unit;
        }
    } else {
        /* normal icon view */
        if (container->details->is_desktop) {
            return nemo_get_desktop_text_width_for_zoom_level (nemo_icon_container_get_zoom_level (container));
        } else {
            return nemo_get_icon_text_width_for_zoom_level (nemo_icon_container_get_zoom_level (container));
        }
    }
}

void
nemo_icon_canvas_item_set_entire_text (NemoIconCanvasItem       *item,
					   gboolean                      entire_text)
{
	if (item->details->entire_text != entire_text) {
		item->details->entire_text = entire_text;

		nemo_icon_canvas_item_invalidate_label_size (item);
		eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
	}
}

gint
nemo_icon_canvas_item_get_fixed_text_height_for_layout (NemoIconCanvasItem *item)
{
    NemoIconContainer *container;
    PangoLayout *layout;
    gint line_height, total_height, lines;

    container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);

    if (nemo_icon_container_get_zoom_level (container) == NEMO_ZOOM_LEVEL_SMALLEST) {
        // No label/info, just a bit of padding.
        return 0;
    }

    lines = nemo_icon_container_get_max_layout_lines (container);
    lines += nemo_icon_container_get_additional_text_line_count (container);

    layout = create_label_layout (item, "-");
    pango_layout_get_pixel_size (layout, NULL, &line_height);

    total_height = (line_height * lines) + (LABEL_LINE_SPACING * (lines - 1));
    g_object_unref (layout);

    return total_height;
}

/* Class initialization function for the icon canvas item. */
static void
nemo_icon_canvas_item_class_init (NemoIconCanvasItemClass *class)
{
	GObjectClass *object_class;
	EelCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = EEL_CANVAS_ITEM_CLASS (class);

	object_class->finalize = nemo_icon_canvas_item_finalize;
	object_class->set_property = nemo_icon_canvas_item_set_property;
	object_class->get_property = nemo_icon_canvas_item_get_property;

        g_object_class_install_property (
		object_class,
		PROP_EDITABLE_TEXT,
		g_param_spec_string ("editable_text",
				     "editable text",
				     "the editable label",
				     "", G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_ADDITIONAL_TEXT,
		g_param_spec_string ("additional_text",
				     "additional text",
				     "some more text",
				     "", G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_SELECTION,
		g_param_spec_boolean ("highlighted_for_selection",
				      "highlighted for selection",
				      "whether we are highlighted for a selection",
				      FALSE, G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
		g_param_spec_boolean ("highlighted_as_keyboard_focus",
				      "highlighted as keyboard focus",
				      "whether we are highlighted to render keyboard focus",
				      FALSE, G_PARAM_READWRITE));


        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_DROP,
		g_param_spec_boolean ("highlighted_for_drop",
				      "highlighted for drop",
				      "whether we are highlighted for a D&D drop",
				      FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_CLIPBOARD,
		g_param_spec_boolean ("highlighted_for_clipboard",
				      "highlighted for clipboard",
				      "whether we are highlighted for a clipboard paste (after we have been cut)",
 				      FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (
        object_class,
        PROP_PINNED,
        g_param_spec_boolean ("pinned",
                              "pinned",
                              "backing file is pinned",
                      FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (
        object_class,
        PROP_FAV_UNAVAILABLE,
        g_param_spec_boolean ("fav-unavailable",
                              "fav-unavailable",
                              "backing file is a favorite and is not reachable",
                      FALSE, G_PARAM_READWRITE));

	item_class->update = nemo_icon_canvas_item_update;
	item_class->draw = nemo_icon_canvas_item_draw;
	item_class->point = nemo_icon_canvas_item_point;
	item_class->translate = nemo_icon_canvas_item_translate;
	item_class->bounds = nemo_icon_canvas_item_bounds;
	item_class->event = nemo_icon_canvas_item_event;

	atk_registry_set_factory_type (atk_get_default_registry (),
				       NEMO_TYPE_ICON_CANVAS_ITEM,
				       nemo_icon_canvas_item_accessible_factory_get_type ());

	g_type_class_add_private (class, sizeof (NemoIconCanvasItemDetails));
}

static GailTextUtil *
nemo_icon_canvas_item_get_text (GObject *text)
{
	return NEMO_ICON_CANVAS_ITEM (text)->details->text_util;
}

static void
nemo_icon_canvas_item_text_interface_init (EelAccessibleTextIface *iface)
{
	iface->get_text = nemo_icon_canvas_item_get_text;
}

/* ============================= a11y interfaces =========================== */

static const char *nemo_icon_canvas_item_accessible_action_names[] = {
        "open",
        "menu",
        NULL
};

static const char *nemo_icon_canvas_item_accessible_action_descriptions[] = {
        "Open item",
        "Popup context menu",
        NULL
};

enum {
	ACTION_OPEN,
	ACTION_MENU,
	LAST_ACTION
};

typedef struct {
        char *action_descriptions[LAST_ACTION];
	char *image_description;
	char *description;
} NemoIconCanvasItemAccessiblePrivate;

typedef struct {
	NemoIconCanvasItem *item;
	gint action_number;
} NemoIconCanvasItemAccessibleActionContext;

static GType nemo_icon_canvas_item_accessible_get_type (void);

#define GET_PRIV(o) \
	G_TYPE_INSTANCE_GET_PRIVATE(o,\
				    nemo_icon_canvas_item_accessible_get_type (),\
				    NemoIconCanvasItemAccessiblePrivate);

/* accessible AtkAction interface */
static gboolean
nemo_icon_canvas_item_accessible_idle_do_action (gpointer data)
{
	NemoIconCanvasItem *item;
	NemoIconCanvasItemAccessibleActionContext *ctx;
	NemoIcon *icon;
	NemoIconContainer *container;
	GList* selection;
	GList file_list;
        GdkEventButton button_event = { 0 };
	gint action_number;

	container = NEMO_ICON_CONTAINER (data);
	container->details->a11y_item_action_idle_handler = 0;
	while (!g_queue_is_empty (container->details->a11y_item_action_queue)) {
		ctx = g_queue_pop_head (container->details->a11y_item_action_queue);
		action_number = ctx->action_number;
		item = ctx->item;
		g_free (ctx);
		icon = item->user_data;

		switch (action_number) {
		case ACTION_OPEN:
			file_list.data = icon->data;
			file_list.next = NULL;
			file_list.prev = NULL;
        		g_signal_emit_by_name (container, "activate", &file_list);
			break;
		case ACTION_MENU:
			selection = nemo_icon_container_get_selection (container);
			if (selection == NULL ||
			    g_list_length (selection) != 1 ||
 			    selection->data != icon->data)  {
				g_list_free (selection);
				return FALSE;
			}
			g_list_free (selection);
        		g_signal_emit_by_name (container, "context_click_selection", &button_event);
			break;
		default :
			g_assert_not_reached ();
			break;
		}
	}
	return FALSE;
}

static gboolean
nemo_icon_canvas_item_accessible_do_action (AtkAction *accessible,
						int i)
{
	NemoIconCanvasItem *item;
	NemoIconCanvasItemAccessibleActionContext *ctx;
	NemoIconContainer *container;

	g_assert (i < LAST_ACTION);

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		return FALSE;
	}

	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	switch (i) {
	case ACTION_OPEN:
	case ACTION_MENU:
		if (container->details->a11y_item_action_queue == NULL) {
			container->details->a11y_item_action_queue = g_queue_new ();
		}
		ctx = g_new (NemoIconCanvasItemAccessibleActionContext, 1);
		ctx->action_number = i;
		ctx->item = item;
		g_queue_push_head (container->details->a11y_item_action_queue, ctx);
		if (container->details->a11y_item_action_idle_handler == 0) {
			container->details->a11y_item_action_idle_handler = g_idle_add (nemo_icon_canvas_item_accessible_idle_do_action, container);
		}
		break;
        default :
                g_warning ("Invalid action passed to NemoIconCanvasItemAccessible::do_action");
                return FALSE;
        }

	return TRUE;
}

static int
nemo_icon_canvas_item_accessible_get_n_actions (AtkAction *accessible)
{
	return LAST_ACTION;
}

static const char *
nemo_icon_canvas_item_accessible_action_get_description (AtkAction *accessible,
                                                             int i)
{
	NemoIconCanvasItemAccessiblePrivate *priv;

	g_assert (i < LAST_ACTION);

	priv = GET_PRIV (accessible);

	if (priv->action_descriptions[i]) {
		return priv->action_descriptions[i];
	} else {
		return nemo_icon_canvas_item_accessible_action_descriptions[i];
	}
}

static const char *
nemo_icon_canvas_item_accessible_action_get_name (AtkAction *accessible, int i)
{
	g_assert (i < LAST_ACTION);

	return nemo_icon_canvas_item_accessible_action_names[i];
}

static const char *
nemo_icon_canvas_item_accessible_action_get_keybinding (AtkAction *accessible,
							    int i)
{
	g_assert (i < LAST_ACTION);

	return NULL;
}

static gboolean
nemo_icon_canvas_item_accessible_action_set_description (AtkAction *accessible,
							     int i,
							     const char *description)
{
	NemoIconCanvasItemAccessiblePrivate *priv;

	g_assert (i < LAST_ACTION);

	priv = GET_PRIV (accessible);

	if (priv->action_descriptions[i]) {
		g_free (priv->action_descriptions[i]);
	}
	priv->action_descriptions[i] = g_strdup (description);

	return TRUE;
}

static void
nemo_icon_canvas_item_accessible_action_interface_init (AtkActionIface *iface)
{
	iface->do_action = nemo_icon_canvas_item_accessible_do_action;
	iface->get_n_actions = nemo_icon_canvas_item_accessible_get_n_actions;
	iface->get_description = nemo_icon_canvas_item_accessible_action_get_description;
	iface->get_keybinding = nemo_icon_canvas_item_accessible_action_get_keybinding;
	iface->get_name = nemo_icon_canvas_item_accessible_action_get_name;
	iface->set_description = nemo_icon_canvas_item_accessible_action_set_description;
}

static const gchar *
nemo_icon_canvas_item_accessible_get_name (AtkObject *accessible)
{
	NemoIconCanvasItem *item;

	if (accessible->name) {
		return accessible->name;
	}

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		return NULL;
	}
	return item->details->editable_text;
}

static const gchar*
nemo_icon_canvas_item_accessible_get_description (AtkObject *accessible)
{
	NemoIconCanvasItem *item;

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		return NULL;
	}

	return item->details->additional_text;
}

static AtkObject *
nemo_icon_canvas_item_accessible_get_parent (AtkObject *accessible)
{
	NemoIconCanvasItem *item;

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		return NULL;
	}

	return gtk_widget_get_accessible (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas));
}

static int
nemo_icon_canvas_item_accessible_get_index_in_parent (AtkObject *accessible)
{
	NemoIconCanvasItem *item;
	NemoIconContainer *container;
	GList *l;
	NemoIcon *icon;
	int i;

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		return -1;
	}

	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);

	l = container->details->icons;
	i = 0;
	while (l) {
		icon = l->data;

		if (icon->item == item) {
			return i;
		}

		i++;
		l = l->next;
	}

	return -1;
}

static const gchar *
nemo_icon_canvas_item_accessible_get_image_description (AtkImage *image)
{
	NemoIconCanvasItemAccessiblePrivate *priv;
	NemoIconCanvasItem *item;
	NemoIcon *icon;
	NemoIconContainer *container;
	char *description;

	priv = GET_PRIV (image);

	if (priv->image_description) {
		return priv->image_description;
	} else {
		item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (image)));
		if (item == NULL) {
			return NULL;
		}
		icon = item->user_data;
		container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
		description = nemo_icon_container_get_icon_description (container, icon->data);
		g_free (priv->description);
		priv->description = description;
		return priv->description;
	}
}

static void
nemo_icon_canvas_item_accessible_get_image_size
	(AtkImage *image,
	 gint     *width,
	 gint     *height)
{
	NemoIconCanvasItem *item;

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (image)));

    get_scaled_icon_size (item, width, height);
}

static void
nemo_icon_canvas_item_accessible_get_image_position
	(AtkImage		 *image,
	 gint                    *x,
	 gint	                 *y,
	 AtkCoordType	         coord_type)
{
	NemoIconCanvasItem *item;
	gint x_offset, y_offset, itmp;

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (image)));
	if (!item) {
		return;
	}
	if (!item->details->canvas_rect.x0 && !item->details->canvas_rect.x1) {
		return;
	} else {
		x_offset = 0;
		y_offset = 0;
		if (item->details->text_width) {
			itmp = item->details->canvas_rect.x0 -
			       item->details->text_rect.x0;
			if (itmp > x_offset) {
				x_offset = itmp;
			}
			itmp = item->details->canvas_rect.y0 -
			       item->details->text_rect.y0;
			if (itmp > y_offset) {
				y_offset = itmp;
			}
		}
	}
	atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
	*x += x_offset;
	*y += y_offset;
}

static gboolean
nemo_icon_canvas_item_accessible_set_image_description (AtkImage    *image,
							    const gchar *description)
{
	NemoIconCanvasItemAccessiblePrivate *priv;

	priv = GET_PRIV (image);

	g_free (priv->image_description);
	priv->image_description = g_strdup (description);

	return TRUE;
}

static void
nemo_icon_canvas_item_accessible_image_interface_init (AtkImageIface *iface)
{
	iface->get_image_description = nemo_icon_canvas_item_accessible_get_image_description;
	iface->set_image_description = nemo_icon_canvas_item_accessible_set_image_description;
	iface->get_image_size        = nemo_icon_canvas_item_accessible_get_image_size;
	iface->get_image_position    = nemo_icon_canvas_item_accessible_get_image_position;
}

/* accessible text interface */
static gint
nemo_icon_canvas_item_accessible_get_offset_at_point (AtkText	 *text,
                                                          gint           x,
                                                          gint           y,
                                                          AtkCoordType coords)
{
	gint real_x, real_y, real_width, real_height;
	NemoIconCanvasItem *item;
	gint editable_height;
	gint offset = 0;
	gint index;
	PangoLayout *layout, *editable_layout, *additional_layout;
	PangoRectangle rect0;
	char *icon_text;
	gboolean have_editable;
	gboolean have_additional;
	gint text_offset, height;

	atk_component_get_extents (ATK_COMPONENT (text), &real_x, &real_y,
                                   &real_width, &real_height, coords);

	x -= real_x;
	y -= real_y;

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));

	if (item->details->pixbuf) {
        get_scaled_icon_size (item, NULL, &height);
        y -= height;
	}
	have_editable = item->details->editable_text != NULL &&
			item->details->editable_text[0] != '\0';
	have_additional = item->details->additional_text != NULL &&item->details->additional_text[0] != '\0';

	editable_layout = NULL;
	additional_layout = NULL;
	if (have_editable) {
		editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
		prepare_pango_layout_for_draw (item, editable_layout);
		pango_layout_get_pixel_size (editable_layout, NULL, &editable_height);
		if (y >= editable_height &&
                    have_additional) {
			prepare_pango_layout_for_draw (item, editable_layout);
			additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
			layout = additional_layout;
			icon_text = item->details->additional_text;
			y -= editable_height + LABEL_LINE_SPACING;
		} else {
			layout = editable_layout;
			icon_text = item->details->editable_text;
		}
	} else if (have_additional) {
		additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);
		prepare_pango_layout_for_draw (item, additional_layout);
		layout = additional_layout;
		icon_text = item->details->additional_text;
	} else {
		return 0;
	}

	text_offset = 0;
	if (have_editable) {
		pango_layout_index_to_pos (editable_layout, 0, &rect0);
		text_offset = PANGO_PIXELS (rect0.x);
	}
	if (have_additional) {
		gint itmp;

		pango_layout_index_to_pos (additional_layout, 0, &rect0);
		itmp = PANGO_PIXELS (rect0.x);
		if (itmp < text_offset) {
			text_offset = itmp;
		}
	}
	pango_layout_index_to_pos (layout, 0, &rect0);
	x += text_offset;
	if (!pango_layout_xy_to_index (layout,
                                       x * PANGO_SCALE,
                                       y * PANGO_SCALE,
                                       &index, NULL)) {
		if (x < 0 || y < 0) {
			index = 0;
		} else {
			index = -1;
		}
	}
	if (index == -1) {
		offset = g_utf8_strlen (icon_text, -1);
	} else {
		offset = g_utf8_pointer_to_offset (icon_text, icon_text + index);
	}
	if (layout == additional_layout) {
		offset += g_utf8_strlen (item->details->editable_text, -1);
	}

	if (editable_layout != NULL) {
		g_object_unref (editable_layout);
	}

	if (additional_layout != NULL) {
		g_object_unref (additional_layout);
	}

	return offset;
}

static void
nemo_icon_canvas_item_accessible_get_character_extents (AtkText	   *text,
                                                            gint	   offset,
                                                            gint	   *x,
                                                            gint	   *y,
                                                            gint	   *width,
                                                            gint	   *height,
                                                            AtkCoordType coords)
{
	gint pos_x, pos_y;
	gint len, byte_offset;
	gint editable_height;
	gchar *icon_text;
	NemoIconCanvasItem *item;
	PangoLayout *layout, *editable_layout, *additional_layout;
	PangoRectangle rect;
	PangoRectangle rect0;
	gboolean have_editable;
	gint text_offset, pix_height;

	atk_component_get_position (ATK_COMPONENT (text), &pos_x, &pos_y, coords);
	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (text)));

	if (item->details->pixbuf) {
		get_scaled_icon_size (item, NULL, &pix_height);
        pos_y += pix_height;
	}

	have_editable = item->details->editable_text != NULL &&
			item->details->editable_text[0] != '\0';
	if (have_editable) {
		len = g_utf8_strlen (item->details->editable_text, -1);
	} else {
		len = 0;
	}

	editable_layout = get_label_layout (&item->details->editable_text_layout, item, item->details->editable_text);
	additional_layout = get_label_layout (&item->details->additional_text_layout, item, item->details->additional_text);

	if (offset < len) {
		icon_text = item->details->editable_text;
		layout = editable_layout;
	} else {
		offset -= len;
		icon_text = item->details->additional_text;
		layout = additional_layout;
		pos_y += LABEL_LINE_SPACING;
		if (have_editable) {
			pango_layout_get_pixel_size (editable_layout, NULL, &editable_height);
			pos_y += editable_height;
		}
	}
	byte_offset = g_utf8_offset_to_pointer (icon_text, offset) - icon_text;
	pango_layout_index_to_pos (layout, byte_offset, &rect);
	text_offset = 0;
	if (have_editable) {
		pango_layout_index_to_pos (editable_layout, 0, &rect0);
		text_offset = PANGO_PIXELS (rect0.x);
	}
	if (item->details->additional_text != NULL &&
	    item->details->additional_text[0] != '\0') {
		gint itmp;

		pango_layout_index_to_pos (additional_layout, 0, &rect0);
		itmp = PANGO_PIXELS (rect0.x);
		if (itmp < text_offset) {
			text_offset = itmp;
		}
	}

	g_object_unref (editable_layout);
	g_object_unref (additional_layout);

	*x = pos_x + PANGO_PIXELS (rect.x) - text_offset;
	*y = pos_y + PANGO_PIXELS (rect.y);
	*width = PANGO_PIXELS (rect.width);
	*height = PANGO_PIXELS (rect.height);
}

static void
nemo_icon_canvas_item_accessible_text_interface_init (AtkTextIface *iface)
{
 	iface->get_text                = eel_accessibility_text_get_text;
	iface->get_character_at_offset = eel_accessibility_text_get_character_at_offset;
        iface->get_text_before_offset  = eel_accessibility_text_get_text_before_offset;
        iface->get_text_at_offset      = eel_accessibility_text_get_text_at_offset;
   	iface->get_text_after_offset   = eel_accessibility_text_get_text_after_offset;
      	iface->get_character_count     = eel_accessibility_text_get_character_count;
	iface->get_character_extents   = nemo_icon_canvas_item_accessible_get_character_extents;
	iface->get_offset_at_point     = nemo_icon_canvas_item_accessible_get_offset_at_point;
}

typedef struct {
	AtkGObjectAccessible parent;
} NemoIconCanvasItemAccessible;

typedef struct {
	AtkGObjectAccessibleClass parent_class;
} NemoIconCanvasItemAccessibleClass;

G_DEFINE_TYPE_WITH_CODE (NemoIconCanvasItemAccessible,
			 nemo_icon_canvas_item_accessible,
			 ATK_TYPE_GOBJECT_ACCESSIBLE,
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE,
						nemo_icon_canvas_item_accessible_image_interface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT,
						nemo_icon_canvas_item_accessible_text_interface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION,
						nemo_icon_canvas_item_accessible_action_interface_init));

static AtkStateSet*
nemo_icon_canvas_item_accessible_ref_state_set (AtkObject *accessible)
{
	AtkStateSet *state_set;
	NemoIconCanvasItem *item;
	NemoIconContainer *container;
	NemoIcon *icon;
	GList *l;
	gboolean one_item_selected;

	state_set = ATK_OBJECT_CLASS (nemo_icon_canvas_item_accessible_parent_class)->ref_state_set (accessible);

	item = NEMO_ICON_CANVAS_ITEM (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));
	if (!item) {
		atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
		return state_set;
	}
	container = NEMO_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	if (item->details->is_highlighted_as_keyboard_focus) {
		atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
	} else if (!container->details->keyboard_focus) {

		one_item_selected = FALSE;
		l = container->details->icons;
		while (l) {
			icon = l->data;

			if (icon->item == item) {
				if (icon->is_selected) {
					one_item_selected = TRUE;
				} else {
					break;
				}
			} else if (icon->is_selected) {
				one_item_selected = FALSE;
				break;
			}

			l = l->next;
		}

		if (one_item_selected) {
			atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
		}
	}

	return state_set;
}

static void
nemo_icon_canvas_item_accessible_finalize (GObject *object)
{
	NemoIconCanvasItemAccessiblePrivate *priv;
	int i;

	priv = GET_PRIV (object);

	for (i = 0; i < LAST_ACTION; i++) {
		g_free (priv->action_descriptions[i]);
	}
	g_free (priv->image_description);
	g_free (priv->description);

        G_OBJECT_CLASS (nemo_icon_canvas_item_accessible_parent_class)->finalize (object);
}

static void
nemo_icon_canvas_item_accessible_initialize (AtkObject *accessible,
						 gpointer widget)
{
	ATK_OBJECT_CLASS (nemo_icon_canvas_item_accessible_parent_class)->initialize (accessible, widget);

	atk_object_set_role (accessible, ATK_ROLE_ICON);
}

static void
nemo_icon_canvas_item_accessible_class_init (NemoIconCanvasItemAccessibleClass *klass)
{
	AtkObjectClass *aclass = ATK_OBJECT_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->finalize = nemo_icon_canvas_item_accessible_finalize;

	aclass->initialize = nemo_icon_canvas_item_accessible_initialize;

	aclass->get_name = nemo_icon_canvas_item_accessible_get_name;
	aclass->get_description = nemo_icon_canvas_item_accessible_get_description;
	aclass->get_parent = nemo_icon_canvas_item_accessible_get_parent;
	aclass->get_index_in_parent = nemo_icon_canvas_item_accessible_get_index_in_parent;
	aclass->ref_state_set = nemo_icon_canvas_item_accessible_ref_state_set;

	g_type_class_add_private (klass, sizeof (NemoIconCanvasItemAccessiblePrivate));
}

static void
nemo_icon_canvas_item_accessible_init (NemoIconCanvasItemAccessible *self)
{
}

/* dummy typedef */
typedef AtkObjectFactory      NemoIconCanvasItemAccessibleFactory;
typedef AtkObjectFactoryClass NemoIconCanvasItemAccessibleFactoryClass;

G_DEFINE_TYPE (NemoIconCanvasItemAccessibleFactory, nemo_icon_canvas_item_accessible_factory,
	       ATK_TYPE_OBJECT_FACTORY);

static AtkObject *
nemo_icon_canvas_item_accessible_factory_create_accessible (GObject *for_object)
{
	AtkObject *accessible;
	NemoIconCanvasItem *item;
	GString *item_text;

	item = NEMO_ICON_CANVAS_ITEM (for_object);
	g_assert (item != NULL);

	item_text = g_string_new (NULL);
	if (item->details->editable_text) {
        	g_string_append (item_text, item->details->editable_text);
	}
	if (item->details->additional_text) {
        	g_string_append (item_text, item->details->additional_text);
	}

	item->details->text_util = gail_text_util_new ();
	gail_text_util_text_setup (item->details->text_util,
				   item_text->str);
	g_string_free (item_text, TRUE);

	accessible = g_object_new (nemo_icon_canvas_item_accessible_get_type (), NULL);
	atk_object_initialize (accessible, for_object);

	return accessible;
}

static GType
nemo_icon_canvas_item_accessible_factory_get_accessible_type (void)
{
	return nemo_icon_canvas_item_accessible_get_type ();
}

static void
nemo_icon_canvas_item_accessible_factory_init (NemoIconCanvasItemAccessibleFactory *self)
{
}

static void
nemo_icon_canvas_item_accessible_factory_class_init (NemoIconCanvasItemAccessibleFactoryClass *klass)
{
	klass->create_accessible = nemo_icon_canvas_item_accessible_factory_create_accessible;
	klass->get_accessible_type = nemo_icon_canvas_item_accessible_factory_get_accessible_type;
}
