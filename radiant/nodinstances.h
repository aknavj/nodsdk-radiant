/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodinstances.h
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

#ifndef RADIANT_NODINSTANCES_H
#define RADIANT_NODINSTANCES_H

#include "nodtemplates.h"
#include <map>

struct NodInstanceRecord {
	std::string identity, name, type, problem, schema;
	std::vector<NodTemplateProperty> local, effective;
};
struct NodInstanceCatalog {
	std::string sources, schema, diagnostics, archives;
	std::map<std::string, NodInstanceRecord> records;
};

inline bool NodInstance_Sources( const std::string &value ){
	if ( value.size() != 26 || value[8] != ' ' || value[17] != ' ' ) {
		return false;
	}

	for ( size_t i = 0; i < value.size(); ++i ) {
		if ( i != 8 && i != 17 && !( value[i] >= '0' && value[i] <= '9' ) && !( value[i] >= 'a' && value[i] <= 'f' ) ) {
			return false;
		}
	}

	return true;
}

inline bool NodInstance_ParseCatalog( const char *bytes, size_t size, NodInstanceCatalog &catalog, std::string &error ){
	error.clear();
	if ( !bytes || !size || size > 16 * 1024 * 1024 ) { error = "Missing or oversized SDK instance metadata."; return false; }
	xmlDocPtr doc = xmlReadMemory( bytes, static_cast<int>( size ), "instance-catalog.xml", "UTF-8", XML_PARSE_NONET | XML_PARSE_NOBLANKS );
	xmlNodePtr root = doc ? xmlDocGetRootElement( doc ) : NULL;
	if ( !root || doc->intSubset || doc->extSubset || !xmlStrEqual( root->name, BAD_CAST "nod-instance-catalog" ) ||
		NodTemplate_Attribute( root, "version" ) != "1" ) {
		if ( doc ) {
			xmlFreeDoc( doc );
		}

		error = "Invalid SDK instance catalog root, version or DTD."; return false;
	}

	NodInstanceCatalog next;
	next.sources = NodTemplate_Attribute( root, "sources" );
	next.schema = NodTemplate_Attribute( root, "schema" );
	if ( !NodInstance_Sources( next.sources ) || !NodTemplate_Number( next.schema, 15 ) || next.schema == "0" ) {
		error = "Invalid SDK instance source signature or schema.";
	}

	std::set<std::string> archives;
	for ( xmlNodePtr node = root->children; node && error.empty(); node = node->next ) {
		if ( node->type != XML_ELEMENT_NODE ) { error = "Unexpected SDK instance content."; break; }
		if ( xmlStrEqual( node->name, BAD_CAST "archive" ) ) {
			const std::string name = NodTemplate_Attribute( node, "name" );
			if ( !next.records.empty() || !NodTemplate_CanonicalArchive( name ) || !archives.insert( name ).second ||
				archives.size() > 10 || ( archives.size() == 1 && name != "global.not" ) ) {
				error = "Invalid SDK instance archive order."; break;
			}

			if ( name != "global.not" ) { if ( !next.archives.empty() ) {
					next.archives += ';';
				}

				next.archives += name; }
		}
		else if ( xmlStrEqual( node->name, BAD_CAST "warning" ) ) {
			next.diagnostics += NodTemplate_Attribute( node, "text" ) + "\n";
		} else if ( xmlStrEqual( node->name, BAD_CAST "instance" ) ) {
			NodInstanceRecord record;
			record.schema = next.schema;
			record.identity = NodTemplate_Attribute( node, "identity" );
			record.name = NodTemplate_Attribute( node, "name" );
			record.type = NodTemplate_Attribute( node, "type" );
			record.problem = NodTemplate_Attribute( node, "problem" );
			if ( archives.empty() || record.identity.compare( 0, 4, "nsd:" ) ||
				!NodTemplate_Number( record.identity.substr( 4 ), G_GUINT64_CONSTANT( 4294967295 ) ) ||
				record.name.empty() || record.name.size() > 128 || record.problem.size() > 4096 ||
				!NodTemplate_Number( record.type, G_GUINT64_CONSTANT( 4294967295 ) ) || next.records.size() == 20480 ) {
				error = "Invalid SDK instance identity or runtime type."; break;
			}

			std::set<std::string> groups;
			for ( xmlNodePtr group = node->children; group && error.empty(); group = group->next ) {
				const std::string name = reinterpret_cast<const char *>( group->name );
				if ( group->type != XML_ELEMENT_NODE || ( name != "local" && name != "effective" ) || !groups.insert( name ).second ) {
					error = "Invalid or duplicate SDK instance property group."; break;
				}

				std::vector<NodTemplateProperty> &properties = name == "local" ? record.local : record.effective;
				std::set<std::string> ids;
				for ( xmlNodePtr child = group->children; child && error.empty(); child = child->next ) {
					NodTemplateProperty property;
					if ( !NodTemplate_ReadProperty( child, archives, property, error, true ) ) {
						break;
					}

					if ( !ids.insert( property.id ).second || properties.size() == 128 ||
						( name == "local" && !property.local ) || ( !property.local && property.editable ) ||
						( property.local && property.owner != record.name ) ||
						( property.editable && ( next.schema != "14" && next.schema != "15" ) ) ||
						( !record.problem.empty() && property.editable ) ) {
						error = "Invalid SDK instance local property or provenance."; break;
					}

					properties.push_back( property );
				}
			}

			const std::string localCount = NodTemplate_Attribute( node, "local-count" ), effectiveCount = NodTemplate_Attribute( node, "effective-count" );
			bool typed = false;
			for ( size_t i = 0; i < record.effective.size(); ++i ) {
				if ( record.effective[i].id == "20" && record.effective[i].storage == "uint32 bits" &&
					record.effective[i].value == record.type ) {
					typed = true;
				}

				if ( record.effective[i].local ) {
					bool matched = false;
					for ( size_t j = 0; j < record.local.size(); ++j ) {
						if ( record.local[j].id == record.effective[i].id && record.local[j].offset == record.effective[i].offset &&
							record.local[j].storage == record.effective[i].storage && record.local[j].value == record.effective[i].value ) {
							matched = true;
						}
					}

					if ( !matched ) {
						error = "SDK effective local property differs from its serialized NSD source.";
				}
			}
			}

			if ( groups.size() != 2 || !NodTemplate_Number( localCount, 128 ) || !NodTemplate_Number( effectiveCount, 128 ) ||
				g_ascii_strtoull( localCount.c_str(), NULL, 10 ) != record.local.size() ||
				g_ascii_strtoull( effectiveCount.c_str(), NULL, 10 ) != record.effective.size() ||
				( record.problem.empty() && !typed ) || ( !record.problem.empty() && !record.effective.empty() ) ||
				!next.records.insert( std::make_pair( record.identity, record ) ).second ) {
				error = "Incomplete, duplicate or inconsistent SDK instance metadata.";
		}
		} else {
			error = "Unexpected SDK instance element.";
	}
	}

	xmlFreeDoc( doc );
	if ( !error.empty() ) {
		return false;
	}

	if ( archives.empty() ) { error = "Missing SDK instance archive context."; return false; }
	catalog = next;
	return true;
}

inline const NodTemplateProperty *NodInstance_Property( const std::vector<NodTemplateProperty> &properties, const char *key ){
	const size_t prefix = strncmp( key, "nod_prop_", 9 ) == 0 ? 9 : strncmp( key, "nod_raw_", 8 ) == 0 ? 8 : 0;
	if ( !prefix || !NodTemplate_Number( key + prefix, G_GUINT64_CONSTANT( 4294967295 ) ) ) {
		return NULL;
	}

	for ( size_t i = 0; i < properties.size(); ++i ) {
		if ( properties[i].id == key + prefix ) {
			return &properties[i];
		}
	}

	return NULL;
}

inline const char *NodInstance_Restriction( const NodInstanceRecord &record, const char *key, bool present, bool geometry ){
	if ( !record.problem.empty() ) {
		return record.problem.c_str();
	}

	if ( geometry ) {
		return "This placement is linked to baked geometry; edit the source and re-enrich.";
	}

	if ( !present ) {
		return "Inherited value; modifier insertion is unsupported in imported MAPs.";
	}

	const NodTemplateProperty *property = NodInstance_Property( record.local, key );
	if ( !property ) {
		return "This property is not serialized in the bound NSD instance.";
	}

	if ( strncmp( key, "nod_prop_", 9 ) || !property->editable ) {
		return property->problem.empty() ? "This native payload is read-only." : property->problem.c_str();
	}

	return NULL;
}

#endif
