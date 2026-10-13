#include "nodmodel.h"
#include "NMBParser.h"

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
	std::puts( "Native model conversion regressions passed" );
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
