/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodviewstorage.h
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

#ifndef RADIANT_NODVIEWSTORAGE_H
#define RADIANT_NODVIEWSTORAGE_H

#include "nodview.h"
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

inline std::string NodEditor_Attribute( xmlNodePtr node, const char *name ){
	xmlChar *value = xmlGetProp( node, BAD_CAST name );
	const std::string result = value ? reinterpret_cast<const char *>( value ) : "";
	xmlFree( value );
	return result;
}

inline bool NodEditor_Uint( const std::string &text, uint32_t &value ){
	if ( text.empty() ) {
		return false;
	}

	uint32_t result = 0;
	for ( size_t i = 0; i < text.size(); ++i ) {
		if ( text[i] < '0' || text[i] > '9' ) {
			return false;
		}

		const unsigned digit = text[i] - '0';
		if ( result > ( UINT32_MAX - digit ) / 10 ) {
			return false;
		}

		result = result * 10 + digit;
	}

	value = result;
	return true;
}

inline bool NodEditor_SourcePath( const std::string &path ){
	return path.empty() || ( path.size() <= 2048 && path.size() == strlen( path.c_str() ) &&
		g_path_is_absolute( path.c_str() ) && g_utf8_validate( path.c_str(), -1, NULL ) );
}

inline void NodEditor_SetUint( xmlNodePtr node, const char *name, uint32_t value ){
	char text[32];
	g_snprintf( text, sizeof( text ), "%u", static_cast<unsigned>( value ) );
	xmlSetProp( node, BAD_CAST name, BAD_CAST text );
}

inline bool NodEditor_ReadFile( const char *path, std::string &contents, bool missingOK, std::string &error ){
	gchar *bytes = NULL;
	gsize size = 0;
	GError *failure = NULL;
	if ( !g_file_get_contents( path, &bytes, &size, &failure ) ) {
		const bool missing = missingOK && failure && failure->domain == G_FILE_ERROR && failure->code == G_FILE_ERROR_NOENT;
		if ( !missing ) {
			error = failure ? failure->message : "Cannot read editor data.";
		}

		if ( failure ) {
			g_error_free( failure );
		}

		contents.clear();
		return missing;
	}

	contents.assign( bytes, size );
	g_free( bytes );
	return true;
}

inline std::string NodEditor_Hash( const std::string &bytes ){
	gchar *hash = g_compute_checksum_for_data( G_CHECKSUM_SHA256,
		reinterpret_cast<const guchar *>( bytes.data() ), bytes.size() );
	const std::string result = hash ? hash : "";
	g_free( hash );
	return result;
}

inline bool NodEditor_SaveLayers( const char *mapPath, const std::vector<int> &entityIds,
								  const NodViewState &state, std::string &error ){
	error.clear();
	if ( !NodEditor_SourcePath( state.nsdSource ) ) { error = "Invalid NSD source binding path."; return false; }
	std::string map;
	if ( !NodEditor_ReadFile( mapPath, map, false, error ) ) {
		return false;
	}

	const std::string hash = NodEditor_Hash( map );
	if ( hash.empty() ) { error = "Cannot hash the saved map."; return false; }
	xmlDocPtr doc = xmlNewDoc( BAD_CAST "1.0" );
	xmlNodePtr root = xmlNewNode( NULL, BAD_CAST "nod-editor" );
	xmlDocSetRootElement( doc, root );
	xmlSetProp( root, BAD_CAST "version", BAD_CAST "1" );
	xmlSetProp( root, BAD_CAST "map-sha256", BAD_CAST hash.c_str() );
	NodEditor_SetUint( root, "visible-layers", state.visibleLayers );
	NodEditor_SetUint( root, "show-static", state.showStatic ? 1 : 0 );
	if ( !state.nsdSource.empty() ) {
		xmlSetProp( root, BAD_CAST "nsd-source", BAD_CAST state.nsdSource.c_str() );
	}

	for ( int i = 0; i < 32; ++i ) {
		xmlNodePtr layer = xmlNewChild( root, NULL, BAD_CAST "layer", NULL );
		NodEditor_SetUint( layer, "index", i );
		xmlSetProp( layer, BAD_CAST "name", BAD_CAST state.names[i].c_str() );
	}

	for ( int i = 0; i < NOD_VIEW_CATEGORY_COUNT; ++i ) {
		xmlNodePtr category = xmlNewChild( root, NULL, BAD_CAST "category", NULL );
		NodEditor_SetUint( category, "index", i );
		NodEditor_SetUint( category, "visible", state.categories[i] ? 1 : 0 );
	}

	for ( size_t i = 0; i < entityIds.size(); ++i ) {
		const uint32_t mask = state.Membership( entityIds[i] );
		if ( !mask ) {
			continue;
		}

		xmlNodePtr member = xmlNewChild( root, NULL, BAD_CAST "member", NULL );
		NodEditor_SetUint( member, "entity", static_cast<uint32_t>( i ) );
		NodEditor_SetUint( member, "layers", mask );
	}

	xmlChar *buffer = NULL;
	int size = 0;
	xmlDocDumpFormatMemoryEnc( doc, &buffer, &size, "UTF-8", 1 );
	GError *failure = NULL;
	const std::string path = std::string( mapPath ) + ".nodeditor.xml";
	const bool saved = buffer && size > 0 && g_file_set_contents( path.c_str(),
		reinterpret_cast<const char *>( buffer ), size, &failure );
	if ( !saved ) {
		error = failure ? failure->message : "Cannot serialize editor data.";
	}

	if ( failure ) {
		g_error_free( failure );
	}

	xmlFree( buffer );
	xmlFreeDoc( doc );
	return saved;
}

inline bool NodEditor_LoadLayers( const char *mapPath, const std::vector<int> &entityIds,
								  NodViewState &state, std::string &error ){
	error.clear();
	std::string text, map;
	const std::string path = std::string( mapPath ) + ".nodeditor.xml";
	if ( !NodEditor_ReadFile( path.c_str(), text, true, error ) ) {
		return false;
	}

	if ( text.empty() ) {
		if ( g_file_test( path.c_str(), G_FILE_TEST_EXISTS ) ) {
			error = "The editor sidecar is empty."; return false;
		}

		return true;
	}

	if ( text.size() > 16 * 1024 * 1024 ) { error = "The editor sidecar exceeds 16 MiB."; return false; }
	if ( !NodEditor_ReadFile( mapPath, map, false, error ) ) {
		return false;
	}

	xmlDocPtr doc = xmlReadMemory( text.data(), static_cast<int>( text.size() ), path.c_str(), NULL, XML_PARSE_NONET );
	xmlNodePtr root = doc ? xmlDocGetRootElement( doc ) : NULL;
	if ( !root || doc->intSubset || doc->extSubset || !xmlStrEqual( root->name, BAD_CAST "nod-editor" )
		 || NodEditor_Attribute( root, "version" ) != "1" ) {
		if ( doc ) {
			xmlFreeDoc( doc );
		}

		error = "Invalid or unsupported editor sidecar."; return false;
	}

	if ( NodEditor_Attribute( root, "map-sha256" ) != NodEditor_Hash( map ) ) {
		xmlFreeDoc( doc );
		error = "MAP changed externally; mismatched editor sidecar was not applied.";
		return false;
	}

	NodViewState next;
	next.nsdSource = NodEditor_Attribute( root, "nsd-source" );
	if ( !NodEditor_SourcePath( next.nsdSource ) ) {
		xmlFreeDoc( doc ); error = "Invalid NSD source binding in the editor sidecar."; return false;
	}

	uint32_t visible = 0, showStatic = 0;
	bool valid = NodEditor_Uint( NodEditor_Attribute( root, "visible-layers" ), visible )
		&& NodEditor_Uint( NodEditor_Attribute( root, "show-static" ), showStatic ) && showStatic <= 1;
	std::set<uint32_t> layers, categories, members;
	for ( xmlNodePtr node = root->children; valid && node; node = node->next ) {
		if ( node->type != XML_ELEMENT_NODE ) {
			continue;
		}

		uint32_t index = 0, value = 0;
		if ( xmlStrEqual( node->name, BAD_CAST "layer" ) ) {
			valid = NodEditor_Uint( NodEditor_Attribute( node, "index" ), index ) && index < 32
				&& layers.insert( index ).second;
			if ( valid ) {
				valid = next.Rename( index, NodEditor_Attribute( node, "name" ).c_str() ) == NULL;
		}
		} else if ( xmlStrEqual( node->name, BAD_CAST "category" ) ) {
			valid = NodEditor_Uint( NodEditor_Attribute( node, "index" ), index ) && index < NOD_VIEW_CATEGORY_COUNT
				&& categories.insert( index ).second && NodEditor_Uint( NodEditor_Attribute( node, "visible" ), value ) && value <= 1;
			if ( valid ) {
				next.categories[index] = value != 0;
		}
		} else if ( xmlStrEqual( node->name, BAD_CAST "member" ) ) {
			valid = NodEditor_Uint( NodEditor_Attribute( node, "entity" ), index ) && index < entityIds.size()
				&& entityIds[index] > 0 && members.insert( index ).second
				&& NodEditor_Uint( NodEditor_Attribute( node, "layers" ), value ) && value != 0;
			if ( valid ) {
				std::set<int> ids;
				ids.insert( entityIds[index] );
				for ( int bit = 0; bit < 32; ++bit ) {
					if ( value & ( uint32_t( 1 ) << bit ) ) {
						next.Assign( ids, bit, true );
			}
		}
			}
		} else {
			valid = false;
	}
	}

	valid = valid && layers.size() == 32 && categories.size() == NOD_VIEW_CATEGORY_COUNT;
	xmlFreeDoc( doc );
	if ( !valid ) { error = "The editor sidecar contains invalid or duplicate editor data."; return false; }
	next.visibleLayers = visible;
	next.showStatic = showStatic != 0;
	next.ClearHistory();
	state = next;
	return true;
}

#endif
