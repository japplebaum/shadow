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

#ifndef SHD_PACKET_DROPPED_H_
#define SHD_PACKET_DROPPED_H_

#include "shadow.h"

typedef struct _PacketDroppedEvent PacketDroppedEvent;

PacketDroppedEvent* packetdropped_new(Packet* packet);
void packetdropped_run(PacketDroppedEvent* event, Node* node);
void packetdropped_free(PacketDroppedEvent* event);

#endif /* SHD_PACKET_DROPPED_H_ */
