/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: groupdialog.cpp
Version: 0.6.8

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

/*
   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

//
// Floating dialog that contains a notebook with at least Entities and Group tabs
// I merged the 2 MS Windows dialogs in a single class
//
// Leonardo Zide (leo@lokigames.com)
//

#ifndef _WIN32
  #include <unistd.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include "stdafx.h"
#include "nodmodelreference.h"
#include "nodmodelpicker.h"
#include "groupdialog.h"
#include "modeltransform.h"
#include "nodproperties.h"
#include "nodenvironmentdialog.h"
#include "nodinspectorview.h"
#include "nodtemplates.h"
#include "nodtemplateplacement.h"
#include "nodpathplacement.h"
#include "nodviewstorage.h"
#include <map>
#include <algorithm>
#include <vector>

GtkWidget*  EntWidgets[EntLast];
int inspector_mode;                     // W_TEXTURE, W_ENTITY, or W_CONSOLE
qboolean multiple_entities;
qboolean disable_spawn_get = false;
entity_t        *edit_entity;
/*
   static GdkPixmap *tree_pixmaps[7];
   static GdkBitmap *tree_masks[7];
 */
#define IMG_PATCH 0
#define IMG_BRUSH 1
#define IMG_GROUP 2
#define IMG_ENTITY 3
#define IMG_ENTITYGROUP 4
#define IMG_MODEL 5
#define IMG_SCRIPT 6

// misc group support
#define MAX_GROUPS 4096
#define GROUP_DELIMETER '@'
#define GROUPNAME "QER_Group_%i"

GroupDlg g_wndGroup;
GroupDlg *g_pGroupDlg = &g_wndGroup;

// group_t are loaded / saved through "group_info" entities
// they hold epairs for group settings and additionnal access info (tree nodes)
group_t *g_pGroups = NULL;

// the number of active spawnflags
static int spawnflag_count;
// table: index, match spawnflag item to the spawnflag index (i.e. which bit)
static int spawn_table[MAX_FLAGS];
// we change the layout depending on how many spawn flags we need to display
// the table is a 4x4 in which we need to put the comment box EntWidgets[EntComment] and the spawn flags..
static GtkWidget *LayoutTable;
// 0: none of them are hooked
// 1: only the text, 2: text and four checks, 3: text and 8 checks
static int widget_state = 0;

static void entity_check( GtkWidget *widget, gpointer data );

static GtkWidget *nodTypeSearch, *nodPropertySearch, *nodSelectionLabel, *nodConfirm, *nodApply;
static GtkWidget *nodTechnical, *nodSelectionDetails, *nodPropertyDetails, *nodPropertyName, *nodKeyLabel;
static GtkWidget *nodPlacementPage, *nodPlace, *nodPlacementLabel, *nodSelectedShared, *nodBrowsedShared;
static NodInspectorView_ValueEditor nodValueEditor;
static GtkWidget *nodArchives;
static GtkWidget *nodModelBundle;
static std::string nodDisplayedBundle;
static GtkWidget *nodLegacySource;
static bool nodEffectiveProperties = true;

static bool NodInspector_HasKey( entity_t *entity, const char *key );
static bool NodMetadata_Run( const std::vector<std::string> &options, std::string &output, std::string &error );
static bool NodInspector_QueryInstances( const std::string &source, NodInstanceCatalog &catalog, std::string &error );
static void NodInspector_BrowserChanged();

struct NodInspectorMetadata {
	NodTemplateChoice choice;
	std::string error;
};
static std::map<std::pair<std::string, std::string>, NodInspectorMetadata> nodMetadata;
static std::string nodMetadataProject;
static NodInstanceCatalog nodInstances;
static std::string nodInstanceSignature, nodInstanceError;

static std::string NodInspector_Project(){
	return std::string( g_pGameDescription->mEnginePath.GetBuffer() ) + '\n' +
		g_pGameDescription->mBaseGame.GetBuffer() + '\n' + g_pGameDescription->mNodAssetPath.GetBuffer() +
		'\n' + g_pGameDescription->mNodEditorsPath.GetBuffer() + '\n' + std::to_string( NodClasses_Generation() );
}

static std::string NodInspector_InstanceSignature( const std::string &source, const std::string &sources, const std::string &schema ){
	return NodInspector_Project() + '\n' + source + '\n' + sources + '\n' + schema;
}

static entity_t *NodInspector_SceneBinding( std::string &error ){
	entity_t *binding = NULL;
	for ( entity_t *entity = entities.next; entity != &entities; entity = entity->next ) {
		if ( strcmp( ValueForKey( entity, "classname" ), "nod_scene" ) ) {
			continue;
		}

		if ( binding ) { error = "The MAP has multiple native scene bindings; metadata cannot be matched safely."; return NULL; }
		binding = entity;
	}

	if ( !binding || strcmp( ValueForKey( binding, "nod_id" ), "scene" ) ||
		!NodInstance_Sources( ValueForKey( binding, "nod_sources" ) ) ||
		!NodTemplate_Number( ValueForKey( binding, "nod_schema" ), 15 ) ||
		strcmp( ValueForKey( binding, "nod_schema" ), "0" ) == 0 ) {
		error = "The MAP lacks a supported immutable NOD scene/source binding."; return NULL;
	}

	return binding;
}

static const NodInstanceRecord *NodInspector_InstanceMetadata( entity_t *entity, std::string &error ){
	error.clear();
	if ( strncmp( ValueForKey( entity, "nod_id" ), "nsd:", 4 ) ) {
		error = "This metadata workflow supports verified NSD instances only."; return NULL;
	}

	entity_t *binding = NodInspector_SceneBinding( error );
	if ( !binding ) {
		return NULL;
	}

	if ( g_nodViewState.nsdSource.empty() ) {
		error = "Use Bind NSD source to select the original NSD matching this imported MAP."; return NULL;
	}

	const std::string signature = NodInspector_InstanceSignature( g_nodViewState.nsdSource,
		ValueForKey( binding, "nod_sources" ), ValueForKey( binding, "nod_schema" ) );
	if ( signature != nodInstanceSignature ) {
		nodInstances = NodInstanceCatalog();
		nodInstanceError.clear();
		nodInstanceSignature = signature;
		if ( !NodInspector_QueryInstances( g_nodViewState.nsdSource, nodInstances, nodInstanceError ) ) {
			Sys_FPrintf( SYS_ERR, "NSD metadata: %s\n", nodInstanceError.c_str() );
	}
	}

	if ( !nodInstanceError.empty() ) { error = nodInstanceError; return NULL; }
	const std::string identity = ValueForKey( entity, "nod_id" );
	std::map<std::string, NodInstanceRecord>::const_iterator found = nodInstances.records.find( identity );
	int matches = 0;
	for ( entity_t *other = entities.next; other != &entities; other = other->next ) {
		if ( identity == ValueForKey( other, "nod_id" ) ) {
			++matches;
		}
	}

	if ( found == nodInstances.records.end() || found->second.name != ValueForKey( entity, "nod_type" ) || matches != 1 ) {
		error = "NSD identity/type is missing, duplicated or different from the bound source record."; return NULL;
	}

	return &found->second;
}

static void NodInspector_BindSource( GtkWidget *, gpointer ){
	std::string error;
	if ( !NodInspector_SceneBinding( error ) ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NSD source binding unavailable" ), MB_OK ); return;
	}

	GtkWidget *chooser = gtk_file_chooser_dialog_new( _( "Bind the original NSD source (read-only)" ), GTK_WINDOW( g_pGroupDlg->m_pWidget ),
		GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL );
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name( filter, _( "Native NSD scenes" ) );
	gtk_file_filter_add_pattern( filter, "*.nsd" ); gtk_file_filter_add_pattern( filter, "*.NSD" );
	gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( chooser ), filter );
	if ( !g_nodViewState.nsdSource.empty() ) {
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( chooser ), g_nodViewState.nsdSource.c_str() );
	}

	std::string source;
	if ( gtk_dialog_run( GTK_DIALOG( chooser ) ) == GTK_RESPONSE_ACCEPT ) {
		gchar *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( chooser ) );
		if ( filename ) { source = filename; g_free( filename ); }
	}

	gtk_widget_destroy( chooser );
	if ( source.empty() ) {
		return;
	}

	NodInstanceCatalog catalog;
	if ( !NodEditor_SourcePath( source ) || !NodInspector_QueryInstances( source, catalog, error ) ) {
		if ( error.empty() ) {
			error = "The NSD source must have a valid absolute UTF-8 path.";
		}

		Sys_FPrintf( SYS_WRN, "NSD binding: %s\n", error.c_str() );
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NSD source not bound" ), MB_OK ); return;
	}

	if ( g_nodViewState.nsdSource != source ) { g_nodViewState.nsdSource = source; Sys_MarkMapModified(); }
	nodInstances = catalog;
	nodInstanceError.clear();
	nodInstanceSignature = NodInspector_InstanceSignature( source, catalog.sources, catalog.schema );
	SetKeyValuePairs();
}

static bool NodInspector_IsAuthoring( entity_t *entity ){
	return entity && ( strcmp( ValueForKey( entity, "classname" ), "nod_template_instance" ) == 0 ||
		strcmp( ValueForKey( entity, "nod_authoring" ), "not-template-v2" ) == 0 );
}

static const NodTemplateChoice *NodInspector_Metadata( entity_t *entity, std::string &error ){
	const std::string project = NodInspector_Project();
	if ( project != nodMetadataProject ) { nodMetadata.clear(); nodMetadataProject = project; }
	const std::string name = ValueForKey( entity, "nod_template" ), context = ValueForKey( entity, "nod_template_archives" );
	std::string modifiers;
	if ( !NodTemplate_Modifiers( entity, modifiers, error ) ) {
		return NULL;
	}

	if ( !NodInspector_HasKey( entity, "nod_template_archives" ) ||
		( strcmp( ValueForKey( entity, "nod_authoring" ), "map-template-v1" ) != 0 &&
			strcmp( ValueForKey( entity, "nod_authoring" ), "not-template-v2" ) != 0 ) || !NodTemplate_MapString( name, 31 ) ) {
		error = "This MAP instance lacks a supported template binding."; return NULL;
	}

	const std::pair<std::string, std::string> key( context + '\n' + modifiers, name );
	std::map<std::pair<std::string, std::string>, NodInspectorMetadata>::iterator found = nodMetadata.find( key );
	if ( found == nodMetadata.end() ) {
		NodInspectorMetadata metadata;
		NodTemplateCatalog catalog;
		const NodTemplateChoice *registered = modifiers.empty() ? NodClasses_Find( name.c_str() ) : NULL;
		if ( registered && registered->context == context ) {
			metadata.choice = *registered;
		} else if ( NodTemplate_Query( context.c_str(), name.c_str(), catalog, metadata.error, modifiers.c_str() ) ) {
			if ( catalog.choices.size() != 1 || !catalog.choices[0].detailed || !catalog.choices[0].placeable ||
				g_ascii_strcasecmp( catalog.choices[0].name.c_str(), name.c_str() ) != 0 || catalog.archives != context ) {
				metadata.error = "Incomplete SDK metadata. Rebuild NODTemplates and reload.";
			} else {
				metadata.choice = catalog.choices[0];
		}
		}

		metadata.choice.modifiers = modifiers;
		if ( !metadata.error.empty() ) {
			Sys_FPrintf( SYS_ERR, "NOD metadata: %s\n", metadata.error.c_str() );
		}

		found = nodMetadata.insert( std::make_pair( key, metadata ) ).first;
	}

	error = found->second.error;
	if ( !error.empty() ) {
		return NULL;
	}

	const NodTemplateChoice &choice = found->second.choice;
	if ( choice.archive != ValueForKey( entity, "nod_template_archive" ) ||
		choice.type != ValueForKey( entity, "nod_template_type" ) ) {
		error = "Template source/type changed. Resolve the binding mismatch before editing."; return NULL;
	}

	return &choice;
}

static bool NodInspector_RefreshMetadata( std::string &error ){
	const std::string context = NodClasses_Context();
	if ( !NodClasses_Load( context.c_str(), error ) ) {
		return false;
	}

	for ( entity_t *entity = entities.next; entity != &entities; entity = entity->next ) {
		const std::string classname = ValueForKey( entity, "classname" );
		SetKeyValue( entity, "classname", classname.c_str() );
	}

	FillClassList();
	nodMetadata.clear();
	nodInstanceSignature.clear();
	SetKeyValuePairs();
	Sys_UpdateWindows( W_ALL );
	return true;
}

static void NodInspector_ReloadMetadata( GtkWidget *, gpointer ){
	std::string error;
	if ( !NodInspector_RefreshMetadata( error ) ) {
		Sys_FPrintf( SYS_ERR, "NOT reload: %s\n", error.c_str() );
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NOT database not reloaded" ), MB_OK );
	}
}

static bool NodInspector_Enabled(){
	return g_pGameDescription && g_pGameDescription->mGameFile == SCGHOST_GAME;
}

static std::vector<entity_t *> NodInspector_Selection(){
	std::vector<entity_t *> entities;
	for ( brush_t *b = selected_brushes.next; b != &selected_brushes; b = b->next ) {
		if ( std::find( entities.begin(), entities.end(), b->owner ) == entities.end() ) {
			entities.push_back( b->owner );
	}
	}

	if ( entities.empty() && edit_entity ) {
		entities.push_back( edit_entity );
	}

	return entities;
}

static bool NodInspector_IsNative( entity_t *entity ){
	return entity && !NodInspector_IsAuthoring( entity ) &&
		NodProperty_IsNative( ValueForKey( entity, "classname" ), ValueForKey( entity, "nod_id" ) );
}

static bool NodInspector_HasKey( entity_t *entity, const char *key ){
	for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
		if ( strcmp( pair->key, key ) == 0 ) {
			return true;
		}
	}

	return false;
}

static bool NodInspector_HasNativeSelection(){
	const std::vector<entity_t *> entities = NodInspector_Selection();
	for ( size_t i = 0; i < entities.size(); ++i ) {
		if ( NodInspector_IsNative( entities[i] ) ) {
			return true;
		}
	}

	return false;
}

static bool NodInspector_HasAuthoringSelection(){
	const std::vector<entity_t *> entities = NodInspector_Selection();
	for ( size_t i = 0; i < entities.size(); ++i ) {
		if ( NodInspector_IsAuthoring( entities[i] ) ) {
			return true;
		}
	}

	return false;
}

static const char *NodInspector_AuthoringRestriction( entity_t *entity, const char *key ){
	static std::string error;
	if ( !NodInspector_IsAuthoring( entity ) ) {
		return "Template instances and other entities cannot be edited together.";
	}

	if ( strcmp( key, "origin" ) == 0 || strcmp( key, "angles" ) == 0 ) {
		return NULL;
	}

	const NodTemplateChoice *choice = NodInspector_Metadata( entity, error );
	if ( !choice ) {
		return error.c_str();
	}

	return NodTemplate_OverrideRestriction( *choice, key, false, NodInspector_HasKey( entity, key ) );
}

static const char *NodInspector_Restriction( entity_t *entity, const char *key ){
	if ( NodPath_Authoring( entity ) ) {
		return NodPath_Restriction( ValueForKey( entity, "classname" ), key );
	}

	if ( strncmp( key, "nod_prop_", 9 ) == 0 || strncmp( key, "nod_raw_", 8 ) == 0 ) {
		static std::string error;
		const NodInstanceRecord *record = NodInspector_InstanceMetadata( entity, error );
		if ( !record ) {
			return error.c_str();
		}

		if ( NodInspector_HasKey( entity, "nod_reference" ) && record->schema != "15" ) {
			return "The source binding schema does not match this preview.";
		}

		return NodInstance_Restriction( *record, key, NodInspector_HasKey( entity, key ), NodInspector_HasKey( entity, "nod_geometry" ) );
	}

	return NodProperty_Restriction( ValueForKey( entity, "classname" ), ValueForKey( entity, "nod_id" ),
		NodInspector_HasKey( entity, "nod_geometry" ), NodInspector_HasKey( entity, "nod_reference" ),
		*ValueForKey( entity, "model" ) != '\0', NodInspector_HasKey( entity, key ), key,
		NodProperty_Info( entity->eclass->comments, key ) );
}

static NodPropertyInfo NodInspector_FieldInfo( const char *key ){
	NodPropertyInfo info;
	if ( !edit_entity ) {
		return info;
	}

	info = NodProperty_Info( edit_entity->eclass->comments, key );
	std::string error;
	const NodTemplateProperty *property = NULL;
	if ( NodInspector_IsAuthoring( edit_entity ) ) {
		const NodTemplateChoice *choice = NodInspector_Metadata( edit_entity, error );
		if ( choice ) {
			property = NodTemplate_Property( *choice, key );
	}
	} else if ( strncmp( key, "nod_prop_", 9 ) == 0 && strncmp( ValueForKey( edit_entity, "nod_id" ), "nsd:", 4 ) == 0 ) {
		const NodInstanceRecord *record = NodInspector_InstanceMetadata( edit_entity, error );
		if ( record ) {
			property = NodInstance_Property( record->local, key );
			if ( !property ) {
				property = NodInstance_Property( record->effective, key );
		}
	}
	}

	if ( property ) { info.name = property->name; info.storage = property->storage; }
	return info;
}

static void NodInspector_UpdateFields( GtkWidget *, gpointer ){
	if ( !nodApply || !EntWidgets[EntValueField] ) {
		return;
	}

	const char *restriction = edit_entity ? NULL : _( "Select an entity to edit." );
	GtkTreeModel *model;
	GtkTreeIter iter;
	if ( !restriction && nodTechnical && !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( nodTechnical ) ) &&
		!gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW( EntWidgets[EntProps] ) ), &model, &iter ) ) {
		restriction = _( "Select a property to edit." );
	}

	if ( !restriction && NodInspector_HasNativeSelection() ) {
		const std::vector<entity_t *> entities = NodInspector_Selection();
		const char *key = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
		for ( size_t i = 0; i < entities.size() && !restriction; ++i ) {
			restriction = NodInspector_IsNative( entities[i] ) ? NodInspector_Restriction( entities[i], key )
				: _( "Native and ordinary entities cannot be edited together." );
	}
	} else if ( !restriction && NodInspector_HasAuthoringSelection() ) {
		const std::vector<entity_t *> entities = NodInspector_Selection();
		const char *key = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
		for ( size_t i = 0; i < entities.size() && !restriction; ++i ) {
			restriction = NodInspector_AuthoringRestriction( entities[i], key );
	}
	}

	const char *key = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
	nodValueEditor.Configure( key, NodInspector_FieldInfo( key ).storage );
	nodValueEditor.Editable( restriction );
	if ( !NodModel_Resources() ) {
		gtk_widget_set_sensitive( nodValueEditor.resource, FALSE );
		gtk_widget_set_tooltip_text( nodValueEditor.resource, _( "Model browsing requires the current NOD model plugin." ) );
	}

	gtk_widget_set_sensitive( nodApply, restriction == NULL && nodValueEditor.Error() == NULL );
}

static bool NodInspector_Matches( const char *text, GtkWidget *search ){
	if ( !search ) {
		return true;
	}

	gchar *haystack = g_utf8_casefold( text ? text : "", -1 );
	gchar *needle = g_utf8_casefold( gtk_entry_get_text( GTK_ENTRY( search ) ), -1 );
	const bool match = strstr( haystack, needle ) != NULL;
	g_free( haystack );
	g_free( needle );
	return match;
}

static bool NodInspector_PropertyVisible( const char *key, const NodPropertyInfo &info ){
	const bool searching = nodPropertySearch && *gtk_entry_get_text( GTK_ENTRY( nodPropertySearch ) );
	const bool technical = nodTechnical && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( nodTechnical ) );
	return NodProperty_Visible( key, info.storage, technical, searching ) &&
		( NodInspector_Matches( key, nodPropertySearch ) || NodInspector_Matches( info.name.c_str(), nodPropertySearch ) ||
			NodInspector_Matches( NodProperty_DisplayName( key, info.name ).c_str(), nodPropertySearch ) );
}

static bool NodInspector_Apply( const char *inputKey, const char *inputValue ){
	const std::string savedKey = inputKey, savedValue = inputValue;
	const char *key = savedKey.c_str(), *value = savedValue.c_str();
	const std::vector<entity_t *> entities = NodInspector_Selection();
	const bool authoring = NodInspector_HasAuthoringSelection();
	if ( !authoring && NodInspector_HasNativeSelection() && strncmp( key, "nod_prop_", 9 ) == 0 ) {
		std::vector<const NodInstanceRecord *> records;
		std::string error;
		for ( size_t i = 0; i < entities.size(); ++i ) {
			const NodInstanceRecord *record = NodInspector_IsNative( entities[i] ) ? NodInspector_InstanceMetadata( entities[i], error ) : NULL;
			if ( !record ) {
				if ( error.empty() ) {
					error = "Native and ordinary entities cannot be edited together.";
				}

				Sys_FPrintf( SYS_WRN, "NSD property: %s\n", error.c_str() );
				gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NSD property not applied" ), MB_OK ); return false;
			}

			records.push_back( record );
		}

		if ( entities.size() > 1 && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( nodConfirm ) ) &&
			gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Apply this local property to every selected NSD instance?" ),
				_( "Confirm multiple selection" ), MB_YESNO ) != IDYES ) {
			return false;
		}

		bool changed;
		if ( !NodInstance_ApplyLocalProperties( entities, records, key, value, changed, error ) ) {
			Sys_FPrintf( SYS_WRN, "NSD property: %s\n", error.c_str() );
			gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NSD property not applied" ), MB_OK ); return false;
		}

		if ( changed ) {
			Sys_MarkMapModified();
		}

		return true;
	}

	if ( authoring && strcmp( key, "origin" ) != 0 && strcmp( key, "angles" ) != 0 ) {
		std::vector<const NodTemplateChoice *> templates;
		std::string error;
		for ( size_t i = 0; i < entities.size(); ++i ) {
			templates.push_back( NodInspector_IsAuthoring( entities[i] ) ? NodInspector_Metadata( entities[i], error ) : NULL );
		}

		if ( entities.size() > 1 && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( nodConfirm ) ) &&
			gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Apply this local override to every selected template instance?" ),
				_( "Confirm multiple selection" ), MB_YESNO ) != IDYES ) {
			return false;
		}

		bool changed;
		if ( !NodTemplate_ApplyOverrides( entities, templates, key, value, false, changed, error ) ) {
			Sys_FPrintf( SYS_WRN, "NOD instance override: %s\n", error.c_str() );
			gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NOD instance override not applied" ), MB_OK );
			return false;
		}

		if ( changed ) {
			Sys_MarkMapModified();
		}

		return true;
	}

	for ( size_t i = 0; i < entities.size(); ++i ) {
		entity_t *entity = entities[i];
		const char *error = authoring ? NodInspector_AuthoringRestriction( entity, key ) :
			NodInspector_IsNative( entity ) ? NodInspector_Restriction( entity, key )
			: _( "Native and ordinary entities cannot be edited together." );
		if ( !error ) {
			error = NodPath_Authoring( entity ) ? NodPath_Validate( key, value ) :
			NodProperty_Validate( key, value, NodProperty_Info( entity->eclass->comments, key ) );
		}

		if ( error ) {
			Sys_FPrintf( SYS_WRN, "NOD property '%s': %s\n", key, error );
			gtk_MessageBox( g_pGroupDlg->m_pWidget, error, _( "NOD property not applied" ), MB_OK );
			return false;
		}
	}

	if ( entities.size() > 1 && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( nodConfirm ) )
		 && gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Apply this property to every selected entity?" ),
							_( "Confirm multiple selection" ), MB_YESNO ) != IDYES ) {
		return false;
	}

	Undo_Start( "NOD entity property" );
	Undo_AddBrushList( &selected_brushes );
	for ( size_t i = 0; i < entities.size(); ++i ) {
		Undo_AddEntity( entities[i] );
	}

	for ( size_t i = 0; i < entities.size(); ++i ) {
		SetKeyValue( entities[i], key, value );
	}

	Undo_EndBrushList( &selected_brushes );
	for ( size_t i = 0; i < entities.size(); ++i ) {
		Undo_EndEntity( entities[i] );
	}

	Undo_End();
	Sys_MarkMapModified();
	return true;
}

// =============================================================================
// Global functions

static bool NodMetadata_Run( const std::vector<std::string> &options, std::string &result, std::string &error ){
	error.clear();
	result.clear();
	Str directory = g_pGameDescription->mNodEditorsPath;
	if ( !directory.GetLength() ) { directory = g_strAppPath; directory += "nod-editors"; }
	AddSlash( directory );
	directory += "NODTemplates";
#ifdef _WIN32
	directory += ".exe";
#endif
	if ( !g_file_test( directory, G_FILE_TEST_IS_REGULAR ) ) {
		error = std::string( "Cannot find " ) + directory.GetBuffer() +
			". Build NODTemplates and configure SDK editor binaries in Project Settings."; return false;
	}

	Str workspace = g_pGameDescription->mEnginePath;
	AddSlash( workspace );
	workspace += g_pGameDescription->mBaseGame;
	std::vector<std::string> arguments;
	arguments.push_back( directory.GetBuffer() );
	arguments.push_back( "--workspace" ); arguments.push_back( workspace.GetBuffer() );
	if ( g_pGameDescription->mNodAssetPath.GetLength() ) {
		arguments.push_back( "--asset-root" ); arguments.push_back( g_pGameDescription->mNodAssetPath.GetBuffer() );
	}

	arguments.insert( arguments.end(), options.begin(), options.end() );
	std::vector<gchar *> argv;
	for ( size_t i = 0; i < arguments.size(); ++i ) {
		argv.push_back( g_strdup( arguments[i].c_str() ) );
	}

	argv.push_back( NULL );
	gchar *output = NULL, *diagnostics = NULL;
	gint status = 0;
	GError *failure = NULL;
	const bool launched = g_spawn_sync( workspace, &argv[0], NULL, static_cast<GSpawnFlags>( 0 ), NULL, NULL,
		&output, &diagnostics, &status, &failure ) != FALSE;
	for ( size_t i = 0; i + 1 < argv.size(); ++i ) {
		g_free( argv[i] );
	}

	bool success = false;
	if ( !launched || status != 0 ) {
		error = failure ? failure->message : diagnostics && *diagnostics ? diagnostics : "SDK catalog process failed.";
	}
	else if ( !output || !*output || strlen( output ) > 16 * 1024 * 1024 ) {
		error = "Missing or oversized SDK metadata.";
	} else {
		result = output; success = true;
		if ( diagnostics && *diagnostics ) {
			Sys_FPrintf( SYS_WRN, "NOD SDK diagnostics:\n%s", diagnostics );
	}
	}

	if ( failure ) {
		g_error_free( failure );
	}

	g_free( output ); g_free( diagnostics );
	return success;
}

static bool NodInspector_LoadDefaults( NodInspectorView_Template &dialog, const std::string &context,
	const std::string &name, std::string &error ){
	NodTemplateCatalog catalog;
	if ( !NodTemplate_Query( context.c_str(), name.c_str(), catalog, error ) ) {
		return false;
	}

	if ( catalog.archives != context || catalog.choices.size() != 1 || !catalog.choices[0].detailed ||
		g_ascii_strcasecmp( catalog.choices[0].name.c_str(), name.c_str() ) != 0 ) {
		error = "The SDK returned a different shared template/context."; return false;
	}

	dialog.Set( catalog.choices[0] );
	if ( !catalog.diagnostics.empty() ) {
		Sys_FPrintf( SYS_WRN, "Shared NOT diagnostics:\n%s", catalog.diagnostics.c_str() );
	}

	return true;
}

static const NodTemplateChoice *NodInspector_SelectedTemplate(){
	const NodTemplateChoice *choice = NULL;
	const std::vector<entity_t *> selected = NodInspector_Selection();
	for ( size_t i = 0; i < selected.size(); ++i ) {
		const bool authoring = NodInspector_IsAuthoring( selected[i] );
		const NodTemplateChoice *current = NodClasses_Find( ValueForKey( selected[i], authoring ? "nod_template" : "classname" ) );
		if ( !current || ( choice && choice != current ) ) {
			return NULL;
		}

		if ( authoring && ( current->archive != ValueForKey( selected[i], "nod_template_archive" ) ||
			current->context != ValueForKey( selected[i], "nod_template_archives" ) ) ) {
			return NULL;
		}

		choice = current;
	}

	return choice;
}

static void NodInspector_InspectDefaults( GtkWidget *, gpointer data ){
	const bool browsing = GPOINTER_TO_INT( data ) != 0;
	const NodTemplateChoice *selected = browsing ? NULL : NodInspector_SelectedTemplate();
	eclass_t *target = static_cast<eclass_t *>( NodInspectorView_SharedTarget( GTK_TREE_VIEW( EntWidgets[EntList] ),
		selected ? Eclass_ForName( selected->name.c_str(), false ) : NULL, browsing ) );
	const NodTemplateChoice *choice = target ? NodClasses_Find( target->name ) : NULL;
	if ( !choice ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget, browsing ? _( "Choose a registered NOT template on Place entity first." ) :
			_( "Select map instances of a single registered NOT template first." ),
			_( "Shared NOT properties" ), MB_OK ); return;
	}

	const std::string name = choice->name, context = choice->context;
	NodInspectorView_Template dialog;
	GtkWidget *window = dialog.Create( g_pGroupDlg->m_pWidget, name, context );
	std::string error;
	if ( !NodInspector_LoadDefaults( dialog, context, name, error ) ) {
		Sys_FPrintf( SYS_ERR, "Shared NOT: %s\n", error.c_str() );
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "Shared NOT metadata unavailable" ), MB_OK );
		gtk_widget_destroy( window ); return;
	}

	gtk_widget_show_all( window );
	for ( ;; ) {
		const int response = gtk_dialog_run( GTK_DIALOG( window ) );
		if ( response == GTK_RESPONSE_CLOSE || response == GTK_RESPONSE_DELETE_EVENT || response == GTK_RESPONSE_NONE ) {
			break;
		}

		error.clear();
		if ( response != 1 ) {
			continue;
		}

		if ( !NodInspector_RefreshMetadata( error ) || !NodInspector_LoadDefaults( dialog, context, name, error ) ) {
			dialog.Unavailable( error );
			Sys_FPrintf( SYS_ERR, "Shared NOT: %s\n", error.c_str() );
			gtk_MessageBox( window, error.c_str(), _( "Shared NOT operation" ), MB_OK );
		}
	}

	gtk_widget_destroy( window );
}

static bool NodInspector_QueryInstances( const std::string &source, NodInstanceCatalog &catalog, std::string &error ){
	entity_t *binding = NodInspector_SceneBinding( error );
	if ( !binding ) {
		return false;
	}

	std::vector<std::string> options;
	options.push_back( "--scene" ); options.push_back( source );
	options.push_back( "--sources" ); options.push_back( ValueForKey( binding, "nod_sources" ) );
	options.push_back( "--schema" ); options.push_back( ValueForKey( binding, "nod_schema" ) );
	std::string output;
	if ( !NodMetadata_Run( options, output, error ) || !NodInstance_ParseCatalog( output.data(), output.size(), catalog, error ) ) {
		return false;
	}

	if ( catalog.sources != ValueForKey( binding, "nod_sources" ) || catalog.schema != ValueForKey( binding, "nod_schema" ) ) {
		error = "The SDK returned metadata for a different scene/source binding."; return false;
	}

	if ( !catalog.diagnostics.empty() ) {
		Sys_FPrintf( SYS_WRN, "NSD archive diagnostics:\n%s", catalog.diagnostics.c_str() );
	}

	return true;
}

/*
   ===============================================================

   ENTITY WINDOW

   ===============================================================
 */

void FillClassList(){
	GtkListStore *store;
	GtkTreeModel * model;
	model = GTK_TREE_MODEL( gtk_tree_view_get_model( GTK_TREE_VIEW( EntWidgets[EntList] ) ) );
	store = GTK_LIST_STORE( model );

	gpointer previous = NodInspector_Enabled() ? NodInspectorView_SharedTarget( GTK_TREE_VIEW( EntWidgets[EntList] ), NULL, true ) : NULL;
	gtk_list_store_clear( store );

	for ( eclass_t* e = eclass ; e ; e = e->next )
	{
		if ( NodInspector_Enabled() && !NodClass_Browsable( e->name, e->comments ) ) {
			continue;
		}

		if ( NodInspector_Enabled() && NodClass_Registered( e->comments ) && !NodClasses_Find( e->name ) ) {
			continue;
		}

		if ( NodInspector_Enabled() && !NodInspector_Matches( e->comments, nodTypeSearch )
			 && !NodInspector_Matches( e->name, nodTypeSearch ) ) {
			continue;
		}

		GtkTreeIter iter;
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter, 0, e->name, 1, e, -1 );
		if ( previous == e ) {
			gtk_tree_selection_select_iter( gtk_tree_view_get_selection( GTK_TREE_VIEW( EntWidgets[EntList] ) ), &iter );
	}
	}

	if ( NodInspector_Enabled() ) {
		NodInspector_BrowserChanged();
}
}

// SetKeyValuePairs
//
// Reset the key/value (aka property) listbox and fill it with the
// k/v pairs from the entity being edited.
//

void SetKeyValuePairs( bool bClearMD3 ){
	if ( nodSelectedShared ) {
		const NodTemplateChoice *choice = NodInspector_SelectedTemplate();
		gtk_widget_set_sensitive( nodSelectedShared, choice != NULL );
		const std::string scope = choice ? "Read-only defaults: " + choice->name + "." :
			"Select instances of one NOT template.";
		gtk_widget_set_tooltip_text( nodSelectedShared, scope.c_str() );
	}

	if ( nodModelBundle ) {
		const std::string bundle = world_entity ? ValueForKey( world_entity, "nod_model_bundle" ) : "";
		if ( bundle != nodDisplayedBundle ) {
			gtk_entry_set_text( GTK_ENTRY( nodModelBundle ), bundle.c_str() );
			nodDisplayedBundle = bundle;
		}
	}

	static unsigned databaseGeneration;
	if ( nodArchives && databaseGeneration != NodClasses_Generation() ) {
		gtk_entry_set_text( GTK_ENTRY( nodArchives ), NodClasses_Context() );
		databaseGeneration = NodClasses_Generation();
	}

	if ( nodLegacySource ) {
		gtk_widget_set_visible( nodLegacySource,
		edit_entity && NodInspector_IsNative( edit_entity ) && strncmp( ValueForKey( edit_entity, "nod_id" ), "nsd:", 4 ) == 0 );
	}

	GtkListStore *store;
	GtkTreeModel * model;
	model = GTK_TREE_MODEL( gtk_tree_view_get_model( GTK_TREE_VIEW( EntWidgets[EntProps] ) ) );
	store = GTK_LIST_STORE( model );

	gtk_list_store_clear( store );

	if ( edit_entity == NULL ) {
		// if there's no entity, then display no key/values
		if ( nodSelectionLabel ) {
			gtk_label_set_text( GTK_LABEL( nodSelectionLabel ), _( "Select an entity to inspect." ) );
		}

		if ( nodSelectionDetails ) {
			gtk_label_set_text( GTK_LABEL( nodSelectionDetails ), "" );
		}

		return;
	}

	// save current key/val pair around filling epair box
	// row_select wipes it and sets to first in list
	Str strKey = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
	Str strVal = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntValueField] ) );


	// Walk through list and add pairs
	std::string metadataError;
	const NodTemplateChoice *metadata = NodInspector_Enabled() && NodInspector_IsAuthoring( edit_entity )
		? NodInspector_Metadata( edit_entity, metadataError ) : NULL;
	const NodInstanceRecord *native = NodInspector_Enabled() && !metadata && !NodInspector_IsAuthoring( edit_entity ) &&
		strncmp( ValueForKey( edit_entity, "nod_id" ), "nsd:", 4 ) == 0
		? NodInspector_InstanceMetadata( edit_entity, metadataError ) : NULL;
	for ( epair_t* epair = edit_entity->epairs ; epair ; epair = epair->next )
	{
		NodPropertyInfo info = NodProperty_Info( edit_entity->eclass->comments, epair->key );
		const NodTemplateProperty *property = metadata ? NodTemplate_Property( *metadata, epair->key ) : NULL;
		const NodTemplateProperty *local = native ? NodInstance_Property( native->local, epair->key ) : NULL;
		if ( property ) { info.name = property->name; info.storage = property->storage; }
		if ( local ) { info.name = local->name; info.storage = local->storage; }
		if ( metadata && strcmp( epair->key, "origin" ) == 0 ) { info.name = "position (editor coordinates)"; info.storage = "vector3"; }
		if ( metadata && strcmp( epair->key, "angles" ) == 0 ) { info.name = "orientation (editor degrees)"; info.storage = "vector3"; }
		if ( NodInspector_Enabled() && !NodInspector_PropertyVisible( epair->key, info ) ) {
			continue;
		}

		GtkTreeIter iter;
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter, 0, epair->key, 1, epair->value, -1 );
		if ( NodInspector_Enabled() ) {
			const char *restriction = NodInspector_IsAuthoring( edit_entity ) ? NodInspector_AuthoringRestriction( edit_entity, epair->key ) :
				NodInspector_IsNative( edit_entity ) ? NodInspector_Restriction( edit_entity, epair->key ) : NULL;
			const std::string description = info.name.empty() ? epair->key : info.name + " (" + info.storage + ")";
			std::string access = restriction ? restriction : _( "Editable" );
			if ( property && !restriction ) {
				access = "Local override; default from " + property->archive + ":" + property->owner;
			}

			if ( local ) {
				access = "Local NSD payload";
				const NodTemplateProperty *effective = NodInstance_Property( native->effective, epair->key );
				if ( effective && ( !effective->local || effective->offset != local->offset ) ) {
					access += "; shadowed by " + ( effective->local ? "a later NSD payload" : effective->archive + ":" + effective->owner ) +
						"; effective value: " + effective->value;
				} else if ( effective ) {
					access += "; effective local value";
				}

				if ( restriction ) {
					access += "; " + std::string( restriction );
			}
			}

			gtk_list_store_set( store, &iter, 2, description.c_str(),
				3, access.c_str(), 4, NodProperty_DisplayName( epair->key, info.name ).c_str(), -1 );
		}
	}

	if ( metadata && nodEffectiveProperties ) {
		for ( size_t i = 0; i < metadata->effective.size(); ++i ) {
			const NodTemplateProperty &property = metadata->effective[i];
			if ( property.id == "1" || property.id == "2" ||
				( property.id == "3" && NodInspector_HasKey( edit_entity, "nod_sector" ) ) ) {
				continue;
			}

			const std::string key = "nod_prop_" + property.id;
			NodPropertyInfo info;
			info.name = property.name; info.storage = property.storage;
			if ( NodInspector_HasKey( edit_entity, key.c_str() ) ||
				!NodInspector_PropertyVisible( key.c_str(), info ) ) {
				continue;
			}

			GtkTreeIter iter;
			gtk_list_store_append( store, &iter );
			const std::string description = property.name + " (" + property.storage + ")";
			std::string access = property.local ? "Imported placement modifier" : "Inherited from " + property.archive + ":" + property.owner;
			access += property.editable ? "; Apply creates a local override" : "; " + property.problem;
			gtk_list_store_set( store, &iter, 0, key.c_str(), 1, property.value.c_str(), 2, description.c_str(),
				3, access.c_str(), 4, NodProperty_DisplayName( key, property.name ).c_str(), -1 );
		}
	}

	if ( native && nodEffectiveProperties ) {
			for ( size_t i = 0; i < native->effective.size(); ++i ) {
				const NodTemplateProperty &property = native->effective[i];
				const std::string key = "nod_prop_" + property.id, rawKey = "nod_raw_" + property.id;
				NodPropertyInfo info;
				info.name = property.name; info.storage = property.storage;
				if ( NodInspector_HasKey( edit_entity, key.c_str() ) ||
					NodInspector_HasKey( edit_entity, rawKey.c_str() ) ||
					!NodInspector_PropertyVisible( key.c_str(), info ) ) {
				continue;
			}

			GtkTreeIter iter;
				gtk_list_store_append( store, &iter );
				const std::string description = property.name + " (" + property.storage + ")";
				const std::string access = ( property.local ? "Effective serialized NSD payload" :
					"Inherited from " + property.archive + ":" + property.owner ) +
					"; read-only; use mapped transform fields";
				gtk_list_store_set( store, &iter, 0, key.c_str(), 1, property.value.c_str(), 2, description.c_str(),
					3, access.c_str(), 4, NodProperty_DisplayName( key, property.name ).c_str(), -1 );
		}
	}

	if ( NodInspector_Enabled() ) {
		GtkTreeIter iter;
		for ( gboolean valid = gtk_tree_model_get_iter_first( model, &iter ); valid; valid = gtk_tree_model_iter_next( model, &iter ) ) {
			gchar *key;
			gtk_tree_model_get( model, &iter, 0, &key, -1 );
			const bool match = strcmp( key, strKey.GetBuffer() ) == 0;
			g_free( key );
			if ( match ) {
				gtk_tree_selection_select_iter( gtk_tree_view_get_selection( GTK_TREE_VIEW( EntWidgets[EntProps] ) ), &iter );
				break;
			}
		}
	}

	gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntKeyField] ), strKey.GetBuffer() );
	gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), strVal.GetBuffer() );

	if ( nodSelectionLabel ) {
		const std::vector<entity_t *> entities = NodInspector_Selection();
		Str summary;
		summary.Format( _( "%u selected; showing first. Apply changes all." ),
						static_cast<unsigned int>( entities.size() ) );
		std::string text = summary.GetBuffer();
		if ( metadata ) {
			text += "\nTemplate: " + metadata->archive + ":" + metadata->name +
			". MAP instance only.";
		}

		if ( native ) {
			text += "\nNSD " + native->identity + " (" + native->name + "), runtime type " +
				( native->problem.empty() ? native->type : "unresolved" ) +
				"; NOT context: Global" + ( nodInstances.archives.empty() ? "" : ";" + nodInstances.archives ) +
				". Existing local properties only.";
			text += "\nRead-only source: " + g_nodViewState.nsdSource;
			if ( !native->problem.empty() ) {
				text += "\nResolution unavailable: " + native->problem;
			}

			if ( !nodInstances.diagnostics.empty() ) {
				text += "\nArchive warnings: see console.";
		}
		}

		if ( !metadataError.empty() ) {
			text += "\nMetadata unavailable: " + metadataError;
		}

		if ( nodSelectionDetails ) {
			gtk_label_set_text( GTK_LABEL( nodSelectionDetails ), text.c_str() );
		}

		std::string summaryText = edit_entity == world_entity ? _( "Map settings" ) :
			std::string( _( "Selected: " ) ) + ( native ? native->name : ValueForKey( edit_entity, "classname" ) );
		if ( entities.size() > 1 ) {
			summary.Format( _( "%u selected; showing first. Edits affect all." ),
				static_cast<unsigned int>( entities.size() ) );
			summaryText = summary.GetBuffer();
		}

		if ( !metadataError.empty() ) {
			summaryText += "\nMetadata unavailable: " + metadataError;
		}

		if ( native && !native->problem.empty() ) {
			summaryText += "\nResolution unavailable: " + native->problem;
		}

		if ( native && !nodInstances.diagnostics.empty() ) {
			summaryText += "\nArchive warnings: see console.";
		}

		gtk_label_set_text( GTK_LABEL( nodSelectionLabel ), summaryText.c_str() );
	}

	NodInspector_UpdateFields( NULL, NULL );
	Sys_UpdateWindows( W_CAMERA | W_XY );
}

// SetSpawnFlags
//
// Update the checkboxes to reflect the flag state of the entity
//
void SetSpawnFlags( void ){
	int f, i, v;

	disable_spawn_get = true;

	f = atoi( ValueForKey( edit_entity, "spawnflags" ) );
	for ( i = 0 ; i < spawnflag_count ; i++ )
	{
		v = !!( f & ( 1 << spawn_table[i] ) );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( EntWidgets[EntCheck1 + i] ), v );
	}

	// take care of the remaining ones
	for ( i = spawnflag_count ; i < MAX_FLAGS ; i++ )
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( EntWidgets[EntCheck1 + i] ), FALSE );
	}

	disable_spawn_get = false;
}

// GetSpawnFlags
//
// Update the entity flags to reflect the state of the checkboxes
//
// NOTE: this function had a tendency to add "spawnflags" "0" on most entities
//   if this wants to set spawnflags to zero, remove the key

void GetSpawnFlags( void ){
	if ( NodInspector_Enabled() && ( NodInspector_HasNativeSelection() || NodInspector_HasAuthoringSelection() ) ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Quake spawnflags are not native NOD properties. Use the named native property instead." ),
						_( "NOD entity protected" ), MB_OK );
		return;
	}

	int f, i, v;
	char sz[32];

	f = 0;
	for ( i = 0 ; i < spawnflag_count ; i++ )
	{
		v = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( EntWidgets[EntCheck1 + i] ) );
		f |= v << spawn_table[i];
	}

	if ( f == 0 ) {
		// remove all "spawnflags" keys
		if ( multiple_entities ) {
			brush_t   *b;

			for ( b = selected_brushes.next ; b != &selected_brushes ; b = b->next ) {
				DeleteKey( b->owner, "spawnflags" );
		}
		} else{
			DeleteKey( edit_entity, "spawnflags" );
		}
	}
	else
	{
		sprintf( sz, "%i", f );
		if ( multiple_entities ) {
			brush_t   *b;

			for ( b = selected_brushes.next ; b != &selected_brushes ; b = b->next ) {
				SetKeyValue( b->owner, "spawnflags", sz );
		}
		} else{
			SetKeyValue( edit_entity, "spawnflags", sz );
		}
	}

	SetKeyValuePairs();
}

//#define DBG_UPDATESEL

// UpdateSel
//
// Update the listbox, checkboxes and k/v pairs to reflect the new selection
// iIndex is the index in the list box with the class name, -1 if not found
bool UpdateSel( int iIndex, eclass_t *pec ){
	int i, next_state;
	brush_t *b;

	// syndrom of crappy code, we may get into stack overflowing crap with this function and Gtk
	// if we play with the list of entity classes
	// using a static flag to prevent recursion
	static bool bBlockUpdate = false;

	if ( bBlockUpdate ) {
		return FALSE; // NOTE TTimo wtf is the return value for anyway?

	}
#ifdef DBG_UPDATESEL
	Sys_FPrintf( SYS_WRN, "UpdateSel\n" );
#endif

	if ( selected_brushes.next == &selected_brushes ) {
		edit_entity = world_entity;
		multiple_entities = false;
	}
	else
	{
		edit_entity = selected_brushes.next->owner;
		for ( b = selected_brushes.next->next ; b != &selected_brushes ; b = b->next )
		{
			if ( b->owner != edit_entity ) {
				multiple_entities = true;
				break;
			}
		}
	}

	if ( iIndex != -1 ) {
#ifdef DBG_UPDATESEL
		Sys_FPrintf( SYS_WRN,"Setting focus_row to %d\n", iIndex );
#endif
		bBlockUpdate = true;

		GtkTreeView* view = GTK_TREE_VIEW( EntWidgets[EntList] );
		GtkTreePath* path = gtk_tree_path_new();
		gtk_tree_path_append_index( path, iIndex );
		gtk_tree_selection_select_path( gtk_tree_view_get_selection( view ), path );
		gtk_tree_view_scroll_to_cell( view, path, NULL, FALSE, 0, 0 );
		gtk_tree_path_free( path );

		bBlockUpdate = false;
	}

	if ( pec == NULL ) {
		return TRUE;
	}

	// Set up the description
	if ( !NodInspector_Enabled() ) {
		GtkTextBuffer* buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( EntWidgets[EntComment] ) );
		gtk_text_buffer_set_text( buffer, pec->comments, -1 );
	}

	spawnflag_count = 0;

	// do a first pass to count the spawn flags, don't touch the widgets, we don't know in what state they are
	for ( i = 0 ; i < MAX_FLAGS ; i++ )
	{
		if ( pec->flagnames[i] && pec->flagnames[i][0] != 0 && strcmp( pec->flagnames[i],"-" ) ) {
			spawn_table[spawnflag_count] = i;
			spawnflag_count++;
		}
	}

	// what's new widget state
	if ( spawnflag_count == 0 ) {
		next_state = 1;
	}
	else if ( spawnflag_count <= 4 ) {
		next_state = 2;
	}
	else if ( spawnflag_count <= 8 ) {
		next_state = 3;
	}
	else if ( spawnflag_count <= 12 ) {
		next_state = 4;
	}
	else{
		next_state = 5;
	}

	widget_state = next_state;
	static int last_count = 0;

	// disable all remaining boxes
	// NOTE: these boxes might not even be on display
	for ( i = 0; i < last_count; i++ )
	{
		GtkWidget *widget = EntWidgets[EntCheck1 + i];

		gtk_button_set_label( GTK_BUTTON( widget ), " " );

		g_object_ref( widget );
		gtk_widget_hide( widget );
		gtk_container_remove( GTK_CONTAINER( LayoutTable ), widget );
	}

	last_count = spawnflag_count;

	for ( i = 0 ; i < spawnflag_count ; i++ )
	{
		GtkWidget* widget = EntWidgets[EntCheck1 + i];
		gtk_widget_show( widget );

		Str str;
		str = pec->flagnames[spawn_table[i]];
		str.MakeLower();

//    gtk_table_attach (GTK_TABLE (LayoutTable), widget, i%4, i%4+1, i/4, i/4+1,
		gtk_table_attach( GTK_TABLE( LayoutTable ), widget, i % 4, i % 4 + 1, i / 4, i / 4 + 1,
						  (GtkAttachOptions) ( GTK_FILL ),
						  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
		g_object_unref( widget );

		gtk_button_set_label( GTK_BUTTON( widget ), str.GetBuffer() );
	}

	SetSpawnFlags();

	SetKeyValuePairs();

	return TRUE;
}

bool UpdateEntitySel( eclass_t *pec ){
#ifdef DBG_UPDATESEL
	Sys_FPrintf( SYS_WRN, "UpdateEntitySel\n" );
#endif

	if ( NodInspector_Enabled() ) {
		return UpdateSel( -1, pec );
	}

	GtkTreeModel* model = GTK_TREE_MODEL( gtk_tree_view_get_model( GTK_TREE_VIEW( EntWidgets[EntList] ) ) );
	GtkTreeIter iter;
	unsigned int i = 0;
	for ( gboolean good = gtk_tree_model_get_iter_first( model, &iter ); good != FALSE; good = gtk_tree_model_iter_next( model, &iter ) )
	{
		char* text;
		gtk_tree_model_get( model, &iter, 0, &text, -1 );
		if ( strcmp( text, pec->name ) == 0 ) {
#ifdef DBG_UPDATESEL
			Sys_FPrintf( SYS_WRN, "found a match: %d %s\n", i, pec->name );
#endif
			return UpdateSel( i, pec );
		}

		g_free( text );
		++i;
	}

	return UpdateSel( -1, pec );
}

// CreateEntity
//
// Creates a new entity based on the currently selected brush and entity type.
//

void CreateEntity( void ){
	GtkTreeView* view = GTK_TREE_VIEW( EntWidgets[EntList] );

	// check to make sure we have a brush
	if ( selected_brushes.next == &selected_brushes && !NodInspector_Enabled() ) {
		gtk_MessageBox( g_pParentWnd->m_pWidget, _( "You must have a selected brush to create an entity" ), _( "info" ) );
		return;
	}

	// find out what type of entity we are trying to create
	GtkTreeModel* model;
	GtkTreeIter iter;
	if ( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( view ), &model, &iter ) == FALSE ) {
		gtk_MessageBox( g_pParentWnd->m_pWidget, _( "You must have a selected class to create an entity" ), _( "info" ) );
		return;
	}

	char* text;
	gtk_tree_model_get( model, &iter, 0, &text, -1 );
	if ( NodInspector_Enabled() && ModelTransform_IsNativeEntity( text ) ) {
		g_free( text );
		gtk_MessageBox( g_pGroupDlg->m_pWidget,
			_( "Reference only; new native records cannot be created." ),
			_( "Native entity creation unavailable" ), MB_OK );
		return;
	}

	vec3_t position = { 0, 0, 0 };
	if ( NodInspector_Enabled() && selected_brushes.next != &selected_brushes ) {
		Select_GetMid( position );
	}

	CreateEntityFromName( text, position );
	g_free( text );

	if ( selected_brushes.next == &selected_brushes ) {
		edit_entity = world_entity;
	}
	else{
		edit_entity = selected_brushes.next->owner;
	}

	SetKeyValuePairs();
	Select_Deselect();
	Select_Brush( edit_entity->brushes.onext );
	Sys_UpdateWindows( W_ALL );
}

/*
   ===============
   AddProp

   ===============
 */
void AddProp(){
	if ( edit_entity == NULL ) {
		return;
	}

	// Get current selection text
	const char* key = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
	const char* value = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntValueField] ) );

	if ( NodInspector_Enabled() ) {
		if ( const char *error = nodValueEditor.Error() ) {
			Sys_FPrintf( SYS_WRN, "NOD property: %s\n", error );
			gtk_MessageBox( g_pGroupDlg->m_pWidget, error, _( "Invalid property value" ), MB_OK );
			return;
		}
	}

	if ( NodInspector_Enabled() && ( NodInspector_HasNativeSelection() || NodInspector_HasAuthoringSelection() ) ) {
		if ( NodInspector_Apply( key, value ) ) {
			SetKeyValuePairs();
		}

		return;
	}

	// TTimo: if you change the classname to worldspawn you won't merge back in the structural brushes but create a parasite entity
	if ( !strcmp( key, "classname" ) && !strcmp( value, "worldspawn" ) ) {
		gtk_MessageBox( g_pParentWnd->m_pWidget,  _( "Cannot change \"classname\" key back to worldspawn." ), NULL, MB_OK );
		return;
	}


	// RR2DO2: we don't want spaces in entity keys
	if ( strstr( key, " " ) ) {
		gtk_MessageBox( g_pParentWnd->m_pWidget, _( "No spaces are allowed in entity keys." ), NULL, MB_OK );
		return;
	}

	bool modelScaleEdit = false;
	bool modelRotationEdit = false;
	bool vectorOverride = false;
	const bool uniformScaleKey = strcmp( key, "modelscale" ) == 0;
	const bool scaleKey = uniformScaleKey || strcmp( key, "modelscale_vec" ) == 0;
	const bool yawKey = strcmp( key, "angle" ) == 0;
	const bool rotationKey = yawKey || strcmp( key, "angles" ) == 0;
	if ( scaleKey || rotationKey ) {
		if ( multiple_entities ) {
			for ( brush_t *b = selected_brushes.next; b != &selected_brushes; b = b->next ) {
				if ( ModelTransform_IsEntity( ValueForKey( b->owner, "classname" ), ValueForKey( b->owner, "model" ) ) ) {
					modelScaleEdit = scaleKey;
					modelRotationEdit = rotationKey;
					vectorOverride = vectorOverride || ValueForKey( b->owner, "modelscale_vec" )[0] != '\0';
				}
			}
		}
		else if ( ModelTransform_IsEntity( ValueForKey( edit_entity, "classname" ), ValueForKey( edit_entity, "model" ) ) ) {
			modelScaleEdit = scaleKey;
			modelRotationEdit = rotationKey;
			vectorOverride = ValueForKey( edit_entity, "modelscale_vec" )[0] != '\0';
		}
	}

	if ( modelScaleEdit ) {
		float scale[3];
		if ( value[0] != '\0' && !ModelTransform_Parse( value, uniformScaleKey ? 1 : 3, scale ) ) {
			gtk_MessageBox( g_pParentWnd->m_pWidget,
							_( "Scale: one finite nonzero value, or three for modelscale_vec." ),
							_( "Invalid model scale" ), MB_OK );
			return;
		}
	}

	if ( modelRotationEdit ) {
		float angles[3];
		if ( value[0] != '\0' && !ModelTransform_Parse( value, yawKey ? 1 : 3, angles, true ) ) {
			gtk_MessageBox( g_pParentWnd->m_pWidget,
							_( "Degrees: yaw for angle; pitch yaw roll for angles. Values must be finite." ),
							_( "Invalid model rotation" ), MB_OK );
			return;
		}
	}

	if ( modelScaleEdit || modelRotationEdit ) {
		Undo_Start( modelScaleEdit ? "model scale" : "model rotation" );
		Undo_AddBrushList( &selected_brushes );
		Undo_AddEntity( edit_entity );
	}

	if ( multiple_entities ) {
		brush_t *b;

		for ( b = selected_brushes.next ; b != &selected_brushes ; b = b->next ) {
			if ( modelRotationEdit && ModelTransform_IsEntity( ValueForKey( b->owner, "classname" ), ValueForKey( b->owner, "model" ) ) ) {
				DeleteKey( b->owner, yawKey ? "angles" : "angle" );
			}

			SetKeyValue( b->owner, key, value );
		}
	}
	else{
		if ( modelRotationEdit ) {
			DeleteKey( edit_entity, yawKey ? "angles" : "angle" );
		}

		SetKeyValue( edit_entity, key, value );
	}

	if ( modelScaleEdit || modelRotationEdit ) {
		Undo_EndBrushList( &selected_brushes );
		Undo_EndEntity( edit_entity );
		Undo_End();
		Sys_MarkMapModified();
	}

	// refresh the prop listbox
	SetKeyValuePairs();

	if ( modelScaleEdit && uniformScaleKey && vectorOverride ) {
		gtk_MessageBox( g_pParentWnd->m_pWidget,
						_( "Saved, but modelscale_vec takes priority. Edit/delete it or use Scale..." ),
						_( "Per-axis model scale is active" ), MB_OK );
	}


#ifdef USEPLUGINENTITIES
	// if it's a plugin entity, perhaps we need to update some drawing parameters
	// NOTE: perhaps moving this code to a seperate func would help if we need it in other places
	// TODO: we need to call some update func in the IPluginEntity in case model name changes etc.
	// ( for the moment only bounding brush is updated ), see UpdateModelBrush in Ritual's Q3Radiant
	if ( edit_entity->eclass->nShowFlags & ECLASS_PLUGINENTITY ) {
		vec3_t mins, maxs;
		edit_entity->pPlugEnt->GetBounds( mins, maxs );
		// replace old bounding brush by newly computed one
		// NOTE: this part is similar to Entity_BuildModelBrush in Ritual's Q3Radiant, it can be
		// usefull moved into a seperate func
		brush_t *b,*oldbrush;
		if ( edit_entity->brushes.onext != &edit_entity->brushes ) {
			oldbrush = edit_entity->brushes.onext;
		}

		b = Brush_Create( mins, maxs, &edit_entity->eclass->texdef );
		Entity_LinkBrush( edit_entity, b );
		Brush_Build( b, true );
		Select_Deselect();
		Brush_AddToList( edit_entity->brushes.onext, &selected_brushes );
		if ( oldbrush ) {
			Brush_Free( oldbrush );
		}
	}
#endif // USEPLUGINENTITIES
}

/*
   ===============
   DelProp

   ===============
 */
void DelProp( void ){
	if ( edit_entity == NULL ) {
		return;
	}

	if ( NodInspector_Enabled() && NodInspector_HasAuthoringSelection() ) {
		const std::string key = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
		const std::vector<entity_t *> selection = NodInspector_Selection();
		std::vector<const NodTemplateChoice *> templates;
		std::string error;
		for ( size_t i = 0; i < selection.size(); ++i ) {
			templates.push_back( NodInspector_IsAuthoring( selection[i] ) ? NodInspector_Metadata( selection[i], error ) : NULL );
		}

		if ( gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Remove this override from all selected instances and use their inherited defaults?" ),
			_( "Remove instance override" ), MB_YESNO ) != IDYES ) {
			return;
		}

		bool changed;
		if ( !NodTemplate_ApplyOverrides( selection, templates, key.c_str(), "", true, changed, error ) ) {
			Sys_FPrintf( SYS_WRN, "NOD remove override: %s\n", error.c_str() );
			gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NOD override not removed" ), MB_OK );
			return;
		}

		if ( changed ) {
			Sys_MarkMapModified();
		}

		SetKeyValuePairs();
		return;
	}

	if ( NodInspector_Enabled() && NodInspector_HasNativeSelection() ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Native key deletion is unsupported; record structure is protected." ),
						_( "NOD property protected" ), MB_OK );
		return;
	}

	// Get current selection text
	const char* key = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );

	if ( multiple_entities ) {
		brush_t *b;

		for ( b = selected_brushes.next ; b != &selected_brushes ; b = b->next ) {
			DeleteKey( b->owner, key );
	}
	} else{
		DeleteKey( edit_entity, key );
	}

	// refresh the prop listbox
	SetKeyValuePairs();
}

void ResetEntity(){
	epair_t *pep;
	int i;

	if ( edit_entity == NULL ) {
		return;
	}

	if ( NodInspector_Enabled() && ( NodInspector_HasNativeSelection() || NodInspector_HasAuthoringSelection() ) ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Native entities cannot be reset; identity and properties are protected." ),
						_( "NOD entity protected" ), MB_OK );
		return;
	}

	if ( multiple_entities ) {
		brush_t *b;

		for ( b = selected_brushes.next; b != &selected_brushes; b = b->next ) {
			for ( pep = b->owner->epairs; pep; )
			{
				if ( strcmp( pep->key, "classname" ) != 0 ) {
					DeleteKey( b->owner, pep->key );
					pep = b->owner->epairs;
				}
				else{
					pep = pep->next;
				}
			}
	}
	} else{
		for ( pep = edit_entity->epairs; pep; )
		{
			if ( strcmp( pep->key, "classname" ) != 0 ) {
				DeleteKey( edit_entity, pep->key );
				pep = edit_entity->epairs;
			}
			else{
				pep = pep->next;
			}
		}
	}

	// refresh the dialog
	SetKeyValuePairs();
	for ( i = EntCheck1; i <= EntCheck16; i++ ) {
		g_signal_handlers_block_by_func( G_OBJECT( EntWidgets[i] ), (gpointer)G_CALLBACK( entity_check ), NULL );
	}

	SetSpawnFlags();
	for ( i = EntCheck1; i <= EntCheck16; i++ ) {
		g_signal_handlers_unblock_by_func( G_OBJECT( EntWidgets[i] ), (gpointer)G_CALLBACK( entity_check ), NULL );
}
}

bool GetSelectAllCriteria( CString &strKey, CString &strVal ){
	GtkTreeModel* model;
	GtkTreeIter iter;
	if ( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW( EntWidgets[EntProps] ) ), &model, &iter )
		 && ( inspector_mode == W_ENTITY )
		 && gtk_widget_get_visible( g_pGroupDlg->m_pWidget ) ) {
		strKey = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) );
		strVal = gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntValueField] ) );
		return TRUE;
	}

	return FALSE;
}


void AssignSound(){
	char buffer[NAME_MAX];

	strcpy( buffer, g_qeglobals.m_strHomeMaps.GetBuffer() );
	strcat( buffer, "sound/" );

	if ( access( buffer, R_OK ) != 0 ) {
		// just go to fsmain
		strcpy( buffer, g_qeglobals.m_strHomeMaps.GetBuffer() );
		strcat( buffer, "/" );
	}

	const char *filename = file_dialog( g_pGroupDlg->m_pWidget, TRUE, _( "Open Wav File" ), buffer, "sound" );
	if ( filename != NULL ) {
		gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntKeyField] ), "noise" );
		char *aux = vfsExtractRelativePath( filename );
		CString str;
		if ( aux ) {
			str = aux;
		}
		else
		{
			Sys_FPrintf( SYS_WRN, "WARNING: could not extract the relative path, using full path instead\n" );
			str = filename;
		}

		gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), str.GetBuffer() );
		AddProp();
	}
}

void AssignModel(){
	char buffer[NAME_MAX];

	strcpy( buffer, g_qeglobals.m_strHomeMaps.GetBuffer() );
	strcat( buffer, "models/" );

	if ( access( buffer, R_OK ) != 0 ) {
		// just go to fsmain
		strcpy( buffer, g_qeglobals.m_strHomeMaps.GetBuffer() );
		strcat( buffer, "/" );
	}

	const char *filename = file_dialog( g_pGroupDlg->m_pWidget, TRUE, _( "Open Model" ), buffer, MODEL_MAJOR );
	if ( filename != NULL ) {
		gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntKeyField] ), "model" );
		// use VFS to get the correct relative path
		char *aux = vfsExtractRelativePath( filename );
		CString str;
		if ( aux ) {
			str = aux;
		}
		else
		{
			Sys_FPrintf( SYS_WRN, "WARNING: could not extract the relative path, using full path instead\n" );
			str = filename;
		}

		gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), str.GetBuffer() );
		AddProp();
		edit_entity->brushes.onext->bModelFailed = false;
	}
}

void cam2angles()
{
	Str value;
	camera_t *cam;

	cam = g_pParentWnd->GetCamWnd()->Camera();
	//pitch yaw roll
	value.Format( "%g %g %g", cam->angles[0], cam->angles[1], cam->angles[2] );

	gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntKeyField] ), "angles" );
	gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), value.GetBuffer() );
	AddProp();
}

/*
   ==============
   SetInspectorMode
   ==============
 */
void SetInspectorMode( int iType ){
	if ( iType == W_GROUP ) {
		gtk_MessageBox( g_pParentWnd->m_pWidget, _( "Brush grouping is not functional yet" ), NULL, MB_OK | MB_ICONWARNING );
	}

	if ( !g_pParentWnd->FloatingGroupDialog() &&
		 ( iType == W_TEXTURE || iType == W_CONSOLE ) ) {
		return;
	}

	// Is the caller asking us to cycle to the next window?
	if ( iType == -1 ) {
		if ( inspector_mode == W_ENTITY ) {
			iType = W_TEXTURE;
		}
		else if ( inspector_mode == W_TEXTURE ) {
			iType = W_CONSOLE;
		}
		else if ( inspector_mode == W_CONSOLE ) {
			iType = W_GROUP;
		}
		else{
			iType = W_ENTITY;
		}
	}

	switch ( iType ) {
	case W_ENTITY:
		// entity is always first in the inspector
		gtk_window_set_title( GTK_WINDOW( g_qeglobals_gui.d_entity ), _( "Entities" ) );
		gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 0 );
		break;

	case W_TEXTURE:
		g_pParentWnd->GetTexWnd()->FocusEdit();
		gtk_window_set_title( GTK_WINDOW( g_qeglobals_gui.d_entity ), _( "Textures" ) );
		if ( g_pParentWnd->FloatingGroupDialog() ) {
                  // if the notebook page is already at 1, no expose event fires up on the embedded GLWindow, leading in the texture window not drawing
                  // I did witness an expose event on the notebook widget though, but for some reason it's not traveling down..
                  // so when hiding the group dialog, we are setting the page to 0, the page switch does an expose and triggers drawing.. (see OnDialogKey)
                  gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 1 );
                  // FIXME: https://github.com/TTimo/GtkRadiant/issues/192
//                  Sys_Printf( "show widget: set page to 1\n" );
		}

		break;

	case W_CONSOLE:
		gtk_window_set_title( GTK_WINDOW( g_qeglobals_gui.d_entity ), _( "Console" ) );
		if ( g_pParentWnd->FloatingGroupDialog() ) {
			gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 2 );
		}

		break;

	case W_GROUP:
		if ( g_pParentWnd->FloatingGroupDialog() ) {
			gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 3 );
		} else {
			gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 1 );
		}

		break;

	default:
		break;
	}
}

void Group_Add( entity_t *e ){
	/*
	   group_t *g = (group_t*)qmalloc(sizeof(group_t));
	   g->epairs = e->epairs;
	   g->next = NULL;
	   e->epairs = NULL;

	   // create a new group node
	   char *text = ValueForKey(g->epairs, "group");
	   g->itemOwner = gtk_ctree_insert_node (GTK_CTREE (g_wndGroup.m_pTree), g_wndGroup.m_hWorld, NULL, &text, 0,
	                  tree_pixmaps[IMG_GROUP], tree_masks[IMG_GROUP],
	                  tree_pixmaps[IMG_GROUP], tree_masks[IMG_GROUP], TRUE, TRUE);
	   g->next = g_pGroups;
	   g_pGroups = g;
	 */
}

/*
   group_t* Group_Alloc(char *name)
   {
   group_t *g = (group_t*)qmalloc(sizeof(group_t));
   SetKeyValue( g->epairs, "group", name );
   return g;
   }

   group_t* Group_ForName(const char * name)
   {
   group_t *g = g_pGroups;
   while (g != NULL)
   {
    if (strcmp( ValueForKey(g->epairs,"group"), name ) == 0)
      break;
    g = g->next;
   }
   return g;
   }

   void Group_AddToItem(brush_t *b, GtkCTreeNode* item)
   {
   int nImage = IMG_BRUSH;
   if (!g_qeglobals.m_bBrushPrimitMode)
   {
    return;
   }
   const char *pName = NULL;
   //  const char *pNamed = Brush_GetKeyValue(b, "name");

   if (!b->owner || (b->owner == world_entity))
   {
    if (b->patchBrush)
    {
      pName = "Generic Patch";
      nImage = IMG_PATCH;
    }
    else
    {
      pName = "Generic Brush";
      nImage = IMG_BRUSH;
    }
   }
   else
   {
    pName = b->owner->eclass->name;
    if (b->owner->eclass->fixedsize)
    {
      nImage = IMG_ENTITY;
    }
    else
    {
      nImage = IMG_ENTITYGROUP;
    }
   }

   GtkCTreeNode *newItem;
   int i = (b->patchBrush) ? IMG_PATCH : IMG_BRUSH;
   newItem = gtk_ctree_insert_node (GTK_CTREE (g_wndGroup.m_pTree), item, NULL, (gchar**)&pName, 0,
                   tree_pixmaps[i], tree_masks[i], tree_pixmaps[i],
                   tree_masks[i], TRUE, TRUE);
   gtk_ctree_node_set_row_data (GTK_CTREE (g_wndGroup.m_pTree), newItem, b);
   b->itemOwner = newItem;
   }
 */
void Group_RemoveBrush( brush_t *b ){
	/*
	   if (!g_qeglobals.m_bBrushPrimitMode)
	   {
	   return;
	   }
	   if (b->itemOwner)
	   {
	   gtk_ctree_remove_node (GTK_CTREE (g_pGroupDlg->m_pTree), b->itemOwner);
	   b->itemOwner = NULL;
	   }
	   DeleteKey(b->epairs, "group");
	 */
}

/*
   void Group_AddToWorld(brush_t *b)
   {
   if (!g_qeglobals.m_bBrushPrimitMode)
   {
    return;
   }
   GtkCTreeNode *parent = gtk_ctree_node_nth (GTK_CTREE (g_pGroupDlg->m_pTree), 0);
   Group_AddToItem(b, parent);
   }
 */
void Group_AddToProperGroup( brush_t *b ){
	/*
	   if (!g_qeglobals.m_bBrushPrimitMode)
	   {
	   return;
	   }

	   // NOTE: we do a local copy of the "group" key because it gets erased by Group_RemoveBrush
	   const char *pGroup = Brush_GetKeyValue(b, "group");
	   // remove the entry in the tree if there's one
	   if (b->itemOwner)
	   {
	   gtk_ctree_remove_node (GTK_CTREE (g_pGroupDlg->m_pTree), b->itemOwner);
	   b->itemOwner = NULL;
	   }

	   if (*pGroup != 0)
	   {
	   // find the item
	   group_t *g = Group_ForName(pGroup);
	   if (g)
	    Group_AddToItem(b, g->itemOwner);
	   #ifdef _DEBUG
	   else
	    Sys_FPrintf(SYS_WRN, "WARNING: unexpected Group_ForName not found in Group_AddToProperGroup\n");
	   #endif
	   }
	   else
	   {
	   Group_AddToWorld(b);
	   }
	 */
}

/*
   void Group_AddToSelected(brush_t *b)
   {
   if (!g_qeglobals.m_bBrushPrimitMode)
   {
    return;
   }
   GtkCTreeNode *item;
   item = gtk_ctree_node_nth (GTK_CTREE (g_pGroupDlg->m_pTree), GTK_CLIST (g_pGroupDlg->m_pTree)->focus_row);
   if (item == NULL)
   {
    item = gtk_ctree_node_nth (GTK_CTREE (g_pGroupDlg->m_pTree), 0);
   }
   Group_AddToItem(b, item);
   }
 */
/*
   void Group_Save(FILE *f)
   {
   group_t *g = g_pGroups;
   while (g)
   {
    fprintf(f,"{\n\"classname\" \"group_info\"\n\"group\" \"%s\"\n}\n", ValueForKey( g->epairs, "group" ));
    g = g->next;
   }
   }
 */

void Group_Init(){
	if ( !g_qeglobals.m_bBrushPrimitMode ) {
		return;
	}

	// start by cleaning everything
	// clean the groups
	//++timo FIXME: we leak, delete the groups on the way (I don't have time to do it now)
#ifdef _DEBUG
	Sys_Printf( "TODO: fix leak in Group_Init\n" );
#endif
	group_t *g = g_pGroups;
	while ( g )
	{
		epair_t *ep,*enext;
		for ( ep = g->epairs ; ep ; ep = enext )
		{
			enext = ep->next;
			free( ep->key );
			free( ep->value );
			free( ep );
		}

		g = g->next;
	}

	/*
	   GtkCTreeNode *world;
	   char *text = "World";
	   g_pGroups = NULL;
	   gtk_clist_clear (GTK_CLIST (g_wndGroup.m_pTree));
	   world = gtk_ctree_insert_node (GTK_CTREE (g_wndGroup.m_pTree), NULL, NULL, &text, 0,
	               tree_pixmaps[IMG_GROUP], tree_masks[IMG_GROUP], tree_pixmaps[IMG_GROUP],
	               tree_masks[IMG_GROUP], FALSE, TRUE);
	 */
	// walk through all the brushes, remove the itemOwner key and add them back where they belong
	brush_t *b;
	for ( b = active_brushes.next; b != &active_brushes; b = b->next )
	{
		b->itemOwner = NULL;
		Group_AddToProperGroup( b );
	}

	for ( b = selected_brushes.next ; b != &selected_brushes ; b = b->next )
	{
		b->itemOwner = NULL;
		Group_AddToProperGroup( b );
	}
}

/*
   // scan through world_entity for groups in this map?
   // we use GROUPNAME "QER_group_%i" to look for existing groups and their naming
   //++timo FIXME: is this actually needed for anything?
   void Group_GetListFromWorld(GSList **pArray)
   {
   if (!g_qeglobals.m_bBrushPrimitMode)
   {
    return;
   }

   if (world_entity == NULL)
   {
    return;
   }

   char cBuff[1024];
   for (int i =0; i < MAX_GROUPS; i++)
   {
    sprintf(cBuff, GROUPNAME, i);
    char *pGroup = ValueForKey(world_entity, cBuff);
    if (pGroup && strlen(pGroup) > 0)
    {
   *pArray = g_slist_append (*pArray, g_strdup (pGroup));
    }
    else
    {
      break;
    }
   }
   }

   void Group_RemoveListFromWorld()
   {
   if (!g_qeglobals.m_bBrushPrimitMode)
   {
    return;
   }
   GSList* array = NULL;
   Group_GetListFromWorld(&array);

   while (array)
   {
    DeleteKey(world_entity, (char*)array->data);
    g_free (array->data);
    array = g_slist_remove (array, array->data);
   }
   }

   int CountChar(const char *p, char c)
   {
   int nCount = 0;
   int nLen = strlen(p)-1;
   while (nLen-- >= 0)
   {
    if (p[nLen] == c)
    {
      nCount++;
    }
   }
   return nCount;
   }
 */
// =============================================================================
// callbacks

static void NodInspector_BrowserChanged(){
	if ( !nodPlace || !EntWidgets[EntList] ) {
		return;
	}

	eclass_t *type = static_cast<eclass_t *>( NodInspectorView_SharedTarget( GTK_TREE_VIEW( EntWidgets[EntList] ), NULL, true ) );
	const NodTemplateChoice *choice = type ? NodClasses_Find( type->name ) : NULL;
	const bool reference = type && ModelTransform_IsNativeEntity( type->name );
	const bool placeable = type && strcmp( type->name, "worldspawn" ) != 0 && !reference &&
		( choice ? choice->placeable : !NodClass_Registered( type->comments ) );
	gtk_widget_set_sensitive( nodPlace, placeable );
	gtk_widget_set_sensitive( nodBrowsedShared, choice != NULL );
	std::string text = type ? std::string( _( "Template: " ) ) + type->name : _( "Choose a template to place." );
	if ( type && !placeable ) {
		text += "\n";
		text += choice && !choice->problem.empty() ? choice->problem : _( "This class cannot be placed here." );
	}

	gtk_label_set_text( GTK_LABEL( nodPlacementLabel ), text.c_str() );
	gtk_widget_set_tooltip_text( nodPlace, choice ?
		_( "Place at selection centre, or origin if empty. Keeps existing geometry." ) :
		_( "Create the chosen class using standard Radiant entity creation." ) );
	if ( EntWidgets[EntComment] ) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( EntWidgets[EntComment] ) );
		gtk_text_buffer_set_text( buffer, type && type->comments ? type->comments : "", -1 );
	}
}

static void eclasslist_selection_changed( GtkTreeSelection* selection, gpointer data ){
	GtkTreeModel* model;
	GtkTreeIter selected;
	if ( NodInspector_Enabled() ) {
		NodInspector_BrowserChanged();
		return;
	}

	// no world entity, we are not ready yet
	if ( !world_entity ) {
		return;
	}

	if ( gtk_tree_selection_get_selected( selection, &model, &selected ) ) {
		eclass_t* eclass;
		gtk_tree_model_get( model, &selected, 1, &eclass, -1 );
		if ( eclass != NULL ) {
			GtkTreePath* path = gtk_tree_model_get_path( model, &selected );
			UpdateSel( gtk_tree_path_get_indices( path )[0], eclass );
			gtk_tree_path_free( path );
		}
	}
}

static gint eclasslist_button_press( GtkWidget *widget, GdkEventButton *event, gpointer data ){
	if ( event->type == GDK_2BUTTON_PRESS ) {
		CreateEntity();
		return TRUE;
	}

	return FALSE;
}

static gint eclasslist_keypress( GtkWidget* widget, GdkEventKey* event, gpointer data ){
	unsigned int code = gdk_keyval_to_upper( event->keyval );

	if ( event->keyval == GDK_KEY_Return ) {
		CreateEntity();
		return TRUE;
	}

	// select the entity that starts with the key pressed
	if ( code <= 'Z' && code >= 'A' ) {
		GtkTreeView* view = GTK_TREE_VIEW( EntWidgets[EntList] );
		GtkTreeModel* model;
		GtkTreeIter iter;
		if ( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( view ), &model, &iter ) == FALSE
			 || gtk_tree_model_iter_next( model, &iter ) == FALSE ) {
			gtk_tree_model_get_iter_first( model, &iter );
		}

		for ( unsigned int count = gtk_tree_model_iter_n_children( model, NULL ); count > 0; --count )
		{
			char* text;
			gtk_tree_model_get( model, &iter, 0, &text, -1 );

			if ( toupper( text[0] ) == (int)code ) {
				GtkTreePath* path = gtk_tree_model_get_path( model, &iter );
				gtk_tree_selection_select_path( gtk_tree_view_get_selection( view ), path );
				gtk_tree_view_scroll_to_cell( view, path, NULL, FALSE, 0, 0 );
				gtk_tree_path_free( path );
				count = 1;
			}

			g_free( text );

			if ( gtk_tree_model_iter_next( model, &iter ) == FALSE ) {
				gtk_tree_model_get_iter_first( model, &iter );
			}
		}

		return TRUE;
	}

	return FALSE;
}

static void proplist_selection_changed( GtkTreeSelection* selection, gpointer data ){
	// find out what type of entity we are trying to create
	GtkTreeModel* model;
	GtkTreeIter iter;
	GtkWidget *dialog, *del_button;
	gboolean selected;

	dialog = GTK_WIDGET( data );
	del_button = GTK_WIDGET( g_object_get_data( G_OBJECT( dialog ), "del_button" ) );

	selected = gtk_tree_selection_get_selected( selection, &model, &iter );

	gtk_widget_set_sensitive( del_button, selected );

	if ( selected == FALSE ) {
		if ( nodPropertyName ) {
			gtk_label_set_text( GTK_LABEL( nodPropertyName ), _( "Select a property" ) );
		}

		if ( nodPropertyDetails ) {
			gtk_label_set_text( GTK_LABEL( nodPropertyDetails ), _( "Select a property to see its source and edit policy." ) );
		}

		NodInspector_UpdateFields( NULL, NULL );
		return;
	}

	char* key;
	char* val;
	gtk_tree_model_get( model, &iter, 0, &key, 1, &val, -1 );
	if ( NodInspector_Enabled() ) {
		gchar *name, *description, *access;
		gtk_tree_model_get( model, &iter, 4, &name, 2, &description, 3, &access, -1 );
		if ( nodPropertyName ) {
			gtk_label_set_text( GTK_LABEL( nodPropertyName ), name );
		}

		const std::string details = std::string( key ) + "\n" + ( description ? description : "" ) + "\n" + ( access ? access : "" );
		if ( nodPropertyDetails ) {
			gtk_label_set_text( GTK_LABEL( nodPropertyDetails ), details.c_str() );
		}

		g_free( name ); g_free( description ); g_free( access );
	}

	if ( stricmp( key, "classname" ) == 0 ) {
		gtk_widget_set_sensitive( del_button, FALSE );
	}

	gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntKeyField] ), key );
	gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), val );
	if ( NodInspector_Enabled() && NodInspector_HasNativeSelection() ) {
		gtk_widget_set_sensitive( del_button, FALSE );
	} else if ( NodInspector_Enabled() && NodInspector_HasAuthoringSelection() ) {
		const std::vector<entity_t *> entities = NodInspector_Selection();
		bool removable = true;
		for ( size_t i = 0; i < entities.size() && removable; ++i ) {
			std::string error;
			const NodTemplateChoice *choice = NodInspector_IsAuthoring( entities[i] ) ? NodInspector_Metadata( entities[i], error ) : NULL;
			removable = choice && !NodTemplate_OverrideRestriction( *choice, key, true, NodInspector_HasKey( entities[i], key ) );
		}

		gtk_widget_set_sensitive( del_button, removable );
	}

	g_free( key );
	g_free( val );
}

static void proplist_view_realize( GtkWidget *widget, gpointer data )
{
	proplist_selection_changed( gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ) ), data );
}

static void entity_check( GtkWidget *widget, gpointer data ){
	if ( !disable_spawn_get ) {
		GetSpawnFlags();
	}
}

static void entitylist_angle( GtkWidget *widget, gpointer data ){
	if ( NodInspector_Enabled() && ( NodInspector_HasNativeSelection() || NodInspector_HasAuthoringSelection() ) ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget,
			_( "Use angles (degrees) or nod_rotation (radians), not Quake direction shortcuts." ),
			_( "NOD orientation" ), MB_OK );
		return;
	}

	if ( edit_entity != NULL && ModelTransform_IsEntity( ValueForKey( edit_entity, "classname" ), ValueForKey( edit_entity, "model" ) ) ) {
		gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntKeyField] ), "angle" );
		gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), (char*)data );
		AddProp();
		return;
	}

	SetKeyValue( edit_entity, "angle", (char*)data );
	SetKeyValuePairs();
}

static gint entityentry_keypress( GtkWidget* widget, GdkEventKey* event, gpointer data ){
	if ( event->keyval == GDK_KEY_Tab ) {
		if ( NodInspector_Enabled() && widget != EntWidgets[EntKeyField] ) {
			return FALSE;
		}

		if ( widget == EntWidgets[EntKeyField] ) {
			//gtk_entry_set_text (GTK_ENTRY (EntWidgets[EntValueField]), "");
			gtk_window_set_focus( GTK_WINDOW( g_pGroupDlg->m_pWidget ),
				NodInspector_Enabled() ? nodValueEditor.FocusWidget() : EntWidgets[EntValueField] );
		}
		else{
			gtk_window_set_focus( GTK_WINDOW( g_pGroupDlg->m_pWidget ), EntWidgets[EntKeyField] );
		}

		return TRUE;
	}
	else if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter ) {
		if ( widget == EntWidgets[EntKeyField] ) {
			gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), "" );
			gtk_window_set_focus( GTK_WINDOW( g_pGroupDlg->m_pWidget ), EntWidgets[EntValueField] );
		}
		else
		{
			AddProp();
		}

		return TRUE;
	}

	return FALSE;
}

static void switch_page( GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer data ){
	const gchar *text;
	text = gtk_label_get_text( GTK_LABEL( gtk_notebook_get_tab_label( notebook, gtk_notebook_get_nth_page( notebook, page_num ) ) ) );
	gtk_window_set_title( GTK_WINDOW( data ), text );

	gpointer item = g_object_get_data( G_OBJECT( g_pParentWnd->m_pWidget ), "menu_misc_selectentitycolor" );

	if ( g_pParentWnd->FloatingGroupDialog() ) {
		switch ( page_num )
		{
		case 0: inspector_mode = W_ENTITY; break;
		case 1: inspector_mode = W_TEXTURE; break;
		case 2: inspector_mode = W_CONSOLE; break;
		default: inspector_mode = W_GROUP; break;
		}
	}
	else
	{
		if ( page_num == 0 ) {
			inspector_mode = W_ENTITY;
		}
		else{
			inspector_mode = W_GROUP;
		}
	}

	if ( inspector_mode == W_ENTITY ) {
		gtk_widget_set_sensitive( GTK_WIDGET( item ), TRUE );
	}
	else{
		gtk_widget_set_sensitive( GTK_WIDGET( item ), FALSE );
	}
}

// =============================================================================
// GroupDlg class

static gint OnDeleteHide( GtkWidget *widget ) {
  gtk_widget_hide( widget );
  // see OnDialogKey
  gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 0 );
  return TRUE;
}

// NOTE: when a key is hit with group window focused, we catch in this handler but it gets propagated to mainframe too
//   therefore the message will be intercepted and used as a ID_SELECTION_DESELECT
static gint OnDialogKey( GtkWidget* widget, GdkEventKey* event, gpointer data ) {
  // make the "ViewTextures" and "ViewEntityInfo" keys that normally bring this dialog up hide it as well - copypasta from mainframe_keypress
  // NOTE: maybe we could also check the state of the notebook, see if those are actually displayed .. if they are not, then switch the notebook pages rather than hiding?
  bool hide = false;
  // Disable this, it makes the dialog hide whenever you type 'T' when editing entities etc. - Esc key is enough
#if 0
  unsigned int code = gdk_keyval_to_upper( event->keyval );
  for ( int i = 0; i < g_nCommandCount; i++ ) {
    if ( g_Commands[i].m_nKey == code ) { // find a match?
      // check modifiers
      unsigned int nState = 0;
      if ( Sys_AltDown() ) {
        nState |= RAD_ALT;
      }

      if ( ( event->state & GDK_CONTROL_MASK ) != 0 ) {
        nState |= RAD_CONTROL;
      }

      if ( ( event->state & GDK_SHIFT_MASK ) != 0 ) {
        nState |= RAD_SHIFT;
      }

      if ( ( g_Commands[i].m_nModifiers & 0x7 ) == nState ) {
        Sys_Printf( "Floating group dialog: %s\n", g_Commands[i].m_strCommand );
        if ( g_Commands[i].m_nCommand == ID_VIEW_TEXTURE || g_Commands[i].m_nCommand == ID_VIEW_ENTITY ) {
          hide = true;
        }

        break;
      }
    }
  }
#endif
  if ( g_pParentWnd->CurrentStyle() != MainFrame::eFloating && ( hide || event->keyval == GDK_KEY_Escape ) ) {
    // toggle off the group view (whatever part of it is currently displayed)
    // this used to be done with a g_pParentWnd->OnViewEntity(); but it had bad consequences
    gtk_widget_hide( widget );
    // set the group notebook page back to 0, so that when we recall the texture view there is an expose event coming up
    gtk_notebook_set_current_page( GTK_NOTEBOOK( g_pGroupDlg->m_pNotebook ), 0 );
    // FIXME: https://github.com/TTimo/GtkRadiant/issues/192
//    Sys_Printf( "hide widget and set page to 0\n" );
    return TRUE;
  }

  return FALSE;
}

GroupDlg::GroupDlg (){
	m_pWidget = NULL;
}

#ifdef _WIN32
extern void PositionWindowOnPrimaryScreen( window_position_t& position );
#endif

static void NodInspector_TypeSearchChanged( GtkWidget *, gpointer ){
	FillClassList();
}

static void NodInspector_PropertySearchChanged( GtkWidget *, gpointer ){
	SetKeyValuePairs();
}

static void NodInspector_PropertyViewChanged( GtkComboBox *combo, gpointer ){
	nodEffectiveProperties = gtk_combo_box_get_active( combo ) == 1;
	SetKeyValuePairs();
}

static void NodInspector_TechnicalChanged( GtkToggleButton *toggle, gpointer ){
	const bool technical = gtk_toggle_button_get_active( toggle ) != FALSE;
	nodValueEditor.Technical( technical );
	NodInspectorView_Technical( GTK_TREE_VIEW( EntWidgets[EntProps] ), technical );
	GtkTreeViewColumn *parent = gtk_tree_view_get_column( GTK_TREE_VIEW( EntWidgets[EntList] ), 1 );
	if ( parent ) {
		gtk_tree_view_column_set_visible( parent, technical );
	}

	gtk_widget_set_visible( EntWidgets[EntKeyField], technical );
	gtk_widget_set_visible( nodPropertyName, !technical );
	gtk_label_set_text( GTK_LABEL( nodKeyLabel ), technical ? _( "Key" ) : _( "Property" ) );
	SetKeyValuePairs();
}

static void NodInspector_Place( GtkWidget *, gpointer ){
	CreateEntity();
}

static GtkWidget *NodInspector_SharedSection( GtkWidget *parent, bool browsing ){
	GtkWidget *content = gtk_vbox_new( FALSE, 5 );
	GtkWidget *scope = gtk_label_new( browsing ?
		_( "Inspect the browsed NOT template's defaults (read-only), independently of map selection." ) :
		_( "Template defaults only; read-only." ) );
	gtk_label_set_line_wrap( GTK_LABEL( scope ), TRUE );
	gtk_label_set_line_wrap_mode( GTK_LABEL( scope ), PANGO_WRAP_WORD_CHAR );
	gtk_misc_set_alignment( GTK_MISC( scope ), 0, 0.5 );
	gtk_box_pack_start( GTK_BOX( content ), scope, FALSE, FALSE, 0 );
	GtkWidget *button = gtk_button_new_with_label( _( "Inspect template defaults..." ) );
	gtk_widget_set_sensitive( button, FALSE );
	gtk_box_pack_start( GTK_BOX( content ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( NodInspector_InspectDefaults ), GINT_TO_POINTER( browsing ? 1 : 0 ) );
	GtkWidget *section = NodInspectorView_Section( _( "Shared template defaults" ), content );
	gtk_box_pack_end( GTK_BOX( parent ), section, FALSE, FALSE, 0 );
	gtk_widget_show_all( section );
	return button;
}

static void NodInspector_LoadArchives( GtkWidget *, gpointer ){
	const std::string previous = NodClasses_Context();
	const std::string input = gtk_entry_get_text( GTK_ENTRY( nodArchives ) );
	std::string error;
	for ( entity_t *entity = entities.next; entity != &entities; entity = entity->next ) {
		if ( !NodInspector_IsAuthoring( entity ) ) {
			continue;
		}

		if ( input != ValueForKey( entity, "nod_template_archives" ) ) {
			error = "Archive order differs from existing instances. Use a new map to change it.";
			break;
		}
	}

	if ( error.empty() && NodClasses_Load( input.c_str(), error ) ) {
		gtk_entry_set_text( GTK_ENTRY( nodArchives ), NodClasses_Context() );
		if ( world_entity && previous != NodClasses_Context() ) {
			SetKeyValue( world_entity, "nod_template_archives", NodClasses_Context() );
			Sys_MarkMapModified();
		}

		NodInspector_ReloadMetadata( NULL, NULL );
		return;
	}

	Sys_FPrintf( SYS_WRN, "NOT archive context: %s\n", error.c_str() );
	gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "NOT context not loaded" ), MB_OK );
}

static void NodInspector_ParentCell( GtkTreeViewColumn *, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer ){
	eclass_t *type = NULL;
	gtk_tree_model_get( model, iter, 1, &type, -1 );
	const std::string parent = type ? NodClass_Field( type->comments, "parent" ) : "";
	g_object_set( renderer, "text", parent.c_str(), NULL );
}

static void NodInspector_PickModel( GtkWidget *, gpointer ){
	const _QERNodModelTable *api = NodModel_Resources();
	if ( !api || !gtk_editable_get_editable( GTK_EDITABLE( EntWidgets[EntValueField] ) ) ||
		strcmp( gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) ), "nod_prop_4" ) != 0 ) {
		gtk_MessageBox( g_pGroupDlg->m_pWidget, _( "Select an editable native model property and use the current model plugin." ),
			_( "Model browsing unavailable" ), MB_OK );
		return;
	}

	const std::string bundle = world_entity ? ValueForKey( world_entity, "nod_model_bundle" ) : "";
	const std::vector<entity_t *> selection = NodInspector_Selection();
	NodModelPicker picker( *api, bundle, gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntValueField] ) ) );
	GtkWidget *window = picker.Create( g_pGroupDlg->m_pWidget );
	std::string error;
	if ( !picker.Load( error ) ) {
		Sys_FPrintf( SYS_WRN, "NOD model catalog: %s\n", error.c_str() );
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "Model catalog unavailable" ), MB_OK );
	}

	gtk_widget_show_all( window );
	for ( ;; ) {
		const int response = gtk_dialog_run( GTK_DIALOG( window ) );
		if ( response != 1 && response != GTK_RESPONSE_ACCEPT ) {
			break;
		}

		if ( response == 1 ) {
			if ( picker.Load( error ) ) {
				continue;
		}
		} else {
			std::string reference;
			if ( picker.Check( reference, error ) ) {
				if ( selection != NodInspector_Selection() || bundle != ( world_entity ? ValueForKey( world_entity, "nod_model_bundle" ) : "" ) ||
					strcmp( gtk_entry_get_text( GTK_ENTRY( EntWidgets[EntKeyField] ) ), "nod_prop_4" ) != 0 ) {
					error = _( "The selection or model context changed. Close and reopen the model picker." );
				} else {
					gtk_entry_set_text( GTK_ENTRY( EntWidgets[EntValueField] ), reference.c_str() );
					break;
				}
			}
		}

		Sys_FPrintf( SYS_WRN, "NOD model selection: %s\n", error.c_str() );
		gtk_MessageBox( window, error.c_str(), _( "Model reference not selected" ), MB_OK );
	}

	gtk_widget_destroy( window );
}

static NodEnvironmentValues NodInspector_EnvironmentValues(){
	NodEnvironmentValues values;
	for ( const auto &field : NodEnvironment_Fields() ) {
		if ( world_entity && NodInspector_HasKey( world_entity, field.key ) ) {
			values[field.key] = ValueForKey( world_entity, field.key );
		}
	}

	return values;
}

static void NodInspector_Environment( GtkWidget *, gpointer ){
	if ( !world_entity || NodInspector_HasKey( world_entity, "nod_reference" ) ||
		NodInspector_HasKey( world_entity, "nod_id" ) ) {
		const char *error = _( "Open an authoring MAP to edit its scene environment. Source-bound previews remain read-only." );
		Sys_FPrintf( SYS_WRN, "NOD environment: %s\n", error );
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error, _( "Environment not changed" ), MB_OK );
		return;
	}

	entity_t *world = world_entity;
	const NodEnvironmentValues original = NodInspector_EnvironmentValues();
	NodEnvironmentDialog ui;
	GtkWidget *dialog = ui.Create( g_pGroupDlg->m_pWidget, original );
	while ( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_ACCEPT ) {
		const NodEnvironmentValues values = ui.Read();
		std::string error = NodEnvironment_Validate( values );
		if ( world != world_entity || original != NodInspector_EnvironmentValues() ) {
			error = _( "The map or environment changed. Close and reopen this dialog." );
		}

		if ( !error.empty() ) {
			Sys_FPrintf( SYS_WRN, "NOD environment: %s\n", error.c_str() );
			gtk_MessageBox( dialog, error.c_str(), _( "Environment not changed" ), MB_OK );
			continue;
		}

		if ( values != original ) {
			Undo_Start( "change NOD scene environment" );
			Undo_AddEntity( world );
			for ( const auto &field : NodEnvironment_Fields() ) {
				const auto found = values.find( field.key );
				if ( found == values.end() ) {
					DeleteKey( world, field.key );
				}
				else {
					SetKeyValue( world, field.key, found->second.c_str() );
				}
			}

			Undo_EndEntity( world );
			Undo_End();
			Sys_MarkMapModified();
			SetKeyValuePairs();
			Sys_UpdateWindows( W_ALL );
		}

		break;
	}

	gtk_widget_destroy( dialog );
}

static void NodInspector_SetModelBundle( GtkWidget *, gpointer ){
	const std::string input = gtk_entry_get_text( GTK_ENTRY( nodModelBundle ) );
	std::string error;
	if ( !world_entity ) {
		error = "Open a map before selecting its model bundle.";
	} else if ( !NodModelReference_Bundle( input ) ) {
		error = "Enter an NMB filename from 3D/Models, or leave the field empty.";
	} else if ( !input.empty() && vfsGetFileCount( ( "3D/Models/" + input ).c_str(), 0 ) == 0 ) {
		error = "The selected NMB bundle is not available in the mounted assets.";
	}

	if ( !error.empty() ) {
		Sys_FPrintf( SYS_WRN, "NOT model bundle: %s\n", error.c_str() );
		gtk_MessageBox( g_pGroupDlg->m_pWidget, error.c_str(), _( "Model bundle not changed" ), MB_OK ); return;
	}

	if ( input == ValueForKey( world_entity, "nod_model_bundle" ) ) {
		return;
	}

	Undo_Start( "change NOD model bundle" );
	Undo_AddEntity( world_entity );
	if ( input.empty() ) {
		DeleteKey( world_entity, "nod_model_bundle" );
	} else {
		SetKeyValue( world_entity, "nod_model_bundle", input.c_str() );
	}

	Undo_EndEntity( world_entity );
	Undo_End();
	Sys_MarkMapModified();
	SetKeyValuePairs();
	Sys_UpdateWindows( W_ALL );
}

void GroupDlg::Create(){
	GtkWidget *dialog, *content_area;
	GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;

	if ( m_pWidget != NULL ) {
		return;
	}

	dialog = gtk_dialog_new_with_buttons( _( "Entities/Entity View" ), NULL, flags, NULL );

	content_area = gtk_dialog_get_content_area( GTK_DIALOG( dialog ) );

#ifdef _WIN32
	if ( g_PrefsDlg.m_bStartOnPrimMon ) {
		PositionWindowOnPrimaryScreen( g_PrefsDlg.mWindowInfo.posEntityWnd );
	}
#endif
	load_window_pos( dialog, g_PrefsDlg.mWindowInfo.posEntityWnd );

	g_signal_connect( G_OBJECT( dialog ), "delete-event", G_CALLBACK( OnDeleteHide ), NULL );
	// catch 'Esc'
	g_signal_connect( G_OBJECT( dialog ), "key-press-event", G_CALLBACK( OnDialogKey ), NULL );

	gtk_window_set_transient_for( GTK_WINDOW( dialog ), GTK_WINDOW( g_pParentWnd->m_pWidget ) );
	g_qeglobals_gui.d_entity = dialog;

	{
		GtkWidget* notebook = gtk_notebook_new();
		gtk_container_add( GTK_CONTAINER( content_area ), notebook );
		gtk_notebook_set_tab_pos( GTK_NOTEBOOK( notebook ), GTK_POS_BOTTOM );
		gtk_widget_show( notebook );
		m_pNotebook = notebook;

		{
			GtkWidget* vbox = gtk_vbox_new( FALSE, 2 );
			GtkWidget *nodActions = NULL;
			GtkWidget *entityPage = vbox;
			gtk_container_set_border_width( GTK_CONTAINER( vbox ), 2 );
			gtk_widget_show( vbox );

			{
				GtkWidget* label = gtk_label_new( _( "Entities" ) );
				gtk_notebook_append_page( GTK_NOTEBOOK( notebook ), vbox, label );
				gtk_widget_show( label );
			}

			if ( NodInspector_Enabled() ) {
				GtkWidget *selection;
				GtkWidget *pages = NodInspectorView_Pages( selection, nodPlacementPage );
				gtk_box_pack_start( GTK_BOX( entityPage ), pages, TRUE, TRUE, 0 );
				vbox = selection;
				GtkWidget *resources = gtk_vbox_new( FALSE, 5 );
				GtkWidget *section = NodInspectorView_Section( _( "Map resources and metadata" ), resources );
				gtk_box_pack_end( GTK_BOX( entityPage ), section, FALSE, FALSE, 0 );
				GtkWidget *archives = gtk_hbox_new( FALSE, 5 );
				gtk_box_pack_start( GTK_BOX( resources ), archives, FALSE, FALSE, 0 );
				gtk_box_pack_start( GTK_BOX( archives ), gtk_label_new( _( "NOT archives" ) ), FALSE, FALSE, 0 );
				nodArchives = gtk_entry_new();
				gtk_entry_set_text( GTK_ENTRY( nodArchives ), NodClasses_Context() );
				gtk_widget_set_tooltip_text( nodArchives, _( "Ordered archive names, separated by ';'. Global is implicit." ) );
				gtk_box_pack_start( GTK_BOX( archives ), nodArchives, TRUE, TRUE, 0 );
				GtkWidget *load = gtk_button_new_with_label( _( "Load" ) );
				gtk_box_pack_start( GTK_BOX( archives ), load, FALSE, FALSE, 0 );
				g_signal_connect( G_OBJECT( load ), "clicked", G_CALLBACK( NodInspector_LoadArchives ), NULL );
				gtk_widget_show_all( archives );
				GtkWidget *bundle = gtk_hbox_new( FALSE, 5 );
				gtk_box_pack_start( GTK_BOX( resources ), bundle, FALSE, FALSE, 0 );
				gtk_box_pack_start( GTK_BOX( bundle ), gtk_label_new( _( "Map model bundle" ) ), FALSE, FALSE, 0 );
				nodModelBundle = gtk_entry_new();
				nodDisplayedBundle = world_entity ? ValueForKey( world_entity, "nod_model_bundle" ) : "";
				gtk_entry_set_text( GTK_ENTRY( nodModelBundle ), nodDisplayedBundle.c_str() );
				gtk_entry_set_max_length( GTK_ENTRY( nodModelBundle ), 255 );
				gtk_widget_set_tooltip_text( nodModelBundle, _( "NMB filename in 3D/Models. Blank: standalone models only. Saved per map." ) );
				gtk_box_pack_start( GTK_BOX( bundle ), nodModelBundle, TRUE, TRUE, 0 );
				GtkWidget *setBundle = gtk_button_new_with_label( _( "Apply" ) );
				gtk_box_pack_start( GTK_BOX( bundle ), setBundle, FALSE, FALSE, 0 );
				g_signal_connect( G_OBJECT( setBundle ), "clicked", G_CALLBACK( NodInspector_SetModelBundle ), NULL );
				gtk_widget_show_all( bundle );
				GtkWidget *environment = gtk_button_new_with_label( _( "Scene environment..." ) );
				gtk_box_pack_start( GTK_BOX( resources ), environment, FALSE, FALSE, 0 );
				g_signal_connect( G_OBJECT( environment ), "clicked", G_CALLBACK( NodInspector_Environment ), NULL );
				GtkWidget *reload = gtk_button_new_with_label( _( "Reload NOT metadata" ) );
				gtk_box_pack_start( GTK_BOX( resources ), reload, FALSE, FALSE, 0 );
				g_signal_connect( G_OBJECT( reload ), "clicked", G_CALLBACK( NodInspector_ReloadMetadata ), NULL );
				nodSelectionDetails = gtk_label_new( "" );
				gtk_label_set_line_wrap( GTK_LABEL( nodSelectionDetails ), TRUE );
				gtk_label_set_line_wrap_mode( GTK_LABEL( nodSelectionDetails ), PANGO_WRAP_WORD_CHAR );
				gtk_label_set_selectable( GTK_LABEL( nodSelectionDetails ), TRUE );
				gtk_misc_set_alignment( GTK_MISC( nodSelectionDetails ), 0, 0.5 );
				gtk_box_pack_start( GTK_BOX( resources ), nodSelectionDetails, FALSE, FALSE, 0 );
				gtk_widget_show_all( section );
				GtkWidget *row = gtk_hbox_new( FALSE, 5 );
				gtk_box_pack_start( GTK_BOX( nodPlacementPage ), row, FALSE, FALSE, 0 );
				gtk_box_pack_start( GTK_BOX( row ), gtk_label_new( _( "Find template" ) ), FALSE, FALSE, 0 );
				nodTypeSearch = gtk_entry_new();
				gtk_box_pack_start( GTK_BOX( row ), nodTypeSearch, TRUE, TRUE, 0 );
				gtk_widget_set_tooltip_text( nodTypeSearch, _( "Search registered NOT templates, parent names and property descriptions." ) );
				g_signal_connect( G_OBJECT( nodTypeSearch ), "changed", G_CALLBACK( NodInspector_TypeSearchChanged ), NULL );
				gtk_widget_show_all( row );
				GtkWidget *placement = gtk_hbox_new( FALSE, 5 );
				gtk_box_pack_start( GTK_BOX( nodPlacementPage ), placement, FALSE, FALSE, 0 );
				nodPlacementLabel = gtk_label_new( _( "Choose a template to place." ) );
				gtk_label_set_line_wrap( GTK_LABEL( nodPlacementLabel ), TRUE );
				gtk_label_set_line_wrap_mode( GTK_LABEL( nodPlacementLabel ), PANGO_WRAP_WORD_CHAR );
				gtk_misc_set_alignment( GTK_MISC( nodPlacementLabel ), 0, 0.5 );
				gtk_box_pack_start( GTK_BOX( placement ), nodPlacementLabel, TRUE, TRUE, 0 );
				nodPlace = gtk_button_new_with_label( _( "Place entity" ) );
				gtk_widget_set_sensitive( nodPlace, FALSE );
				gtk_box_pack_start( GTK_BOX( placement ), nodPlace, FALSE, FALSE, 0 );
				g_signal_connect( G_OBJECT( nodPlace ), "clicked", G_CALLBACK( NodInspector_Place ), NULL );
				gtk_widget_show_all( placement );
				nodBrowsedShared = NodInspector_SharedSection( nodPlacementPage, true );
				nodSelectedShared = NodInspector_SharedSection( vbox, false );
				nodSelectionLabel = gtk_label_new( _( "Select an entity to inspect its local properties." ) );
				gtk_label_set_line_wrap( GTK_LABEL( nodSelectionLabel ), TRUE );
				gtk_misc_set_alignment( GTK_MISC( nodSelectionLabel ), 0, 0.5 );
				gtk_box_pack_start( GTK_BOX( vbox ), nodSelectionLabel, FALSE, FALSE, 0 );
				gtk_widget_show( nodSelectionLabel );
			}

			{
				GtkWidget* split1 = NodInspector_Enabled() ? NULL : gtk_vpaned_new();
				if ( split1 ) {
					gtk_box_pack_start( GTK_BOX( vbox ), split1, TRUE, TRUE, 0 );
					gtk_widget_show( split1 );
				}

				{
					GtkWidget* split2 = split1 ? gtk_vpaned_new() : NULL;
					if ( split2 ) {
						gtk_paned_pack1( GTK_PANED( split1 ), split2, TRUE, FALSE );
						gtk_widget_show( split2 );
						g_object_set_data( G_OBJECT( dialog ), "split1", split1 );
						g_object_set_data( G_OBJECT( dialog ), "split2", split2 );
					}

					{
						GtkWidget* vbox2 = gtk_vbox_new( FALSE, 2 );
						if ( split1 ) {
							gtk_paned_pack2( GTK_PANED( split1 ), vbox2, TRUE, FALSE );
						} else {
							gtk_box_pack_start( GTK_BOX( vbox ), vbox2, TRUE, TRUE, 0 );
						}

						gtk_widget_show( vbox2 );

						{
							GtkWidget* scr = gtk_scrolled_window_new( NULL, NULL );
							if ( split2 ) {
								gtk_paned_pack1( GTK_PANED( split2 ), scr, TRUE, FALSE );
							} else {
								gtk_box_pack_start( GTK_BOX( nodPlacementPage ), scr, TRUE, TRUE, 0 );
							}

							gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
							gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );
							gtk_widget_show( scr );

							{
								GtkListStore* store = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_POINTER );

								GtkWidget* view = gtk_tree_view_new_with_model( GTK_TREE_MODEL( store ) );
								gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( view ), NodInspector_Enabled() );
								if ( NodInspector_Enabled() ) {
									gtk_widget_set_tooltip_text( view,
									_( "Place with Enter, double-click or Place entity. Browsing keeps map selection." ) );
								}

								g_signal_connect( G_OBJECT( view ), "button-press-event", G_CALLBACK( eclasslist_button_press ), NULL );
								g_signal_connect( G_OBJECT( view ), "key-press-event", G_CALLBACK( eclasslist_keypress ), this );

								{
									GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
									GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes( NodInspector_Enabled() ? _( "Template" ) : "Key", renderer, "text", 0, (char *) NULL );
									gtk_tree_view_append_column( GTK_TREE_VIEW( view ), column );
								}

								if ( NodInspector_Enabled() ) {
									GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
									GtkTreeViewColumn *column = gtk_tree_view_column_new();
									gtk_tree_view_column_set_title( column, _( "First base (display only)" ) );
									gtk_tree_view_column_pack_start( column, renderer, TRUE );
									gtk_tree_view_column_set_cell_data_func( column, renderer, NodInspector_ParentCell, NULL, NULL );
									gtk_tree_view_column_set_visible( column, FALSE );
									gtk_tree_view_append_column( GTK_TREE_VIEW( view ), column );
								}

								{
									GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );
									g_signal_connect( G_OBJECT( selection ), "changed", G_CALLBACK( eclasslist_selection_changed ), dialog );
								}

								gtk_container_add( GTK_CONTAINER( scr ), view );

								gtk_widget_show( view );

								g_object_unref( G_OBJECT( store ) );
								EntWidgets[EntList] = view;
							}
						}

						{
							GtkWidget* scr = gtk_scrolled_window_new( NULL, NULL );
							if ( NodInspector_Enabled() ) {
								gtk_widget_set_size_request( scr, 0, 110 );
								GtkWidget *section = NodInspectorView_Section( _( "Template details (technical)" ), scr );
								gtk_box_pack_start( GTK_BOX( nodPlacementPage ), section, FALSE, FALSE, 0 );
								gtk_widget_show( section );
							}
							else {
								gtk_paned_pack2( GTK_PANED( split2 ), scr, TRUE, FALSE );
							}

							gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
							gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );
							gtk_widget_show( scr );

							{
								GtkWidget* text = gtk_text_view_new();
								gtk_widget_set_size_request( text, 0, -1 ); // allow shrinking
								gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( text ), GTK_WRAP_WORD );
								gtk_text_view_set_editable( GTK_TEXT_VIEW( text ), FALSE );
								gtk_container_add( GTK_CONTAINER( scr ), text );
								gtk_widget_show( text );
								EntWidgets[EntComment] = text;
							}
						}

						{
							// Spawnflags (4 colums wide max, or window gets too wide.)
							LayoutTable = gtk_table_new( 4, 4, FALSE );
							gtk_box_pack_start( GTK_BOX( vbox2 ), LayoutTable, FALSE, TRUE, 0 );
							gtk_widget_show( LayoutTable );

							for ( int i = 0; i < MAX_FLAGS; i++ )
							{
								GtkWidget* check = gtk_check_button_new_with_label( "" );
								g_object_ref( check );
								g_signal_connect( G_OBJECT( check ), "toggled", G_CALLBACK( entity_check ), NULL );
								EntWidgets[EntCheck1 + i] = check;
							}

						}

						if ( NodInspector_Enabled() ) {
							nodLegacySource = gtk_button_new_with_label( _( "Bind legacy NSD source (diagnostics)..." ) );
							gtk_widget_set_no_show_all( nodLegacySource, TRUE );
							gtk_box_pack_start( GTK_BOX( vbox2 ), nodLegacySource, FALSE, FALSE, 0 );
							g_signal_connect( G_OBJECT( nodLegacySource ), "clicked", G_CALLBACK( NodInspector_BindSource ), NULL );
							GtkWidget *views = gtk_combo_box_text_new();
							gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( views ), _( "Properties (local)" ) );
							gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( views ), _( "Effective Properties" ) );
							gtk_combo_box_set_active( GTK_COMBO_BOX( views ), 1 );
							g_signal_connect( G_OBJECT( views ), "changed", G_CALLBACK( NodInspector_PropertyViewChanged ), NULL );
							gtk_box_pack_start( GTK_BOX( vbox2 ), views, FALSE, FALSE, 0 );
							gtk_widget_show( views );
							GtkWidget *row = gtk_hbox_new( FALSE, 5 );
							gtk_box_pack_start( GTK_BOX( vbox2 ), row, FALSE, FALSE, 0 );
							gtk_box_pack_start( GTK_BOX( row ), gtk_label_new( _( "Find property" ) ), FALSE, FALSE, 0 );
							nodPropertySearch = gtk_entry_new();
							gtk_box_pack_start( GTK_BOX( row ), nodPropertySearch, TRUE, TRUE, 0 );
							g_signal_connect( G_OBJECT( nodPropertySearch ), "changed", G_CALLBACK( NodInspector_PropertySearchChanged ), NULL );
							gtk_widget_show_all( row );
							nodTechnical = gtk_check_button_new_with_label( _( "Show technical details" ) );
							gtk_widget_set_tooltip_text( nodTechnical, _( "Show native keys, payloads and sources. Protected values stay read-only." ) );
							g_signal_connect( G_OBJECT( nodTechnical ), "toggled", G_CALLBACK( NodInspector_TechnicalChanged ), NULL );
							gtk_box_pack_start( GTK_BOX( vbox2 ), nodTechnical, FALSE, FALSE, 0 );
							gtk_widget_show( nodTechnical );
						}

						{
							GtkWidget* scr = gtk_scrolled_window_new( NULL, NULL );
							gtk_box_pack_start( GTK_BOX( vbox2 ), scr, TRUE, TRUE, 0 );
							gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
							gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );
							gtk_widget_show( scr );

							{
								GtkListStore* store = NodInspector_Enabled()
									? gtk_list_store_new( 5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING )
									: gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_STRING );

								GtkWidget* view = gtk_tree_view_new_with_model( GTK_TREE_MODEL( store ) );
								gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( view ), NodInspector_Enabled() );

								if ( !NodInspector_Enabled() ) {
									GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
									GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes( _( "Key" ), renderer, "text", 0, (char *) NULL );
									gtk_tree_view_append_column( GTK_TREE_VIEW( view ), column );
								}

								if ( !NodInspector_Enabled() ) {
									GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
									GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes( _( "Value (first selected)" ), renderer, "text", 1, (char *) NULL );
									gtk_tree_view_append_column( GTK_TREE_VIEW( view ), column );
								}

								if ( NodInspector_Enabled() ) {
									NodInspectorView_Columns( GTK_TREE_VIEW( view ) );
								}

								{
									GtkTreeSelection* selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );
									g_signal_connect( G_OBJECT( selection ), "changed", G_CALLBACK( proplist_selection_changed ), dialog );
									g_signal_connect( G_OBJECT( view ), "realize", G_CALLBACK( proplist_view_realize ), dialog );
								}

								gtk_container_add( GTK_CONTAINER( scr ), view );

								gtk_widget_show( view );
								g_object_unref( G_OBJECT( store ) );

								EntWidgets[EntProps] = view;
							}
						}
					}

					int x = g_PrefsDlg.mWindowInfo.nEntitySplit1;
					if ( split1 && x != -1 ) {
						gtk_paned_set_position( GTK_PANED( split1 ), x );

						while ( gtk_events_pending() ) {
							gtk_main_iteration();
						}

						x = g_PrefsDlg.mWindowInfo.nEntitySplit2;

						if ( x != -1 ) {
							gtk_paned_set_position( GTK_PANED( split2 ), x );
						}
					}
				}
			}

			{
				if ( NodInspector_Enabled() ) {
					nodPropertyDetails = gtk_label_new( _( "Select a property for source and edit access." ) );
					gtk_label_set_line_wrap( GTK_LABEL( nodPropertyDetails ), TRUE );
					gtk_label_set_line_wrap_mode( GTK_LABEL( nodPropertyDetails ), PANGO_WRAP_WORD_CHAR );
					gtk_label_set_selectable( GTK_LABEL( nodPropertyDetails ), TRUE );
					gtk_misc_set_alignment( GTK_MISC( nodPropertyDetails ), 0, 0.5 );
					GtkWidget *section = NodInspectorView_Section( _( "Property details" ), nodPropertyDetails );
					gtk_box_pack_start( GTK_BOX( vbox ), section, FALSE, FALSE, 0 );
					gtk_widget_show_all( section );
				}

				GtkWidget* table = gtk_table_new( 2, 2, FALSE );
				gtk_box_pack_start( GTK_BOX( vbox ), table, FALSE, TRUE, 0 );
				gtk_table_set_row_spacings( GTK_TABLE( table ), 3 );
				gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
				gtk_widget_show( table );

				{
					GtkWidget* entry = gtk_entry_new();
					gtk_table_attach( GTK_TABLE( table ), entry, 1, 2, 0, 1,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_widget_show( entry );
					gtk_widget_set_events( entry, GDK_KEY_PRESS_MASK );
					g_signal_connect( G_OBJECT( entry ), "key-press-event",
										G_CALLBACK( entityentry_keypress ), this );
					EntWidgets[EntKeyField] = entry;
					if ( NodInspector_Enabled() ) {
						g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( NodInspector_UpdateFields ), NULL );
						g_signal_connect( G_OBJECT( nodValueEditor.resource ), "clicked", G_CALLBACK( NodInspector_PickModel ), NULL );
						gtk_widget_set_no_show_all( entry, TRUE );
						gtk_widget_hide( entry );
						nodPropertyName = gtk_label_new( _( "Select a property" ) );
						gtk_misc_set_alignment( GTK_MISC( nodPropertyName ), 0, 0.5 );
						gtk_label_set_ellipsize( GTK_LABEL( nodPropertyName ), PANGO_ELLIPSIZE_END );
						gtk_table_attach( GTK_TABLE( table ), nodPropertyName, 1, 2, 0, 1,
							(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ), (GtkAttachOptions)0, 0, 0 );
						gtk_widget_set_no_show_all( nodPropertyName, TRUE );
						gtk_widget_show( nodPropertyName );
					}
				}

				{
					GtkWidget* entry = gtk_entry_new();
					GtkWidget *value = NodInspector_Enabled() ? nodValueEditor.Create( entry ) : entry;
					gtk_table_attach( GTK_TABLE( table ), value, 1, 2, 1, 2,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_widget_show( value );
					gtk_widget_set_events( entry, GDK_KEY_PRESS_MASK );
					g_signal_connect( G_OBJECT( entry ), "key-press-event",
										G_CALLBACK( entityentry_keypress ), this );
					EntWidgets[EntValueField] = entry;
					gtk_widget_set_tooltip_text( entry, _( "Enter to apply. modelscale_vec takes priority. angles: pitch yaw roll (degrees)." ) );
					if ( NodInspector_Enabled() ) {
						g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( NodInspector_UpdateFields ), NULL );
						for ( int i = 0; i < 3; ++i ) {
							g_signal_connect( G_OBJECT( nodValueEditor.components[i] ), "activate", G_CALLBACK( AddProp ), NULL );
					}
				}
				}

				{
					GtkWidget* label = gtk_label_new( _( "Value" ) );
					gtk_table_attach( GTK_TABLE( table ), label, 0, 1, 1, 2,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_show( label );
				}

				{
					GtkWidget* label = gtk_label_new( NodInspector_Enabled() ? _( "Property" ) : _( "Key" ) );
					if ( NodInspector_Enabled() ) {
						nodKeyLabel = label;
					}

					gtk_table_attach( GTK_TABLE( table ), label, 0, 1, 0, 1,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
					gtk_widget_show( label );
				}
			}

			if ( NodInspector_Enabled() ) {
				GtkWidget *row = gtk_hbox_new( FALSE, 5 );
				nodActions = row;
				gtk_box_pack_start( GTK_BOX( vbox ), row, FALSE, FALSE, 0 );
				nodApply = gtk_button_new_with_label( _( "Apply" ) );
				gtk_box_pack_start( GTK_BOX( row ), nodApply, FALSE, FALSE, 0 );
				g_signal_connect( G_OBJECT( nodApply ), "clicked", G_CALLBACK( AddProp ), NULL );
				nodConfirm = gtk_check_button_new_with_label( _( "Confirm multi-edit" ) );
				gtk_widget_set_tooltip_text( nodConfirm, _( "Ask before applying a property to multiple selected entities." ) );
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( nodConfirm ), TRUE );
				gtk_box_pack_start( GTK_BOX( row ), nodConfirm, TRUE, TRUE, 0 );
				gtk_widget_show_all( row );
			}

			{
				GtkWidget* hbox = gtk_hbox_new( FALSE, 5 );
				if ( NodInspector_Enabled() ) {
					GtkWidget *section = NodInspectorView_Section( _( "More entity tools" ), hbox );
					gtk_box_pack_start( GTK_BOX( vbox ), section, FALSE, FALSE, 0 );
					gtk_widget_show( section );
				}
				else {
					gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, TRUE, 0 );
				}

				gtk_widget_show( hbox );

				{
					GtkWidget* table = gtk_table_new( 3, 3, TRUE );
					gtk_box_pack_start( GTK_BOX( hbox ), table, FALSE, TRUE, 0 );
					gtk_widget_show( table );

					{
						GtkWidget* button = gtk_button_new_with_label( _( "360" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 2, 3, 1, 2,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"360" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "45" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 2, 3, 0, 1,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"45" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "90" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 1, 2, 0, 1,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"90" );
					}


					{
						GtkWidget* button = gtk_button_new_with_label( _( "135" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 0, 1, 0, 1,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"135" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "180" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 0, 1, 1, 2,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"180" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "225" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 0, 1, 2, 3,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"225" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "270" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 1, 2, 2, 3,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"270" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "315" ) );
						gtk_table_attach( GTK_TABLE( table ), button, 2, 3, 2, 3,
										  (GtkAttachOptions) ( GTK_FILL ),
										  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"315" );
					}
				}

				{
					GtkWidget* vbox2 = gtk_vbox_new( FALSE, 0 );
					gtk_box_pack_start( GTK_BOX( hbox ), vbox2, TRUE, TRUE, 0 );
					gtk_widget_show( vbox2 );

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Reset" ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( ResetEntity ), NULL );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Up" ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"-1" );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Dn" ) );

						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( entitylist_angle ), (void *)"-2" );
					}
				}

				{
					GtkWidget* vbox2 = gtk_vbox_new( FALSE, 0 );
					gtk_box_pack_start( GTK_BOX( hbox ), vbox2, TRUE, TRUE, 0 );
					gtk_widget_show( vbox2 );

					{
						GtkWidget* button = gtk_button_new_with_label( NodInspector_Enabled() ? _( "Remove" ) : _( "Del Key/Pair" ) );
						gtk_box_pack_start( GTK_BOX( nodActions ? nodActions : vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( DelProp ), NULL );
						g_object_set_data( G_OBJECT( dialog ), "del_button", button );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Sound..." ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( AssignSound ), NULL );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Model..." ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( AssignModel ), NULL );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Scale..." ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						gtk_widget_set_tooltip_text( button, _( "X/Y/Z scale factors; models resize in place." ) );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( DoScaleDlg ), NULL );
					}

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Rotate..." ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						gtk_widget_set_tooltip_text( button, _( "Rotate models in place around world axes (degrees)." ) );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( DoRotateDlg ), NULL );
					}
				}

				{
					GtkWidget* vbox2 = gtk_vbox_new( FALSE, 0 );
					gtk_box_pack_start( GTK_BOX( hbox ), vbox2, TRUE, TRUE, 0 );
					gtk_widget_show( vbox2 );

					{
						GtkWidget* button = gtk_button_new_with_label( _( "Cam to angles" ) );
						gtk_box_pack_start( GTK_BOX( vbox2 ), button, FALSE, FALSE, 0 );
						gtk_widget_show( button );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( cam2angles ), NULL );

					}
				}
			}
		}

		if ( g_pParentWnd->FloatingGroupDialog() ) {
			{
				GtkWidget* scr = gtk_scrolled_window_new( NULL, NULL );
				gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
				gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );
				gtk_container_set_border_width( GTK_CONTAINER( scr ), 3 );
				gtk_widget_show( scr );

				{
					GtkWidget* text = gtk_text_view_new();
					gtk_widget_set_size_request( text, 0, -1 ); // allow shrinking
					gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( text ), GTK_WRAP_WORD );
					gtk_text_view_set_editable( GTK_TEXT_VIEW( text ), FALSE );
					gtk_container_add( GTK_CONTAINER( scr ), text );
					gtk_widget_show( text );
					g_qeglobals_gui.d_edit = text;
				}

				{
					GtkWidget* label = gtk_label_new( _( "Console" ) );
					gtk_widget_show( label );
					gtk_notebook_append_page( GTK_NOTEBOOK( notebook ), scr, label );
				}
			}
		}

		inspector_mode = W_ENTITY;
		m_pWidget = dialog;
		g_signal_connect( G_OBJECT( notebook ), "switch-page", G_CALLBACK( switch_page ), dialog );
	}
}
