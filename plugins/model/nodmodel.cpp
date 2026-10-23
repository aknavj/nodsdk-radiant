/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodmodel.cpp
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
#include "NODParser.h"
#include "NMBParser.h"
#include "../../radiant/nodmodelreference.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

bool NodModelExtensionSupported( const char *extension ){
	if ( !extension ) {
		return false;
	}

	std::string lower( extension );
	for ( char &c : lower ) {
		c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
	}

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
			if ( a == b || b == c || a == c ) {
				return true;
			}

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
			if ( !triangle( indices[0], indices[1], indices[2] ) ) {
				return NULL;
		}
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

			if ( !triangle( previous[parity & 1], previous[1 - ( parity & 1 )], index ) ) {
				return NULL;
			}

			++parity;
			previous[0] = previous[1];
			previous[1] = index;
		}

		if ( triangles.empty() ) {
			continue;
		}

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
		for ( uint32_t vs = 0; vs < group.vertex_set_index; ++vs ) {
			colorOffset += data.vertex_sets[vs].count;
		}

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

static bool NativeAssetIndex( const NodModelSource &source, std::vector<std::string> &assets, std::string &error ){
	if ( !source.assets( assets, error ) ) {
		return false;
	}

	for ( const std::string &asset : assets ) {
		if ( !NodModelReference_Path( asset ) || !NodModelIsSupported( asset.c_str() ) ) {
			error = "Invalid native asset index entry"; return false;
		}
	}

	return true;
}

static unsigned FindNativeAsset( const std::vector<std::string> &assets, const std::string &name, std::string &selected ){
	unsigned matches = 0;
	for ( const std::string &asset : assets ) {
		const std::string candidate = name.find( '/' ) == std::string::npos ?
			asset.substr( asset.find_last_of( '/' ) + 1 ) : asset;
		if ( NodModelReference_Matches( name, candidate ) ) { selected = asset; ++matches; }
	}

	return matches;
}

std::shared_ptr<NMBParser> NodModelReferences::loadBundle( const std::string &path,
	const NodModelSource &source, std::string &error ){
	const std::string identity = source.identity( path );
	auto found = bundles.find( identity );
	if ( !identity.empty() && found != bundles.end() ) {
		return found->second;
	}

	std::vector<unsigned char> bytes;
	if ( !source.load( path, bytes, error ) ) {
		return std::shared_ptr<NMBParser>();
	}

	auto parser = std::make_shared<NMBParser>();
	if ( !parser->parseBuffer( bytes.data(), bytes.size() ) ) {
		error = parser->getLastError(); return std::shared_ptr<NMBParser>();
	}

	if ( !identity.empty() ) {
		if ( bundles.size() >= 4 ) {
			bundles.clear();
		}

		bundles[identity] = parser;
	}

	return parser;
}

bool NodModelReferences::catalog( const std::string &bundle, const NodModelSource &source,
	std::vector<NodModelResource> &resources, std::string &error ){
	resources.clear(); error.clear();
	if ( !NodModelReference_Bundle( bundle ) ) { error = "Invalid per-map model bundle"; return false; }
	std::vector<std::string> assets;
	if ( !NativeAssetIndex( source, assets, error ) ) {
		return false;
	}

	std::string selected;
	if ( !bundle.empty() ) {
		if ( FindNativeAsset( assets, "3D/Models/" + bundle, selected ) != 1 ) {
			error = "Selected per-map bundle is missing or ambiguous"; return false;
		}

		auto parser = loadBundle( selected, source, error );
		if ( !parser ) {
			return false;
		}

		const auto &data = parser->getBundleData();
		for ( uint32_t i = 0; i < data.parsed_count; ++i ) {
			resources.push_back( { data.models[i].name, selected, static_cast<int>( i ) } );
	}
	}

	for ( const std::string &asset : assets ) {
		if ( g_ascii_strcasecmp( asset.c_str(), selected.c_str() ) != 0 ) {
			resources.push_back( { asset, asset,
				g_ascii_strcasecmp( asset.c_str() + asset.size() - 4, ".nmb" ) == 0 ? -1 : 0 } );
		}
	}

	return true;
}

picoModel_t *NodModelReferences::load( const std::string &request, const NodModelSource &source, std::string &error ){
	error.clear();
	std::string reference, bundle;
	if ( !NodModelReference_Decode( request, reference, bundle ) ) {
		error = "Invalid native model reference or per-map bundle"; return NULL;
	}

	std::vector<std::string> assets;
	if ( !NativeAssetIndex( source, assets, error ) ) {
		return NULL;
	}

	if ( !bundle.empty() ) {
		std::string path;
		if ( FindNativeAsset( assets, "3D/Models/" + bundle, path ) != 1 ) {
			error = "Selected per-map bundle is missing or ambiguous"; return NULL;
		}

		auto parser = loadBundle( path, source, error );
		if ( !parser ) {
			return NULL;
		}

		const bool flat = reference.find( '/' ) == std::string::npos ||
			( g_ascii_strncasecmp( reference.c_str(), "3D/Models/", 10 ) == 0 &&
			  reference.find( '/', 10 ) == std::string::npos );
		unsigned matches = 0, frame = 0;
		const auto &data = parser->getBundleData();
		if ( flat ) {
			for ( uint32_t i = 0; i < data.parsed_count; ++i ) {
				if ( NodModelReference_Matches( reference.substr( reference.find_last_of( '/' ) + 1 ), data.models[i].name ) ) {
				++matches; frame = i;
			}
			}
		}

		if ( matches > 1 ) { error = "Ambiguous native bundle member"; return NULL; }
		if ( matches == 1 ) {
			const auto &entry = data.models[frame];
			return BuildModel( entry, path.c_str(), frame, static_cast<int>( data.parsed_count ), entry.vertex_colors, error );
		}
	}

	std::string path;
	const unsigned matches = FindNativeAsset( assets, reference, path );
	if ( matches != 1 ) {
		error = matches ? "Ambiguous standalone native model reference" : "Native model reference not found";
		return NULL;
	}

	if ( g_ascii_strcasecmp( path.c_str() + path.size() - 4, ".nmb" ) == 0 ) {
		auto parser = loadBundle( path, source, error );
		if ( !parser ) {
			return NULL;
		}

		const auto &data = parser->getBundleData();
		if ( data.parsed_count != 1 ) { error = "A multi-entry NMB requires an explicit member reference and per-map bundle"; return NULL; }
		return BuildModel( data.models[0], path.c_str(), 0, 1, data.models[0].vertex_colors, error );
	}

	std::vector<unsigned char> bytes;
	if ( !source.load( path, bytes, error ) ) {
		return NULL;
	}

	return NodModelLoadBuffer( path.c_str(), 0, bytes.data(), bytes.size(), error );
}
