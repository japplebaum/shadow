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

#ifndef SHD_LIBRARY_H_
#define SHD_LIBRARY_H_

#include <glib.h>
#include <netinet/in.h>

/**
 * plug-ins must implement a function with this name to hook into Shadow. We
 * call this function during plugin initialization. A symbol with this name
 * must exist or the dlsym lookup will fail.
 */
#define PLUGININITSYMBOL "__shadow_plugin_init__"

/**
 * Signature of a function that Shadow calls when creating a new node instance
 * with the plug-in. The parameters are meant to mirror those passed to the
 * main() function of a standard C program.
 *
 * @param argc the number of arguments passed in the argument vector
 * @param argv a vector of arguments for node creation, as parsed from the XML
 * simulation input file
 *
 * @see #PluginFunctionTable
 */
typedef void (*PluginNewInstanceFunc)(gint argc, gchar* argv[]);

/**
 * Signature of a function that Shadow calls to notify of an event or to
 * execute destruction.
 *
 * @see #PluginFunctionTable
 */
typedef void (*PluginNotifyFunc)();

typedef struct _PluginFunctionTable PluginFunctionTable;

/**
 * A collection of functions implemented by a plug-in. The functions
 * essentially define the plug-in interface that Shadow uses to communicate
 * with the plug-in, allowing Shadow to perform callbacks into user-specified
 * functions when creating and freeing nodes, and when a socket may be read or
 * written without blocking.
 */
struct _PluginFunctionTable {
	/**
	 * Pointer to a function to call when creating a new node instance.
	 */
	PluginNewInstanceFunc new;

	/**
	 * Pointer to a function to call when freeing a node instance.
	 */
	PluginNotifyFunc free;

	/**
	 * Pointer to a function to call when descriptors are ready.
	 */
	PluginNotifyFunc notify;
};

/*
 * signature for plug-in callback functions
 */
typedef void (*ShadowPluginCallbackFunc)(gpointer data);

/* function signatures for available shadow functions */
typedef gboolean (*ShadowlibRegisterFunc)(PluginFunctionTable* callbackFunctions, guint nVariables, ...);
typedef void (*ShadowlibLogFunc)(GLogLevelFlags level, const gchar* functionName, gchar* format, ...);
typedef void (*ShadowlibCreateCallbackFunc)(ShadowPluginCallbackFunc callback, gpointer data, guint millisecondsDelay);
typedef gboolean (*ShadowlibGetBandwidthFloorFunc)(in_addr_t ip, guint* bwdown, guint* bwup);
typedef gboolean (*ShadowlibCryptoSetupFunc)(gint numLocks, gpointer* shadowLockFunc, gpointer* shadowIdFunc, gconstpointer* shadowRandomMethod);

typedef struct _ShadowlibFunctionTable ShadowlibFunctionTable;
extern ShadowlibFunctionTable shadowlibFunctionTable;

/**
 * A collection of functions exported to a plug-in. Each pointer in this table
 * may be dereferenced to call a function in Shadow. Plug-ins may use these
 * functions to hook into Shadow's logging and event systems.
 */
struct _ShadowlibFunctionTable {
	ShadowlibRegisterFunc registerPlugin;
	ShadowlibLogFunc log;
	ShadowlibCreateCallbackFunc createCallback;
	ShadowlibGetBandwidthFloorFunc getBandwidth;
	ShadowlibCryptoSetupFunc cryptoSetup;
};

/* Plug-ins must implement this function to communicate with Shadow.
 * the function name symbol must be PLUGININITSYMBOL */
typedef void (*ShadowPluginInitializeFunc)(ShadowlibFunctionTable* shadowlibFunctions);

#endif /* SHD_LIBRARY_H_ */
