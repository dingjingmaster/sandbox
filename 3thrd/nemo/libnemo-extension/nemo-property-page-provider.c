/*
 *  nemo-property-page-provider.c - Interface for Nemo extensions
 *                                      that provide property pages for 
 *                                      files.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include "../config.h"
#include "nemo-property-page-provider.h"

#include <glib-object.h>

G_DEFINE_INTERFACE (NemoPropertyPageProvider, nemo_property_page_provider, G_TYPE_OBJECT)

/**
 * SECTION:nemo-property-page-provider
 * @Title: NemoPropertyPageProvider
 * @Short_description: Allows additional file property pages to be added.
 *
 * This interface allows you to provide additional property pages for the right-click
 * Properties menu item.  A new widget is requested each time the property page is opened.
 **/

static void
nemo_property_page_provider_default_init (NemoPropertyPageProviderInterface *klass)
{
}

/**
 * nemo_property_page_provider_get_pages:
 * @provider: a #NemoPropertyPageProvider
 * @files: (element-type NemoFileInfo): a #GList of #NemoFileInfo
 *
 * This function is called by Nemo when it wants property page
 * items from the extension.
 *
 * This function is called in the main thread before a property page
 * is shown, so it should return quickly.
 *
 * Returns: (element-type NemoPropertyPage) (transfer full): A #GList of allocated #NemoPropertyPage items.
 */
GList *
nemo_property_page_provider_get_pages (NemoPropertyPageProvider *provider,
					   GList *files)
{
	g_return_val_if_fail (NEMO_IS_PROPERTY_PAGE_PROVIDER (provider), NULL);
	g_return_val_if_fail (NEMO_PROPERTY_PAGE_PROVIDER_GET_IFACE (provider)->get_pages != NULL, NULL);

	return NEMO_PROPERTY_PAGE_PROVIDER_GET_IFACE (provider)->get_pages 
		(provider, files);	
}

					       
