#ifndef NOD_MATERIALS_H
#define NOD_MATERIALS_H

#include <algorithm>
#include <map>
#include <string>

class NodTextureIndex {
	std::map<std::string, std::string> paths;
	std::map<std::string, std::string> aliases;

	static std::string Key( std::string value ){
		for ( size_t i = 0; i < value.size(); ++i ) {
			if ( value[i] == '\\' ) {
				value[i] = '/';
			} else if ( value[i] >= 'A' && value[i] <= 'Z' ) {
				value[i] += 'a' - 'A';
			}
		}
		const size_t dot = value.find_last_of( '.' );
		if ( dot != std::string::npos ) {
			const std::string extension = value.substr( dot );
			if ( extension == ".dds" || extension == ".tga" || extension == ".jpg" || extension == ".png" ) {
				value.erase( dot );
			}
		}
		return value;
	}

public:
	void Clear(){ paths.clear(); aliases.clear(); }

	void Add( const std::string &path ){
		const std::string key = Key( path );
		if ( !paths.insert( std::make_pair( key, path ) ).second ) {
			return;
		}
		const std::string alias = key.substr( key.find_last_of( '/' ) + 1 );
		std::map<std::string, std::string>::iterator found = aliases.find( alias );
		if ( found == aliases.end() ) {
			aliases[alias] = path;
		} else if ( Key( found->second ) != key ) {
			found->second.clear();
		}
	}

	std::string Resolve( const std::string &reference ) const {
		std::string key = Key( reference );
		std::map<std::string, std::string>::const_iterator path = paths.find( key );
		if ( path != paths.end() ) {
			return path->second;
		}
		if ( key.compare( 0, 10, "materials/" ) != 0 ) {
			key = "materials/" + key;
		}
		path = paths.find( key );
		if ( path != paths.end() ) {
			return path->second;
		}
		if ( reference.find_first_of( "/\\" ) == std::string::npos ) {
			std::map<std::string, std::string>::const_iterator alias = aliases.find( Key( reference ) );
			if ( alias != aliases.end() ) {
				return alias->second;
			}
		}
		return std::string();
	}
};

#endif
