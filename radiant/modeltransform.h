#ifndef RADIANT_MODELTRANSFORM_H
#define RADIANT_MODELTRANSFORM_H

#include <ctype.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "mathlib.h"

inline bool ModelTransform_IsEntity( const char *classname ){
	return strcmp( classname, "misc_model" ) == 0
		|| strcmp( classname, "misc_gamemodel" ) == 0
		|| strcmp( classname, "model_static" ) == 0;
}

inline bool ModelTransform_Parse( const char *text, int components, float values[3], bool allowZero = false ){
	if ( components != 1 && components != 3 ) {
		return false;
	}
	float parsed[3];
	for ( int i = 0; i < components; ++i ) {
		while ( isspace( static_cast<unsigned char>( *text ) ) ) {
			++text;
		}
		char *end;
		double value = strtod( text, &end );
		if ( end == text || !( value >= -FLT_MAX && value <= FLT_MAX ) ) {
			return false;
		}
		parsed[i] = static_cast<float>( value );
		if ( ( parsed[i] == 0 && !allowZero ) || ( *end != '\0' && !isspace( static_cast<unsigned char>( *end ) ) ) ) {
			return false;
		}
		text = end;
	}
	while ( isspace( static_cast<unsigned char>( *text ) ) ) {
		++text;
	}
	if ( *text != '\0' ) {
		return false;
	}
	for ( int i = 0; i < 3; ++i ) {
		values[i] = parsed[components == 1 ? 0 : i];
	}
	return true;
}

inline bool ModelScale_Scaled( const char *uniform, const char *axes, const float factors[3], float scale[3] ){
	float current[3] = { 1, 1, 1 };
	if ( axes[0] != '\0' ) {
		if ( !ModelTransform_Parse( axes, 3, current ) ) {
			return false;
		}
	}
	else if ( uniform[0] != '\0' && !ModelTransform_Parse( uniform, 1, current ) ) {
		return false;
	}

	float scaled[3];
	for ( int i = 0; i < 3; ++i ) {
		if ( !( factors[i] > 0 && factors[i] <= FLT_MAX ) ) {
			return false;
		}
		double value = static_cast<double>( current[i] ) * factors[i];
		if ( !( value >= -FLT_MAX && value <= FLT_MAX ) ) {
			return false;
		}
		scaled[i] = static_cast<float>( value );
		if ( scaled[i] == 0 ) {
			return false;
		}
	}
	for ( int i = 0; i < 3; ++i ) {
		scale[i] = scaled[i];
	}
	return true;
}

inline bool ModelRotation_Rotated( const float euler[3], int axis, float degrees, float angles[3] ){
	if ( axis < 0 || axis > 2 || !( degrees >= -FLT_MAX && degrees <= FLT_MAX ) ) {
		return false;
	}
	for ( int i = 0; i < 3; ++i ) {
		if ( !( euler[i] >= -FLT_MAX && euler[i] <= FLT_MAX ) ) {
			return false;
		}
	}
	vec3_t delta = { 0, 0, 0 };
	// Match Select_RotateAxis's world-axis directions, not Euler-component addition.
	delta[axis] = static_cast<float>( fmod( degrees, 360.0 ) ) * ( axis == 1 ? 1 : -1 );
	m4x4_t matrix, rotation;
	m4x4_rotation_for_vec3( matrix, euler, eXYZ );
	m4x4_rotation_for_vec3( rotation, delta, eXYZ );
	m4x4_premultiply_by_m4x4( matrix, rotation );

	double horizontal = sqrt( static_cast<double>( matrix[0] ) * matrix[0]
							  + static_cast<double>( matrix[1] ) * matrix[1] );
	float result[3];
	result[0] = static_cast<float>( atan2( -matrix[2], horizontal ) * 180.0 / Q_PI );
	if ( horizontal > 1e-6 ) {
		result[1] = static_cast<float>( atan2( matrix[1], matrix[0] ) * 180.0 / Q_PI );
		result[2] = static_cast<float>( atan2( matrix[6], matrix[10] ) * 180.0 / Q_PI );
	}
	else {
		// At gimbal lock, a zero roll and the combined yaw preserve the same matrix.
		result[1] = static_cast<float>( atan2( -matrix[4], matrix[5] ) * 180.0 / Q_PI );
		result[2] = 0;
	}
	for ( int i = 0; i < 3; ++i ) {
		if ( !( result[i] >= -FLT_MAX && result[i] <= FLT_MAX ) ) {
			return false;
		}
	}
	for ( int i = 0; i < 3; ++i ) {
		angles[i] = result[i];
	}
	return true;
}

#endif
