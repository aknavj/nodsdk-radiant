/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodtemplateplacement.h
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

#ifndef RADIANT_NODTEMPLATEPLACEMENT_H
#define RADIANT_NODTEMPLATEPLACEMENT_H

#include "nodtemplates.h"
#include "nodinstances.h"
#include "nodclass.h"
#include "nodpoint.h"

inline bool NodTemplate_Modifiers( entity_t *entity, std::string &stream, std::string &error ){
	return NodClass_Modifiers( ValueForKey( entity, "nod_modifier_count" ),
		[entity]( const char *key ){ return ValueForKey( entity, key ); }, stream, error );
}

inline entity_t *NodTemplate_Create( const NodTemplateChoice &choice, const std::string &archives,
	const std::string &origin, std::string &error, bool registered = false ){
	std::vector<std::pair<std::string, std::string> > keys;
	if ( !NodTemplate_Keys( choice, archives, origin, keys, error, registered ) ) {
		return NULL;
	}

	vec3_t position;
	if ( !ModelTransform_Parse( origin.c_str(), 3, position, true ) ) { error = "Invalid position."; return NULL; }
	eclass_t *type = Eclass_ForName( registered ? choice.name.c_str() : "nod_template_instance", false );
	if ( !type || !type->fixedsize || !type->comments ||
		( registered ? !NodClass_Registered( type->comments ) : !strstr( type->comments, "MAP-only template authoring instance" ) ) ) {
		error = "Reload SCGhostPack definitions; nod_template_instance must be fixed-size and MAP-only."; return NULL;
	}

	return NodPoint_Create( type, position, keys, "Place NOD template", error );
}

inline bool NodTemplate_ApplyOverrides( const std::vector<entity_t *> &selection,
	const std::vector<const NodTemplateChoice *> &templates, const char *key, const char *value, bool remove, bool &changed, std::string &error ){
	error.clear();
	changed = false;
	if ( selection.empty() || selection.size() != templates.size() ) {
		error = "Select template instances with resolved metadata."; return false;
	}

	std::set<entity_t *> unique;
	bool needsChange = false;
	for ( size_t i = 0; i < selection.size(); ++i ) {
		entity_t *entity = selection[i];
		const NodTemplateChoice *choice = templates[i];
		bool contextPresent = false;
		std::string modifiers;
		if ( entity && !NodTemplate_Modifiers( entity, modifiers, error ) ) {
			return false;
		}

		if ( entity ) {
			for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
				if ( strcmp( pair->key, "nod_template_archives" ) == 0 ) {
					contextPresent = true;
				}
			}
		}

		if ( !entity || !choice || !unique.insert( entity ).second ||
			!contextPresent ||
			!( ( strcmp( ValueForKey( entity, "classname" ), "nod_template_instance" ) == 0 &&
				strcmp( ValueForKey( entity, "nod_authoring" ), "map-template-v1" ) == 0 ) ||
				( g_ascii_strcasecmp( ValueForKey( entity, "classname" ), choice->name.c_str() ) == 0 &&
				strcmp( ValueForKey( entity, "nod_authoring" ), "not-template-v2" ) == 0 ) ) ||
			g_ascii_strcasecmp( choice->name.c_str(), ValueForKey( entity, "nod_template" ) ) != 0 ||
			choice->archive != ValueForKey( entity, "nod_template_archive" ) ||
			choice->type != ValueForKey( entity, "nod_template_type" ) ||
			choice->context != ValueForKey( entity, "nod_template_archives" ) ||
			choice->modifiers != modifiers ) {
			error = "Template binding mismatch. Reload metadata; select template instances only."; return false;
		}

		bool local = false;
		for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
			if ( strcmp( pair->key, key ) == 0 ) {
				local = true;
			}
		}

		const char *issue = NodTemplate_OverrideRestriction( *choice, key, remove, local );
		const NodTemplateProperty *property = NodTemplate_Property( *choice, key );
		NodPropertyInfo info;
		if ( property ) {
			info.storage = property->storage;
		}

		if ( !issue && !remove ) {
			issue = NodProperty_Validate( key, value, info );
		}

		if ( issue ) { error = issue; return false; }
		if ( strcmp( key, "nod_prop_6" ) == 0 || strcmp( key, "nod_prop_7" ) == 0 ) {
			std::string bounds[2];
			for ( int axis = 0; axis < 2; ++axis ) {
				const char *boundKey = axis == 0 ? "nod_prop_6" : "nod_prop_7";
				const NodTemplateProperty *inherited = NodTemplate_Property( *choice, boundKey );
				if ( inherited ) {
					bounds[axis] = inherited->value;
				}

				for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
					if ( strcmp( pair->key, boundKey ) == 0 ) {
						bounds[axis] = pair->value;
					}
				}

				if ( strcmp( boundKey, key ) == 0 ) {
					bounds[axis] = remove ? inherited->value : value;
			}
			}

			float mins[3], maxs[3], position[3];
			if ( !NodClass_Bounds( bounds[0].c_str(), bounds[1].c_str(), mins, maxs ) ||
				!ModelTransform_Parse( ValueForKey( entity, "origin" ), 3, position, true ) ) {
				error = "Editor bounds require finite MIN and MAX vectors, with MIN below MAX on every axis."; return false;
			}

			for ( int axis = 0; axis < 3; ++axis ) {
				if ( static_cast<double>( position[axis] ) + mins[axis] < g_MinWorldCoord ||
					static_cast<double>( position[axis] ) + maxs[axis] > g_MaxWorldCoord ) {
					error = "Editor bounds must fit inside the editor's world-coordinate limits."; return false;
				}
		}
		}

		needsChange = needsChange || remove || !local || strcmp( ValueForKey( entity, key ), value ) != 0;
	}

	if ( !needsChange ) {
		return true;
	}

	Undo_Start( remove ? "Remove NOD instance override" : "NOD instance override" );
	Undo_AddBrushList( &selected_brushes );
	for ( size_t i = 0; i < selection.size(); ++i ) {
		Undo_AddEntity( selection[i] );
	}

	for ( size_t i = 0; i < selection.size(); ++i ) {
		if ( remove ) {
			DeleteKey( selection[i], key );
		} else {
			SetKeyValue( selection[i], key, value );
	}
	}

	Undo_EndBrushList( &selected_brushes );
	for ( size_t i = 0; i < selection.size(); ++i ) {
		Undo_EndEntity( selection[i] );
	}

	Undo_End();
	changed = true;
	return true;
}

inline bool NodInstance_ApplyLocalProperties( const std::vector<entity_t *> &selection,
	const std::vector<const NodInstanceRecord *> &records, const char *key, const char *value,
	bool &changed, std::string &error ){
	error.clear(); changed = false;
	if ( selection.empty() || selection.size() != records.size() ) { error = "Select NSD instances with resolved source bindings."; return false; }
	std::set<entity_t *> unique;
	std::set<std::string> identities;
	bool needsChange = false;
	for ( size_t i = 0; i < selection.size(); ++i ) {
		entity_t *entity = selection[i];
		const NodInstanceRecord *record = records[i];
		if ( !entity || !record || !unique.insert( entity ).second || !identities.insert( record->identity ).second ||
			record->identity != ValueForKey( entity, "nod_id" ) || record->name != ValueForKey( entity, "nod_type" ) ||
			( strcmp( ValueForKey( entity, "classname" ), "misc_model" ) &&
				!ModelTransform_IsNativeEntity( ValueForKey( entity, "classname" ) ) ) ||
			( record->schema != "14" && record->schema != "15" ) ) {
			error = "The selection differs from its resolved NSD binding or mixes native and ordinary entities."; return false;
		}

		bool present = false, geometry = false, preview = false;
		for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
			if ( strcmp( pair->key, key ) == 0 ) {
				present = true;
			}

			if ( strcmp( pair->key, "nod_geometry" ) == 0 ) {
				geometry = true;
			}

			if ( strcmp( pair->key, "nod_reference" ) == 0 ) {
				preview = true;
		}
		}

		if ( preview && record->schema != "15" ) { error = "The source binding schema does not match this preview."; return false; }
		const char *issue = NodInstance_Restriction( *record, key, present, geometry );
		const NodTemplateProperty *property = NodInstance_Property( record->local, key );
		NodPropertyInfo info;
		if ( property ) {
			info.storage = property->storage;
		}

		if ( !issue ) {
			issue = NodProperty_Validate( key, value, info );
		}

		if ( issue ) { error = issue; return false; }
		needsChange = needsChange || strcmp( ValueForKey( entity, key ), value ) != 0;
	}

	if ( !needsChange ) {
		return true;
	}

	Undo_Start( "NSD local property" );
	Undo_AddBrushList( &selected_brushes );
	for ( size_t i = 0; i < selection.size(); ++i ) {
		Undo_AddEntity( selection[i] );
	}

	for ( size_t i = 0; i < selection.size(); ++i ) {
		SetKeyValue( selection[i], key, value );
	}

	Undo_EndBrushList( &selected_brushes );
	for ( size_t i = 0; i < selection.size(); ++i ) {
		Undo_EndEntity( selection[i] );
	}

	Undo_End();
	changed = true;
	return true;
}

#endif
