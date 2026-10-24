/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodsurface.h
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

#ifndef RADIANT_NODSURFACE_H
#define RADIANT_NODSURFACE_H

#include <float.h>
#include <string>
#include <string.h>

/*
 */
enum NodSurfaceOperation
{
	NOD_SURFACE_RESET_UV,
	NOD_SURFACE_PLACEHOLDER,
	NOD_SURFACE_CAPTURED,
	NOD_SURFACE_CAPTURED_MATERIAL,
	NOD_SURFACE_CAPTURED_UV
};

/*
 */
struct NodSurfacePreset
{
	std::string material;
	float shift[2];
	float scale[2];
	float rotation;
	NodSurfacePreset() : rotation( 0 ){
		shift[0] = shift[1] = 0;
		scale[0] = scale[1] = 1;
	}
};

/*
 */
inline bool NodSurface_IsProfile( const char *gameFile ){
	return gameFile && strcmp( gameFile, "scghost.game" ) == 0;
}

/*
 */
inline bool NodSurface_UsesCapture( NodSurfaceOperation operation ){
	return operation == NOD_SURFACE_CAPTURED || operation == NOD_SURFACE_CAPTURED_MATERIAL
		|| operation == NOD_SURFACE_CAPTURED_UV;
}

/*
 */
inline const char *NodSurface_Validate( const NodSurfacePreset &preset, bool material, bool uv ){
	if ( material ) {
		if ( preset.material.compare( 0, 9, "textures/" ) != 0 || preset.material.size() <= 9
			 || preset.material.size() > 127 ) {
			return "Use a material name beginning with textures/ (at most 127 bytes).";
		}

		for ( size_t i = 0; i < preset.material.size(); ++i ) {
			const unsigned char c = static_cast<unsigned char>( preset.material[i] );
			if ( c <= 32 || c >= 127 || c == '"' || c == '\\' || c == '(' || c == ')' || c == '{' || c == '}' ) {
				return "The material name contains a byte that cannot be written safely to a MAP file.";
		}
	}
	}

	if ( uv ) {
		const float values[] = { preset.shift[0], preset.shift[1], preset.scale[0], preset.scale[1], preset.rotation };
		for ( size_t i = 0; i < sizeof( values ) / sizeof( values[0] ); ++i ) {
			if ( !( values[i] >= -FLT_MAX && values[i] <= FLT_MAX ) ) {
				return "UV values must be finite float32 numbers.";
		}
		}

		if ( preset.scale[0] == 0 || preset.scale[1] == 0 ) {
			return "UV scales must not be zero.";
	}
	}

	return NULL;
}

/*
 */
template<typename Texdef>
inline NodSurfacePreset NodSurface_Capture( const Texdef &texdef ){
	NodSurfacePreset preset;
	preset.material = texdef.GetName();
	for ( int i = 0; i < 2; ++i ) {
		preset.shift[i] = texdef.shift[i];
		preset.scale[i] = texdef.scale[i];
	}

	preset.rotation = texdef.rotate;
	return preset;
}

/*
 */
template<typename Texdef>
inline const char *NodSurface_Apply( Texdef &texdef, NodSurfaceOperation operation,
								   const NodSurfacePreset *captured ){

	if ( operation < NOD_SURFACE_RESET_UV || operation > NOD_SURFACE_CAPTURED_UV ) {
		return "Select a surface preset.";
	}

	if ( NodSurface_UsesCapture( operation ) && !captured ) {
		return "Capture one selected face first.";
	}

	NodSurfacePreset preset;

	if ( captured && NodSurface_UsesCapture( operation ) ) {
		preset = *captured;
	}

	if ( operation == NOD_SURFACE_PLACEHOLDER ) {
		preset.material = "textures/nod/placeholder";
	}

	const bool material = operation != NOD_SURFACE_RESET_UV && operation != NOD_SURFACE_CAPTURED_UV;
	const bool uv = operation != NOD_SURFACE_PLACEHOLDER && operation != NOD_SURFACE_CAPTURED_MATERIAL;
	const char *error = NodSurface_Validate( preset, material, uv );

	if ( error ) {
		return error;
	}

	if ( material ) {
		texdef.SetName( preset.material.c_str() );
	}

	if ( uv ) {
		for ( int i = 0; i < 2; ++i ) {
			texdef.shift[i] = preset.shift[i];
			texdef.scale[i] = preset.scale[i];
		}

		texdef.rotate = preset.rotation;
	}

	return NULL;
}

/*
 */
template<typename Texdef>
inline bool NodSurface_Equal( const Texdef &a, const Texdef &b ){
	return strcmp( a.GetName(), b.GetName() ) == 0 && a.shift[0] == b.shift[0]
		&& a.shift[1] == b.shift[1] && a.scale[0] == b.scale[0] && a.scale[1] == b.scale[1]
		&& a.rotate == b.rotate && a.flags == b.flags && a.contents == b.contents && a.value == b.value;
}

#endif
