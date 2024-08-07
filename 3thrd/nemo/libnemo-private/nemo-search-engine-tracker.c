/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 */

#include "../config.h"
#include "nemo-search-engine-tracker.h"
#include <gmodule.h>
#include <string.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

/* If defined, we use fts:match, this has to be enabled in Tracker to
 * work which it usually is. The alternative is to undefine it and
 * use filename matching instead. This doesn't use the content of the
 * file however.
 */
#undef FTS_MATCHING

struct NemoSearchEngineTrackerDetails {
	TrackerSparqlConnection *connection;
	NemoQuery *query;

	gboolean       query_pending;
	GCancellable  *cancellable;
};

G_DEFINE_TYPE (NemoSearchEngineTracker,
	       nemo_search_engine_tracker,
	       NEMO_TYPE_SEARCH_ENGINE);

static void
finalize (GObject *object)
{
	NemoSearchEngineTracker *tracker;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (object);

	g_clear_object (&tracker->details->query);
	g_clear_object (&tracker->details->cancellable);
	g_clear_object (&tracker->details->connection);

	G_OBJECT_CLASS (nemo_search_engine_tracker_parent_class)->finalize (object);
}


/* stolen from tracker sources, tracker.c */
static void
sparql_append_string_literal (GString     *sparql,
                              const gchar *str)
{
	char *s;

	s = tracker_sparql_escape_string (str);

	g_string_append_c (sparql, '"');
	g_string_append (sparql, s);
	g_string_append_c (sparql, '"');

	g_free (s);
}

static void cursor_callback (GObject      *object,
			     GAsyncResult *result,
			     gpointer      user_data);

static void
cursor_next (NemoSearchEngineTracker *tracker,
             TrackerSparqlCursor    *cursor)
{
	tracker_sparql_cursor_next_async (cursor,
	                                  tracker->details->cancellable,
	                                  cursor_callback,
	                                  tracker);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	NemoSearchEngineTracker *tracker;
    FileSearchResult *fsr = NULL;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (user_data);

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (error) {
		tracker->details->query_pending = FALSE;
		nemo_search_engine_error (NEMO_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		g_object_unref (cursor);

		return;
	}

	if (!success) {
		tracker->details->query_pending = FALSE;
		nemo_search_engine_finished (NEMO_SEARCH_ENGINE (tracker));
		g_object_unref (cursor);

		return;
	}

	/* We iterate result by result, not n at a time. */

    fsr = file_search_result_new (g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL)), NULL);
    hits = g_list_append (NULL, fsr);
    nemo_search_engine_hits_added (NEMO_SEARCH_ENGINE (tracker), hits);
    g_list_free (hits);

	/* Get next */
	cursor_next (tracker, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
	NemoSearchEngineTracker *tracker;
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (user_data);

	connection = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (connection,
	                                                 result,
	                                                 &error);

	if (error) {
		tracker->details->query_pending = FALSE;
		nemo_search_engine_error (NEMO_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		return;
	}

	if (!cursor) {
		tracker->details->query_pending = FALSE;
		nemo_search_engine_finished (NEMO_SEARCH_ENGINE (tracker));
		return;
	}

	cursor_next (tracker, cursor);
}

static void
nemo_search_engine_tracker_start (NemoSearchEngine *engine)
{
	NemoSearchEngineTracker *tracker;
	gchar	*search_text, *location_uri;
	GString *sparql;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (engine);

	if (tracker->details->query_pending) {
		return;
	}

	if (tracker->details->query == NULL) {
		return;
	}

	g_cancellable_reset (tracker->details->cancellable);

	search_text = nemo_query_get_file_pattern (tracker->details->query);
	location_uri = nemo_query_get_location (tracker->details->query);

// FIXME: Re-implement content search, align capabilities with normal search and
//        make switching a run-time, not build-time option.

#if TRACKER_CHECK_VERSION (3, 0, 0)


	sparql = g_string_new ("SELECT ?url "
			       "WHERE {"
			       "  ?file a nfo:FileDataObject ; nie:url ?url; nfo:fileName ?fileName; .");

	g_string_append (sparql, " FILTER (contains(?fileName,");

	sparql_append_string_literal (sparql, search_text);

	g_string_append (sparql, ")");

	if (location_uri)  {
		g_string_append (sparql, " && fn:starts-with(?url,");
		sparql_append_string_literal (sparql, location_uri);
		g_string_append (sparql, ")");
	}

	g_string_append (sparql, ")");

	g_string_append (sparql, 
			 "} ORDER BY DESC(?url) DESC(?filename)");


#else /* TRACKER_CHECK_VERSION */


    sparql = g_string_new ("SELECT nie:url(?urn) "
                   "WHERE {"
                   "  ?urn a nfo:FileDataObject ;");

    g_string_append (sparql, "    tracker:available true ."
             "  FILTER (fn:contains(nfo:fileName(?urn),");

    sparql_append_string_literal (sparql, search_text);

    g_string_append (sparql, ")");

    if (location_uri)  {
        g_string_append (sparql, " && fn:starts-with(nie:url(?urn),");
        sparql_append_string_literal (sparql, location_uri);
        g_string_append (sparql, ")");
    }

    g_string_append (sparql, ")");

    g_string_append (sparql, 
             "} ORDER BY DESC(nie:url(?urn)) DESC(nfo:fileName(?urn))");


#endif /* TRACKER_CHECK_VERSION */

	tracker_sparql_connection_query_async (tracker->details->connection,
					       sparql->str,
					       tracker->details->cancellable,
					       query_callback,
					       tracker);
	g_string_free (sparql, TRUE);

	tracker->details->query_pending = TRUE;

	g_free (search_text);
	g_free (location_uri);
}

static void
nemo_search_engine_tracker_stop (NemoSearchEngine *engine)
{
	NemoSearchEngineTracker *tracker;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (engine);
	
	if (tracker->details->query && tracker->details->query_pending) {
		g_cancellable_cancel (tracker->details->cancellable);
		tracker->details->query_pending = FALSE;
	}
}

static void
nemo_search_engine_tracker_set_query (NemoSearchEngine *engine, NemoQuery *query)
{
	NemoSearchEngineTracker *tracker;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (engine);

	if (query) {
		g_object_ref (query);
	}

	if (tracker->details->query) {
		g_object_unref (tracker->details->query);
	}

	tracker->details->query = query;
}

static void
nemo_search_engine_tracker_class_init (NemoSearchEngineTrackerClass *class)
{
	GObjectClass *gobject_class;
	NemoSearchEngineClass *engine_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NEMO_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nemo_search_engine_tracker_set_query;
	engine_class->start = nemo_search_engine_tracker_start;
	engine_class->stop = nemo_search_engine_tracker_stop;

	g_type_class_add_private (class, sizeof (NemoSearchEngineTrackerDetails));
}

static void
nemo_search_engine_tracker_init (NemoSearchEngineTracker *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NEMO_TYPE_SEARCH_ENGINE_TRACKER,
						       NemoSearchEngineTrackerDetails);
}


NemoSearchEngine *
nemo_search_engine_tracker_new (void)
{
	NemoSearchEngineTracker *engine;
	TrackerSparqlConnection *connection;
	GError *error = NULL;

#if TRACKER_CHECK_VERSION(3, 0, 0)
    connection = tracker_sparql_connection_bus_new ("org.freedesktop.Tracker3.Miner.Files", NULL, NULL, &error);
#else
    connection = tracker_sparql_connection_get (NULL, &error);
#endif

	if (error) {
		g_warning ("Could not establish a connection to Tracker: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE_TRACKER, NULL);
	engine->details->connection = connection;
	engine->details->cancellable = g_cancellable_new ();

	return NEMO_SEARCH_ENGINE (engine);
}
