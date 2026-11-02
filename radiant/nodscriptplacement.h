/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodscriptplacement.h
Version: 0.6.8

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#ifndef RADIANT_NODSCRIPTPLACEMENT_H
#define RADIANT_NODSCRIPTPLACEMENT_H

#include "nodscript.h"
#include "nodpoint.h"
#include "nodtemplates.h"
#include <set>

inline const char *NodScript_Kind( entity_t *entity, const char *key ){
	if ( strncmp( key, "nod_script_param_", 17 ) != 0 ) {
		return "";
	}

	return ValueForKey( entity, ( "nod_script_kind_" + std::string( key + 17 ) ).c_str() );
}

inline bool NodScript_Template( entity_t *entity ){
	return strcmp( ValueForKey( entity, "nod_authoring" ), "not-template-v2" ) == 0 ||
		strcmp( ValueForKey( entity, "nod_authoring" ), "map-template-v1" ) == 0;
}

inline bool NodScript_SetHandler( entity_t *entity, const NodTemplateChoice &choice,
	const std::string &handler, std::string &error ){
	error.clear();
	if ( !entity || !NodScript_Template( entity ) || choice.name != ValueForKey( entity, "nod_template" ) ||
		( strcmp( ValueForKey( entity, "nod_authoring" ), "not-template-v2" ) == 0 ?
			choice.name != ValueForKey( entity, "classname" ) :
			strcmp( ValueForKey( entity, "classname" ), "nod_template_instance" ) != 0 ) ||
		choice.archive != ValueForKey( entity, "nod_template_archive" ) ||
		choice.type != ValueForKey( entity, "nod_template_type" ) ||
		choice.context != ValueForKey( entity, "nod_template_archives" ) ||
		( !handler.empty() && !choice.handlers.count( handler ) ) ) {
		error = "Select one NOT placement with a verified native handler schema.";
		return false;
	}

	vec3_t position;
	if ( !ModelTransform_Parse( ValueForKey( entity, "origin" ), 3, position, true ) ) {
		error = "The placement needs a finite origin before initializing its frames.";
		return false;
	}

	vec3_t rotation = { 0, 0, 0 };
	if ( handler == "cDoorMove" ) {
		vec3_t angles;
		if ( !ModelTransform_Parse( ValueForKey( entity, "angles" ), 3, angles, true ) ) {
			error = "The mover needs finite editor angles before initializing its frames.";
			return false;
		}

		ModelTransform_NativeAngles( angles, rotation );
	}

	std::map<std::string, std::string> fields;
	std::string targetname = ValueForKey( entity, "targetname" );
	if ( !handler.empty() ) {
		fields = choice.handlers.find( handler )->second;
		if ( targetname.empty() ) {
			std::set<std::string> used;
			for ( entity_t *other = entities.next; other != &entities; other = other->next ) {
				used.insert( ValueForKey( other, "targetname" ) );
			}

			for ( unsigned i = 1; i <= 1024; ++i ) {
				targetname = "native_" + std::to_string( i );
				if ( !used.count( targetname ) ) {
					break;
				}

				if ( i == 1024 ) {
					error = "No free native target name.";
					return false;
				}
			}
		}

		for ( unsigned i = 0; i < strtoul( fields["nod_frame_count"].c_str(), NULL, 10 ); ++i ) {
			vec3_t destination;
			VectorCopy( position, destination );
			if ( i == 1 ) {
				destination[handler == "cDoorMove" && choice.name == "floatingPlatform" ? 2 : 1] += 2;
			}

			fields["nod_frame_origin_" + std::to_string( i )] = NodTemplate_FormatPosition( destination );
			if ( handler == "cDoorMove" ) {
				fields["nod_frame_rotation_" + std::to_string( i )] = NodTemplate_FormatPosition( rotation );
			}
		}
	}

	std::vector<std::string> oldKeys;
	for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
		if ( strncmp( pair->key, "nod_script_", 11 ) == 0 || strncmp( pair->key, "nod_frame_", 10 ) == 0 ||
			strcmp( pair->key, "nod_prop_15" ) == 0 ) {
			oldKeys.push_back( pair->key );
		}
	}

	Undo_Start( "Configure native handler" );
	Undo_AddEntity( entity );
	for ( size_t i = 0; i < oldKeys.size(); ++i ) {
		DeleteKey( entity, oldKeys[i].c_str() );
	}

	if ( !handler.empty() ) {
		SetKeyValue( entity, "nod_prop_15", handler.c_str() );
		SetKeyValue( entity, "targetname", targetname.c_str() );
		for ( std::map<std::string, std::string>::const_iterator field = fields.begin(); field != fields.end(); ++field ) {
			SetKeyValue( entity, field->first.c_str(), field->second.c_str() );
		}
	}

	Undo_EndEntity( entity );
	Undo_End();
	return true;
}

inline entity_t *NodScript_Create( const char *name, const vec3_t position, std::string &error ){
	error.clear();
	eclass_t *type = Eclass_ForName( name, false );
	if ( !type || !type->fixedsize || !type->comments || !strstr( type->comments, "Native control authoring" ) ) {
		error = "Reload the updated SCGhost control definitions.";
		return NULL;
	}

	std::set<std::string> used;
	for ( entity_t *entity = entities.next; entity != &entities; entity = entity->next ) {
		used.insert( ValueForKey( entity, "targetname" ) );
	}

	std::string target;
	for ( unsigned i = 1; i <= 1024; ++i ) {
		target = std::string( name + 4 ) + "_" + std::to_string( i );
		if ( !used.count( target ) ) {
			break;
		}

		if ( i == 1024 ) {
			error = "No free native control target name.";
			return NULL;
		}
	}

	std::vector<std::pair<std::string, std::string> > keys;
	keys.push_back( std::make_pair( "classname", name ) );
	keys.push_back( std::make_pair( "origin", NodTemplate_FormatPosition( position ) ) );
	keys.push_back( std::make_pair( "angles", "0 0 0" ) );
	keys.push_back( std::make_pair( "targetname", target ) );
	if ( strcmp( name, "nod_console" ) == 0 ) {
		keys.push_back( std::make_pair( "target", "" ) );
	} else if ( strcmp( name, "nod_sound_cue" ) == 0 ) {
		keys.push_back( std::make_pair( "nod_cue", "" ) );
	} else if ( strcmp( name, "nod_trigger_relay" ) == 0 ) {
		keys.push_back( std::make_pair( "nod_relay_count", "1" ) );
		keys.push_back( std::make_pair( "nod_relay_target_0", "" ) );
		keys.push_back( std::make_pair( "nod_relay_delay_0", "0" ) );
	} else {
		vec3_t destination;
		VectorCopy( position, destination );
		destination[1] += 2;
		keys.push_back( std::make_pair( "nod_enemy", "Marine" ) );
		keys.push_back( std::make_pair( "nod_max_spawns", "25" ) );
		keys.push_back( std::make_pair( "nod_destination", NodTemplate_FormatPosition( destination ) ) );
	}

	return NodPoint_Create( type, position, keys, "Place native control", error );
}

inline bool NodScript_Connect( entity_t *from, entity_t *to, std::string &error ){
	error.clear();
	const bool preset = strcmp( ValueForKey( from, "classname" ), "nod_console" ) == 0;
	const bool imported = strcmp( ValueForKey( from, "nod_prop_15" ), "cTriggerOnAction" ) == 0 &&
		( strncmp( ValueForKey( from, "nod_id" ), "nsd:", 4 ) == 0 || NodScript_Template( from ) );
	const bool relay = strcmp( ValueForKey( from, "nod_prop_15" ), "cTriggerRelay" ) == 0 &&
		( strncmp( ValueForKey( from, "nod_id" ), "nsd:", 4 ) == 0 || NodScript_Template( from ) );
	const bool button = strcmp( ValueForKey( from, "nod_prop_15" ), "cButton" ) == 0 &&
		NodScript_Template( from ) && strcmp( ValueForKey( from, "nod_template" ), "button_wall_activate" ) == 0;
	if ( from == to || ( !preset && !imported && !relay && !button ) ) {
		error = "Select a native console, wall button or relay first, then a cue, relay, model mover or spawn control.";
		return false;
	}

	const bool ai = strcmp( ValueForKey( to, "nod_prop_15" ), "cAIFightPath" ) == 0;
	const char *target = ValueForKey( to, "targetname" );
	if ( strcmp( ValueForKey( to, "classname" ), "nod_sound_cue" ) != 0 &&
		strcmp( ValueForKey( to, "classname" ), "nod_enemy_spawn" ) != 0 &&
		strcmp( ValueForKey( to, "classname" ), "nod_trigger_relay" ) != 0 ) {
		const char *script = ValueForKey( to, "nod_prop_15" );
		target = ValueForKey( to, NodScript_Template( to ) ? "targetname" : "nod_id" );
		if ( ( !NodScript_Template( to ) && strncmp( target, "nsd:", 4 ) != 0 ) ||
			( strcmp( script, "cSoundStinger" ) != 0 && strcmp( script, "cTriggerRelay" ) != 0 &&
				strcmp( script, "cDoorMove" ) != 0 && !( relay && ai ) ) ) {
			error = "The receiver must be a sound cue, relay or model mover; AI paths require a relay pulse.";
			return false;
		}
	}

	if ( !NodScript_Reference( target ) ) {
		error = "The receiver needs a valid target name or source object identity.";
		return false;
	}

	if ( ( imported || relay || button ) && ( strcmp( ValueForKey( from, "nod_script_authoring" ), "parameters-v1" ) != 0 ||
		strcmp( ValueForKey( from, relay || button ? "nod_script_kind_0" : "nod_script_kind_16" ), "3" ) != 0 ) ) {
		error = "Re-export this scene with the updated NODscene export-properties to edit its script links.";
		return false;
	}

	unsigned slot = 0;
	if ( button ) {
		for ( ; slot < 8; ++slot ) {
			const char *value = ValueForKey( from, ( "nod_script_param_" + std::to_string( slot ) ).c_str() );
			if ( strcmp( value, target ) == 0 ) {
				return true;
			}
		}

		for ( slot = 0; slot < 8; ++slot ) {
			const char *value = ValueForKey( from, ( "nod_script_param_" + std::to_string( slot ) ).c_str() );
			if ( strcmp( value, "-2" ) == 0 || strcmp( value, "-1" ) == 0 ) {
				break;
			}
		}

		if ( slot == 8 ) {
			error = "The wall button already has eight linked targets.";
			return false;
		}
	}

	if ( relay ) {
		for ( ; slot < 12; ++slot ) {
			const char *value = ValueForKey( from, ( "nod_script_param_" + std::to_string( slot * 3 ) ).c_str() );
			const char *next = slot < 11 ? ValueForKey( from, ( "nod_script_param_" + std::to_string( ( slot + 1 ) * 3 ) ).c_str() ) : "";
			if ( ( strcmp( value, "-2" ) == 0 || strcmp( value, "-1" ) == 0 ) &&
				( !ai || strcmp( next, "-2" ) == 0 || strcmp( next, "-1" ) == 0 ) ) {
				break;
			}
		}

		if ( slot == 12 ) {
			error = ai ? "The relay needs two consecutive free actions for an AI start/stop pulse." :
				"The relay already has twelve actions.";
			return false;
		}
	}

	Undo_Start( "Connect native action" );
	Undo_AddEntity( from );
	if ( relay ) {
		for ( unsigned action = 0; action < ( ai ? 2u : 1u ); ++action ) {
			const unsigned parameter = ( slot + action ) * 3;
			SetKeyValue( from, ( "nod_script_param_" + std::to_string( parameter ) ).c_str(), target );
			SetKeyValue( from, ( "nod_script_param_" + std::to_string( parameter + 1 ) ).c_str(), action == 0 ? "0" : "0.25" );
			SetKeyValue( from, ( "nod_script_param_" + std::to_string( parameter + 2 ) ).c_str(),
				ai && action == 0 ? "1 0 0 0" : "0 0 0 0" );
		}
	} else {
		const std::string key = button ? "nod_script_param_" + std::to_string( slot ) :
			preset ? "target" : "nod_script_param_16";
		SetKeyValue( from, key.c_str(), target );
	}
	Undo_EndEntity( from );
	Undo_End();
	return true;
}

#endif
