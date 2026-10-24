/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodclass.h
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

#ifndef RADIANT_NODCLASS_H
#define RADIANT_NODCLASS_H

#include "nodproperties.h"
#include "nodpath.h"
#include <glib.h>

inline bool NodClass_Registered( const char *comments ){
	return comments && strncmp( comments, "NOD template database v1\n", 25 ) == 0;
}

inline bool NodClass_Browsable( const char *name, const char *comments ){
	return strncmp( name, "nod_", 4 ) != 0 || NodClass_Registered( comments ) ||
		NodPath_Class( name ) || strcmp( name, "nod_static_light" ) == 0;
}

inline std::string NodClass_Field( const char *comments, const std::string &field ){
	const std::string text = comments ? comments : "";
	const std::string marker = "\nNOD " + field + ": ";
	const size_t start = text.find( marker );
	if ( start == std::string::npos ) {
		return "";
	}

	const size_t value = start + marker.size();
	return text.substr( value, text.find( '\n', value ) - value );
}

inline std::string NodClass_Default( const char *comments, const char *key ){
	return NodClass_Field( comments, std::string( "default " ) + key );
}

inline bool NodClass_Bounds( const char *minimum, const char *maximum, float mins[3], float maxs[3] ){
	if ( !ModelTransform_Parse( minimum, 3, mins, true ) || !ModelTransform_Parse( maximum, 3, maxs, true ) ) {
		return false;
	}

	for ( int i = 0; i < 3; ++i ) {
		if ( mins[i] >= maxs[i] ) {
			return false;
		}
	}

	return true;
}

inline bool NodClass_DecodeModifier( const std::string &text, guint32 &id, unsigned char payload[32] ){
	if ( text.size() != 72 ) {
		return false;
	}

	unsigned char bytes[36];
	for ( size_t i = 0; i < 36; ++i ) {
		const unsigned char high = text[i * 2], low = text[i * 2 + 1];
		if ( !( ( high >= '0' && high <= '9' ) || ( high >= 'a' && high <= 'f' ) ) ||
			!( ( low >= '0' && low <= '9' ) || ( low >= 'a' && low <= 'f' ) ) ) {
			return false;
		}

		bytes[i] = static_cast<unsigned char>( g_ascii_xdigit_value( high ) * 16 + g_ascii_xdigit_value( low ) );
	}

	id = static_cast<guint32>( bytes[0] ) | static_cast<guint32>( bytes[1] ) << 8 |
		static_cast<guint32>( bytes[2] ) << 16 | static_cast<guint32>( bytes[3] ) << 24;
	memcpy( payload, bytes + 4, 32 );
	return true;
}

template<typename Getter>
inline bool NodClass_Modifiers( const char *countText, Getter get, std::string &stream, std::string &error ){
	stream.clear();
	if ( !*countText ) {
		return true;
	}

	if ( !NodProperty_Unsigned( countText ) || strlen( countText ) > 3 ||
		( countText[0] == '0' && countText[1] ) || atoi( countText ) < 1 || atoi( countText ) > 128 ) {
		error = "Invalid imported modifier count (expected 1 through 128)."; return false;
	}

	for ( int i = 0; i < atoi( countText ); ++i ) {
		const std::string key = "nod_modifier_" + std::to_string( i );
		const std::string text = get( key.c_str() );
		guint32 id; unsigned char payload[32];
		if ( !NodClass_DecodeModifier( text, id, payload ) ) {
			error = "Missing or invalid ordered imported modifier: " + key; return false;
		}

		stream += text;
	}

	return true;
}

template<typename Lookup>
inline bool NodClass_ResolveValue( const char *comments, const std::string &stream, const char *key,
	const char *storage, Lookup lookup, std::string &value ){
	value = NodClass_Default( comments, key );
	const guint32 requested = static_cast<guint32>( strtoul( key + 9, NULL, 10 ) );
	for ( size_t offset = 0; offset < stream.size(); offset += 72 ) {
		guint32 id; unsigned char payload[32];
		if ( !NodClass_DecodeModifier( stream.substr( offset, 72 ), id, payload ) ) {
			return false;
		}

		if ( id == 0 ) {
			if ( !memchr( payload, 0, 32 ) ) {
				return false;
			}

			const char *base = lookup( reinterpret_cast<const char *>( payload ) );
			if ( !NodClass_Registered( base ) ) {
				return false;
			}

			const std::string marker = std::string( "\nNOD default " ) + key + ": ";
			if ( std::string( base ).find( marker ) != std::string::npos ) {
				value = NodClass_Default( base, key );
		}
		} else if ( id == requested ) {
			if ( strcmp( storage, "string32" ) == 0 ) {
				if ( !memchr( payload, 0, 32 ) ) {
					return false;
				}

				value = reinterpret_cast<const char *>( payload );
			} else {
				value.clear();
				const int components = strcmp( storage, "vector3" ) == 0 ? 3 : 1;
				for ( int axis = 0; axis < components; ++axis ) {
					guint32 bits; memcpy( &bits, payload + axis * 4, 4 ); bits = GUINT32_FROM_LE( bits );
					float component; memcpy( &component, &bits, 4 );
					if ( !( component >= -FLT_MAX && component <= FLT_MAX ) ) {
						return false;
					}

					char text[G_ASCII_DTOSTR_BUF_SIZE];
					g_ascii_formatd( text, sizeof( text ), "%.9g", component );
					if ( axis ) {
						value += ' ';
					}

					value += text;
				}
			}
		}
	}

	return true;
}

#endif
