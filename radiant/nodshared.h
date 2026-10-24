/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodshared.h
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

#ifndef RADIANT_NODSHARED_H
#define RADIANT_NODSHARED_H

#include "nodtemplates.h"
#include "nodclass.h"

enum class NodSharedOperation { edit, insert, remove };

struct NodSharedChange {
	std::string context, owner, archive, id, before, after;
	std::string inheritedOwner, inheritedArchive;
	std::string slot, payload, order;
	NodSharedOperation operation;
	NodSharedChange() : operation( NodSharedOperation::edit ) {}
};

typedef bool ( *NodSharedRunner )( const std::vector<std::string> &, std::string &, std::string & );

inline bool NodShared_PreviewRemoval( NodSharedChange &change, NodSharedRunner run, std::string &error ){
	std::vector<std::string> archives, options;
	if ( !NodTemplate_Archives( change.context.c_str(), archives, error ) ) {
		return false;
	}

	for ( size_t i = 0; i < archives.size(); ++i ) {
		options.push_back( "--archive" ); options.push_back( archives[i] );
	}

	const char *keys[] = { "--template", "--target-archive", "--preview-remove", "--expect" };
	const std::string values[] = { change.owner, change.archive, change.id, change.before };
	for ( size_t i = 0; i < 4; ++i ) { options.push_back( keys[i] ); options.push_back( values[i] ); }
	std::string output;
	if ( !run( options, output, error ) ) {
		return false;
	}

	NodTemplateCatalog catalog;
	if ( NodTemplate_ParseCatalog( output.data(), output.size(), catalog, error ) && catalog.archives == change.context &&
		catalog.choices.size() == 1 && catalog.choices[0].detailed && catalog.choices[0].archive == change.archive &&
		g_ascii_strcasecmp( catalog.choices[0].name.c_str(), change.owner.c_str() ) == 0 ) {
		const NodTemplateChoice &choice = catalog.choices[0];
		guint32 id;
		unsigned char payload[32];
		if ( !choice.removalSlot.empty() && NodClass_DecodeModifier( choice.removalPayload, id, payload ) &&
			change.id == std::to_string( id ) ) {
			for ( size_t i = 0; i < choice.effective.size(); ++i ) {
				const NodTemplateProperty &property = choice.effective[i];
				if ( property.id != change.id || property.local || !property.editable ||
					g_ascii_strcasecmp( property.owner.c_str(), choice.name.c_str() ) == 0 ) {
					continue;
				}

				NodSharedChange next = change;
				next.operation = NodSharedOperation::remove;
				next.inheritedOwner = property.owner; next.inheritedArchive = property.archive; next.after = property.value;
				next.slot = choice.removalSlot; next.payload = choice.removalPayload; next.order = choice.removalOrder;
				change = next;
				return true;
			}
		}
	}

	error = "Invalid SDK removal preview; nothing was saved. " + error;
	return false;
}

inline bool NodShared_Write( const NodSharedChange &change, bool forward, NodSharedRunner run,
	std::string &canonical, std::string &error ){
	std::vector<std::string> archives, options;
	if ( !NodTemplate_Archives( change.context.c_str(), archives, error ) ) {
		return false;
	}

	for ( size_t i = 0; i < archives.size(); ++i ) {
		options.push_back( "--archive" ); options.push_back( archives[i] );
	}

	const bool structural = change.operation != NodSharedOperation::edit;
	const bool removing = change.operation == NodSharedOperation::insert ? !forward :
		change.operation == NodSharedOperation::remove && forward;
	const char *operation = change.operation == NodSharedOperation::insert ? ( forward ? "--override-property" : "--remove-override" ) :
		change.operation == NodSharedOperation::remove ? ( forward ? "--remove-override" : "--restore-override" ) : "--write-property";
	const char *keys[] = { "--template", "--target-archive", operation, "--expect", "--value" };
	const std::string values[] = { change.owner, change.archive, change.id,
		forward ? change.before : change.after, forward ? change.after : change.before };
	for ( size_t i = 0; i < 5; ++i ) { options.push_back( keys[i] ); options.push_back( values[i] ); }
	if ( structural ) {
		options.push_back( "--expect-owner" ); options.push_back( change.inheritedOwner );
		options.push_back( "--expect-archive" ); options.push_back( change.inheritedArchive );
	}

	if ( change.operation == NodSharedOperation::remove ) {
		options.push_back( "--slot" ); options.push_back( change.slot );
		options.push_back( "--modifier" ); options.push_back( change.payload );
		options.push_back( "--order" ); options.push_back( change.order );
	}

	std::string output;
	if ( !run( options, output, error ) ) {
		return false;
	}

	NodTemplateCatalog catalog;
	if ( NodTemplate_ParseCatalog( output.data(), output.size(), catalog, error ) && catalog.archives == change.context &&
		catalog.choices.size() == 1 && catalog.choices[0].detailed &&
		g_ascii_strcasecmp( catalog.choices[0].name.c_str(), change.owner.c_str() ) == 0 && catalog.choices[0].archive == change.archive ) {
		const NodTemplateChoice &choice = catalog.choices[0];
		const std::string expectedOwner = removing ? change.inheritedOwner : choice.name;
		const std::string expectedArchive = removing ? change.inheritedArchive : change.archive;
		for ( size_t i = 0; i < choice.effective.size(); ++i ) {
			const NodTemplateProperty &property = choice.effective[i];
			if ( property.id == change.id && !property.local && g_ascii_strcasecmp( property.owner.c_str(), expectedOwner.c_str() ) == 0 &&
				property.archive == expectedArchive && property.editable ) {
				canonical = property.value;
				return true;
			}
		}
	}

	error = "SDK write completed; result validation failed. Reload NOT metadata. " + error;
	return false;
}

class NodSharedHistory {
	std::vector<NodSharedChange> changes;
	size_t cursor;
public:
	NodSharedHistory() : cursor( 0 ) {}
	bool canUndo() const { return cursor != 0; }
	bool canRedo() const { return cursor < changes.size(); }

	bool save( NodSharedChange change, NodSharedRunner run, std::string &error ){
		std::string canonical;
		if ( !NodShared_Write( change, true, run, canonical, error ) ) {
			return false;
		}

		if ( canonical == change.before && change.operation == NodSharedOperation::edit ) {
			return true;
		}

		change.after = canonical;
		changes.resize( cursor );
		changes.push_back( change );
		++cursor;
		return true;
	}

	bool undo( NodSharedRunner run, std::string &error ){
		if ( !canUndo() ) { error = "No shared save to undo."; return false; }
		std::string canonical;
		if ( !NodShared_Write( changes[cursor - 1], false, run, canonical, error ) ) {
			return false;
		}

		if ( canonical != changes[cursor - 1].before ) {
			error = "Shared undo returned an unexpected value; reload NOT metadata."; return false;
		}

		--cursor;
		return true;
	}

	bool redo( NodSharedRunner run, std::string &error ){
		if ( !canRedo() ) { error = "No shared save to redo."; return false; }
		std::string canonical;
		if ( !NodShared_Write( changes[cursor], true, run, canonical, error ) ) {
			return false;
		}

		if ( canonical != changes[cursor].after ) {
			error = "Shared redo returned an unexpected value; reload NOT metadata."; return false;
		}

		++cursor;
		return true;
	}
};

#endif
