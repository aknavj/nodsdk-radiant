#include "nodmodel.h"
#include "NODParser.h"
#include "NMBParser.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

bool NodModelExtensionSupported( const char *extension ){
	if ( !extension ) return false;
	std::string lower( extension );
	for ( char &c : lower ) c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
	return lower == "nod" || lower == "nmb";
}

bool NodModelIsSupported( const char *name ){
	const char *extension = name ? std::strrchr( name, '.' ) : NULL;
	return extension && NodModelExtensionSupported( extension + 1 );
}

namespace {

template<typename Model>
picoModel_t *BuildModel( const Model &data, const char *name, int frame, int frames,
	const unsigned char *colors, std::string &error ){
	std::unique_ptr<picoModel_t, decltype( &PicoFreeModel )> model( PicoNewModel(), PicoFreeModel );
	if ( !model ) {
		error = "Cannot allocate PicoModel";
		return NULL;
	}
	std::string filename( name );
	PicoSetModelName( model.get(), filename.data() );
	PicoSetModelFileName( model.get(), filename.data() );
	PicoSetModelFrameNum( model.get(), frame );
	PicoSetModelNumFrames( model.get(), frames );

	for ( uint32_t g = 0; g < data.mesh_group_count; ++g ) {
		const auto &group = data.mesh_groups[g];
		const auto &lod = group.lods[0];
		if ( group.vertex_set_index >= data.vertex_set_count || group.shader_index >= data.shader_count ) {
			error = "Native mesh references an invalid vertex set or shader";
			return NULL;
		}
		const auto &set = data.vertex_sets[group.vertex_set_index];
		if ( group.base_vertex > set.count ||
			lod.list_start > data.index_count || lod.list_count > data.index_count - lod.list_start ||
			lod.strip_start > data.index_count || lod.strip_count > data.index_count - lod.strip_start ) {
			error = "Native mesh index range extends beyond its buffer";
			return NULL;
		}
		if ( lod.list_count % 3 ) {
			error = "LOD-0 triangle list is not a multiple of three";
			return NULL;
		}
		std::vector<uint32_t> triangles;
		auto triangle = [&]( uint16_t a, uint16_t b, uint16_t c ) {
			if ( a == b || b == c || a == c ) return true;
			for ( uint16_t index : { a, b, c } ) {
				if ( static_cast<uint32_t>( index ) + group.base_vertex >= set.count ) {
					error = "LOD-0 triangle references a vertex outside its set";
					return false;
				}
			}
			triangles.insert( triangles.end(), { a, b, c } );
			return true;
		};
		for ( uint32_t i = 0; i < lod.list_count; i += 3 ) {
			const uint16_t *indices = data.indices + lod.list_start + i;
			if ( !triangle( indices[0], indices[1], indices[2] ) ) return NULL;
		}
		uint16_t previous[2] = {};
		unsigned count = 0, parity = 0;
		for ( uint32_t i = 0; i < lod.strip_count; ++i ) {
			const uint16_t index = data.indices[lod.strip_start + i];
			if ( index == 0xffff ) {
				count = parity = 0;
				continue;
			}
			if ( count < 2 ) {
				previous[count++] = index;
				continue;
			}
			if ( !triangle( previous[parity & 1], previous[1 - ( parity & 1 )], index ) ) return NULL;
			++parity;
			previous[0] = previous[1];
			previous[1] = index;
		}
		if ( triangles.empty() ) continue;

		std::vector<int> remap( set.count, -1 );
		std::vector<uint32_t> vertices;
		for ( uint32_t &index : triangles ) {
			const uint32_t source = group.base_vertex + index;
			if ( remap[source] < 0 ) {
				remap[source] = static_cast<int>( vertices.size() );
				vertices.push_back( source );
			}
			index = static_cast<uint32_t>( remap[source] );
		}
		picoSurface_t *surface = PicoNewSurface( model.get() );
		picoShader_t *shader = PicoNewShader( model.get() );
		if ( !surface || !shader ||
			!PicoAdjustSurface( surface, static_cast<int>( vertices.size() ), 1, 1,
				static_cast<int>( triangles.size() ), 0 ) ) {
			error = "Cannot allocate native model surface";
			return NULL;
		}
		char surfaceName[32];
		std::snprintf( surfaceName, sizeof( surfaceName ), "nod_group_%u", g );
		std::string material = "textures/nod/";
		material += data.shader_names[group.shader_index];
		PicoSetSurfaceType( surface, PICO_TRIANGLES );
		PicoSetSurfaceName( surface, surfaceName );
		PicoSetShaderName( shader, material.data() );
		PicoSetSurfaceShader( surface, shader );

		uint32_t colorOffset = 0;
		for ( uint32_t vs = 0; vs < group.vertex_set_index; ++vs )
			colorOffset += data.vertex_sets[vs].count;
		for ( std::size_t i = 0; i < vertices.size(); ++i ) {
			// Tangents occupy a separate stream; only base_stride addresses vertex attributes.
			const std::size_t offset = set.data_offset +
				static_cast<std::size_t>( vertices[i] ) * set.base_stride;
			if ( set.base_stride < 32 || offset > data.vertex_data_size ||
				data.vertex_data_size - offset < 32 ) {
				error = "Native vertex attributes extend beyond their buffer";
				return NULL;
			}
			float attributes[8];
			std::memcpy( attributes, data.vertex_data + offset, sizeof( attributes ) );
			for ( float value : attributes ) {
				if ( !std::isfinite( value ) ) {
					error = "Native model contains non-finite vertex attributes";
					return NULL;
				}
			}
			const int vertex = static_cast<int>( i );
			PicoSetSurfaceXYZ( surface, vertex, attributes );
			PicoSetSurfaceNormal( surface, vertex, attributes + 3 );
			PicoSetSurfaceST( surface, 0, vertex, attributes + 6 );
			picoColor_t color = { 255, 255, 255, 255 };
			if ( colors ) {
				const unsigned char *packed = colors + static_cast<std::size_t>( colorOffset + vertices[i] ) * 4;
				// Xbox D3DCOLOR is packed ARGB, hence BGRA bytes in little-endian files.
				color[0] = packed[2]; color[1] = packed[1]; color[2] = packed[0]; color[3] = packed[3];
			}
			PicoSetSurfaceColor( surface, 0, vertex, color );
		}
		// Radiant culls GL_FRONT: native counterclockwise faces must become clockwise.
		for ( std::size_t i = 0; i < triangles.size(); i += 3 ) {
			PicoSetSurfaceIndex( surface, static_cast<int>( i ), triangles[i] );
			PicoSetSurfaceIndex( surface, static_cast<int>( i + 1 ), triangles[i + 2] );
			PicoSetSurfaceIndex( surface, static_cast<int>( i + 2 ), triangles[i + 1] );
		}
	}
	if ( PicoGetModelNumSurfaces( model.get() ) == 0 ) {
		error = "Selected native model has no LOD-0 triangles";
		return NULL;
	}
	return model.release();
}

}

picoModel_t *NodModelLoadBuffer( const char *name, int frame,
	const unsigned char *data, std::size_t size, std::string &error ){
	error.clear();
	if ( !NodModelIsSupported( name ) ) {
		error = "Unsupported native model extension";
		return NULL;
	}
	const char *extension = std::strrchr( name, '.' ) + 1;
	if ( std::tolower( static_cast<unsigned char>( extension[2] ) ) == 'd' ) {
		if ( frame != 0 ) {
			error = "NOD rest-pose models require _frame 0";
			return NULL;
		}
		NODParser parser;
		if ( !parser.parseBuffer( data, size ) ) {
			error = parser.getLastError();
			return NULL;
		}
		return BuildModel( parser.getModelData(), name, 0, 1, NULL, error );
	}
	auto parser = std::make_unique<NMBParser>();
	if ( !parser->parseBuffer( data, size ) ) {
		error = parser->getLastError();
		return NULL;
	}
	const auto &bundle = parser->getBundleData();
	if ( frame < 0 || static_cast<uint32_t>( frame ) >= bundle.parsed_count ) {
		error = "NMB _frame is outside the bundle's model-entry range";
		return NULL;
	}
	const auto &entry = bundle.models[frame];
	return BuildModel( entry, name, frame, static_cast<int>( bundle.parsed_count ), entry.vertex_colors, error );
}
