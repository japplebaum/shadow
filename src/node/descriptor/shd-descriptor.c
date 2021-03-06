/**
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

#include "shadow.h"

void descriptor_init(Descriptor* descriptor, enum DescriptorType type,
		DescriptorFunctionTable* funcTable, gint handle) {
	g_assert(descriptor && funcTable);
	g_assert(handle >= MIN_DESCRIPTOR);

	MAGIC_INIT(descriptor);
	MAGIC_INIT(funcTable);
	descriptor->funcTable = funcTable;
	descriptor->handle = handle;
	descriptor->type = type;
	descriptor->readyListeners = NULL;
	descriptor->referenceCount = 1;
}

static void _descriptor_free(Descriptor* descriptor) {
	MAGIC_ASSERT(descriptor);
	MAGIC_ASSERT(descriptor->funcTable);


	if(descriptor->readyListeners) {
		g_slist_free(descriptor->readyListeners);
	}

	MAGIC_CLEAR(descriptor);
	descriptor->funcTable->free(descriptor);
}

void descriptor_ref(gpointer data) {
	Descriptor* descriptor = data;
	MAGIC_ASSERT(descriptor);

	(descriptor->referenceCount)++;
}

void descriptor_unref(gpointer data) {
	Descriptor* descriptor = data;
	MAGIC_ASSERT(descriptor);

	(descriptor->referenceCount)--;
	g_assert(descriptor->referenceCount >= 0);
	if(descriptor->referenceCount == 0) {
		_descriptor_free(descriptor);
	}
}

void descriptor_close(Descriptor* descriptor) {
	MAGIC_ASSERT(descriptor);
	MAGIC_ASSERT(descriptor->funcTable);
	descriptor->funcTable->close(descriptor);
}

gint descriptor_compare(const Descriptor* foo, const Descriptor* bar, gpointer user_data) {
	MAGIC_ASSERT(foo);
	MAGIC_ASSERT(bar);
	return foo->handle > bar->handle ? +1 : foo->handle == bar->handle ? 0 : -1;
}

enum DescriptorType descriptor_getType(Descriptor* descriptor) {
	MAGIC_ASSERT(descriptor);
	return descriptor->type;
}

gint* descriptor_getHandleReference(Descriptor* descriptor) {
	MAGIC_ASSERT(descriptor);
	return &(descriptor->handle);
}

static void _descriptor_notifyListener(gpointer data, gpointer user_data) {
	Listener* listener = data;
	listener_notify(listener);
}

void descriptor_adjustStatus(Descriptor* descriptor, enum DescriptorStatus status, gboolean doSetBits){
	MAGIC_ASSERT(descriptor);

	/* adjust our status as requested */
	if(doSetBits) {
		if((status & DS_ACTIVE) && !(descriptor->status & DS_ACTIVE)) {
			/* status changed - is now active */
			descriptor->status |= DS_ACTIVE;
		}
		if((status & DS_READABLE) && !(descriptor->status & DS_READABLE)) {
			/* status changed - is now readable */
			descriptor->status |= DS_READABLE;
		}
		if((status & DS_WRITABLE) && !(descriptor->status & DS_WRITABLE)) {
			/* status changed - is now writable */
			descriptor->status |= DS_WRITABLE;
		}
		if((status & DS_CLOSED) && !(descriptor->status & DS_CLOSED)) {
			/* status changed - is now closed to user */
			descriptor->status |= DS_CLOSED;
		}
	} else {
		if((status & DS_ACTIVE) && (descriptor->status & DS_ACTIVE)) {
			/* status changed - no longer active */
			descriptor->status &= ~DS_ACTIVE;
		}
		if((status & DS_READABLE) && (descriptor->status & DS_READABLE)) {
			/* status changed - no longer readable */
			descriptor->status &= ~DS_READABLE;
		}
		if((status & DS_WRITABLE) && (descriptor->status & DS_WRITABLE)) {
			/* status changed - no longer writable */
			descriptor->status &= ~DS_WRITABLE;
		}
		if((status & DS_CLOSED) && (descriptor->status & DS_CLOSED)) {
			/* status changed - no longer closed to user */
			descriptor->status &= ~DS_CLOSED;
		}
	}

	/* tell our listeners their was some activity on this descriptor */
	g_slist_foreach(descriptor->readyListeners, _descriptor_notifyListener, NULL);
}

enum DescriptorStatus descriptor_getStatus(Descriptor* descriptor) {
	MAGIC_ASSERT(descriptor);

	enum DescriptorStatus status = DS_NONE;

	if(descriptor->status & DS_ACTIVE) {
		status |= DS_ACTIVE;
	}
	if(descriptor->status & DS_READABLE) {
		status |= DS_READABLE;
	}
	if(descriptor->status & DS_WRITABLE) {
		status |= DS_WRITABLE;
	}

	return status;
}

void descriptor_addStatusListener(Descriptor* descriptor, Listener* listener) {
	MAGIC_ASSERT(descriptor);
	descriptor->readyListeners = g_slist_prepend(descriptor->readyListeners, listener);
}

void descriptor_removeStatusListener(Descriptor* descriptor, Listener* listener) {
	MAGIC_ASSERT(descriptor);
	descriptor->readyListeners = g_slist_remove(descriptor->readyListeners, listener);
}
