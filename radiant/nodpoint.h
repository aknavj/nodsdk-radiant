/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodpoint.h
Version: 0.6.7

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#ifndef RADIANT_NODPOINT_H
#define RADIANT_NODPOINT_H
#include <string>
#include <cmath>
#include <utility>
#include <vector>

inline entity_t *NodPoint_Create( eclass_t *type, const vec3_t position,
	const std::vector<std::pair<std::string, std::string> > &keys, const char *undo, std::string &error ){
	vec3_t mins, maxs;
	for ( int axis = 0; axis < 3; ++axis ) {
		mins[axis] = position[axis] + type->mins[axis];
		maxs[axis] = position[axis] + type->maxs[axis];
		if ( !std::isfinite( mins[axis] ) || !std::isfinite( maxs[axis] ) ||
			mins[axis] < g_MinWorldCoord || maxs[axis] > g_MaxWorldCoord || mins[axis] >= maxs[axis] ) {
			error = "The point marker must fit inside the editor's world-coordinate limits."; return NULL;
		}
	}

	entity_t *entity = Entity_Alloc();
	for ( size_t i = 0; i < keys.size(); ++i ) {
		SetKeyValue( entity, keys[i].first.c_str(), keys[i].second.c_str() );
	}

	VectorCopy( position, entity->origin );
	brush_t *brush = Brush_Create( mins, maxs, &type->texdef );
	if ( !brush ) { Entity_Free( entity ); error = "Could not create the authoring marker."; return NULL; }
	Undo_Start( undo );
	Entity_AddToList( entity, &entities );
	Entity_LinkBrush( entity, brush );
	Brush_AddToList( brush, &active_brushes );
	Brush_Build( brush, false, true, false, false );
	Select_Deselect();
	Select_Brush( brush );
	Undo_EndBrush( brush );
	Undo_EndEntity( entity );
	Undo_End();
	return entity;
}
#endif
