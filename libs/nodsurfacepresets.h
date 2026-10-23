/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodsurfacepresets.h
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

#ifndef RADIANT_NODSURFACEPRESETS_H
#define RADIANT_NODSURFACEPRESETS_H

#include "nodsurface.h"
#include <map>
#include <errno.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/*
 */
inline const char *NodPreset_ValidateName( const std::string &name ){
	if ( name.empty() || name.size() > 63 ) {
		return "Preset names require 1 through 63 bytes.";
	}

	bool nonSpace = false;
	for ( size_t i = 0; i < name.size(); ++i ) {
		const unsigned char c = static_cast<unsigned char>( name[i] );
		if ( c < 32 || c == 127 ) {
			return "Preset names cannot contain control characters.";
		}

		if ( c != ' ' ) {
			nonSpace = true;
	}
	}

	return nonSpace ? NULL : "Preset names cannot consist only of spaces.";
}

/*
 */
class NodSurfacePresetLibrary
{
	static std::string attribute( xmlNodePtr node, const char *name ){
		xmlChar *text = xmlGetProp( node, BAD_CAST name );
		const std::string value = text ? reinterpret_cast<const char *>( text ) : "";
		xmlFree( text );
		return value;
	}
	
	static bool number( xmlNodePtr node, const char *name, float &value ){
		const std::string text = attribute( node, name );
		if ( text.empty() ) {
			return false;
		}

		char *end;
		errno = 0;
		const double result = g_ascii_strtod( text.c_str(), &end );
		if ( *end || errno == ERANGE || !( result >= -FLT_MAX && result <= FLT_MAX ) ) {
			return false;
		}

		value = static_cast<float>( result );
		return value != 0 || result == 0;
	}

public:
	std::map<std::string, NodSurfacePreset> presets;

	bool Load( const char *path, std::string &error ){
		error.clear();
		GError *failure = NULL;
		gchar *bytes = NULL;
		gsize size = 0;

		if ( !g_file_get_contents( path, &bytes, &size, &failure ) ) {
			const bool missing = failure && failure->domain == G_FILE_ERROR && failure->code == G_FILE_ERROR_NOENT;
			if ( !missing ) {
				error = failure ? failure->message : "Cannot read surface presets.";
			}

			if ( failure ) {
				g_error_free( failure );
			}

			if ( missing ) {
				presets.clear();
			}

			return missing;
		}

		xmlDocPtr doc = size > 0 && size <= 1024 * 1024
			? xmlReadMemory( bytes, static_cast<int>( size ), path, NULL, XML_PARSE_NONET ) : NULL;
		g_free( bytes );

		xmlNodePtr root = doc ? xmlDocGetRootElement( doc ) : NULL;
		bool valid = root && !doc->intSubset && !doc->extSubset
			&& xmlStrEqual( root->name, BAD_CAST "nod-surface-presets" ) && attribute( root, "version" ) == "1";
		std::map<std::string, NodSurfacePreset> next;

		for ( xmlNodePtr node = valid ? root->children : NULL; valid && node; node = node->next ) {
			if ( node->type != XML_ELEMENT_NODE ) {
				continue;
			}

			NodSurfacePreset value;
			const std::string name = attribute( node, "name" );
			value.material = attribute( node, "material" );
			valid = xmlStrEqual( node->name, BAD_CAST "preset" ) && !NodPreset_ValidateName( name )
				&& number( node, "shift-s", value.shift[0] ) && number( node, "shift-t", value.shift[1] )
				&& number( node, "scale-s", value.scale[0] ) && number( node, "scale-t", value.scale[1] )
				&& number( node, "rotation", value.rotation ) && !NodSurface_Validate( value, true, true )
				&& next.insert( std::make_pair( name, value ) ).second && next.size() <= 256;
		}

		if ( doc ) {
			xmlFreeDoc( doc );
		}

		if ( !valid ) { error = "Invalid or unsupported surface preset library; existing presets were retained."; return false; }

		presets.swap( next );
		return true;
	}

	bool Save( const char *path, std::string &error ) const {

		error.clear();

		if ( presets.size() > 256 ) { 
			error = "A preset library supports at most 256 entries."; 
			return false; 
		}

		for ( std::map<std::string, NodSurfacePreset>::const_iterator i = presets.begin(); i != presets.end(); ++i ) {
			const char *invalid = NodPreset_ValidateName( i->first );
			if ( !invalid ) {
				invalid = NodSurface_Validate( i->second, true, true );
			}

			if ( invalid ) { 
				error = invalid; 
				return false; 
			}
		}

		xmlDocPtr doc = xmlNewDoc( BAD_CAST "1.0" );
		xmlNodePtr root = xmlNewNode( NULL, BAD_CAST "nod-surface-presets" );
		xmlDocSetRootElement( doc, root );
		xmlSetProp( root, BAD_CAST "version", BAD_CAST "1" );

		const char *fields[] = { "shift-s", "shift-t", "scale-s", "scale-t", "rotation" };
		for ( std::map<std::string, NodSurfacePreset>::const_iterator i = presets.begin(); i != presets.end(); ++i ) {
			
			xmlNodePtr node = xmlNewChild( root, NULL, BAD_CAST "preset", NULL );
			xmlSetProp( node, BAD_CAST "name", BAD_CAST i->first.c_str() );
			xmlSetProp( node, BAD_CAST "material", BAD_CAST i->second.material.c_str() );

			const float values[] = { 
				i->second.shift[0], 
				i->second.shift[1], 
				i->second.scale[0], 
				i->second.scale[1], 
				i->second.rotation 
			};

			for ( size_t j = 0; j < G_N_ELEMENTS( fields ); ++j ) {
				char text[G_ASCII_DTOSTR_BUF_SIZE];
				g_ascii_formatd( text, sizeof( text ), "%.9g", values[j] );
				xmlSetProp( node, BAD_CAST fields[j], BAD_CAST text );
			}
		}

		xmlChar *bytes = NULL;
		int size = 0;

		xmlDocDumpFormatMemoryEnc( doc, &bytes, &size, "UTF-8", 1 );
		gchar *parent = g_path_get_dirname( path );

		bool saved = bytes && size > 0;
		if ( saved && g_mkdir_with_parents( parent, 0775 ) != 0 ) { 
			error = g_strerror( errno ); 
			saved = false; 
		}

		GError *failure = NULL;
		if ( saved ) {
			saved = g_file_set_contents( path, reinterpret_cast<const char *>( bytes ), size, &failure ) != FALSE;
		}

		if ( !saved && error.empty() ) {
			error = failure ? failure->message : "Cannot serialize surface presets.";
		}

		if ( failure ) {
			g_error_free( failure );
		}

		g_free( parent );
		xmlFree( bytes );
		xmlFreeDoc( doc );

		return saved;
	}
};

#endif
