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
	const NodBehavior *behavior = handler.empty() ? NULL : NodScript_Behavior( handler.c_str() );
	if ( !handler.empty() && !behavior ) {
		error = "Reload the NOT catalog with the updated SDK behavior metadata.";
		return false;
	}

	if ( behavior && behavior->Get( "frame-orientation" ) == "owner" ) {
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
		const char *bounds[] = { "nod_region_min", "nod_region_max" };
		for ( size_t i = 0; i < G_N_ELEMENTS( bounds ); ++i ) {
			if ( fields.count( bounds[i] ) && *ValueForKey( entity, bounds[i] ) ) {
				fields[bounds[i]] = ValueForKey( entity, bounds[i] );
			}
		}

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
			vec3_t step = { 0, 0, 0 };
			if ( behavior && !behavior->Get( "frame-step" ).empty() ) {
				ModelTransform_Parse( behavior->Get( "frame-step" ).c_str(), 3, step, true );
			}

			for ( unsigned axis = 0; axis < 3; ++axis ) {
				destination[axis] += step[axis] * i;
			}

			fields["nod_frame_origin_" + std::to_string( i )] = NodTemplate_FormatPosition( destination );
			if ( behavior && behavior->Get( "frame-orientation" ) == "owner" ) {
				fields["nod_frame_rotation_" + std::to_string( i )] = NodTemplate_FormatPosition( rotation );
			}
		}
	}

	std::vector<std::string> oldKeys;
	for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
		if ( strncmp( pair->key, "nod_script_", 11 ) == 0 || strncmp( pair->key, "nod_frame_", 10 ) == 0 ||
			strcmp( pair->key, "nod_prop_15" ) == 0 ||
			strcmp( pair->key, "nod_region_min" ) == 0 || strcmp( pair->key, "nod_region_max" ) == 0 ) {
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
		keys.push_back( std::make_pair( "nod_console_template", NodScript_ModelTemplate( name ) ) );
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
		keys.push_back( std::make_pair( "nod_enemy", "" ) );
		keys.push_back( std::make_pair( "nod_max_spawns", "25" ) );
		keys.push_back( std::make_pair( "nod_destination", NodTemplate_FormatPosition( destination ) ) );
	}

	return NodPoint_Create( type, position, keys, "Place native control", error );
}

inline bool NodScript_Connect( entity_t *from, entity_t *to, std::string &error ){
	error.clear();
	const bool preset = strcmp( ValueForKey( from, "classname" ), "nod_console" ) == 0;
	const NodBehavior *source = NodScript_Behavior( ValueForKey( from, "nod_prop_15" ) );
	if ( from == to || ( !preset && ( !source || source->Get( "first" ).empty() ||
		( !NodScript_Template( from ) && strncmp( ValueForKey( from, "nod_id" ), "nsd:", 4 ) != 0 ) ) ) ) {
		error = "Select a native output handler first, then a compatible input handler. Reload NOT metadata if unavailable.";
		return false;
	}

	const NodBehavior *receiver = NodScript_Behavior( ValueForKey( to, "nod_prop_15" ) );
	bool pulse = false;
	const char *target = ValueForKey( to, "targetname" );
	if ( strcmp( ValueForKey( to, "classname" ), "nod_sound_cue" ) != 0 &&
		strcmp( ValueForKey( to, "classname" ), "nod_enemy_spawn" ) != 0 &&
		strcmp( ValueForKey( to, "classname" ), "nod_trigger_relay" ) != 0 ) {
		target = ValueForKey( to, NodScript_Template( to ) ? "targetname" : "nod_id" );
		if ( ( !NodScript_Template( to ) && strncmp( target, "nsd:", 4 ) != 0 ) ||
			!receiver || ( receiver->Get( "input" ) != "trigger" && receiver->Get( "input" ) != "pulse" ) ) {
			error = "The receiver has no supported native trigger input in the SDK metadata.";
			return false;
		}

		pulse = receiver->Get( "input" ) == "pulse";
		if ( pulse && ( preset || !source || source->Get( "data" ).empty() || source->Get( "delay" ).empty() ||
			source->Get( "start" ).empty() || source->Get( "stop" ).empty() || source->Get( "pulse-delay" ).empty() ) ) {
			error = "This receiver needs a start/stop pulse; connect it through a native pulse-capable relay.";
			return false;
		}
	}

	if ( !NodScript_Reference( target ) ) {
		error = "The receiver needs a valid target name or source object identity.";
		return false;
	}

	if ( !preset && strcmp( ValueForKey( from, "nod_script_authoring" ), "parameters-v1" ) != 0 ) {
		error = "Re-export this scene with the updated NODscene export-properties to edit its script links.";
		return false;
	}

	const unsigned first = preset ? 0 : static_cast<unsigned>( strtoul( source->Get( "first" ).c_str(), NULL, 10 ) );
	const unsigned stride = preset ? 1 : static_cast<unsigned>( strtoul( source->Get( "stride" ).c_str(), NULL, 10 ) );
	const unsigned count = preset ? 1 : static_cast<unsigned>( strtoul( source->Get( "count" ).c_str(), NULL, 10 ) );
	const unsigned actions = pulse ? 2 : 1;
	unsigned slot = 0;
	if ( !preset ) {
		if ( actions == 1 && source->Get( "data" ).empty() ) {
			for ( unsigned existing = 0; existing < count; ++existing ) {
				const unsigned parameter = first + existing * stride;
				if ( strcmp( ValueForKey( from, ( "nod_script_param_" + std::to_string( parameter ) ).c_str() ), target ) == 0 ) {
					return true;
				}
			}
		}

		if ( source->Get( "replace" ) == "1" ) {
			slot = 0;
		} else {
			for ( ; slot + actions <= count; ++slot ) {
				bool available = true;
				for ( unsigned action = 0; action < actions; ++action ) {
					const unsigned parameter = first + ( slot + action ) * stride;
					const char *value = ValueForKey( from, ( "nod_script_param_" + std::to_string( parameter ) ).c_str() );
					available = available && ( strcmp( value, "-2" ) == 0 || strcmp( value, "-1" ) == 0 );
				}

				if ( available ) {
					break;
				}
			}
		}

		if ( slot + actions > count ) {
			error = pulse ? "The source needs two consecutive free actions for a start/stop pulse." :
				"The source has no free action slots.";
			return false;
		}

		for ( unsigned action = 0; action < actions; ++action ) {
			const unsigned parameter = first + ( slot + action ) * stride;
			if ( strcmp( ValueForKey( from, ( "nod_script_kind_" + std::to_string( parameter ) ).c_str() ), "3" ) != 0 ) {
				error = "Native target slot has a mismatched schema; re-export or reinitialize this handler.";
				return false;
			}

			const char *offsets[] = { "delay", "data" };
			const char *kinds[] = { "2", "7" };
			for ( unsigned i = 0; i < 2; ++i ) {
				if ( source->Get( offsets[i] ).empty() ) {
					continue;
				}

				const unsigned offset = static_cast<unsigned>( strtoul( source->Get( offsets[i] ).c_str(), NULL, 10 ) );
				if ( strcmp( ValueForKey( from, ( "nod_script_kind_" + std::to_string( parameter + offset ) ).c_str() ), kinds[i] ) != 0 ) {
					error = "Native action slot has a mismatched delay/data schema; re-export or reinitialize this handler.";
					return false;
				}
			}
		}
	}

	Undo_Start( "Connect native action" );
	Undo_AddEntity( from );
	if ( !preset ) {
		for ( unsigned action = 0; action < actions; ++action ) {
			const unsigned parameter = first + ( slot + action ) * stride;
			SetKeyValue( from, ( "nod_script_param_" + std::to_string( parameter ) ).c_str(), target );
			if ( !source->Get( "delay" ).empty() ) {
				const unsigned offset = static_cast<unsigned>( strtoul( source->Get( "delay" ).c_str(), NULL, 10 ) );
				SetKeyValue( from, ( "nod_script_param_" + std::to_string( parameter + offset ) ).c_str(),
					action == 0 ? "0" : source->Get( "pulse-delay" ).c_str() );
			}

			if ( !source->Get( "data" ).empty() ) {
				const unsigned offset = static_cast<unsigned>( strtoul( source->Get( "data" ).c_str(), NULL, 10 ) );
				SetKeyValue( from, ( "nod_script_param_" + std::to_string( parameter + offset ) ).c_str(),
					source->Get( pulse && action == 0 ? "start" : "stop" ).c_str() );
			}
		}
	} else {
		SetKeyValue( from, "target", target );
	}
	Undo_EndEntity( from );
	Undo_End();
	return true;
}

#endif
