/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodinspectorview.h
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

#ifndef RADIANT_NODINSPECTORVIEW_H
#define RADIANT_NODINSPECTORVIEW_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "nodtemplates.h"

inline void NodInspectorView_PropertyStyle( GtkTreeViewColumn *, GtkCellRenderer *renderer,
	GtkTreeModel *model, GtkTreeIter *iter, gpointer ){
	gchar *access = NULL;
	gtk_tree_model_get( model, iter, 3, &access, -1 );
	const bool inherited = access && g_str_has_prefix( access, "Inherited from " );
	const bool overridden = access && ( g_str_has_prefix( access, "Local override;" ) ||
		g_str_has_prefix( access, "Local NSD payload" ) );
	g_object_set( renderer, "style", inherited ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
		"weight", overridden ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL );
	g_free( access );
}

inline void NodInspectorView_Technical( GtkTreeView *view, bool technical ){
	for ( int i = 2; i < 5; ++i ) {
		gtk_tree_view_column_set_visible( gtk_tree_view_get_column( view, i ), technical );
}
}

inline void NodInspectorView_Columns( GtkTreeView *view ){
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( gtk_tree_view_get_model( view ) ), 4, GTK_SORT_ASCENDING );
	const char *titles[] = { _( "Property" ), _( "Value" ), _( "Key" ), _( "Type / storage" ), _( "Access / source" ) };
	const int fields[] = { 4, 1, 0, 2, 3 };
	for ( size_t i = 0; i < G_N_ELEMENTS( fields ); ++i ) {
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		g_object_set( renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
		GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes( titles[i], renderer, "text", fields[i], (char *)NULL );
		gtk_tree_view_column_set_resizable( column, TRUE );
		gtk_tree_view_column_set_sizing( column, GTK_TREE_VIEW_COLUMN_FIXED );
		gtk_tree_view_column_set_fixed_width( column, i < 2 ? 170 : 180 );
		gtk_tree_view_column_set_expand( column, i < 2 );
		if ( i == 0 ) {
			gtk_tree_view_column_set_cell_data_func( column, renderer, NodInspectorView_PropertyStyle, NULL, NULL );
		}

		gtk_tree_view_append_column( view, column );
	}

	gtk_tree_view_set_tooltip_column( view, 3 );
	NodInspectorView_Technical( view, false );
}

inline GtkWidget *NodInspectorView_Section( const char *title, GtkWidget *content ){
	GtkWidget *section = gtk_expander_new( title );
	gtk_container_add( GTK_CONTAINER( section ), content );
	gtk_expander_set_expanded( GTK_EXPANDER( section ), FALSE );
	return section;
}

inline GtkWidget *NodInspectorView_Pages( GtkWidget *&selection, GtkWidget *&placement ){
	GtkWidget *pages = gtk_notebook_new();
	selection = gtk_vbox_new( FALSE, 5 );
	placement = gtk_vbox_new( FALSE, 5 );
	gtk_notebook_append_page( GTK_NOTEBOOK( pages ), selection, gtk_label_new( _( "Selection" ) ) );
	gtk_notebook_append_page( GTK_NOTEBOOK( pages ), placement, gtk_label_new( _( "Place entity" ) ) );
	gtk_widget_show_all( pages );
	gtk_notebook_set_current_page( GTK_NOTEBOOK( pages ), 0 );
	return pages;
}

inline gpointer NodInspectorView_SharedTarget( GtkTreeView *browser, gpointer selected, bool browsing ){
	if ( !browsing ) {
		return selected;
	}

	GtkTreeModel *model;
	GtkTreeIter iter;
	if ( !gtk_tree_selection_get_selected( gtk_tree_view_get_selection( browser ), &model, &iter ) ) {
		return NULL;
	}

	gpointer target = NULL;
	gtk_tree_model_get( model, &iter, 1, &target, -1 );
	return target;
}

struct NodInspectorView_Template {
	GtkWidget *properties, *value, *owner;
	std::string name, archive;
	std::vector<NodTemplateProperty> fields;

	static void Changed( GtkWidget *, gpointer data ){
		NodInspectorView_Template &view = *static_cast<NodInspectorView_Template *>( data );
		const int index = gtk_combo_box_get_active( GTK_COMBO_BOX( view.properties ) );
		const bool selected = index >= 0 && static_cast<size_t>( index ) < view.fields.size();
		gtk_entry_set_text( GTK_ENTRY( view.value ), selected ? view.fields[index].value.c_str() : "" );
		if ( !selected ) {
			gtk_label_set_text( GTK_LABEL( view.owner ), _( "No resolved properties." ) );
			return;
		}

		const NodTemplateProperty &property = view.fields[index];
		std::string text = std::string( _( "Owner: " ) ) + property.owner +
			_( "\nArchive: " ) + property.archive + _( "\nStorage: " ) + property.storage;
		if ( !property.problem.empty() ) {
			text += "\n" + property.problem;
		}

		text += _( "\nShared default; read-only." );
		gtk_label_set_text( GTK_LABEL( view.owner ), text.c_str() );
	}

	GtkWidget *Create( GtkWidget *parent, const std::string &templateName, const std::string &context ){
		GtkWidget *window = gtk_dialog_new_with_buttons( _( "Template defaults (read-only)" ),
			parent ? GTK_WINDOW( parent ) : NULL,
			static_cast<GtkDialogFlags>( GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT ),
			_( "Reload NOT" ), 1, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL );
		gtk_window_set_default_size( GTK_WINDOW( window ), 640, 260 );
		properties = gtk_combo_box_new_text();
		owner = gtk_label_new( "" );
		value = gtk_entry_new();
		gtk_editable_set_editable( GTK_EDITABLE( value ), FALSE );
		gtk_label_set_selectable( GTK_LABEL( owner ), TRUE );
		const std::string scope = std::string( _( "Template: " ) ) + templateName +
			_( "\nArchives (ordered): " ) + context +
			_( "\nEdit shared defaults in NOTEditor; instance overrides in Selection." );
		GtkWidget *label = gtk_label_new( scope.c_str() );
		GtkWidget *labels[] = { label, owner };
		for ( size_t i = 0; i < G_N_ELEMENTS( labels ); ++i ) {
			gtk_label_set_line_wrap( GTK_LABEL( labels[i] ), TRUE );
			gtk_label_set_line_wrap_mode( GTK_LABEL( labels[i] ), PANGO_WRAP_WORD_CHAR );
			gtk_misc_set_alignment( GTK_MISC( labels[i] ), 0, 0.5 );
		}

		GtkWidget *content = GTK_DIALOG( window )->vbox;
		gtk_container_set_border_width( GTK_CONTAINER( content ), 8 );
		GtkWidget *widgets[] = { label, properties, owner, value };
		for ( size_t i = 0; i < G_N_ELEMENTS( widgets ); ++i ) {
			gtk_box_pack_start( GTK_BOX( content ), widgets[i], FALSE, FALSE, 5 );
		}

		g_signal_connect( G_OBJECT( properties ), "changed", G_CALLBACK( Changed ), this );
		return window;
	}

	void Set( const NodTemplateChoice &choice ){
		const int old = gtk_combo_box_get_active( GTK_COMBO_BOX( properties ) );
		const std::string id = old >= 0 && static_cast<size_t>( old ) < fields.size() ? fields[old].id : "";
		fields.clear();
		gtk_list_store_clear( GTK_LIST_STORE( gtk_combo_box_get_model( GTK_COMBO_BOX( properties ) ) ) );
		name = choice.name; archive = choice.archive;
		fields = choice.effective;
		int selected = 0;
		for ( size_t i = 0; i < fields.size(); ++i ) {
			const NodTemplateProperty &field = fields[i];
			if ( field.id == id ) {
				selected = static_cast<int>( i );
			}

			const std::string label = NodProperty_DisplayName( "nod_prop_" + field.id, field.name ) +
				" (#" + field.id + ", " + field.storage + ")";
			gtk_combo_box_append_text( GTK_COMBO_BOX( properties ), label.c_str() );
		}

		gtk_combo_box_set_active( GTK_COMBO_BOX( properties ), fields.empty() ? -1 : selected );
		Changed( NULL, this );
	}

	void Unavailable( const std::string &error ){
		fields.clear();
		gtk_list_store_clear( GTK_LIST_STORE( gtk_combo_box_get_model( GTK_COMBO_BOX( properties ) ) ) );
		gtk_entry_set_text( GTK_ENTRY( value ), "" );
		gtk_label_set_text( GTK_LABEL( owner ), error.c_str() );
	}
};

struct NodInspectorView_ValueEditor {
	GtkWidget *entry, *components[3], *vector, *hint, *resource;
	NodPropertyInfo info;
	std::string key, restriction;
	std::string draftKey, draftValue, draftComponents[3];
	bool updating, technical;

	NodInspectorView_ValueEditor() : entry( NULL ), vector( NULL ), hint( NULL ), resource( NULL ), updating( false ), technical( false ) {
		for ( int i = 0; i < 3; ++i ) {
			components[i] = NULL;
	}
	}

	bool IsVector() const { return info.storage == "vector3"; }
	const char *Error() const {
		if ( info.storage != "vector3" && info.storage != "float32" &&
			info.storage != "uint32 bits" && info.storage != "string32" ) {
			return NULL;
		}

		if ( IsVector() && gtk_widget_get_visible( vector ) ) {
			NodPropertyInfo scalar;
			scalar.storage = "float32";
			for ( int i = 0; i < 3; ++i ) {
				if ( NodProperty_Validate( "", gtk_entry_get_text( GTK_ENTRY( components[i] ) ), scalar ) ) {
					return "Each component needs one finite float32 value.";
		}
			}
		}

		return NodProperty_Validate( key.c_str(), gtk_entry_get_text( GTK_ENTRY( entry ) ), info );
	}

	void Status(){
		std::string text;
		if ( info.storage == "vector3" ) {
			text = key == "angles" ? _( "Pitch / yaw / roll (degrees)." ) :
			key == "nod_rotation" ? _( "X / Y / Z (radians)." ) : _( "X / Y / Z (finite float32)." );
		} else if ( info.storage == "float32" ) {
			text = _( "Finite float32; use '.' for decimals." );
		} else if ( info.storage == "uint32 bits" ) {
			text = _( "Integer: 0-4294967295." );
		} else if ( info.storage == "string32" ) {
			text = _( "Max 31 printable ASCII bytes; no quotes." );
		}

		if ( !restriction.empty() ) {
			text = restriction;
		} else if ( const char *error = Error() ) {
			text = error;
		}

		gtk_label_set_text( GTK_LABEL( hint ), text.c_str() );
	}

	void Synchronize(){
		if ( updating ) {
			return;
		}

		std::string parts[3];
		const char *value = gtk_entry_get_text( GTK_ENTRY( entry ) );
		const char *p = value;
		int count = 0;
		while ( *p && count < 3 ) {
			while ( g_ascii_isspace( *p ) ) {
				++p;
			}

			const char *start = p;
			while ( *p && !g_ascii_isspace( *p ) ) {
				++p;
			}

			if ( p != start ) {
				parts[count++] = std::string( start, p );
		}
		}
		while ( g_ascii_isspace( *p ) ) {
			++p;
		}

		const bool draft = !draftKey.empty() && key == draftKey && draftValue == value;
		const bool split = IsVector() && ( draft || ( count == 3 && !*p ) );
		// Whitespace and missing axes can make a component draft ambiguous in the canonical text.
		if ( draft ) {
			for ( int i = 0; i < 3; ++i ) {
				parts[i] = draftComponents[i];
			}
		}

		updating = true;
		if ( split ) {
			for ( int i = 0; i < 3; ++i ) {
				gtk_entry_set_text( GTK_ENTRY( components[i] ), parts[i].c_str() );
			}
		}

		updating = false;
		gtk_widget_set_visible( vector, split );
		gtk_widget_set_visible( entry, technical || !split );
		Status();
	}

	static void EntryChanged( GtkWidget *, gpointer data ){
		static_cast<NodInspectorView_ValueEditor *>( data )->Synchronize();
	}

	static void ComponentChanged( GtkWidget *, gpointer data ){
		NodInspectorView_ValueEditor &editor = *static_cast<NodInspectorView_ValueEditor *>( data );
		if ( editor.updating ) {
			return;
		}

		std::string value;
		for ( int i = 0; i < 3; ++i ) {
			if ( i ) {
				value += ' ';
			}

			editor.draftComponents[i] = gtk_entry_get_text( GTK_ENTRY( editor.components[i] ) );
			value += editor.draftComponents[i];
		}

		editor.draftKey = editor.key;
		editor.draftValue = value;
		editor.updating = true;
		gtk_entry_set_text( GTK_ENTRY( editor.entry ), value.c_str() );
		editor.updating = false;
		editor.Status();
	}

	GtkWidget *Create( GtkWidget *canonical ){
		entry = canonical;
		GtkWidget *box = gtk_vbox_new( FALSE, 3 );
		vector = gtk_hbox_new( FALSE, 3 );
		hint = gtk_label_new( "" );
		resource = gtk_button_new_with_label( _( "Choose model..." ) );
		gtk_widget_set_no_show_all( resource, TRUE );
		gtk_misc_set_alignment( GTK_MISC( hint ), 0, 0.5 );
		gtk_label_set_line_wrap( GTK_LABEL( hint ), TRUE );
		gtk_label_set_line_wrap_mode( GTK_LABEL( hint ), PANGO_WRAP_WORD_CHAR );
		gtk_box_pack_start( GTK_BOX( box ), entry, FALSE, FALSE, 0 );
		gtk_box_pack_start( GTK_BOX( box ), resource, FALSE, FALSE, 0 );
		gtk_box_pack_start( GTK_BOX( box ), vector, FALSE, FALSE, 0 );
		gtk_box_pack_start( GTK_BOX( box ), hint, FALSE, FALSE, 0 );
		const char *axes[] = { "X", "Y", "Z" };
		for ( int i = 0; i < 3; ++i ) {
			components[i] = gtk_entry_new();
			gtk_entry_set_width_chars( GTK_ENTRY( components[i] ), 6 );
			gtk_box_pack_start( GTK_BOX( vector ), gtk_label_new( axes[i] ), FALSE, FALSE, 0 );
			gtk_box_pack_start( GTK_BOX( vector ), components[i], TRUE, TRUE, 0 );
			g_signal_connect( G_OBJECT( components[i] ), "changed", G_CALLBACK( ComponentChanged ), this );
		}

		gtk_widget_show_all( box );
		gtk_widget_set_no_show_all( entry, TRUE );
		gtk_widget_set_no_show_all( vector, TRUE );
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( EntryChanged ), this );
		Synchronize();
		return box;
	}

	void Configure( const char *property, const std::string &storage ){
		const std::string type = strcmp( property, "origin" ) == 0 || strcmp( property, "angles" ) == 0 ||
			strcmp( property, "nod_rotation" ) == 0 ? "vector3" : strcmp( property, "nod_sector" ) == 0 ? "uint32 bits" : storage;
		if ( key == property && info.storage == type ) {
			return;
		}

		key = property; info.storage = type;
		gtk_widget_set_visible( resource, key == "nod_prop_4" && info.storage == "string32" );
		Synchronize();
	}

	void Editable( const char *reason ){
		restriction = reason ? reason : "";
		gtk_editable_set_editable( GTK_EDITABLE( entry ), reason == NULL );
		gtk_widget_set_tooltip_text( entry, reason );
		gtk_widget_set_sensitive( resource, reason == NULL );
		gtk_widget_set_tooltip_text( resource, reason );
		for ( int i = 0; i < 3; ++i ) {
			gtk_editable_set_editable( GTK_EDITABLE( components[i] ), reason == NULL );
			gtk_widget_set_tooltip_text( components[i], reason );
		}

		Status();
	}

	void Technical( bool enabled ){
		technical = enabled;
		gtk_widget_set_visible( entry, technical || !gtk_widget_get_visible( vector ) );
	}

	GtkWidget *FocusWidget() const { return gtk_widget_get_visible( vector ) ? components[0] : entry; }
};

#endif
