/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodlightplacement.h
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

#ifndef RADIANT_NODLIGHTPLACEMENT_H
#define RADIANT_NODLIGHTPLACEMENT_H
#include "nodpoint.h"
#include "nodtemplates.h"

inline entity_t *NodLight_Create( const vec3_t position, std::string &error ){
	error.clear();
	eclass_t *type = Eclass_ForName( "nod_static_light", false );
	if ( !type || !type->fixedsize || !type->comments || !strstr( type->comments, "MAP-only static lighting" ) ) {
		error = "Reload the SCGhost static-light definitions."; return NULL;
	}

	std::vector<std::pair<std::string, std::string> > keys;
	keys.push_back( std::make_pair( "classname", "nod_static_light" ) );
	keys.push_back( std::make_pair( "origin", NodTemplate_FormatPosition( position ) ) );
	keys.push_back( std::make_pair( "_color", "1 1 1" ) );
	keys.push_back( std::make_pair( "light", "8" ) );
	keys.push_back( std::make_pair( "radius", "10" ) );
	return NodPoint_Create( type, position, keys, "Place NOD light", error );
}
#endif
