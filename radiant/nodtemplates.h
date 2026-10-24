/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodtemplates.h
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

#ifndef RADIANT_NODTEMPLATES_H
#define RADIANT_NODTEMPLATES_H

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <algorithm>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "modeltransform.h"
#include "nodproperties.h"

struct NodTemplateProperty {
	std::string id, name, storage, value, archive, owner, problem, offset;
	bool editable, local;
	NodTemplateProperty() : editable( false ), local( false ) {}
};
struct NodTemplateChoice {
	std::string name, archive, type, properties, model, problem, context, parent, modifiers;
	std::string removalSlot, removalPayload, removalOrder;
	std::vector<NodTemplateProperty> effective;
	bool placeable, detailed;
	NodTemplateChoice() : placeable( false ), detailed( false ) {}
};
struct NodTemplateCatalog {
	std::vector<NodTemplateChoice> choices;
	std::string archives, diagnostics;
};

inline bool NodTemplate_MapString( const std::string &value, size_t maximum ){
	if ( value.empty() || value.size() > maximum ) {
		return false;
	}

	bool nonblank = false;
	for ( size_t i = 0; i < value.size(); ++i ) {
		const unsigned char c = value[i];
		if ( c < 32 || c > 126 || c == '"' ) {
			return false;
		}

		if ( c != ' ' ) {
			nonblank = true;
	}
	}

	return nonblank;
}

inline bool NodTemplate_Archives( const char *input, std::vector<std::string> &archives, std::string &error ){
	archives.clear();
	error.clear();
	if ( !*input ) {
		return true;
	}

	gchar **parts = g_strsplit( input, ";", -1 );
	for ( size_t i = 0; parts[i]; ++i ) {
		const std::string name = g_strstrip( parts[i] );
		if ( !NodTemplate_MapString( name, 31 ) || archives.size() == 9 ) {
			error = "Use up to 9 archive names (1-31 printable ASCII bytes), separated by ';'.";
			break;
		}

		archives.push_back( name );
	}

	g_strfreev( parts );
	if ( !error.empty() ) { archives.clear(); return false; }
	return true;
}

inline std::string NodTemplate_Attribute( xmlNodePtr node, const char *name ){
	xmlChar *value = xmlGetProp( node, BAD_CAST name );
	const std::string text = value ? reinterpret_cast<const char *>( value ) : "";
	xmlFree( value );
	return text;
}

inline bool NodTemplate_Number( const std::string &text, guint64 maximum ){
	if ( text.empty() || ( text.size() > 1 && text[0] == '0' ) ) {
		return false;
	}

	guint64 value = 0;
	for ( size_t i = 0; i < text.size(); ++i ) {
		if ( text[i] < '0' || text[i] > '9' || static_cast<guint64>( text[i] - '0' ) > maximum ||
			value > ( maximum - ( text[i] - '0' ) ) / 10 ) {
			return false;
		}

		value = value * 10 + text[i] - '0';
		if ( value > maximum ) {
			return false;
	}
	}

	return true;
}

inline bool NodTemplate_CanonicalArchive( const std::string &name ){
	if ( !NodTemplate_MapString( name, 31 ) || name.find_first_of( "\\:;" ) != std::string::npos ) {
		return false;
	}

	for ( size_t i = 0; i < name.size(); ++i ) {
		if ( name[i] >= 'A' && name[i] <= 'Z' ) {
			return false;
		}
	}

	size_t start = 0;
	for ( ;; ) {
		const size_t end = name.find( '/', start );
		const std::string part = name.substr( start, end - start );
		if ( part.empty() || part == "." || part == ".." ) {
			return false;
		}

		if ( end == std::string::npos ) {
			return part.find( '.' ) != std::string::npos;
		}

		start = end + 1;
	}
}

inline bool NodTemplate_ReadProperty( xmlNodePtr node, const std::set<std::string> &sources,
	NodTemplateProperty &value, std::string &error, bool native = false ){
	if ( node->type != XML_ELEMENT_NODE || !xmlStrEqual( node->name, BAD_CAST "property" ) || node->children ) {
		error = "Invalid SDK property metadata element."; return false;
	}

	value.id = NodTemplate_Attribute( node, "id" );
	value.name = NodTemplate_Attribute( node, "name" );
	value.storage = NodTemplate_Attribute( node, "storage" );
	value.value = NodTemplate_Attribute( node, "value" );
	value.archive = NodTemplate_Attribute( node, "archive" );
	value.owner = NodTemplate_Attribute( node, "template" );
	value.problem = NodTemplate_Attribute( node, "problem" );
	const std::string access = NodTemplate_Attribute( node, "editable" );
	value.editable = access == "1";
	if ( native ) {
		const std::string source = NodTemplate_Attribute( node, "source" );
		value.local = source == "nsd";
		value.offset = NodTemplate_Attribute( node, "offset" );
		if ( ( source != "nsd" && source != "not" ) ||
			!NodTemplate_Number( value.offset, G_GUINT64_CONSTANT( 4294967295 ) ) ||
			( value.local && !value.archive.empty() ) ) {
			error = "Invalid SDK native property source or offset."; return false;
		}
	}
	else if ( NodTemplate_Attribute( node, "source" ) == "placement" ) {
		value.local = true;
		if ( !value.archive.empty() ) { error = "Invalid placement property provenance."; return false; }
	}

	NodPropertyInfo info;
	info.storage = value.storage;
	if ( !NodTemplate_Number( value.id, G_GUINT64_CONSTANT( 4294967295 ) ) ||
		( !value.local && !sources.count( value.archive ) ) || value.owner.empty() || value.owner.size() > 128 ||
		!NodTemplate_MapString( value.name, 64 ) || value.value.size() > 128 || value.problem.size() > 4096 ||
		( access != "0" && access != "1" ) ||
		( value.storage != "string32" && value.storage != "float32" && value.storage != "uint32 bits" &&
			value.storage != "vector3" && value.storage != "raw" ) ||
		( value.editable && ( !value.problem.empty() || value.id == "0" || value.id == "1" ||
			value.id == "2" || value.id == "3" || value.id == "20" ||
			NodProperty_Validate( ( "nod_prop_" + value.id ).c_str(), value.value.c_str(), info ) ) ) ||
		( !value.editable && value.problem.empty() ) ) {
		error = "Invalid SDK effective property or provenance."; return false;
	}

	return true;
}

inline bool NodTemplate_ParseCatalog( const char *bytes, size_t size, NodTemplateCatalog &catalog, std::string &error ){
	error.clear();
	if ( !bytes || !size || size > 16 * 1024 * 1024 ) { error = "Missing or oversized SDK template catalog."; return false; }
	xmlDocPtr doc = xmlReadMemory( bytes, static_cast<int>( size ), "template-catalog.xml", "UTF-8",
		XML_PARSE_NONET | XML_PARSE_NOBLANKS );
	xmlNodePtr root = doc ? xmlDocGetRootElement( doc ) : NULL;
	if ( !root || doc->intSubset || doc->extSubset || !xmlStrEqual( root->name, BAD_CAST "nod-template-catalog" )
		|| NodTemplate_Attribute( root, "version" ) != "1" ) {
		if ( doc ) {
			xmlFreeDoc( doc );
		}

		error = "Invalid SDK template catalog version, root or DTD."; return false;
	}

	NodTemplateCatalog next;
	std::set<std::string> sources, names;
	for ( xmlNodePtr node = root->children; node && error.empty(); node = node->next ) {
		if ( node->type != XML_ELEMENT_NODE ) { error = "Unexpected catalog content."; break; }
		if ( xmlStrEqual( node->name, BAD_CAST "archive" ) ) {
			const std::string name = NodTemplate_Attribute( node, "name" );
			if ( !NodTemplate_CanonicalArchive( name ) ||
				!sources.insert( name ).second || sources.size() > 10 ||
				( sources.size() == 1 && name != "global.not" ) ) {
				error = "Invalid catalog archive order or identity.";
			} else if ( name != "global.not" ) {
				if ( !next.archives.empty() ) {
					next.archives += ';';
				}

				next.archives += name;
			}
		}
		else if ( xmlStrEqual( node->name, BAD_CAST "warning" ) ) {
			next.diagnostics += NodTemplate_Attribute( node, "text" ) + "\n";
		}
		else if ( xmlStrEqual( node->name, BAD_CAST "template" ) ) {
			NodTemplateChoice choice;
			choice.name = NodTemplate_Attribute( node, "name" );
			choice.archive = NodTemplate_Attribute( node, "archive" );
			choice.type = NodTemplate_Attribute( node, "type" );
			choice.properties = NodTemplate_Attribute( node, "properties" );
			choice.model = NodTemplate_Attribute( node, "model" );
			choice.problem = NodTemplate_Attribute( node, "problem" );
			choice.parent = NodTemplate_Attribute( node, "parent" );
			const std::string enabled = NodTemplate_Attribute( node, "placeable" );
			choice.placeable = enabled == "1";
			const std::string details = NodTemplate_Attribute( node, "details" );
			choice.detailed = details == "1";
			choice.removalSlot = NodTemplate_Attribute( node, "removal-slot" );
			choice.removalPayload = NodTemplate_Attribute( node, "removal-payload" );
			choice.removalOrder = NodTemplate_Attribute( node, "removal-order" );
			if ( xmlHasProp( node, BAD_CAST "removal-slot" ) || xmlHasProp( node, BAD_CAST "removal-payload" ) ||
				xmlHasProp( node, BAD_CAST "removal-order" ) ) {
				if ( !choice.detailed || !xmlHasProp( node, BAD_CAST "removal-slot" ) ||
					!xmlHasProp( node, BAD_CAST "removal-payload" ) || !xmlHasProp( node, BAD_CAST "removal-order" ) ||
					!NodTemplate_Number( choice.removalSlot, 127 ) || choice.removalPayload.size() != 72 ||
					choice.removalPayload.find_first_not_of( "0123456789abcdef" ) != std::string::npos ||
					choice.removalOrder.size() > 127 * 8 || choice.removalOrder.size() % 8 ||
					choice.removalOrder.find_first_not_of( "0123456789abcdef" ) != std::string::npos ||
					g_ascii_strtoull( choice.removalSlot.c_str(), NULL, 10 ) > choice.removalOrder.size() / 8 ) {
					error = "Invalid SDK native override removal proof."; break;
				}
			}

			if ( ( enabled != "0" && enabled != "1" ) || !sources.count( choice.archive ) ||
				( !details.empty() && details != "1" ) ||
				!NodTemplate_Number( choice.type, G_GUINT64_CONSTANT( 4294967295 ) ) ||
				!NodTemplate_Number( choice.properties, 128 ) || next.choices.size() == 20480 ||
				( choice.placeable && ( !NodTemplate_MapString( choice.name, 31 ) || !choice.problem.empty() ||
					choice.properties == "0" ) ) || ( !choice.placeable && choice.problem.empty() ) ) {
				error = "Invalid SDK template entry or provenance.";
			} else {
				std::set<std::string> ids;
				for ( xmlNodePtr property = node->children; property; property = property->next ) {
					if ( property->type == XML_TEXT_NODE && xmlIsBlankNode( property ) ) {
						continue;
					}

					if ( !choice.detailed ) {
						error = "Invalid SDK property metadata element."; break;
					}

					NodTemplateProperty value;
					if ( !NodTemplate_ReadProperty( property, sources, value, error ) ) {
						break;
					}

					if ( !ids.insert( value.id ).second || choice.effective.size() == 128 ) {
						error = "Duplicate or excessive SDK effective properties."; break;
					}

					choice.effective.push_back( value );
				}

				if ( choice.detailed && g_ascii_strtoull( choice.properties.c_str(), NULL, 10 ) != choice.effective.size() ) {
					error = "SDK effective property count does not match its entries.";
				}

				if ( choice.detailed && choice.placeable ) {
					bool typed = false;
					for ( size_t i = 0; i < choice.effective.size(); ++i ) {
						if ( choice.effective[i].id == "20" && choice.effective[i].storage == "uint32 bits" &&
							choice.effective[i].value == choice.type && !choice.effective[i].editable ) {
							typed = true;
						}
					}

					if ( !typed ) {
						error = "SDK effective properties do not contain the declared native type.";
				}
				}

				if ( !error.empty() ) {
					break;
				}

				if ( choice.placeable ) {
					gchar *folded = g_ascii_strdown( choice.name.c_str(), -1 );
					const bool unique = names.insert( folded ).second;
					g_free( folded );
					if ( !unique ) { error = "Duplicate winning template name in SDK catalog."; break; }
				}

				next.choices.push_back( choice );
			}
		}
		else {
			error = "Unsupported SDK catalog element.";
	}
	}

	xmlFreeDoc( doc );
	if ( sources.empty() && error.empty() ) {
		error = "SDK catalog has no Global archive.";
	}

	if ( !error.empty() ) {
		return false;
	}

	for ( size_t i = 0; i < next.choices.size(); ++i ) {
		next.choices[i].context = next.archives;
	}

	catalog = next;
	return true;
}

inline const NodTemplateProperty *NodTemplate_Property( const NodTemplateChoice &choice, const char *key ){
	for ( size_t i = 0; i < choice.effective.size(); ++i ) {
		if ( std::string( "nod_prop_" ) + choice.effective[i].id == key ) {
			return &choice.effective[i];
		}
	}

	return NULL;
}

inline const char *NodTemplate_OverrideRestriction( const NodTemplateChoice &choice, const char *key, bool remove, bool local ){
	if ( !choice.detailed || !choice.placeable ) {
		return "Reload a resolved, typed template before editing.";
	}

	const NodTemplateProperty *property = NodTemplate_Property( choice, key );
	if ( !property ) {
		return "Property absent from the template; arbitrary keys are unsupported.";
	}

	if ( !property->editable ) {
		return property->problem.c_str();
	}

	if ( remove && !local ) {
		return "Inherited default, not a local override. Edit shared values in NOTEditor.";
	}

	return NULL;
}

inline std::string NodTemplate_FormatPosition( const float position[3] ){
	std::string result;
	for ( int axis = 0; axis < 3; ++axis ) {
		char component[G_ASCII_DTOSTR_BUF_SIZE];
		g_ascii_formatd( component, sizeof( component ), "%.9g", position[axis] );
		if ( axis ) {
			result += ' ';
		}

		result += component;
	}

	return result;
}

inline bool NodTemplate_Keys( const NodTemplateChoice &choice, const std::string &archives,
	const std::string &origin, std::vector<std::pair<std::string, std::string> > &keys, std::string &error, bool registered = false ){
	keys.clear();
	if ( !choice.placeable || !choice.problem.empty() || !NodTemplate_MapString( choice.name, 31 ) ||
		!NodTemplate_CanonicalArchive( choice.archive ) || !NodTemplate_Number( choice.properties, 128 ) ||
		choice.properties == "0" || !NodTemplate_Number( choice.type, G_GUINT64_CONSTANT( 4294967295 ) ) ) {
		error = "Select a resolved, MAP-safe template with native type metadata."; return false;
	}

	std::vector<std::string> context;
	if ( !NodTemplate_Archives( archives.c_str(), context, error ) ) {
		return false;
	}

	std::set<std::string> sources;
	std::string canonical;
	for ( size_t i = 0; i < context.size(); ++i ) {
		if ( !NodTemplate_CanonicalArchive( context[i] ) || context[i] == "global.not" || !sources.insert( context[i] ).second ) {
			error = "Archive context must be canonical, ordered and unique; Global is implicit."; return false;
		}

		if ( i ) {
			canonical += ';';
		}

		canonical += context[i];
	}

	if ( canonical != archives ) { error = "Reload to obtain the canonical authoring archive context."; return false; }
	float position[3];
	if ( !ModelTransform_Parse( origin.c_str(), 3, position, true ) ) {
		error = "Enter three finite float32 position components."; return false;
	}

	if ( choice.archive != "global.not" && std::find( context.begin(), context.end(), choice.archive ) == context.end() ) {
		error = "The selected template archive is absent from the authoring context."; return false;
	}

	const std::string serializedPosition = NodTemplate_FormatPosition( position );
	keys.push_back( std::make_pair( "classname", registered ? choice.name : "nod_template_instance" ) );
	keys.push_back( std::make_pair( "nod_authoring", registered ? "not-template-v2" : "map-template-v1" ) );
	keys.push_back( std::make_pair( "nod_template", choice.name ) );
	keys.push_back( std::make_pair( "nod_template_archive", choice.archive ) );
	keys.push_back( std::make_pair( "nod_template_archives", archives ) );
	keys.push_back( std::make_pair( "nod_template_type", choice.type ) );
	keys.push_back( std::make_pair( "origin", serializedPosition ) );
	keys.push_back( std::make_pair( "angles", "0 0 0" ) );
	error.clear();
	return true;
}

#endif
