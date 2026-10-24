/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodview.h
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

#ifndef RADIANT_NODVIEW_H
#define RADIANT_NODVIEW_H

#include <map>
#include <set>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <cmath>

inline float NodView_Luminance( const float color[3] ){
	const float weights[3] = { 0.2126f, 0.7152f, 0.0722f };
	float luminance = 0;
	for ( int i = 0; i < 3; ++i ) {
		const float channel = color[i] < 0 ? 0 : color[i] > 1 ? 1 : color[i];
		luminance += weights[i] * ( channel <= 0.04045f ? channel / 12.92f :
			static_cast<float>( std::pow( ( channel + 0.055f ) / 1.055f, 2.4f ) ) );
	}

	return luminance;
}

inline void NodView_WireColor( const float color[3], const float background[3], float result[3] ){
	const float backdrop = NodView_Luminance( background );
	const auto readable = [backdrop]( const float value[3] ){
		const float luminance = NodView_Luminance( value );
		return luminance > backdrop ? ( luminance + 0.05f ) / ( backdrop + 0.05f ) >= 4.5f :
			( backdrop + 0.05f ) / ( luminance + 0.05f ) >= 4.5f;
	};
	for ( int i = 0; i < 3; ++i ) {
		result[i] = color[i];
	}

	if ( readable( result ) ) {
		return;
	}

	const float target = ( backdrop + 0.05f ) / 0.05f >= 1.05f / ( backdrop + 0.05f ) ? 0 : 1;
	for ( int i = 0; i < 3; ++i ) {
		result[i] = color[i] * 0.25f + target * 0.75f;
	}

	if ( !readable( result ) ) {
		for ( int i = 0; i < 3; ++i ) {
			result[i] = target;
}
	}
}

enum NodViewCategory
{
	NOD_VIEW_GEOMETRY,
	NOD_VIEW_MODELS,
	NOD_VIEW_THINGS,
	NOD_VIEW_NAVIGATION,
	NOD_VIEW_SECTORS,
	NOD_VIEW_BINDINGS,
	NOD_VIEW_CATEGORY_COUNT
};

inline NodViewCategory NodView_Category( const char *classname, const char *identity, const char *model ){
	if ( strcmp( identity, "nil:world" ) == 0 || strcmp( classname, "worldspawn" ) == 0
		 || strcmp( classname, "func_group" ) == 0 ) {
		return NOD_VIEW_GEOMETRY;
	}

	if ( strcmp( classname, "nod_scene" ) == 0 ) {
		return NOD_VIEW_BINDINGS;
	}

	if ( strcmp( classname, "nod_nil_sector" ) == 0 ) {
		return NOD_VIEW_SECTORS;
	}

	if ( strcmp( classname, "nod_npd_waypoint" ) == 0 || strcmp( classname, "nod_npd_link" ) == 0 ) {
		return NOD_VIEW_NAVIGATION;
	}

	if ( strcmp( classname, "misc_model" ) == 0 || *model ) {
		return NOD_VIEW_MODELS;
	}

	return NOD_VIEW_THINGS;
}

class NodViewState
{
	std::map<int, uint32_t> membership;
	struct Snapshot {
		std::map<int, uint32_t> membership;
		std::string names[32];
	};
	std::vector<Snapshot> undo, redo;
	Snapshot snapshot() const {
		Snapshot result;
		result.membership = membership;
		for ( int i = 0; i < 32; ++i ) {
			result.names[i] = names[i];
		}

		return result;
	}

	void restore( const Snapshot &value ){
		membership = value.membership;
		for ( int i = 0; i < 32; ++i ) {
			names[i] = value.names[i];
	}
	}

	void remember(){
		undo.push_back( snapshot() );
		if ( undo.size() > 128 ) {
			undo.erase( undo.begin() );
		}

		redo.clear();
	}

public:
	uint32_t visibleLayers;
	bool showStatic;
	bool categories[NOD_VIEW_CATEGORY_COUNT];
	std::string names[32];
	std::string nsdSource;

	NodViewState(){
		for ( int i = 0; i < NOD_VIEW_CATEGORY_COUNT; ++i ) {
			categories[i] = true;
		}

		ResetLayers();
	}

	void ResetLayers(){
		nsdSource.clear();
		membership.clear();
		ClearHistory();
		visibleLayers = UINT32_MAX;
		showStatic = true;
		for ( int i = 0; i < 32; ++i ) {
			char name[32];
			snprintf( name, sizeof( name ), "Layer %d", i );
			names[i] = name;
		}
	}

	void ClearHistory(){ undo.clear(); redo.clear(); }
	bool Undo(){
		if ( undo.empty() ) {
			return false;
		}

		redo.push_back( snapshot() );
		restore( undo.back() );
		undo.pop_back();
		return true;
	}

	bool Redo(){
		if ( redo.empty() ) {
			return false;
		}

		undo.push_back( snapshot() );
		restore( redo.back() );
		redo.pop_back();
		return true;
	}

	uint32_t Membership( int entityId ) const {
		std::map<int, uint32_t>::const_iterator found = membership.find( entityId );
		return found == membership.end() ? 0 : found->second;
	}

	const char *Assign( const std::set<int> &entities, int layer, bool add ){
		if ( layer < -1 || layer > 31 ) {
			return "Select Static or one of the 32 working layers.";
		}

		if ( entities.empty() ) {
			return "Select one or more entities or entity-owned brushes first.";
		}

		for ( std::set<int>::const_iterator i = entities.begin(); i != entities.end(); ++i ) {
			if ( *i <= 0 ) {
				return "The selection contains an invalid editor entity identity.";
			}
		}

		bool changed = false;
		for ( std::set<int>::const_iterator i = entities.begin(); i != entities.end(); ++i ) {
			const uint32_t next = layer == -1 ? 0 : ( add ? Membership( *i ) : 0 ) | ( uint32_t( 1 ) << layer );
			if ( Membership( *i ) != next ) {
				changed = true;
		}
		}

		if ( !changed ) {
			return NULL;
		}

		remember();
		for ( std::set<int>::const_iterator i = entities.begin(); i != entities.end(); ++i ) {
			if ( layer == -1 ) {
				membership.erase( *i );
			} else {
				membership[*i] = ( add ? Membership( *i ) : 0 ) | ( uint32_t( 1 ) << layer );
		}
		}

		return NULL;
	}

	const char *Rename( int layer, const char *name ){
		if ( layer < 0 || layer > 31 ) {
			return "Static cannot be renamed; select a working layer.";
		}

		const std::string value = name;
		if ( value.empty() || value.size() > 63 ) {
			return "Enter a layer name of 1 through 63 bytes.";
		}

		bool nonSpace = false;
		for ( size_t i = 0; i < value.size(); ++i ) {
			const unsigned char c = static_cast<unsigned char>( value[i] );
			if ( c < 32 || c == 127 ) {
				return "Layer names cannot contain control characters.";
			}

			if ( c != ' ' ) {
				nonSpace = true;
		}
		}

		if ( !nonSpace ) {
			return "Layer names cannot consist only of spaces.";
		}

		if ( names[layer] == value ) {
			return NULL;
		}

		remember();
		names[layer] = value;
		return NULL;
	}

	bool Visible( const char *gameFile, int entityId, const char *classname,
				  const char *identity, const char *model ) const {
		if ( strcmp( gameFile, "scghost.game" ) != 0 ) {
			return true;
		}

		if ( !categories[NodView_Category( classname, identity, model )] ) {
			return false;
		}

		const uint32_t layers = Membership( entityId );
		return layers ? ( layers & visibleLayers ) != 0 : showStatic;
	}

	void ShowAll(){
		visibleLayers = UINT32_MAX;
		showStatic = true;
		for ( int i = 0; i < NOD_VIEW_CATEGORY_COUNT; ++i ) {
			categories[i] = true;
	}
}
};

extern NodViewState g_nodViewState;

#endif
