/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodcompile.h
Version: 0.6.8

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

#ifndef RADIANT_NODCOMPILE_H
#define RADIANT_NODCOMPILE_H

#include <string>
#include <vector>
#include <functional>
#include <array>
#include <cmath>
#include <cfloat>
#include <glib.h>
#include <glib/gstdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif

struct NodCompileOptions {
	std::string map, workspace, assets, tools, output, mapName;
	bool geometry = true, objects = true, pathing = true, lighting = true;
	bool oversample = false, shadows = true, spacingOverride = false, sun = false, ambient = false;
	double spacing = 4, phong = 0, sunIntensity = 1, ambientIntensity = 0.35;
	bool physicsMarginOverride = false;
	double physicsMargin = 1;
	std::array<double, 3> sunColor{{1, 1, 1}}, ambientColor{{1, 1, 1}}, sunDirection{{-1, 2, -4}};
	bool exports( int index ) const {
		return index == 0 ? geometry || lighting : index == 1 ? objects : pathing;
	}
};

inline std::string NodCompile_Number( double value ){
	gchar text[G_ASCII_DTOSTR_BUF_SIZE];
	return g_ascii_dtostr( text, sizeof( text ), value );
}

inline std::vector<std::string> NodCompile_LightingArguments( const NodCompileOptions &options ){
	std::vector<std::string> result;
	result.push_back( "--lighting" ); result.push_back( options.lighting ? "auto" : "off" );
	if ( !options.lighting ) {
		return result;
	}

	result.push_back( "--light-samples" ); result.push_back( options.oversample ? "4" : "1" );
	result.push_back( "--light-shadows" ); result.push_back( options.shadows ? "on" : "off" );
	result.push_back( "--light-phong" ); result.push_back( NodCompile_Number( options.phong ) );
	if ( options.spacingOverride ) {
		result.push_back( "--light-spacing" ); result.push_back( NodCompile_Number( options.spacing ) );
	}

	for ( int kind = 0; kind != 2; ++kind ) {
		if ( !( kind == 0 ? options.sun : options.ambient ) ) {
			continue;
		}

		result.push_back( kind == 0 ? "--sun-light" : "--ambient-light" );
		const auto &color = kind == 0 ? options.sunColor : options.ambientColor;
		for ( double channel : color ) {
			result.push_back( NodCompile_Number( channel ) );
		}

		result.push_back( NodCompile_Number( kind == 0 ? options.sunIntensity : options.ambientIntensity ) );
		if ( kind == 0 ) {
			for ( double component : options.sunDirection ) {
				result.push_back( NodCompile_Number( component ) );
	}
		}
	}

	return result;
}

inline std::string NodCompile_Join( const std::string &directory, const std::string &name ){
	gchar *path = g_build_filename( directory.c_str(), name.c_str(), NULL );
	const std::string result = path;
	g_free( path );
	return result;
}

inline std::string NodCompile_Tool( const std::string &directory, int index ){
	const char *names[] = { "MAPtoNIL", "MAPtoNSD", "MAPtoNPD" };
	return NodCompile_Join( directory, std::string( names[index] )
#ifdef _WIN32
		+ ".exe"
#endif
	);
}

inline bool NodCompile_HasTools( const std::string &directory, const NodCompileOptions *options = NULL ){
	for ( int i = 0; i != 3; ++i ) {
		if ( !options || options->exports( i ) ) {
			if ( !g_file_test( NodCompile_Tool( directory, i ).c_str(), G_FILE_TEST_IS_REGULAR ) ||
			 !g_file_test( NodCompile_Tool( directory, i ).c_str(), G_FILE_TEST_IS_EXECUTABLE ) ) {
				return false;
			}
		}
	}

	return true;
}

inline bool NodCompile_Exists( const std::string &path ){
#ifdef _WIN32
	gunichar2 *wide = g_utf8_to_utf16( path.c_str(), -1, NULL, NULL, NULL );
	if ( !wide ) {
		return false;
	}

	const DWORD attributes = GetFileAttributesW( reinterpret_cast<LPCWSTR>( wide ) );
	g_free( wide );
	return attributes != INVALID_FILE_ATTRIBUTES;
#else
	return g_file_test( path.c_str(), G_FILE_TEST_EXISTS ) ||
		g_file_test( path.c_str(), G_FILE_TEST_IS_SYMLINK );
#endif
}

// Resolve existing ancestors too, so junctions/symlinks cannot redirect staging into assets.
inline std::string NodCompile_Resolve( const std::string &path ){
	if ( path.empty() || !g_path_is_absolute( path.c_str() ) ) {
		return "";
	}

	if ( !NodCompile_Exists( path ) ) {
		gchar *parent = g_path_get_dirname( path.c_str() );
		gchar *name = g_path_get_basename( path.c_str() );
		const std::string resolved = path == parent ? "" : NodCompile_Resolve( parent );
		std::string result;
		if ( !resolved.empty() ) {
			if ( strcmp( name, "." ) == 0 ) {
				result = resolved;
			} else if ( strcmp( name, ".." ) == 0 ) {
				gchar *ancestor = g_path_get_dirname( resolved.c_str() );
				result = ancestor;
				g_free( ancestor );
			}
			else {
				result = NodCompile_Join( resolved, name );
		}
		}

		g_free( parent );
		g_free( name );
		return result;
	}
#ifdef _WIN32
	gunichar2 *wide = g_utf8_to_utf16( path.c_str(), -1, NULL, NULL, NULL );
	if ( !wide ) {
		return "";
	}

	HANDLE file = CreateFileW( reinterpret_cast<LPCWSTR>( wide ), 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, NULL );
	g_free( wide );
	if ( file == INVALID_HANDLE_VALUE ) {
		return "";
	}

	typedef DWORD (WINAPI *FinalPath)( HANDLE, LPWSTR, DWORD, DWORD );
	FinalPath finalPath = reinterpret_cast<FinalPath>(
		GetProcAddress( GetModuleHandleW( L"kernel32.dll" ), "GetFinalPathNameByHandleW" ) );
	const DWORD size = finalPath ? finalPath( file, NULL, 0, 0 ) : 0;
	std::vector<wchar_t> buffer( size + 1 );
	const DWORD written = size ? finalPath( file, &buffer[0], size + 1, 0 ) : 0;
	CloseHandle( file );
	if ( !written || written > size ) {
		return "";
	}

	std::wstring native( &buffer[0], written );
	if ( native.compare( 0, 8, L"\\\\?\\UNC\\" ) == 0 ) {
		native = L"\\\\" + native.substr( 8 );
	} else if ( native.compare( 0, 4, L"\\\\?\\" ) == 0 ) {
		native.erase( 0, 4 );
	}

	gchar *utf8 = g_utf16_to_utf8( reinterpret_cast<const gunichar2 *>( native.c_str() ),
		-1, NULL, NULL, NULL );
	const std::string result = utf8 ? utf8 : "";
	g_free( utf8 );
	return result;
#else
	char *resolved = realpath( path.c_str(), NULL );
	const std::string result = resolved ? resolved : "";
	free( resolved );
	return result;
#endif
}

inline bool NodCompile_Contains( std::string root, std::string path ){
	if ( root.empty() || path.empty() ) {
		return false;
	}

	for ( size_t i = 0; i < root.size(); ++i ) {
		if ( root[i] == '\\' ) {
			root[i] = '/';
		}
	}

	for ( size_t i = 0; i < path.size(); ++i ) {
		if ( path[i] == '\\' ) {
			path[i] = '/';
		}
	}
#ifdef _WIN32
	gchar *lowerRoot = g_utf8_casefold( root.c_str(), -1 );
	gchar *lowerPath = g_utf8_casefold( path.c_str(), -1 );
	root = lowerRoot;
	path = lowerPath;
	g_free( lowerRoot );
	g_free( lowerPath );
#endif
	if ( root == path ) {
		return true;
	}

	if ( root[root.size() - 1] != '/' ) {
		root += '/';
	}

	return path.compare( 0, root.size(), root ) == 0;
}

inline std::string NodCompile_Stem( const std::string &map ){
	gchar *base = g_path_get_basename( map.c_str() );
	std::string stem = base;
	g_free( base );
	const size_t dot = stem.rfind( '.' );
	if ( dot == std::string::npos || g_ascii_strcasecmp( stem.c_str() + dot, ".map" ) != 0 ) {
		return "";
	}

	stem.erase( dot );
	return stem;
}

inline bool NodCompile_ValidMapName( const std::string &name ){
	if ( name.empty() ) {
		return true;
	}

	if ( name.size() > 251 || !g_utf8_validate( name.c_str(), name.size(), NULL ) ||
		g_ascii_isspace( name.front() ) || g_ascii_isspace( name.back() ) || name.back() == '.' ) {
		return false;
	}

	for ( unsigned char character : name ) {
		if ( character < 32 || character == 127 || strchr( "\\/:*?\"<>|", character ) ) {
			return false;
		}
	}

	const std::string device = name.substr( 0, name.find( '.' ) );
	const char *reserved[] = { "CON", "PRN", "AUX", "NUL", "CLOCK$", "CONIN$", "CONOUT$" };
	for ( const char *word : reserved ) {
		if ( g_ascii_strcasecmp( device.c_str(), word ) == 0 ) {
			return false;
		}
	}

	if ( device.size() == 4 && ( g_ascii_strncasecmp( device.c_str(), "COM", 3 ) == 0 ||
		g_ascii_strncasecmp( device.c_str(), "LPT", 3 ) == 0 ) && device[3] >= '1' && device[3] <= '9' ) {
		return false;
	}

	const size_t dot = name.rfind( '.' );
	if ( dot != std::string::npos ) {
		const char *extensions[] = { ".map", ".nil", ".nsd", ".npd" };
		for ( const char *extension : extensions ) {
			if ( g_ascii_strcasecmp( name.c_str() + dot, extension ) == 0 ) {
				return false;
	}
		}
	}

	return true;
}

inline std::string NodCompile_CreateStage( const std::string &output, std::string &error ){
	for ( int attempt = 0; attempt != 32; ++attempt ) {
		gchar *name = g_strdup_printf( ".nod-convert-%08x%08x", g_random_int(), g_random_int() );
		const std::string directory = NodCompile_Join( output, name );
		g_free( name );
		if ( g_mkdir( directory.c_str(), 0700 ) == 0 ) {
			return directory;
		}

		if ( !NodCompile_Exists( directory ) ) {
			error = "Cannot create staging folder: " + directory;
			return "";
		}
	}

	error = "Cannot reserve a unique staging folder in: " + output;
	return "";
}

inline std::string NodCompile_Validate( const NodCompileOptions &options ){
	if ( !options.exports( 0 ) && !options.exports( 1 ) && !options.exports( 2 ) ) {
		return "Select at least one export: geometry/lighting, objects or pathing.";
	}

	if ( !NodCompile_ValidMapName( options.mapName ) ) {
		return "Override Map Name must be a filename stem without .map/.nil/.nsd/.npd, folders, reserved filename characters or device names, or surrounding whitespace (maximum 251 UTF-8 bytes). Leave it blank to use the MAP filename.";
	}

	if ( options.exports( 0 ) && options.physicsMarginOverride &&
		( !std::isfinite( options.physicsMargin ) || options.physicsMargin <= 0 ||
		options.physicsMargin > FLT_MAX || static_cast<float>( options.physicsMargin ) == 0 ) ) {
		return "Physics margin must be a positive finite float32 distance in MAP units.";
	}

	if ( options.lighting ) {
		if ( !std::isfinite( options.spacing ) || options.spacing < 0.01 || options.spacing > 1024 ||
			 !std::isfinite( options.phong ) || options.phong < 0 || options.phong > 180 ) {
			return "Lighting spacing must be 0.01..1024 and Phong angle 0..180 degrees.";
		}

		for ( int kind = 0; kind != 2; ++kind ) {
			if ( !( kind == 0 ? options.sun : options.ambient ) ) {
				continue;
			}

			const double intensity = kind == 0 ? options.sunIntensity : options.ambientIntensity;
			const auto &color = kind == 0 ? options.sunColor : options.ambientColor;
			double total = 0;
			for ( double channel : color ) {
				if ( !std::isfinite( channel ) || channel < 0 || channel > 1 ) {
					return "Sun and ambient colors require RGB values in 0..1.";
				}

				total += channel;
			}

			if ( !total || !std::isfinite( intensity ) || intensity <= 0 || intensity > 1024 ) {
				return "Enabled sun/ambient light requires nonblack color and positive intensity up to 1024.";
		}
		}

		if ( options.sun ) {
			double length = 0;
			for ( double component : options.sunDirection ) {
				if ( !std::isfinite( component ) ) {
					return "Sun direction must be finite.";
				}

				length += component * component;
			}

			if ( length <= 1e-16 || !std::isfinite( length ) ) {
				return "Sun direction must be nonzero and finite.";
		}
	}
	}

	if ( !g_path_is_absolute( options.map.c_str() ) ||
		 !g_file_test( options.map.c_str(), G_FILE_TEST_IS_REGULAR ) || NodCompile_Stem( options.map ).empty() ) {
		return "Save the current map to a .map file before converting.";
	}

	if ( !g_path_is_absolute( options.tools.c_str() ) || !NodCompile_HasTools( options.tools, &options ) ) {
		return "Select one SDK binaries folder containing the selected export compilers.";
	}

	if ( !g_file_test( options.workspace.c_str(), G_FILE_TEST_IS_DIR ) ) {
		return "The authoring workspace does not exist.";
	}

	if ( options.assets.empty() || !g_file_test( NodCompile_Join( options.assets, "Materials" ).c_str(), G_FILE_TEST_IS_DIR ) ) {
		return "Select the read-only asset root containing Materials.";
	}

	const std::string workspace = NodCompile_Resolve( options.workspace );
	const std::string assets = NodCompile_Resolve( options.assets );
	const std::string output = NodCompile_Resolve( options.output );
	if ( workspace.empty() || assets.empty() || output.empty() ) {
		return "Cannot resolve workspace, asset root or output folder. Use absolute paths.";
	}

	if ( !NodCompile_Contains( workspace, output ) || NodCompile_Contains( assets, output ) ) {
		return "The output folder must be inside the authoring workspace and outside the read-only assets.";
	}

	return "";
}

inline bool NodCompile_Run( const NodCompileOptions &options,
						   const std::function<void( const std::string & )> &log, std::string &error ){
	error = NodCompile_Validate( options );
	if ( !error.empty() ) {
		return false;
	}

	const std::string output = NodCompile_Resolve( options.output );
	if ( g_mkdir_with_parents( output.c_str(), 0700 ) != 0 ) {
		error = "Cannot create output folder: " + output; return false;
	}

	const std::string stage = NodCompile_CreateStage( output, error );
	if ( stage.empty() ) {
		return false;
	}

	const std::string stem = options.mapName.empty() ? NodCompile_Stem( options.map ) : options.mapName;
	const std::string snapshot = NodCompile_Join( stage, stem + ".map" );
	const char *extensions[] = { ".nil", ".nsd", ".npd" };
	std::string staged[3], final[3], backup[3];
	bool saved[3] = { false, false, false }, published[3] = { false, false, false };
	for ( int i = 0; i != 3; ++i ) {
		staged[i] = NodCompile_Join( stage, stem + extensions[i] );
		final[i] = NodCompile_Join( output, stem + extensions[i] );
		backup[i] = NodCompile_Join( stage, std::string( "previous" ) + extensions[i] + ".backup" );
	}

	gchar *contents = NULL;
	gsize length = 0;
	GError *failure = NULL;
	bool ok = g_file_get_contents( options.map.c_str(), &contents, &length, &failure ) != FALSE;
	if ( ok ) {
		ok = g_file_set_contents( snapshot.c_str(), contents, length, &failure ) != FALSE;
	}

	g_free( contents );
	if ( !ok ) {
		error = std::string( "Cannot snapshot MAP: " ) + ( failure ? failure->message : "unknown error" );
		if ( failure ) {
			g_error_free( failure );
	}
	}

	for ( int i = 0; ok && i != 3; ++i ) {
		if ( !options.exports( i ) ) {
			continue;
		}

		std::vector<std::string> arguments;
		arguments.push_back( NodCompile_Tool( options.tools, i ) );
		if ( i == 0 ) {
			arguments.push_back( "--world-only" );
		}

		arguments.push_back( "--workspace" ); arguments.push_back( options.workspace );
		arguments.push_back( "--asset-root" ); arguments.push_back( options.assets );
		if ( i == 0 ) {
			const auto lighting = NodCompile_LightingArguments( options );
			arguments.insert( arguments.end(), lighting.begin(), lighting.end() );
			if ( options.physicsMarginOverride ) {
				arguments.push_back( "--physics-margin" );
				arguments.push_back( NodCompile_Number( options.physicsMargin ) );
			}
		}

		arguments.push_back( snapshot ); arguments.push_back( staged[i] );
		std::vector<gchar *> argv;
		for ( size_t j = 0; j < arguments.size(); ++j ) {
			argv.push_back( &arguments[j][0] );
		}

		argv.push_back( NULL );
		log( "\nRunning " + arguments[0] + "\n" );
		gchar *standard = NULL, *diagnostic = NULL;
		gint status = 0;
		failure = NULL;
		ok = g_spawn_sync( options.workspace.c_str(), &argv[0], NULL, static_cast<GSpawnFlags>( 0 ),
			NULL, NULL, &standard, &diagnostic, &status, &failure ) != FALSE;
		if ( standard ) {
			log( standard );
		}

		if ( diagnostic ) {
			log( diagnostic );
		}

		g_free( standard ); g_free( diagnostic );
		if ( !ok ) {
			error = std::string( "Cannot start compiler: " ) + ( failure ? failure->message : arguments[0] );
		} else if ( status != 0 ) {
			ok = false; error = "Compiler failed (process status " + std::to_string( status ) + "): " + arguments[0];
		}
		else if ( !g_file_test( staged[i].c_str(), G_FILE_TEST_IS_REGULAR ) ) {
			ok = false; error = "Compiler returned success without writing: " + staged[i];
		}

		if ( failure ) {
			g_error_free( failure );
	}
	}

	for ( int i = 0; ok && i != 3; ++i ) {
		if ( !options.exports( i ) ) {
			continue;
		}

		if ( NodCompile_Exists( final[i] ) ) {
			if ( !g_file_test( final[i].c_str(), G_FILE_TEST_IS_REGULAR ) ||
				 g_rename( final[i].c_str(), backup[i].c_str() ) != 0 ) {
				ok = false; error = "Cannot back up existing output: " + final[i];
			}
			else {
				saved[i] = true;
		}
	}
	}

	for ( int i = 0; ok && i != 3; ++i ) {
		if ( !options.exports( i ) ) {
			continue;
		}

		if ( g_rename( staged[i].c_str(), final[i].c_str() ) != 0 ) {
			ok = false; error = "Cannot publish output: " + final[i];
		}
		else {
			published[i] = true;
	}
	}

	bool restored = true;
	if ( !ok ) {
		for ( int i = 0; i != 3; ++i ) {
			if ( published[i] && g_unlink( final[i].c_str() ) != 0 ) {
				restored = false;
			}

			if ( saved[i] && g_rename( backup[i].c_str(), final[i].c_str() ) != 0 ) {
				restored = false;
		}
		}

		if ( !restored ) {
			error += "\nRollback incomplete. Recover previous files from: " + stage;
	}
	}

	if ( restored ) {
		std::vector<std::string> cleanup;
		cleanup.push_back( snapshot );
		for ( int i = 0; i != 3; ++i ) { cleanup.push_back( staged[i] ); cleanup.push_back( backup[i] ); }
		bool clean = true;
		for ( size_t i = 0; i < cleanup.size(); ++i ) {
			if ( NodCompile_Exists( cleanup[i] ) && g_unlink( cleanup[i].c_str() ) != 0 ) {
				clean = false;
			}
		}

		if ( g_rmdir( stage.c_str() ) != 0 ) {
			clean = false;
		}

		if ( !clean ) {
			log( "\nWarning: could not fully remove staging folder: " + stage + "\n" );
	}
	}

	if ( ok ) {
		for ( int i = 0; i != 3; ++i ) {
			if ( options.exports( i ) ) {
				log( "\nWrote " + final[i] + "\n" );
	}
		}
	}

	return ok;
}

#endif
