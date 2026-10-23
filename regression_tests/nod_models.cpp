/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_models.cpp
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

#include "nodmodel.h"
#include "NMBParser.h"
#include "../radiant/nodmodelreference.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <vector>

using Bytes = std::vector<unsigned char>;
using Model = std::unique_ptr<picoModel_t, decltype( &PicoFreeModel )>;

static void check( bool condition, const char *message ){
	if ( !condition ) {
		std::fprintf( stderr, "FAIL: %s\n", message );
		std::exit( 1 );
	}
}

static void put32( Bytes &data, std::size_t offset, uint32_t value ){
	for ( unsigned i = 0; i < 4; ++i ) data[offset + i] = static_cast<unsigned char>( value >> ( i * 8 ) );
}

static void attributes( Bytes &data, std::size_t offset, unsigned count, unsigned stride, float start ){
	for ( unsigned i = 0; i < count; ++i ) {
		const float vertex[] = { start + i, 20.0f + i * i, 30.0f + i, 0, 0, 1, i * 0.1f, i * 0.2f };
		std::memcpy( data.data() + offset + i * stride, vertex, sizeof( vertex ) );
	}
}

static Bytes nod( unsigned type ){
	const unsigned base[] = {32, 36, 48, 32, 36, 48};
	const unsigned stride[] = {32, 36, 48, 68, 72, 84};
	const std::size_t vertices = 124, indices = vertices + 7 * stride[type], groups = indices + 22;
	Bytes data( groups + 112 + 4, 0 );
	data[0] = 10;
	data[4] = data[6] = 1;
	data[7] = 2;
	put32( data, 0x24, type );
	put32( data, 0x28, 7 );
	put32( data, 0x44, 11 );
	std::memcpy( data.data() + 92, "test_material", 13 );
	attributes( data, vertices, 7, base[type], 10 );
	const uint16_t stream[] = {0, 1, 2, 3, 0xffff, 3, 4, 5, 0, 1, 2};
	std::memcpy( data.data() + indices, stream, sizeof( stream ) );
	data[groups + 28] = 1;
	data[groups + 56 + 4] = 8;
	data[groups + 56 + 6] = 3;
	data[groups + 56 + 8] = data[groups + 56 + 28] = 6;
	return data;
}

static void bundleEntry( Bytes &data, bool versionTwo, float start ){
	const std::size_t entry = data.size();
	data.resize( entry + 66 + 5 + 96 + 4 + 6 + 32 + 4 + 33, 0 );
	data[entry] = data[entry + 2] = data[entry + 37] = 1;
	std::memcpy( data.data() + entry + 5, "test_model", 10 );
	put32( data, entry + 67, 3 );
	attributes( data, entry + 71, 3, 32, start );
	put32( data, entry + 167, 3 );
	data[entry + 173] = 1;
	data[entry + 175] = 2;
	std::memcpy( data.data() + entry + 177, "test_material", 13 );
	data[entry + 220] = data[entry + 222] = 3;
	if ( versionTwo ) {
		data.push_back( 1 );
		for ( int i = 0; i < 3; ++i ) data.insert( data.end(), { 10, 20, 30, 255 } );
	}
}

static Model load( const char *name, int frame, const Bytes &data, std::string &error ){
	return Model( NodModelLoadBuffer( name, frame, data.data(), data.size(), error ), PicoFreeModel );
}

static void checkWinding( picoSurface_t *surface ){
	const float *a = PicoGetSurfaceXYZ( surface, PicoGetSurfaceIndex( surface, 0 ) );
	const float *b = PicoGetSurfaceXYZ( surface, PicoGetSurfaceIndex( surface, 1 ) );
	const float *c = PicoGetSurfaceXYZ( surface, PicoGetSurfaceIndex( surface, 2 ) );
	const float area = ( b[0] - a[0] ) * ( c[1] - a[1] ) - ( b[1] - a[1] ) * ( c[0] - a[0] );
	check( area < 0, "native exterior triangle must be clockwise for Radiant GL_FRONT culling" );
	check( PicoGetSurfaceNormal( surface, 0 )[2] == 1, "outward vertex normal must remain unchanged" );
}

static void synthetic(){
	std::string error;
	for ( unsigned type = 0; type < 6; ++type ) {
		const Bytes data = nod( type );
		auto model = load( "test.NOD", 0, data, error );
		check( bool( model ), error.c_str() );
		check( PicoGetModelNumSurfaces( model.get() ) == 1, "empty groups must not create surfaces" );
		auto *surface = PicoGetModelSurface( model.get(), 0 );
		check( PicoGetSurfaceNumVertexes( surface ) == 6, "compact group vertices" );
		check( PicoGetSurfaceNumIndexes( surface ) == 12, "list and restart-strip triangle count" );
		const int expected[] = {0, 2, 1, 0, 2, 1, 2, 3, 1, 3, 5, 4};
		for ( int i = 0; i < 12; ++i )
			check( PicoGetSurfaceIndex( surface, i ) == expected[i],
				"list, alternating strip and restarted strip must all reverse native winding" );
		checkWinding( surface );
		const float *xyz = PicoGetSurfaceXYZ( surface, 0 );
		check( xyz[0] == 11 && xyz[1] == 21 && xyz[2] == 31, "base vertex, base stride and Z-up coordinates" );
		const float *uv = PicoGetSurfaceST( surface, 0, 0 );
		check( uv[0] == 0.1f && uv[1] == 0.2f, "native UV direction" );
		check( !std::strcmp( PicoGetShaderName( PicoGetSurfaceShader( surface ) ), "textures/nod/test_material" ),
			"native material namespace" );
		check( !load( "test.nod", 1, data, error ) && !error.empty(), "NOD rejects animation frame selection" );
		Bytes truncated = data;
		truncated.resize( 100 );
		check( !load( "test.nod", 0, truncated, error ) && !error.empty(), "truncated NOD error" );
		Bytes nonfinite = data;
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const unsigned strides[] = {32, 36, 48, 32, 36, 48};
		std::memcpy( nonfinite.data() + 124 + strides[type], &nan, sizeof( nan ) );
		check( !load( "test.nod", 0, nonfinite, error ) && !error.empty(), "non-finite coordinates rejected" );
	}
	for ( unsigned version = 1; version <= 2; ++version ) {
		Bytes data( 12, 0 );
		put32( data, 0, version );
		put32( data, 4, 2 );
		bundleEntry( data, version == 2, 100 );
		bundleEntry( data, version == 2, 200 );
		auto model = load( "test.nmb", 1, data, error );
		check( bool( model ), error.c_str() );
		check( PicoGetModelNumFrames( model.get() ) == 2 && PicoGetModelFrameNum( model.get() ) == 1,
			"NMB model-entry selection" );
		auto *surface = PicoGetModelSurface( model.get(), 0 );
		checkWinding( surface );
		check( PicoGetSurfaceXYZ( surface, 0 )[0] == 200, "selected NMB model geometry" );
		if ( version == 2 )
			check( PicoGetSurfaceColor( surface, 0, 0 )[0] == 30, "packed NMB colour order" );
		check( !load( "test.nmb", -1, data, error ), "negative NMB selection" );
		check( !load( "test.nmb", 2, data, error ), "out-of-range NMB selection" );
		data.pop_back();
		check( !load( "test.nmb", 0, data, error ), "incomplete NMB is not a partial success" );
	}
	Bytes bundle( 12, 0 );
	put32( bundle, 0, 2 ); put32( bundle, 4, 2 );
	const size_t first = bundle.size();
	bundleEntry( bundle, true, 100 );
	const size_t second = bundle.size();
	bundleEntry( bundle, true, 200 );
	auto rename = [&]( size_t offset, const char *name ){
		std::memset( bundle.data() + offset + 5, 0, 32 );
		std::memcpy( bundle.data() + offset + 5, name, std::strlen( name ) );
	};
	rename( first, "first.nod" ); rename( second, "second.nod" );
	std::map<std::string, Bytes> files = {
		{ "3D/Models/mission.nmb", bundle }, { "3D/Models/second.NOD", nod( 0 ) }
	};
	unsigned reads = 0;
	NodModelSource source;
	source.assets = [&]( std::vector<std::string> &assets, std::string & ){
		for ( const auto &file : files ) assets.push_back( file.first ); return true;
	};
	source.load = [&]( const std::string &path, Bytes &bytes, std::string &issue ){
		++reads;
		auto file = files.find( path );
		if ( file == files.end() ) { issue = "Missing fixture"; return false; }
		bytes = file->second; return true;
	};
	source.identity = []( const std::string &path ){ return path; };
	NodModelReferences resolver;
	std::vector<NodModelResource> catalog;
	check( resolver.catalog( "MISSION.NMB", source, catalog, error ) && catalog.size() == 3, "catalog contains both bundle members and standalone file" );
	check( catalog[0].reference == "first.nod" && catalog[0].frame == 0 &&
		catalog[1].reference == "second.nod" && catalog[1].frame == 1 &&
		catalog[1].asset == "3D/Models/mission.nmb", "catalog preserves exact native member names and frames" );
	check( resolver.catalog( "", source, catalog, error ) && catalog.size() == 2, "standalone catalog requires no implicit bundle" );
	check( !resolver.catalog( "missing.nmb", source, catalog, error ) && catalog.empty() && !error.empty(), "failed catalog has no partial success" );
	check( !resolver.catalog( "../mission.nmb", source, catalog, error ) && catalog.empty(), "catalog validates the per-map bundle" );
	auto reference = [&]( const char *name, const char *selected ){
		return Model( resolver.load( NodModelReference_Request( name, selected ), source, error ), PicoFreeModel );
	};
	auto standalone = reference( "SECOND", "" );
	check( bool( standalone ), error.c_str() );
	check( PicoGetSurfaceXYZ( PicoGetModelSurface( standalone.get(), 0 ), 0 )[0] == 11, "extensionless, case-insensitive standalone reference" );
	auto selected = reference( "SeCoNd.NoD", "MISSION.NMB" );
	check( bool( selected ), error.c_str() );
	check( PicoGetModelFrameNum( selected.get() ) == 1 &&
		PicoGetSurfaceXYZ( PicoGetModelSurface( selected.get(), 0 ), 0 )[0] == 200, "explicit bundle member takes precedence over different standalone geometry" );
	const unsigned loaded = reads;
	check( bool( reference( "3d/models/first", "mission.nmb" ) ) && reads == loaded, "bundle parse reused across references" );
	check( !reference( "mission.nmb", "" ) && !error.empty(), "no arbitrary frame-zero selection for a bare multi-entry bundle" );
	check( !reference( "second", "missing.nmb" ) && !error.empty(), "missing explicit bundle never falls back silently" );
	check( !reference( "../second", "" ) && !error.empty(), "unsafe references rejected" );
	files["3D/Models/nested/second.nod"] = nod( 0 );
	check( !reference( "second", "" ) && !error.empty(), "ambiguous flat standalone references rejected" );
	rename( first, "second.nod" );
	files["3D/Models/mission.nmb"] = bundle;
	resolver.clear();
	check( !reference( "second", "mission.nmb" ) && !error.empty(), "duplicate bundle members rejected" );
	files["3D/Models/mission.nmb"].resize( 20 );
	resolver.clear();
	check( !resolver.catalog( "mission.nmb", source, catalog, error ) && catalog.empty() && !error.empty(), "corrupt catalog cannot become standalone fallback" );
	check( !reference( "second", "mission.nmb" ) && !error.empty(), "corrupt selected bundle never becomes standalone success" );
	check( !NodModelReference_Bundle( "../mission.nmb" ) && !NodModelReference_Bundle( "mission.nod" ), "bundle identity validation" );
	std::puts( "Native model conversion and per-map reference resolution regressions passed" );
}

static bool convertFile( const std::filesystem::path &path, unsigned &converted, unsigned &empty ){
	std::ifstream input( path, std::ios::binary | std::ios::ate );
	check( bool( input ), "open asset for read-only conversion" );
	const auto size = input.tellg();
	check( size >= 0 && size <= 256 * 1024 * 1024, "asset size limit" );
	Bytes data( static_cast<std::size_t>( size ) );
	input.seekg( 0 );
	input.read( reinterpret_cast<char*>( data.data() ), static_cast<std::streamsize>( data.size() ) );
	check( bool( input ), "complete asset read" );
	const std::string name = path.string();
	unsigned frames = 1;
	if ( path.extension() == ".nmb" ) {
		auto parser = std::make_unique<NMBParser>();
		check( parser->parseBuffer( data.data(), data.size() ) != 0, parser->getLastError().c_str() );
		frames = parser->getBundleData().parsed_count;
		if ( !frames ) ++empty;
	}
	bool ok = true;
	for ( unsigned frame = 0; frame < frames; ++frame ) {
		std::string error;
		auto model = load( name.c_str(), static_cast<int>( frame ), data, error );
		if ( model ) ++converted;
		else if ( error == "Selected native model has no LOD-0 triangles" ) ++empty;
		else {
			std::fprintf( stderr, "%s [%u]: %s\n", name.c_str(), frame, error.c_str() );
			ok = false;
		}
	}
	return ok;
}

int main( int argc, char **argv ){
	PicoSetMallocFunc( std::malloc );
	PicoSetFreeFunc( std::free );
	if ( argc == 1 ) {
		synthetic();
		return 0;
	}
	if ( ( argc == 5 && std::strcmp( argv[1], "--reference" ) == 0 ) ||
		( argc == 4 && std::strcmp( argv[1], "--bundle-references" ) == 0 ) ) {
		const std::filesystem::path root( argv[2] );
		NodModelSource source;
		source.assets = [&]( std::vector<std::string> &assets, std::string & ){
			for ( const char *directory : { "Models", "Terrains" } ) {
				const auto path = root / "3D" / directory;
				if ( !std::filesystem::exists( path ) ) continue;
				for ( const auto &entry : std::filesystem::recursive_directory_iterator( path ) )
					if ( entry.is_regular_file() && NodModelIsSupported( entry.path().string().c_str() ) )
						assets.push_back( std::filesystem::relative( entry.path(), root ).generic_string() );
			}
			return true;
		};
		source.load = [&]( const std::string &path, Bytes &bytes, std::string &issue ){
			std::ifstream stream( root / std::filesystem::u8path( path ), std::ios::binary );
			if ( !stream ) { issue = "Cannot read reference asset"; return false; }
			bytes.assign( std::istreambuf_iterator<char>( stream ), std::istreambuf_iterator<char>() );
			return !stream.bad();
		};
		source.identity = [&]( const std::string &path ){ return ( root / std::filesystem::u8path( path ) ).string(); };
		NodModelReferences resolver;
		std::string error;
		if ( argc == 4 ) {
			Bytes bytes;
			check( source.load( std::string( "3D/Models/" ) + argv[3], bytes, error ), error.c_str() );
			auto parser = std::make_unique<NMBParser>();
			check( parser->parseBuffer( bytes.data(), bytes.size() ) != 0, parser->getLastError().c_str() );
			const auto &bundle = parser->getBundleData();
			unsigned rendered = 0, empty = 0;
			for ( uint32_t frame = 0; frame < bundle.parsed_count; ++frame ) {
				Model model( resolver.load( NodModelReference_Request( bundle.models[frame].name, argv[3] ), source, error ), PicoFreeModel );
				if ( !model && error == "Selected native model has no LOD-0 triangles" ) { ++empty; continue; }
				check( bool( model ), error.c_str() );
				check( PicoGetModelFrameNum( model.get() ) == static_cast<int>( frame ), "exact original bundle member identity" );
				++rendered;
			}
			std::printf( "Original bundle references: %u rendered, %u empty, %u verified members\n", rendered, empty, bundle.parsed_count );
			return 0;
		}
		Model model( resolver.load( NodModelReference_Request( argv[4], std::strcmp( argv[3], "-" ) ? argv[3] : "" ), source, error ), PicoFreeModel );
		check( bool( model ), error.c_str() );
		std::printf( "Native reference resolved: %s, frame %d, %d surfaces\n",
			PicoGetModelFileName( model.get() ), PicoGetModelFrameNum( model.get() ), PicoGetModelNumSurfaces( model.get() ) );
		return 0;
	}
	unsigned files = 0, converted = 0, empty = 0, failures = 0;
	const std::filesystem::path root( argv[1] );
	if ( std::filesystem::is_directory( root ) ) {
		for ( const auto &entry : std::filesystem::recursive_directory_iterator( root ) ) {
			if ( !entry.is_regular_file() || !NodModelIsSupported( entry.path().string().c_str() ) ) continue;
			++files;
			if ( !convertFile( entry.path(), converted, empty ) ) ++failures;
		}
	}
	else {
		++files;
		if ( !convertFile( root, converted, empty ) ) ++failures;
	}
	std::printf( "Conversion sweep: %u files, %u models, %u empty entries, %u failed files\n",
		files, converted, empty, failures );
	return failures ? 1 : 0;
}
