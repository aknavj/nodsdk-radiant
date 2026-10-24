/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodpathplacement.h
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

#ifndef RADIANT_NODPATHPLACEMENT_H
#define RADIANT_NODPATHPLACEMENT_H
#include "nodpath.h"
#include "nodpoint.h"
#include "nodtemplates.h"
#include <set>

inline bool NodPath_Authoring( entity_t *entity ){
	return entity && NodPath_Class( ValueForKey( entity, "classname" ) ) &&
		strcmp( ValueForKey( entity, "nod_authoring" ), "npd-world-v1" ) == 0 &&
		!*ValueForKey( entity, "nod_id" );
}

inline entity_t *NodPath_Create( const char *name, const vec3_t position, const char *from,
	const char *to, std::string &error ){
	error.clear();
	eclass_t *type = Eclass_ForName( name, false );
	if ( !type || !type->fixedsize || !type->comments || !strstr( type->comments, "MAP-only path authoring" ) ) {
		error = "Reload the SCGhost waypoint definitions."; return NULL;
	}

	std::vector<std::pair<std::string, std::string> > keys;
	keys.push_back( std::make_pair( "classname", name ) );
	keys.push_back( std::make_pair( "nod_authoring", "npd-world-v1" ) );
	keys.push_back( std::make_pair( "nod_space", "world" ) );
	keys.push_back( std::make_pair( "origin", NodTemplate_FormatPosition( position ) ) );
	if ( strcmp( name, "nod_npd_waypoint" ) == 0 ) {
		std::set<unsigned> used;
		for ( entity_t *entity = entities.next; entity != &entities; entity = entity->next ) {
			if ( strcmp( ValueForKey( entity, "classname" ), name ) != 0 ) {
				continue;
			}

			unsigned id;
			if ( !NodPath_Number( ValueForKey( entity, "nod_waypoint_id" ), id ) ) {
				error = "Fix invalid existing waypoint IDs first."; return NULL;
			}

			used.insert( id );
		}

		unsigned id = 0;
		while ( id < 65535 && used.count( id ) ) {
			++id;
		}

		if ( id == 65535 ) { error = "No free waypoint IDs."; return NULL; }
		keys.push_back( std::make_pair( "nod_waypoint_id", std::to_string( id ) ) );
		keys.push_back( std::make_pair( "nod_flags", "0" ) );
	} else {
		keys.push_back( std::make_pair( "nod_from", from ) );
		keys.push_back( std::make_pair( "nod_to", to ) );
		keys.push_back( std::make_pair( "nod_link_flags", "0" ) );
	}

	return NodPoint_Create( type, position, keys, "Place NOD path", error );
}

inline bool NodPath_Connect( entity_t *from, entity_t *to, std::string &error ){
	error.clear();
	if ( !NodPath_Authoring( from ) || !NodPath_Authoring( to ) || from == to ||
		strcmp( ValueForKey( from, "classname" ), "nod_npd_waypoint" ) != 0 ||
		strcmp( ValueForKey( to, "classname" ), "nod_npd_waypoint" ) != 0 ) {
		error = "Select two MAP-only waypoints, source first."; return false;
	}

	const std::string a = ValueForKey( from, "nod_waypoint_id" ), b = ValueForKey( to, "nod_waypoint_id" );
	unsigned id;
	if ( !NodPath_Number( a.c_str(), id ) || !NodPath_Number( b.c_str(), id ) || a == b ) {
		error = "Waypoints require different valid IDs."; return false;
	}

	if ( from->origin[0] == to->origin[0] && from->origin[1] == to->origin[1] && from->origin[2] == to->origin[2] ) {
		error = "Move the waypoints apart before connecting them."; return false;
	}

	for ( entity_t *entity = entities.next; entity != &entities; entity = entity->next ) {
		if ( NodPath_Authoring( entity ) && strcmp( ValueForKey( entity, "classname" ), "nod_npd_link" ) == 0 &&
			a == ValueForKey( entity, "nod_from" ) && b == ValueForKey( entity, "nod_to" ) ) {
			error = "This directed link already exists."; return false;
		}
	}

	vec3_t position;
	for ( int axis = 0; axis < 3; ++axis ) {
		position[axis] = ( from->origin[axis]+to->origin[axis] )*0.5f;
	}

	return NodPath_Create( "nod_npd_link", position, a.c_str(), b.c_str(), error ) != NULL;
}
#endif
