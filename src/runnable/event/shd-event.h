/*
 * The Shadow Simulator
 *
 * Copyright (c) 2010-2012 Rob Jansen <jansen@cs.umn.edu>
 *
 * This file is part of Shadow.
 *
 * Shadow is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Shadow is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Shadow.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SHD_EVENT_H_
#define SHD_EVENT_H_

#include "shadow.h"

typedef struct _Event Event;
typedef struct _EventFunctionTable EventFunctionTable;

/* required functions */
typedef void (*EventRunFunc)(Event* event, gpointer node); /* XXX: type is "Node*" */
typedef void (*EventFreeFunc)(Event* event);

/*
 * Virtual function table for base event, storing pointers to required
 * callable functions.
 */
struct _EventFunctionTable {
	EventRunFunc run;
	EventFreeFunc free;
	MAGIC_DECLARE;
};

/*
 * A basic event connected to a specific node. This is meant
 * to be extended by most other events.
 */
struct _Event {
	Runnable super;
	EventFunctionTable* vtable;
	SimulationTime time;
	gpointer node; /* XXX: type is "Node*" */

	GQuark ownerID;
	MAGIC_DECLARE;
};

void shadowevent_init(Event* event, EventFunctionTable* vtable);
gboolean shadowevent_run(Event* event);
gint shadowevent_compare(const Event* a, const Event* b, gpointer user_data);
void shadowevent_free(Event* event);

#endif /* SHD_EVENT_H_ */
