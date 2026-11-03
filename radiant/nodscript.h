/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodscript.h
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

#ifndef RADIANT_NODSCRIPT_H
#define RADIANT_NODSCRIPT_H

#include "nodproperties.h"
#include "nodtemplates.h"
#include <cstdlib>

inline bool NodScript_Class( const char *name ){
	return strcmp( name, "nod_console" ) == 0 || strcmp( name, "nod_sound_cue" ) == 0 ||
		strcmp( name, "nod_enemy_spawn" ) == 0 || strcmp( name, "nod_trigger_relay" ) == 0;
}

inline bool NodScript_RelayField( const char *key ){
	const char *index = strncmp( key, "nod_relay_target_", 17 ) == 0 ? key + 17 :
		strncmp( key, "nod_relay_delay_", 16 ) == 0 ? key + 16 : NULL;
	return index && ( ( index[0] >= '0' && index[0] <= '9' && !index[1] ) ||
		strcmp( index, "10" ) == 0 || strcmp( index, "11" ) == 0 );
}

inline const char *NodScript_ModelTemplate( const char *name, const char *selected = "" ){
	return strcmp( name, "nod_console" ) == 0 ? ( *selected ? selected : "hackConsole_wall_activate" ) : "";
}

inline bool NodScript_Field( const char *key ){
	if ( strcmp( key, "nod_region_min" ) == 0 || strcmp( key, "nod_region_max" ) == 0 ) {
		return true;
	}

	const char *suffix = strncmp( key, "nod_script_param_", 17 ) == 0 ? key + 17 :
		strncmp( key, "nod_frame_origin_", 17 ) == 0 ? key + 17 :
		strncmp( key, "nod_frame_rotation_", 19 ) == 0 ? key + 19 :
		strncmp( key, "nod_frame_sector_", 17 ) == 0 ? key + 17 : NULL;
	if ( !suffix || !*suffix || ( suffix[0] == '0' && suffix[1] ) ) {
		return false;
	}

	unsigned index = 0;
	for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( suffix ); *p; ++p ) {
		if ( *p < '0' || *p > '9' ) {
			return false;
		}

		index = index * 10 + *p - '0';
		if ( index > 1023 ) {
			return false;
		}
	}

	return true;
}

inline bool NodScript_Reference( const char *value ){
	if ( !*value || strlen( value ) > 63 ) {
		return false;
	}

	for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( value ); *p; ++p ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' ) ||
			( *p >= '0' && *p <= '9' ) || strchr( "_-.:", *p ) ) ) {
			return false;
		}
	}

	return true;
}

inline const NodBehavior *NodScript_Behavior( const char *script ){
	const std::map<std::string, NodBehavior> &behaviors = NodClasses_Catalog().behaviors;
	std::map<std::string, NodBehavior>::const_iterator found = behaviors.find( script );
	return found == behaviors.end() ? NULL : &found->second;
}

inline const char *NodScript_Restriction( const char *name, const char *key, bool present, const char *script = "" ){
	if ( NodScript_Class( name ) ) {
		if ( strcmp( key, "origin" ) == 0 || strcmp( key, "angles" ) == 0 ||
			strcmp( key, "targetname" ) == 0 ||
			( strcmp( name, "nod_console" ) == 0 &&
				( strcmp( key, "target" ) == 0 || strcmp( key, "nod_console_template" ) == 0 ) ) ||
			( strcmp( name, "nod_sound_cue" ) == 0 && strcmp( key, "nod_cue" ) == 0 ) ||
			( strcmp( name, "nod_trigger_relay" ) == 0 &&
				( strcmp( key, "nod_relay_count" ) == 0 || NodScript_RelayField( key ) ) ) ||
			( strcmp( name, "nod_enemy_spawn" ) == 0 && ( strcmp( key, "nod_enemy" ) == 0 ||
				strcmp( key, "nod_destination" ) == 0 || strcmp( key, "nod_max_spawns" ) == 0 ) ) ) {
			return NULL;
		}

		return "Edit the preset's position, rotation, target or supported settings only.";
	}

	if ( *script ) {
		const NodBehavior *behavior = NodScript_Behavior( script );
		if ( !behavior ) {
			return "Reload the NOT catalog with the updated SDK to edit this handler.";
		}

		return present && behavior->Field( key, "editable" ) == "1" ? NULL :
			"This native schema field is read-only.";
	}

	return present && NodScript_Field( key ) ? NULL : "Script schema and source identities are read-only.";
}

inline NodPropertyInfo NodScript_FieldInfo( const char *key, const char *kind, const char *script = "" ){
	NodPropertyInfo info;
	const NodBehavior *behavior = NodScript_Behavior( script );
	if ( behavior && behavior->fields.count( key ) ) {
		info.name = behavior->Field( key, "label" );
		info.storage = behavior->Field( key, "storage" );
		return info;
	}

	if ( strncmp( key, "nod_script_param_", 17 ) == 0 ) {
		info.name = "Script parameter " + std::string( key + 17 );
		info.storage = strcmp( kind, "1" ) == 0 ? "uint32 bits" : strcmp( kind, "2" ) == 0 ? "float32" :
			strcmp( kind, "3" ) == 0 ? "object reference" : strcmp( kind, "7" ) == 0 ? "string" : "raw";
	} else if ( strncmp( key, "nod_frame_origin_", 17 ) == 0 ) {
		info.name = "Frame position " + std::string( key + 17 );
		info.storage = "vector3";
	} else if ( strncmp( key, "nod_frame_rotation_", 19 ) == 0 ) {
		info.name = "Frame native rotation " + std::string( key + 19 );
		info.storage = "vector3";
	} else if ( strncmp( key, "nod_frame_sector_", 17 ) == 0 ) {
		info.name = "Frame sector " + std::string( key + 17 );
		info.storage = "uint32 bits";
	}

	return info;
}

inline const char *NodScript_Validate( const char *key, const char *value, const char *kind, const char *name = "",
	const char *script = "", const NodTemplateCatalog *context = NULL ){
	const NodBehavior *behavior = NodScript_Behavior( script );
	if ( behavior && strcmp( kind, "1" ) == 0 &&
		behavior->Field( key, "min" ) == "0" && behavior->Field( key, "max" ) == "1" &&
		strcmp( value, "0" ) != 0 && strcmp( value, "1" ) != 0 ) {
		return "Enter 0 or 1.";
	}

	if ( behavior && ( !behavior->Field( key, "min" ).empty() || !behavior->Field( key, "max" ).empty() ) ) {
		float number[3], minimum[3], maximum[3];
		if ( !ModelTransform_Parse( value, 1, number, true ) ||
			( ModelTransform_Parse( behavior->Field( key, "min" ).c_str(), 1, minimum, true ) &&
				( behavior->Field( key, "exclusive-min" ) == "1" ? number[0] <= minimum[0] : number[0] < minimum[0] ) ) ||
			( ModelTransform_Parse( behavior->Field( key, "max" ).c_str(), 1, maximum, true ) && number[0] > maximum[0] ) ) {
			return "Value is outside the SDK behavior's permitted numeric range.";
		}

		if ( strcmp( kind, "1" ) == 0 &&
			( !NodProperty_Unsigned( value ) || ( value[0] == '0' && value[1] ) ) ) {
			return "Enter a canonical unsigned integer within the SDK behavior's permitted range.";
		}
	}

	if ( strcmp( key, "origin" ) == 0 || strcmp( key, "angles" ) == 0 ||
		( behavior && behavior->Field( key, "storage" ) == "vector3" ) ||
		strcmp( key, "nod_destination" ) == 0 || strncmp( key, "nod_frame_origin_", 17 ) == 0 ||
		strncmp( key, "nod_frame_rotation_", 19 ) == 0 ) {
		float vector[3];
		return ModelTransform_Parse( value, 3, vector, true ) ? NULL : "Enter three finite world-coordinate values.";
	}

	if ( strcmp( key, "target" ) == 0 || strcmp( key, "targetname" ) == 0 || strcmp( kind, "3" ) == 0 ||
		strncmp( key, "nod_relay_target_", 17 ) == 0 ) {
		if ( strcmp( name, "nod_console" ) == 0 && strcmp( key, "targetname" ) == 0 && !*value ) {
			return NULL;
		}

		if ( strcmp( kind, "3" ) == 0 && ( strcmp( value, "-1" ) == 0 || strcmp( value, "-2" ) == 0 ) ) {
			return NULL;
		}

		return NodScript_Reference( value ) ? NULL : "Enter a target name or nsd:OFFSET object reference (1..63 ASCII characters).";
	}

	if ( strncmp( key, "nod_relay_delay_", 16 ) == 0 ) {
		float delay[3];
		return ModelTransform_Parse( value, 1, delay, true ) && delay[0] >= 0 ?
			NULL : "Enter a finite nonnegative delay in seconds.";
	}
	if ( strcmp( key, "nod_relay_count" ) == 0 ) {
		return NodProperty_Unsigned( value ) && !( value[0] == '0' && value[1] ) &&
			strtoul( value, NULL, 10 ) >= 1 && strtoul( value, NULL, 10 ) <= 12 ?
			NULL : "Enter a canonical action count from 1 through 12.";
	}

	if ( strcmp( key, "nod_enemy" ) == 0 || ( behavior && behavior->Field( key, "resource" ) == "actor" ) ) {
		const NodTemplateCatalog &catalog = context ? *context : NodClasses_Catalog();
		for ( size_t i = 0; i < catalog.choices.size(); ++i ) {
			if ( catalog.choices[i].name == value && catalog.choices[i].spawnable ) {
				return NULL;
			}
		}

		return "Choose a modeled AI actor from the active NOT catalog (exact spelling).";
	}

	if ( strcmp( key, "nod_console_template" ) == 0 ) {
		const NodTemplateCatalog &catalog = context ? *context : NodClasses_Catalog();
		for ( size_t i = 0; i < catalog.choices.size(); ++i ) {
			if ( catalog.choices[i].name == value && catalog.choices[i].handlers.count( "cTriggerOnAction" ) ) {
				return NULL;
			}
		}

		return "Choose an interactive modeled prop from the active NOT catalog.";
	}

	if ( strcmp( key, "nod_max_spawns" ) == 0 ) {
		if ( !NodProperty_Unsigned( value ) || ( value[0] == '0' && value[1] ) ) {
			return "Enter a canonical lifetime spawn limit from 1 through 25.";
		}

		const unsigned long number = strtoul( value, NULL, 10 );
		return number >= 1 && number <= 25 ? NULL : "Enter a lifetime spawn limit from 1 through 25.";
	}

	if ( behavior && ( behavior->Field( key, "resource" ) == "reverb" ||
		behavior->Field( key, "resource" ) == "save-marker" ) ) {
		const bool marker = behavior->Field( key, "resource" ) == "save-marker";
		if ( strcmp( value, "-1" ) == 0 ||
			( NodProperty_Unsigned( value ) && !( value[0] == '0' && value[1] ) &&
				strtoul( value, NULL, 10 ) <= ( marker ? 15 : 25 ) ) ) {
			return NULL;
		}

		return marker ? "Enter -1 for no marker registration, or a canonical objective marker ID from 0 through 15." :
			"Enter -1 to keep the current reverb, or a canonical preset ID from 0 through 25.";
	}

	if ( strcmp( key, "nod_cue" ) == 0 ||
		( behavior && ( behavior->Field( key, "resource" ) == "cue" || behavior->Field( key, "resource" ) == "ambient" ||
			behavior->Field( key, "resource" ) == "native-resource" ) ) ) {
		if ( behavior && ( behavior->Field( key, "resource" ) == "ambient" ||
			behavior->Field( key, "allow-empty" ) == "1" ) && !*value ) {
			return NULL;
		}

		if ( !NodScript_Reference( value ) || strlen( value ) > 31 || strchr( value, ':' ) ||
			value[0] == '.' || value[strlen( value ) - 1] == '.' ) {
			return behavior && behavior->Field( key, "resource" ) == "native-resource" ?
				"Enter a deployed resource/key: 1..31 ASCII letters/digits/underscore/hyphen/dot, without a path." :
				"Enter a cue in the deployed level bank: 1..31 ASCII letters/digits/underscore/hyphen/dot, without a path.";
		}

		return NULL;
	}

	if ( behavior && behavior->Field( key, "resource" ) == "objective" ) {
		if ( strlen( value ) > 31 || strspn( value, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" ) != strlen( value ) ) {
			return "Enter an objective key from the active mission's OBJECTIVE entries (up to 31 ASCII letters/digits/underscores), or leave blank.";
		}

		return NULL;
	}

	if ( behavior && behavior->Field( key, "resource" ) == "cutscene" ) {
		const char *dot = strchr( value, '.' );
		if ( !*value || strlen( value ) > 31 || dot == value ||
			strspn( value, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-." ) != strlen( value ) ||
			( dot && g_ascii_strcasecmp( dot, ".ncs" ) != 0 ) ) {
			return "Enter a deployed cutscene name: 1..31 ASCII letters/digits/underscore/hyphen, optionally ending in .ncs; no path.";
		}

		return NULL;
	}

	if ( strcmp( kind, "7" ) == 0 ) {
		if ( strlen( value ) > 255 ) {
			return "Script strings allow at most 255 printable ASCII bytes in the MAP.";
		}

		for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( value ); *p; ++p ) {
			if ( *p < 32 || *p > 126 || *p == '"' || *p == '\\' ) {
				return "Script strings require printable ASCII without quotes or backslashes.";
			}
		}

		return NULL;
	}

	if ( strncmp( key, "nod_frame_sector_", 17 ) == 0 ) {
		return strcmp( value, "0" ) == 0 ? NULL : "Edited script frames currently support sector 0 only.";
	}

	return NodProperty_Validate( key, value, NodScript_FieldInfo( key, kind ) );
}

#endif
