/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodbounds.h
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

#ifndef RADIANT_NODBOUNDS_H
#define RADIANT_NODBOUNDS_H

#include "modeltransform.h"

inline bool NodEntity_IsBrushFallback( const char *classname, const char *identity ){
	if ( strcmp( classname, "func_group" ) != 0 ) {
		return false;
	}

	if ( strcmp( identity, "nil:world" ) == 0 ) {
		return true;
	}

	const char *suffix = strncmp( identity, "nsd:", 4 ) == 0 ? identity + 4 :
		strncmp( identity, "nsdgeom:", 8 ) == 0 ? identity + 8 : NULL;
	if ( !suffix || !*suffix ) {
		return false;
	}

	for ( const char *digit = suffix; *digit; ++digit ) {
		if ( *digit < '0' || *digit > '9' ) {
			return false;
		}
	}

	return true;
}

inline bool NodEntity_IsPoint( const char *classname ){
	return ModelTransform_IsNativeEntity( classname )
		|| strcmp( classname, "nod_nil_sector" ) == 0
		|| strcmp( classname, "nod_npd_waypoint" ) == 0
		|| strcmp( classname, "nod_npd_link" ) == 0
		|| strcmp( classname, "nod_static_light" ) == 0
		|| strcmp( classname, "nod_scene" ) == 0;
}

inline bool NodEntity_IsUVBrush( const char *classname, const char *identity, const char *mapping ){
	return NodEntity_IsBrushFallback( classname, identity )
		|| ( strcmp( classname, "worldspawn" ) == 0 && strcmp( mapping, "brush-primitives" ) == 0 );
}

inline bool NodEntity_ComputeBounds( const char *minimum, const char *maximum,
	const char *rotation, const float origin[3], aabb_t &bounds ){
	vec3_t mins = { -0.125f, -0.125f, -0.125f };
	vec3_t maxs = { 0.125f, 0.125f, 0.125f };
	const bool hasBounds = *minimum || *maximum;
	if ( hasBounds && ( !ModelTransform_Parse( minimum, 3, mins, true )
		|| !ModelTransform_Parse( maximum, 3, maxs, true ) ) ) {
		return false;
	}

	for ( int axis = 0; axis < 3; ++axis ) {
		if ( mins[axis] > maxs[axis] || !( origin[axis] >= -FLT_MAX && origin[axis] <= FLT_MAX ) ) {
			return false;
	}
	}

	vec3_t angles = { 0, 0, 0 };
	if ( hasBounds && *rotation && !ModelTransform_Parse( rotation, 3, angles, true ) ) {
		return false;
	}

	const double x = angles[0] * Q_PI / 180, y = angles[1] * Q_PI / 180, z = angles[2] * Q_PI / 180;
	const double sx = sin( x ), cx = cos( x ), sy = sin( y ), cy = cos( y ), sz = sin( z ), cz = cos( z );
	// Native MatrixPYR is Ry * Rx * Rz, not Radiant's XYZ Euler order.
	m4x4_t transform;
	m4x4_identity( transform );
	transform[0] = static_cast<float>( cy * cz + sy * sx * sz );
	transform[1] = static_cast<float>( cx * sz );
	transform[2] = static_cast<float>( -sy * cz + cy * sx * sz );
	transform[4] = static_cast<float>( -cy * sz + sy * sx * cz );
	transform[5] = static_cast<float>( cx * cz );
	transform[6] = static_cast<float>( sy * sz + cy * sx * cz );
	transform[8] = static_cast<float>( sy * cx );
	transform[9] = static_cast<float>( -sx );
	transform[10] = static_cast<float>( cy * cx );
	for ( int axis = 0; axis < 3; ++axis ) {
		transform[12 + axis] = origin[axis];
	}

	aabb_t local, result;
	for ( int axis = 0; axis < 3; ++axis ) {
		local.origin[axis] = static_cast<float>( ( static_cast<double>( mins[axis] ) + maxs[axis] ) * 0.5 );
		local.extents[axis] = static_cast<float>( ( static_cast<double>( maxs[axis] ) - mins[axis] ) * 0.5 );
	}

	aabb_for_transformed_aabb( &result, &local, transform );
	for ( int axis = 0; axis < 3; ++axis ) {
		const double low = static_cast<double>( result.origin[axis] ) - result.extents[axis];
		const double high = static_cast<double>( result.origin[axis] ) + result.extents[axis];
		if ( !( low >= -FLT_MAX && high <= FLT_MAX ) ) {
			return false;
	}
	}

	aabb_update_radius( &result );
	if ( !( result.radius >= 0 && result.radius <= FLT_MAX ) ) {
		return false;
	}

	bounds = result;
	return true;
}

#endif
