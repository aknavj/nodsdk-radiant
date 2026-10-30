/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodenvironment.h
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

#ifndef RADIANT_NODENVIRONMENT_H
#define RADIANT_NODENVIRONMENT_H

#include <array>
#include <map>
#include <string>
#include <glib.h>
#include "modeltransform.h"

/*
 */
struct NodEnvironmentField {
	const char *key, *name, *hint;
};

/*
 */
inline const std::array<NodEnvironmentField, 10> &NodEnvironment_Fields(){
	static const std::array<NodEnvironmentField, 10> fields{{
		{ "nod_music", "Music", "Optional filename, at most 31 ASCII bytes: letters, digits, _, -, .; no paths or leading/trailing dot. Playback is not verified." },
		{ "nod_ambient_loop", "Ambient loop", "Optional audio filename, at most 31 ASCII bytes; no paths or leading/trailing dot. Playback is not verified." },
		{ "nod_sky_model", "Sky model", "Optional .nod filename, at most 31 ASCII bytes; no paths. Does not deploy assets or establish rendering." },
		{ "nod_sky_animation", "Sky animation", "Optional .nad filename, at most 31 ASCII bytes; independent of sky model. Animation playback is not verified." },
		{ "nod_fog_mode", "Fog mode", "Blank or none: no fog; remove color/start/end. Linear mode requires all three fields. Case-sensitive: none or linear." },
		{ "nod_fog_color", "Fog color (RGB)", "Three finite RGB values in 0..255, not 0..1. Required only in linear mode." },
		{ "nod_fog_start", "Fog start", "Finite nonnegative distance in MAP units. Required only in linear mode." },
		{ "nod_fog_end", "Fog end", "Finite distance greater than fog start after float32 rounding. Required only in linear mode." },
		{ "nod_far_clip", "Far clip", "Finite nonnegative distance in MAP units. Blank or 0: native default. Independent of physics bounds." },
		{ "nod_reverb", "Reverb preset ID", "Canonical decimal integer 0..25. Blank: native default. No assumed off meaning for 0; audible effects are not verified." }
	}};
	return fields;
}

/*
 */
typedef std::map<std::string, std::string> NodEnvironmentValues;

/*
 */
inline bool NodEnvironment_IsReference( const std::string &key ){
	return key == "nod_music" || key == "nod_ambient_loop" ||
		key == "nod_sky_model" || key == "nod_sky_animation";
}

/*
 */
inline bool NodEnvironment_IsKey( const std::string &key ){
	for ( const auto &field : NodEnvironment_Fields() ) {
		if ( key == field.key ) {
			return true;
		}
	}

	return false;
}

/*
 */
inline bool NodEnvironment_Distance( const std::string &value, float parsed[3] ){
	return !value.empty() && !isspace( static_cast<unsigned char>( value.back() ) ) &&
		ModelTransform_Parse( value.c_str(), 1, parsed, true ) && parsed[0] >= 0;
}

/*
 */
inline std::string NodEnvironment_Validate( const NodEnvironmentValues &values ){
	for ( const auto &field : NodEnvironment_Fields() ) {
		const auto found = values.find( field.key );
		if ( found == values.end() ) {
			continue;
		}

		const std::string &key = found->first, &value = found->second;
		const char *error = NULL;
		float parsed[3];
		if ( NodEnvironment_IsReference( key ) ) {
			const char *extension = key == "nod_sky_model" ? ".nod" : key == "nod_sky_animation" ? ".nad" : "";
			bool valid = value.size() <= 31;
			for ( unsigned char c : value ) {
				valid = valid && ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
					( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '.' );
			}

			if ( !value.empty() ) {
				if ( *extension ) {
					valid = valid && value.size() > 4 &&
						g_ascii_strcasecmp( value.c_str() + value.size() - 4, extension ) == 0;
				}
				else {
					valid = valid && value.front() != '.' && value.back() != '.';
				}
			}

			if ( !valid ) {
				error = field.hint;
			}
		} else if ( key == "nod_fog_mode" ) {
			if ( value != "none" && value != "linear" ) {
				error = field.hint;
			}
		} else if ( key == "nod_fog_color" ) {
			if ( !ModelTransform_Parse( value.c_str(), 3, parsed, true ) ||
				parsed[0] < 0 || parsed[0] > 255 || parsed[1] < 0 || parsed[1] > 255 ||
				parsed[2] < 0 || parsed[2] > 255 ) {
				error = field.hint;
			}
		} else if ( key == "nod_reverb" ) {
			unsigned preset = 0;
			bool valid = !value.empty() && value.size() <= 2 && ( value.size() == 1 || value[0] != '0' );
			for ( unsigned char c : value ) {
				if ( c < '0' || c > '9' ) {
					valid = false;
					break;
				}

				preset = preset * 10 + c - '0';
			}

			if ( !valid || preset > 25 ) {
				error = field.hint;
			}
		} else if ( !NodEnvironment_Distance( value, parsed ) ) {
			error = field.hint;
		}

		if ( error ) {
			return key + ": " + error;
		}
	}

	const auto mode = values.find( "nod_fog_mode" );
	const bool linear = mode != values.end() && mode->second == "linear";
	for ( const char *key : { "nod_fog_color", "nod_fog_start", "nod_fog_end" } ) {
		if ( ( values.count( key ) != 0 ) != linear ) {
			return std::string( key ) + ( linear ? ": required with nod_fog_mode linear." :
				": remove this key unless nod_fog_mode is linear." );
		}
	}

	if ( linear ) {
		float start[3], end[3];
		if ( !NodEnvironment_Distance( values.at( "nod_fog_start" ), start ) ||
			!NodEnvironment_Distance( values.at( "nod_fog_end" ), end ) || end[0] <= start[0] ) {
			return "nod_fog_end: must exceed nod_fog_start after float32 rounding.";
		}
	}

	return "";
}

#endif
