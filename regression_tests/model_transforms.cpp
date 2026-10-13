#include "../radiant/modeltransform.h"

#include <assert.h>
#include <stdio.h>
#include <limits>

static void check( const char *uniform, const char *axes, float x, float y, float z,
				   float expectedX, float expectedY, float expectedZ ){
	const float factors[3] = { x, y, z };
	float scale[3];
	assert( ModelScale_Scaled( uniform, axes, factors, scale ) );
	assert( scale[0] == expectedX && scale[1] == expectedY && scale[2] == expectedZ );
	char saved[128];
	sprintf( saved, "%.9g %.9g %.9g", scale[0], scale[1], scale[2] );
	float restored[3];
	assert( ModelTransform_Parse( saved, 3, restored ) );
	for ( int i = 0; i < 3; ++i ) {
		assert( restored[i] == scale[i] );
	}
}

static double maximumRotationError = 0;
static unsigned int rotationCases = 0;

static void checkRotation( const float euler[3], int axis, float degrees ){
	float angles[3];
	assert( ModelRotation_Rotated( euler, axis, degrees, angles ) );
	char saved[128];
	sprintf( saved, "%.9g %.9g %.9g", angles[0], angles[1], angles[2] );
	float restored[3];
	assert( ModelTransform_Parse( saved, 3, restored, true ) );
	vec3_t rotatedEuler = { restored[2], restored[0], restored[1] };
	m4x4_t before, after;
	m4x4_rotation_for_vec3( before, euler, eXYZ );
	m4x4_rotation_for_vec3( after, rotatedEuler, eXYZ );

	double c = cos( degrees * Q_PI / 180.0 );
	double s = sin( degrees * Q_PI / 180.0 );
	for ( int column = 0; column < 3; ++column ) {
		double x = before[column * 4], y = before[column * 4 + 1], z = before[column * 4 + 2];
		double expected[3] = { x, y, z };
		switch ( axis ) {
		case 0: expected[1] = c * y + s * z; expected[2] = -s * y + c * z; break;
		case 1: expected[0] = c * x + s * z; expected[2] = -s * x + c * z; break;
		case 2: expected[0] = c * x + s * y; expected[1] = -s * x + c * y; break;
		}
		for ( int row = 0; row < 3; ++row ) {
			double error = fabs( after[column * 4 + row] - expected[row] );
			if ( error > maximumRotationError ) {
				maximumRotationError = error;
			}
			assert( error < 0.00003 );
		}
	}
	++rotationCases;
}

int main(){
	assert( ModelTransform_IsEntity( "misc_model" ) );
	assert( ModelTransform_IsEntity( "misc_gamemodel" ) );
	assert( ModelTransform_IsEntity( "model_static" ) );
	assert( !ModelTransform_IsEntity( "ghost" ) );
	assert( !ModelTransform_IsEntity( "worldspawn" ) );
	check( "", "", 2, 3, 4, 2, 3, 4 );
	check( "32", "", 1.5f, 1.5f, 1.5f, 48, 48, 48 );
	check( "32", "", 1.1f, 1.1f, 1.1f, 35.2f, 35.2f, 35.2f );
	check( "32", "", 0.9f, 0.9f, 0.9f, 28.8f, 28.8f, 28.8f );
	check( "32", "", 2, 1, 0.5f, 64, 32, 16 );
	check( "64", "32 16 8", 2, 3, 4, 64, 48, 32 );
	check( "invalid", "32 32 32", 0.5f, 0.5f, 0.5f, 16, 16, 16 );
	check( "", " 32\t16\n8 ", 1, 1, 1, 32, 16, 8 );
	check( "-32", "", 2, 1, 1, -64, -32, -32 );
	check( "", "-32 16 8", 0.5f, 1, 1, -16, 16, 8 );

	const char *invalid[] = { "0", "nan", "inf", "-inf", "1e100", "1e-100", "garbage", "32junk", "32", "32 16", "32 0 16", "32 nan 16", "32 16 8 4", "32-16 8" };
	float output[3] = { 7, 8, 9 };
	const float factors[3] = { 2, 2, 2 };
	for ( unsigned int i = 0; i < sizeof( invalid ) / sizeof( invalid[0] ); ++i ) {
		assert( !ModelScale_Scaled( "", invalid[i], factors, output ) );
		assert( output[0] == 7 && output[1] == 8 && output[2] == 9 );
	}
	assert( !ModelScale_Scaled( "0", "", factors, output ) );
	assert( !ModelScale_Scaled( "nan", "", factors, output ) );
	assert( !ModelScale_Scaled( "32 32", "", factors, output ) );
	assert( ModelScale_Scaled( "1e38", "", factors, output ) );
	const float overflow[3] = { 10, 1, 1 };
	assert( !ModelScale_Scaled( "1e38", "", overflow, output ) );
	const float zero[3] = { 0, 1, 1 };
	const float negative[3] = { -1, 1, 1 };
	const float infinite[3] = { std::numeric_limits<float>::infinity(), 1, 1 };
	const float nan[3] = { std::numeric_limits<float>::quiet_NaN(), 1, 1 };
	assert( !ModelScale_Scaled( "32", "", zero, output ) );
	assert( !ModelScale_Scaled( "32", "", negative, output ) );
	assert( !ModelScale_Scaled( "32", "", infinite, output ) );
	assert( !ModelScale_Scaled( "32", "", nan, output ) );
	const float tiny[3] = { 1e-10f, 1, 1 };
	assert( !ModelScale_Scaled( "1e-38", "", tiny, output ) );
	assert( !ModelTransform_Parse( "32", 2, output ) );

	const float orientations[][3] = {
		{ 0, 0, 0 }, { 0, 0, 45 }, { 20, 30, 40 }, { -35, 75, 120 },
		{ 20, 90, 40 }, { 20, -90, 40 }, { 20, 89.999f, 40 }, { 20, -89.999f, 40 },
		{ 120, -435, 360 }
	};
	const float increments[] = { 0, 90, -90, 45, -27.5f, 180, 359, 360 };
	for ( unsigned int i = 0; i < sizeof( orientations ) / sizeof( orientations[0] ); ++i ) {
		for ( int axis = 0; axis < 3; ++axis ) {
			for ( unsigned int j = 0; j < sizeof( increments ) / sizeof( increments[0] ); ++j ) {
				checkRotation( orientations[i], axis, increments[j] );
			}
		}
	}
	float angles[3];
	const float identity[3] = { 0, 0, 0 };
	assert( ModelRotation_Rotated( identity, 2, 90, angles ) );
	assert( fabs( angles[1] + 90 ) < 0.00003 );
	assert( ModelTransform_Parse( "0 0 0", 3, angles, true ) );
	assert( ModelTransform_Parse( "0", 1, angles, true ) );
	assert( !ModelTransform_Parse( "0 90", 3, angles, true ) );
	assert( !ModelTransform_Parse( "0 nan 0", 3, angles, true ) );
	assert( !ModelRotation_Rotated( identity, -1, 90, angles ) );
	assert( !ModelRotation_Rotated( identity, 3, 90, angles ) );
	assert( !ModelRotation_Rotated( identity, 0, std::numeric_limits<float>::infinity(), angles ) );
	assert( !ModelRotation_Rotated( identity, 0, std::numeric_limits<float>::quiet_NaN(), angles ) );
	const float invalidEuler[3] = { 0, std::numeric_limits<float>::quiet_NaN(), 0 };
	assert( !ModelRotation_Rotated( invalidEuler, 0, 90, angles ) );

	printf( "Rotation matrices verified: %u cases, maximum component error %.9g (limit 0.00003).\n",
			rotationCases, maximumRotationError );
	puts( "Model transform tests passed: scale precedence/factors, rotation world axes and signs, mixed orientations, gimbal lock, key round-trip and invalid-input rejection." );
	return 0;
}
