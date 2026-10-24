/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodpath.h
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

#ifndef RADIANT_NODPATH_H
#define RADIANT_NODPATH_H
#include <cstring>
#include <string>
#include "modeltransform.h"

inline bool NodPath_Class( const char *name ){
	return strcmp( name, "nod_npd_waypoint" ) == 0 || strcmp( name, "nod_npd_link" ) == 0;
}

inline bool NodPath_Number( const char *text, unsigned &number ){
	number = 0;
	if ( !*text || ( text[0] == '0' && text[1] ) ) {
		return false;
	}

	for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( text ); *p; ++p ) {
		if ( *p < '0' || *p > '9' ) {
			return false;
		}

		number = number*10 + ( *p-'0' );
		if ( number > 65534 ) {
			return false;
	}
	}

	return true;
}

inline const char *NodPath_Restriction( const char *name, const char *key ){
	const bool node = strcmp( name, "nod_npd_waypoint" ) == 0;
	if ( node && ( strcmp( key, "origin" ) == 0 || strcmp( key, "nod_waypoint_id" ) == 0 ||
		strcmp( key, "nod_flags" ) == 0 ) ) {
		return NULL;
	}

	if ( !node && ( strcmp( key, "nod_from" ) == 0 || strcmp( key, "nod_to" ) == 0 ||
		strcmp( key, "nod_link_flags" ) == 0 ) ) {
		return NULL;
	}

	return "Path identity/space and link marker position are read-only.";
}

inline const char *NodPath_Validate( const char *key, const char *value ){
	if ( strcmp( key, "origin" ) == 0 ) {
		float position[3];
		return ModelTransform_Parse( value, 3, position, true ) ? NULL : "Enter three finite world coordinates.";
	}

	unsigned number;
	if ( !NodPath_Number( value, number ) ) {
		return "Enter a canonical waypoint ID from 0 through 65534.";
	}

	if ( ( strcmp( key, "nod_flags" ) == 0 || strcmp( key, "nod_link_flags" ) == 0 ) && number != 0 ) {
		return "Only normal path flags 0 are supported.";
	}

	return NULL;
}
#endif
