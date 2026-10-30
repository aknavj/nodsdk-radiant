/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodproperties.h
Version: 0.6.8

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

#ifndef RADIANT_NODPROPERTIES_H
#define RADIANT_NODPROPERTIES_H

#include <string>
#include "modeltransform.h"
#include "nodenvironment.h"

struct NodPropertyInfo
{
	std::string name;
	std::string storage;
	std::string access;
};

inline std::string NodProperty_DisplayName( const std::string &key, const std::string &nativeName ){
	if ( key == "origin" ) {
		return "Position";
	}

	if ( key == "angles" || key == "nod_rotation" ) {
		return "Rotation";
	}

	if ( key == "nod_sector" ) {
		return "Sector";
	}

	if ( key == "classname" ) {
		return "Entity Class";
	}

	if ( key == "nod_light_spacing" ) {
		return "Lighting Sample Spacing";
	}

	for ( const auto &field : NodEnvironment_Fields() ) {
		if ( key == field.key ) {
			return field.name;
		}
	}

	if ( key == "nod_waypoint_id" ) {
		return "Waypoint ID";
	}

	if ( key == "nod_from" ) {
		return "From waypoint ID";
	}

	if ( key == "nod_to" ) {
		return "To waypoint ID";
	}

	if ( key == "nod_link_flags" || key == "nod_flags" ) {
		return "Path flags";
	}

	if ( nativeName.empty() && key.compare( 0, 9, "nod_prop_" ) == 0 ) {
		return "Property " + key.substr( 9 );
	}

	const std::string &source = nativeName.empty() ? key : nativeName;
	std::string result;
	bool word = true;
	bool uppercase = true;
	for ( size_t i = 0; i < source.size(); ++i ) {
		if ( source[i] >= 'a' && source[i] <= 'z' ) {
			uppercase = false;
		}
	}

	for ( size_t i = 0; i < source.size(); ++i ) {
		const unsigned char c = source[i];
		if ( c == '_' || c == ' ' ) {
			if ( !result.empty() && result.back() != ' ' ) {
				result += ' ';
			}

			word = true;
		}
		else {
			result += static_cast<char>( word && c >= 'a' && c <= 'z' ? c - 'a' + 'A' :
				uppercase && !word && c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c );
			word = false;
		}
	}

	if ( !result.empty() && result.back() == ' ' ) {
		result.pop_back();
	}

	return result;
}

inline bool NodProperty_Visible( const std::string &key, const std::string &storage, bool technical, bool searching ){
	if ( technical || searching ) {
		return true;
	}

	if ( NodEnvironment_IsKey( key ) || key == "nod_light_spacing" || key == "nod_waypoint_id" || key == "nod_from" || key == "nod_to" ||
		key == "nod_link_flags" || key == "nod_flags" ) {
		return true;
	}

	if ( key == "classname" || storage == "raw" ) {
		return false;
	}

	if ( key.compare( 0, 9, "nod_prop_" ) == 0 ) {
		const std::string id = key.substr( 9 );
		return !storage.empty() && id != "0" && id != "1" && id != "2" && id != "3" && id != "20";
	}

	return key.compare( 0, 4, "nod_" ) != 0 || key == "nod_sector" || key == "nod_rotation";
}

inline NodPropertyInfo NodProperty_Info( const char *comments, const char *key ){
	NodPropertyInfo info;
	const std::string marker = std::string( "\"" ) + key + "\" - ";
	const std::string text = comments ? comments : "";
	size_t start = text.find( marker );
	if ( start == std::string::npos ) {
		return info;
	}

	start += marker.size();
	const size_t end = text.find( '\n', start );
	const std::string line = text.substr( start, end - start );
	const size_t first = line.find( "; " );
	const size_t second = first == std::string::npos ? first : line.find( "; ", first + 2 );
	if ( second == std::string::npos ) {
		return info;
	}

	info.name = line.substr( 0, first );
	info.storage = line.substr( first + 2, second - first - 2 );
	info.access = line.substr( second + 2 );
	return info;
}

inline bool NodProperty_IsNative( const char *classname, const char *identity ){
	return *identity || ModelTransform_IsNativeEntity( classname )
		|| strcmp( classname, "nod_npd_waypoint" ) == 0 || strcmp( classname, "nod_npd_link" ) == 0;
}

// The generated entity catalog carries the SDK's storage and local/preview edit policy.
inline const char *NodProperty_Restriction( const char *classname, const char *identity,
										  bool geometry, bool preview, bool model, bool present,
										  const char *key, const NodPropertyInfo &info ){
	if ( !present ) {
		return "Edit existing local keys only; inherited values require NOTEditor.";
	}

	if ( geometry || strcmp( identity, "nil:world" ) == 0 ) {
		return "Baked geometry link; edit the source and re-enrich.";
	}

	if ( strncmp( identity, "nsd:", 4 ) != 0 ) {
		return "This inspector currently supports editing existing NSD entities only.";
	}

	if ( strcmp( classname, "misc_model" ) != 0 && !ModelTransform_IsNativeEntity( classname ) ) {
		return "This class is not a supported native NSD record.";
	}

	if ( strcmp( key, "origin" ) == 0 || strcmp( key, "nod_sector" ) == 0 ) {
		return NULL;
	}

	const char *rotation = strcmp( classname, "misc_model" ) == 0
		|| ( strncmp( classname, "nod_nsd_type_", 13 ) == 0 && model ) ? "angles" : "nod_rotation";
	if ( strcmp( key, rotation ) == 0 ) {
		return NULL;
	}

	if ( strncmp( key, "nod_prop_", 9 ) != 0 ) {
		return "Native identity, links, and preview metadata are read-only.";
	}

	if ( info.access.find( "editable local payload" ) == 0 ) {
		return NULL;
	}

	if ( info.access.find( "source-map edit; re-enrich preview" ) == 0 ) {
		return preview ? "Edit this property in the source map, then re-enrich the preview." : NULL;
	}

	return "SDK property is not an editable local payload.";
}

inline bool NodProperty_Unsigned( const char *text ){
	if ( !*text ) {
		return false;
	}

	double value = 0;
	for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( text ); *p; ++p ) {
		if ( *p < '0' || *p > '9' ) {
			return false;
		}

		value = value * 10 + ( *p - '0' );
		if ( value > 4294967295.0 ) {
			return false;
	}
	}

	return true;
}

inline const char *NodProperty_Validate( const char *key, const char *value, const NodPropertyInfo &info ){
	float parsed[3];
	if ( strcmp( key, "nod_sector" ) == 0 || info.storage == "uint32 bits" ) {
		return NodProperty_Unsigned( value ) ? NULL : "Enter a decimal unsigned 32-bit integer (0 through 4294967295).";
	}

	if ( strcmp( key, "origin" ) == 0 || strcmp( key, "angles" ) == 0
		 || strcmp( key, "nod_rotation" ) == 0 || info.storage == "vector3" ) {
		return ModelTransform_Parse( value, 3, parsed, true ) ? NULL : "Enter three space-separated finite float32 values.";
	}

	if ( info.storage == "float32" ) {
		return ModelTransform_Parse( value, 1, parsed, true ) ? NULL : "Enter one finite float32 value.";
	}

	if ( info.storage == "string32" ) {
		if ( strlen( value ) > 31 ) {
			return "Native strings allow at most 31 printable ASCII bytes.";
		}

		for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( value ); *p; ++p ) {
			if ( *p < 32 || *p > 126 ) {
				return "Native strings require printable ASCII.";
			}

			if ( *p == '"' ) {
				return "Quotes cannot be round-tripped by the source-map format.";
		}
		}

		return NULL;
	}

	return "Unknown native storage type; regenerate the SDK entity catalog.";
}

#endif
