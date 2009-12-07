/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2009  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "connman.h"

struct connman_dhcp {
	gint refcount;
	int index;
	enum connman_dhcp_state state;

	struct connman_element *element;

	struct connman_dhcp_driver *driver;
	void *driver_data;
};

/**
 * connman_dhcp_ref:
 * @dhcp: DHCP structure
 *
 * Increase reference counter of DHCP
 */
struct connman_dhcp *connman_dhcp_ref(struct connman_dhcp *dhcp)
{
	g_atomic_int_inc(&dhcp->refcount);

	return dhcp;
}

/**
 * connman_dhcp_unref:
 * @dhcp: DHCP structure
 *
 * Decrease reference counter of DHCP
 */
void connman_dhcp_unref(struct connman_dhcp *dhcp)
{
	if (g_atomic_int_dec_and_test(&dhcp->refcount) == TRUE)
		g_free(dhcp);
}

/**
 * connman_dhcp_get_interface:
 * @dhcp: DHCP structure
 *
 * Get network interface of DHCP
 */
char *connman_dhcp_get_interface(struct connman_dhcp *dhcp)
{
	return connman_inet_ifname(dhcp->index);
}

/**
 * connman_dhcp_bound:
 * @dhcp: DHCP structure
 *
 * Report successful bound of the interface
 */
void connman_dhcp_bound(struct connman_dhcp *dhcp)
{
	DBG("dhcp %p", dhcp);
}

static GSList *driver_list = NULL;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_dhcp_driver *driver1 = a;
	const struct connman_dhcp_driver *driver2 = b;

	return driver2->priority - driver1->priority;
}

/**
 * connman_dhcp_driver_register:
 * @driver: DHCP driver definition
 *
 * Register a new DHCP driver
 *
 * Returns: %0 on success
 */
int connman_dhcp_driver_register(struct connman_dhcp_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_insert_sorted(driver_list, driver,
							compare_priority);

	return 0;
}

/**
 * connman_dhcp_driver_unregister:
 * @driver: DHCP driver definition
 *
 * Remove a previously registered DHCP driver
 */
void connman_dhcp_driver_unregister(struct connman_dhcp_driver *driver)
{
	DBG("driver %p name %s", driver, driver->name);

	driver_list = g_slist_remove(driver_list, driver);
}

static int dhcp_probe(struct connman_element *element)
{
	struct connman_dhcp *dhcp;
	GSList *list;

	DBG("element %p name %s", element, element->name);

	dhcp = g_try_new0(struct connman_dhcp, 1);
	if (dhcp == NULL)
		return -ENOMEM;

	dhcp->refcount = 1;
	dhcp->index = element->index;
	dhcp->state = CONNMAN_DHCP_STATE_IDLE;

	dhcp->element = element;

	connman_element_set_data(element, dhcp);

	for (list = driver_list; list; list = list->next) {
		struct connman_dhcp_driver *driver = list->data;

		DBG("driver %p name %s", driver, driver->name);

		if (driver->request(dhcp) == 0) {
			dhcp->driver = driver;
			break;
		}
	}

	if (dhcp->driver == NULL) {
		connman_dhcp_unref(dhcp);
		return -ENOENT;
	}

	return 0;
}

static void dhcp_remove(struct connman_element *element)
{
	struct connman_dhcp *dhcp = connman_element_get_data(element);

	DBG("element %p name %s", element, element->name);

	connman_element_set_data(element, NULL);

	if (dhcp->driver) {
		dhcp->driver->release(dhcp);
		dhcp->driver = NULL;
	}

	connman_dhcp_unref(dhcp);
}

static struct connman_driver dhcp_driver = {
	.name		= "dhcp",
	.type		= CONNMAN_ELEMENT_TYPE_DHCP,
	.priority	= CONNMAN_DRIVER_PRIORITY_LOW,
	.probe		= dhcp_probe,
	.remove		= dhcp_remove,
};

int __connman_dhcp_init(void)
{
	return connman_driver_register(&dhcp_driver);
}

void __connman_dhcp_cleanup(void)
{
	connman_driver_unregister(&dhcp_driver);
}