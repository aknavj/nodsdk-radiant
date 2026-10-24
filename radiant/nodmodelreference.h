/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodmodelreference.h
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

#ifndef RADIANT_NODMODELREFERENCE_H
#define RADIANT_NODMODELREFERENCE_H

#include <string>
#include <glib.h>

inline bool NodModelReference_Path( const std::string &path ){
	if ( path.empty() || path.size() > 255 || path[0] == '/' || path.back() == '/' ) {
		return false;
	}

	size_t start = 0;
	for ( size_t i = 0; i <= path.size(); ++i ) {
		if ( i != path.size() && path[i] != '/' ) {
			const unsigned char c = path[i];
			if ( c < 32 || c == 127 || c == ':' || c == '?' || c == '&' || c == '\\' ) {
				return false;
			}

			continue;
		}

		const std::string part = path.substr( start, i - start );
		if ( part.empty() || part == "." || part == ".." ) {
			return false;
		}

		start = i + 1;
	}

	return true;
}

inline bool NodModelReference_Bundle( const std::string &name ){
	return name.empty() || ( NodModelReference_Path( name ) && name.find( '/' ) == std::string::npos &&
		name.size() > 4 && g_ascii_strcasecmp( name.c_str() + name.size() - 4, ".nmb" ) == 0 );
}

inline bool NodModelReference_Matches( const std::string &reference, const std::string &asset ){
	if ( g_ascii_strcasecmp( reference.c_str(), asset.c_str() ) == 0 ) {
		return true;
	}

	return asset.size() > 4 &&
		( g_ascii_strcasecmp( asset.c_str() + asset.size() - 4, ".nod" ) == 0 ||
		  g_ascii_strcasecmp( asset.c_str() + asset.size() - 4, ".nmb" ) == 0 ) &&
		g_ascii_strcasecmp( reference.c_str(), asset.substr( 0, asset.size() - 4 ).c_str() ) == 0;
}

inline bool NodModelReference_IsRequest( const char *name ){
	return name && std::string( name ).compare( 0, 5, "@nod/" ) == 0;
}

inline std::string NodModelReference_Request( const std::string &reference, const std::string &bundle ){
	return "@nod/" + ( bundle.empty() ? "-" : bundle ) + "/" + reference;
}

inline bool NodModelReference_Decode( const std::string &request, std::string &reference, std::string &bundle ){
	if ( !NodModelReference_IsRequest( request.c_str() ) ) {
		return false;
	}

	const size_t separator = request.find( '/', 5 );
	if ( separator == std::string::npos ) {
		return false;
	}

	bundle = request.substr( 5, separator - 5 );
	if ( bundle == "-" ) {
		bundle.clear();
	}

	reference = request.substr( separator + 1 );
	return NodModelReference_Path( reference ) && NodModelReference_Bundle( bundle );
}

#endif
