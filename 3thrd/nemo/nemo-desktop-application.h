/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-application: main Nemo application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __NEMO_DESKTOP_APPLICATION_H__
#define __NEMO_DESKTOP_APPLICATION_H__

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "libnemo-private/nemo-undo-manager.h"

#include "nemo-window.h"
#include "nemo-application.h"

#define NEMO_TYPE_DESKTOP_APPLICATION nemo_desktop_application_get_type()
#define NEMO_DESKTOP_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_APPLICATION, NemoDesktopApplication))
#define NEMO_DESKTOP_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_APPLICATION, NemoDesktopApplicationClass))
#define NEMO_IS_DESKTOP_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_APPLICATION))
#define NEMO_IS_DESKTOP_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_APPLICATION))
#define NEMO_DESKTOP_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_APPLICATION, NemoDesktopApplicationClass))

typedef struct _NemoDesktopApplicationPriv NemoDesktopApplicationPriv;

typedef struct {
	NemoApplication parent;

	NemoDesktopApplicationPriv *priv;
} NemoDesktopApplication;

typedef struct {
	NemoApplicationClass parent_class;
} NemoDesktopApplicationClass;

GType nemo_desktop_application_get_type (void);

NemoApplication *nemo_desktop_application_get_singleton (void);

#endif /* __NEMO_DESKTOP_APPLICATION_H__ */
