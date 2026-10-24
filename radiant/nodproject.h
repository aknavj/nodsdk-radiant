/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodproject.h
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

#ifndef RADIANT_NODPROJECT_H
#define RADIANT_NODPROJECT_H

#include <string>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

inline std::string NodProject_MapDirectory( bool nodProfile, const char *subdir,
										   const char *configured, const char *workspace, const char *base ){
	if ( !nodProfile || !subdir || strcmp( subdir, "maps/" ) != 0 ) {
		return "";
	}

	if ( *configured ) {
		return configured;
	}

	std::string path = workspace;
	if ( !path.empty() && path[path.size() - 1] != '/' && path[path.size() - 1] != '\\' ) {
		path += '/';
	}

	path += base;
	path += "/maps/";
	return path;
}

inline std::string NodProject_Attribute( xmlDocPtr doc, const char *name ){
	xmlChar *value = xmlGetProp( xmlDocGetRootElement( doc ), BAD_CAST name );
	const std::string result = value ? reinterpret_cast<const char *>( value ) : "";
	xmlFree( value );
	return result;
}

inline std::string NodProject_CanonicalPath( const char *path ){
	char *resolved;
#ifdef _WIN32
	resolved = _fullpath( NULL, path, 0 );
#else
	resolved = realpath( path, NULL );
#endif
	if ( !resolved ) {
		return "";
	}

	std::string result = resolved;
	free( resolved );
	for ( size_t i = 0; i < result.size(); ++i ) {
		if ( result[i] == '\\' ) {
			result[i] = '/';
		}
#ifdef _WIN32
		result[i] = g_ascii_tolower( result[i] );
#endif
	}

	if ( !result.empty() && result[result.size() - 1] != '/' ) {
		result += '/';
	}

	return result;
}

inline std::string NodProject_ValidatePaths( const char *maps, const char *assets ){
	const char *paths[] = { maps, assets };
	for ( size_t i = 0; i < G_N_ELEMENTS( paths ); ++i ) {
		if ( *paths[i] && ( !g_path_is_absolute( paths[i] ) || !g_file_test( paths[i], G_FILE_TEST_IS_DIR ) ) ) {
			return i == 0 ? "The map folder must be an existing absolute directory."
				: "The asset root must be an existing absolute directory.";
		}
	}

	if ( *assets ) {
		gchar *materials = g_build_filename( assets, "Materials", NULL );
		const bool valid = g_file_test( materials, G_FILE_TEST_IS_DIR ) != FALSE;
		g_free( materials );
		if ( !valid ) {
			return "Select the asset root containing Materials, not Materials itself.";
		}

		if ( *maps ) {
			const std::string mapPath = NodProject_CanonicalPath( maps );
			const std::string assetPath = NodProject_CanonicalPath( assets );
			if ( mapPath.empty() || assetPath.empty() ) {
				return "Could not resolve the selected directories.";
			}

			if ( mapPath.compare( 0, assetPath.size(), assetPath ) == 0 ) {
				return "The writable map folder must be outside the read-only asset root.";
		}
	}
	}

	return "";
}

inline xmlDocPtr NodProject_Load( const char *filename ){
	xmlDocPtr doc = xmlReadFile( filename, NULL, XML_PARSE_NONET );
	xmlNodePtr root = doc ? xmlDocGetRootElement( doc ) : NULL;
	if ( !root || !xmlStrEqual( root->name, BAD_CAST "game" ) ) {
		if ( doc ) {
			xmlFreeDoc( doc );
		}

		return NULL;
	}

	return doc;
}

inline bool NodProject_Save( const char *filename, xmlDocPtr original,
							 const char *maps, const char *assets, std::string &error, const char *editors = NULL ){
	error = NodProject_ValidatePaths( maps, assets );
	if ( !error.empty() ) {
		return false;
	}

	if ( editors && *editors && ( !g_path_is_absolute( editors ) || !g_file_test( editors, G_FILE_TEST_IS_DIR ) ) ) {
		error = "The SDK editor directory must be an existing absolute directory."; return false;
	}

	xmlDocPtr doc = xmlCopyDoc( original, 1 );
	if ( !doc ) {
		error = "Could not copy the NOD profile.";
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement( doc );
	const bool updated = xmlSetProp( root, BAD_CAST "nod_mappath", BAD_CAST maps ) != NULL
		&& xmlSetProp( root, BAD_CAST "nod_assetpath", BAD_CAST assets ) != NULL
		&& ( !editors || xmlSetProp( root, BAD_CAST "nod_editorspath", BAD_CAST editors ) != NULL );
	xmlChar *buffer = NULL;
	int size = 0;
	if ( updated ) {
		xmlDocDumpFormatMemoryEnc( doc, &buffer, &size, "UTF-8", 1 );
	}

	GError *failure = NULL;
	const bool saved = buffer && size > 0
		&& g_file_set_contents( filename, reinterpret_cast<const char *>( buffer ), size, &failure );
	if ( !saved ) {
		error = failure ? failure->message : "Could not serialize the NOD profile.";
	}

	if ( failure ) {
		g_error_free( failure );
	}

	xmlFree( buffer );
	xmlFreeDoc( doc );
	return saved;
}

#endif
