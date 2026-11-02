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

inline const char *NodScript_ModelTemplate( const char *name ){
	return strcmp( name, "nod_console" ) == 0 ? "hackConsole_wall_activate" : "";
}

inline bool NodScript_Field( const char *key ){
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

inline const char *NodScript_Restriction( const char *name, const char *key, bool present, const char *script = "" ){
	if ( NodScript_Class( name ) ) {
		if ( strcmp( key, "origin" ) == 0 || strcmp( key, "angles" ) == 0 ||
			strcmp( key, "targetname" ) == 0 ||
			( strcmp( name, "nod_console" ) == 0 && strcmp( key, "target" ) == 0 ) ||
			( strcmp( name, "nod_sound_cue" ) == 0 && strcmp( key, "nod_cue" ) == 0 ) ||
			( strcmp( name, "nod_trigger_relay" ) == 0 &&
				( strcmp( key, "nod_relay_count" ) == 0 || NodScript_RelayField( key ) ) ) ||
			( strcmp( name, "nod_enemy_spawn" ) == 0 && ( strcmp( key, "nod_enemy" ) == 0 ||
				strcmp( key, "nod_destination" ) == 0 || strcmp( key, "nod_max_spawns" ) == 0 ) ) ) {
			return NULL;
		}

		return "Edit the preset's position, rotation, target or supported settings only.";
	}

	if ( strncmp( key, "nod_script_param_", 17 ) == 0 ) {
		const unsigned parameter = static_cast<unsigned>( strtoul( key + 17, NULL, 10 ) );
		if ( ( strcmp( script, "cDoorMove" ) == 0 && parameter != 1 ) ||
			( strcmp( script, "cButton" ) == 0 && parameter >= 8 && parameter != 12 && parameter != 13 ) ) {
			return "This native setting is fixed for the verified model/button workflow.";
		}
	}

	return present && NodScript_Field( key ) ? NULL : "Script schema and source identities are read-only.";
}

inline NodPropertyInfo NodScript_FieldInfo( const char *key, const char *kind, const char *script = "" ){
	NodPropertyInfo info;
	if ( strncmp( key, "nod_script_param_", 17 ) == 0 ) {
		info.name = "Script parameter " + std::string( key + 17 );
		info.storage = strcmp( kind, "1" ) == 0 ? "uint32 bits" : strcmp( kind, "2" ) == 0 ? "float32" :
			strcmp( kind, "3" ) == 0 ? "object reference" : strcmp( kind, "7" ) == 0 ? "string" : "raw";
		if ( strcmp( script, "cSoundStinger" ) == 0 && strcmp( key + 17, "6" ) == 0 ) {
			info.name = "Sound cue";
		} else if ( strcmp( script, "cTriggerOnAction" ) == 0 &&
			( strcmp( key + 17, "16" ) == 0 || strcmp( key + 17, "18" ) == 0 ||
				strcmp( key + 17, "20" ) == 0 || strcmp( key + 17, "22" ) == 0 ) ) {
			info.name = "Console linked target " + std::string( key + 17 );
		} else if ( strcmp( script, "cAIFightPath" ) == 0 && strcmp( key + 17, "2" ) == 0 ) {
			info.name = "Enemy template";
		} else if ( strcmp( script, "cAIFightPath" ) == 0 && strcmp( key + 17, "3" ) == 0 ) {
			info.name = "Lifetime spawn limit";
		} else if ( strcmp( script, "cTriggerRelay" ) == 0 && NodScript_Field( key ) ) {
			const unsigned parameter = static_cast<unsigned>( strtoul( key + 17, NULL, 10 ) );
			const char *labels[] = { "Relay target ", "Relay delay (seconds) ", "Relay trigger data " };
			info.name = labels[parameter % 3] + std::to_string( parameter / 3 + 1 );
		} else if ( strcmp( script, "cDoorMove" ) == 0 && strcmp( key + 17, "1" ) == 0 ) {
			info.name = "Mover speed (world units/second)";
		} else if ( strcmp( script, "cButton" ) == 0 ) {
			const unsigned parameter = static_cast<unsigned>( strtoul( key + 17, NULL, 10 ) );
			if ( parameter < 8 ) {
				info.name = "Button linked target " + std::to_string( parameter + 1 );
			} else if ( parameter == 12 ) {
				info.name = "Button one-shot (0/1)";
			} else if ( parameter == 13 ) {
				info.name = "Button initially locked (0/1)";
			}
		}
	} else if ( strncmp( key, "nod_frame_origin_", 17 ) == 0 ) {
		info.name = "Frame position " + std::string( key + 17 );
		info.storage = "vector3";
		if ( strcmp( script, "cAIFightPath" ) == 0 && strcmp( key + 17, "1" ) == 0 ) {
			info.name = "Spawn path destination";
		} else if ( strcmp( script, "cDoorMove" ) == 0 ) {
			info.name = strcmp( key + 17, "0" ) == 0 ? "Mover stop 0 (closed / lower)" : "Mover stop 1 (open / upper)";
		}
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
	const char *script = "" ){
	if ( strcmp( script, "cDoorMove" ) == 0 && strcmp( key, "nod_script_param_1" ) == 0 ) {
		float speed[3];
		return ModelTransform_Parse( value, 1, speed, true ) && speed[0] > 0 ?
			NULL : "Enter a finite positive mover speed in world units/second.";
	}

	if ( strcmp( script, "cButton" ) == 0 &&
		( strcmp( key, "nod_script_param_12" ) == 0 || strcmp( key, "nod_script_param_13" ) == 0 ) ) {
		return strcmp( value, "0" ) == 0 || strcmp( value, "1" ) == 0 ? NULL : "Enter 0 or 1.";
	}

	if ( strcmp( key, "origin" ) == 0 || strcmp( key, "angles" ) == 0 ||
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

	if ( strcmp( key, "nod_enemy" ) == 0 ) {
		return strcmp( value, "hydralisk" ) == 0 || strcmp( value, "zealot" ) == 0 ||
			strcmp( value, "Marine" ) == 0 ? NULL : "Choose hydralisk, zealot or Marine (case-sensitive).";
	}

	if ( strcmp( key, "nod_max_spawns" ) == 0 ) {
		if ( !NodProperty_Unsigned( value ) || ( value[0] == '0' && value[1] ) ) {
			return "Enter a canonical lifetime spawn limit from 1 through 25.";
		}

		const unsigned long number = strtoul( value, NULL, 10 );
		return number >= 1 && number <= 25 ? NULL : "Enter a lifetime spawn limit from 1 through 25.";
	}

	if ( strcmp( key, "nod_cue" ) == 0 ) {
		if ( !NodScript_Reference( value ) || strlen( value ) > 31 || strchr( value, ':' ) ||
			value[0] == '.' || value[strlen( value ) - 1] == '.' ) {
			return "Enter a cue in the deployed level bank: 1..31 ASCII letters/digits/underscore/hyphen/dot, without a path.";
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
