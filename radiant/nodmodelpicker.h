/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodmodelpicker.h
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

#ifndef RADIANT_NODMODELPICKER_H
#define RADIANT_NODMODELPICKER_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string>
#include <vector>
#include "inodmodel.h"
#include "nodmodelreference.h"
#include "nodproperties.h"

inline std::string NodModelPicker_Text( const std::string &text ){
	if ( g_utf8_validate( text.c_str(), -1, NULL ) ) {
		return text;
	}

	gchar *escaped = g_strescape( text.c_str(), NULL );
	const std::string result = escaped;
	g_free( escaped );
	return result;
}

inline std::vector<std::string> NodModelPicker_References( const std::string &reference ){
	NodPropertyInfo info;
	info.storage = "string32";
	std::vector<std::string> candidates;
	auto add = [&]( const std::string &name ){
		if ( !NodModelReference_Path( name ) || NodProperty_Validate( "nod_prop_4", name.c_str(), info ) ) {
			return;
		}

		for ( const std::string &candidate : candidates ) {
			if ( candidate == name ) {
				return;
			}
		}

		candidates.push_back( name );
	};
	const std::string basename = reference.substr( reference.find_last_of( '/' ) + 1 );
	for ( const std::string &name : { reference, basename } ) {
		add( name );
		if ( name.size() > 4 && ( g_ascii_strcasecmp( name.c_str() + name.size() - 4, ".nod" ) == 0 ||
			g_ascii_strcasecmp( name.c_str() + name.size() - 4, ".nmb" ) == 0 ) ) {
			add( name.substr( 0, name.size() - 4 ) );
		}
	}

	if ( candidates.empty() ) {
		candidates.push_back( reference );
	}

	return candidates;
}

inline std::string NodModelPicker_Reference( const std::string &reference ){
	return NodModelPicker_References( reference )[0];
}

struct NodModelPicker {
	struct Resource {
		std::string reference, asset;
		int frame;
	};
	const _QERNodModelTable &api;
	const std::string bundle;
	std::string wanted;
	std::string wantedProblem;
	std::vector<Resource> resources, wantedTarget;
	GtkWidget *window, *search, *view, *status;
	GtkListStore *store;

	NodModelPicker( const _QERNodModelTable &table, const std::string &mapBundle, const std::string &draft )
		: api( table ), bundle( mapBundle ), wanted( draft ), window( NULL ), search( NULL ),
		view( NULL ), status( NULL ), store( NULL ) {}

	static void Collect( void *data, const char *reference, const char *asset, int frame ){
		static_cast<std::vector<Resource> *>( data )->push_back( { reference, asset, frame } );
	}

	bool Selected( Resource &resource ) const {
		GtkTreeModel *model;
		GtkTreeIter iter;
		if ( !gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) ), &model, &iter ) ) {
			return false;
		}

		guint index;
		gtk_tree_model_get( model, &iter, 2, &index, -1 );
		if ( index >= resources.size() ) {
			return false;
		}

		resource = resources[index];
		return true;
	}

	bool Resolve( const std::string &reference, std::vector<Resource> &resolved, std::string &error ) const {
		error.clear();
		resolved.clear();
		GError *failure = NULL;
		const bool success = api.m_pfnCheck( bundle.c_str(), reference.c_str(), Collect, &resolved, &failure );
		if ( !success || failure ) {
			error = failure ? failure->message : _( "Model reference could not be resolved." );
			if ( failure ) {
				g_error_free( failure );
			}

			return false;
		}

		if ( resolved.size() != 1 ) {
			error = _( "Invalid model-plugin result." );
			return false;
		}

		return true;
	}

	bool Check( std::string &reference, std::string &error ){
		error.clear();
		Resource selected;
		if ( !Selected( selected ) ) { error = _( "Choose a model from the list." ); return false; }
		NodPropertyInfo info;
		info.storage = "string32";
		for ( const std::string &candidate : NodModelPicker_References( selected.reference ) ) {
			std::string problem;
			if ( const char *invalid = NodProperty_Validate( "nod_prop_4", candidate.c_str(), info ) ) {
				problem = invalid;
			} else if ( !NodModelReference_Path( candidate ) ) {
				problem = _( "Invalid native model path." );
			} else {
				std::vector<Resource> resolved;
				if ( Resolve( candidate, resolved, problem ) ) {
					if ( g_ascii_strcasecmp( selected.asset.c_str(), resolved[0].asset.c_str() ) == 0 &&
						( selected.frame < 0 || selected.frame == resolved[0].frame ) ) {
						reference = candidate; error.clear(); return true;
					}

					problem = _( "Reference selects a different asset/member in this map." );
				}
			}

			if ( error.empty() ) {
				error = problem;
		}
		}

		return false;
	}

	static void Changed( GtkTreeSelection *, gpointer data ){
		NodModelPicker &picker = *static_cast<NodModelPicker *>( data );
		std::string reference, error;
		const bool valid = picker.Check( reference, error );
		gtk_dialog_set_response_sensitive( GTK_DIALOG( picker.window ), GTK_RESPONSE_ACCEPT, valid );
		Resource selected;
		if ( valid && picker.Selected( selected ) ) {
			picker.wanted = reference;
			picker.wantedTarget.assign( 1, selected );
			picker.wantedProblem.clear();
			GtkTreeModel *model;
			GtkTreeIter iter;
			if ( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW( picker.view ) ), &model, &iter ) ) {
				gchar *current;
				gtk_tree_model_get( model, &iter, 0, &current, -1 );
				if ( reference != current ) {
					gtk_list_store_set( picker.store, &iter, 0, reference.c_str(), -1 );
				}

				g_free( current );
			}

			error = std::string( _( "Reference: " ) ) + reference + _( "\nAsset: " ) + selected.asset +
				( selected.frame >= 0 ? _( "\nFrame: " ) + std::to_string( selected.frame ) : "" ) +
				_( "\nUse reference sets draft; Apply in Selection commits." );
		}
		else if ( !picker.Selected( selected ) && !picker.wantedProblem.empty() ) {
			error = picker.wantedProblem;
		}

		gtk_label_set_text( GTK_LABEL( picker.status ), NodModelPicker_Text( error ).c_str() );
	}

	static void SearchChanged( GtkEntry *, gpointer data ){ static_cast<NodModelPicker *>( data )->Fill(); }
	static void Activated( GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, gpointer data ){
		NodModelPicker &picker = *static_cast<NodModelPicker *>( data );
		if ( gtk_widget_get_sensitive( gtk_dialog_get_widget_for_response( GTK_DIALOG( picker.window ), GTK_RESPONSE_ACCEPT ) ) ) {
			gtk_dialog_response( GTK_DIALOG( picker.window ), GTK_RESPONSE_ACCEPT );
	}
	}

	void Fill(){
		gtk_list_store_clear( store );
		gchar *needle = g_utf8_casefold( gtk_entry_get_text( GTK_ENTRY( search ) ), -1 );
		for ( size_t i = 0; i < resources.size(); ++i ) {
			const Resource &resource = resources[i];
			const std::string reference = NodModelPicker_Reference( resource.reference );
			const std::string source = resource.asset + ( resource.frame >= 0 ? " #" + std::to_string( resource.frame ) : "" );
			const std::string text = NodModelPicker_Text( reference + "\n" + resource.reference + "\n" + source );
			gchar *haystack = g_utf8_casefold( text.c_str(), -1 );
			const bool matches = strstr( haystack, needle ) != NULL;
			g_free( haystack );
			if ( !matches ) {
				continue;
			}

			GtkTreeIter iter;
			gtk_list_store_append( store, &iter );
			gtk_list_store_set( store, &iter, 0, NodModelPicker_Text( reference ).c_str(),
				1, NodModelPicker_Text( source ).c_str(), 2, static_cast<guint>( i ), -1 );
			if ( wantedTarget.size() == 1 && g_ascii_strcasecmp( wantedTarget[0].asset.c_str(), resource.asset.c_str() ) == 0 &&
				( resource.frame < 0 || resource.frame == wantedTarget[0].frame ) ) {
				gtk_tree_selection_select_iter( gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) ), &iter );
		}
		}

		g_free( needle );
		Changed( NULL, this );
		if ( gtk_tree_model_iter_n_children( GTK_TREE_MODEL( store ), NULL ) == 0 ) {
			gtk_label_set_text( GTK_LABEL( status ), resources.empty() ?
				_( "No models found. Check the asset root and map bundle." ) :
				_( "No matching models." ) );
	}
	}

	bool Load( std::string &error ){
		error.clear();
		std::vector<Resource> fresh;
		GError *failure = NULL;
		const bool success = api.m_pfnCatalog( bundle.c_str(), Collect, &fresh, &failure );
		resources.clear();
		wantedTarget.clear();
		wantedProblem.clear();
		if ( !success || failure ) {
			error = failure ? failure->message : _( "Cannot list model resources." );
			if ( failure ) {
				g_error_free( failure );
			}

			Fill();
			gtk_label_set_text( GTK_LABEL( status ), NodModelPicker_Text( error ).c_str() );
			return false;
		}

		resources.swap( fresh );
		if ( !wanted.empty() ) {
			std::string problem;
			if ( !Resolve( wanted, wantedTarget, problem ) ) {
				wantedTarget.clear();
				wantedProblem = std::string( _( "Unresolved draft: " ) ) + problem;
			}
		}

		Fill();
		return true;
	}

	GtkWidget *Create( GtkWidget *parent ){
		window = gtk_dialog_new_with_buttons( _( "Choose instance model" ), parent ? GTK_WINDOW( parent ) : NULL,
			static_cast<GtkDialogFlags>( GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT ),
			_( "Refresh list" ), 1, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, _( "Use reference" ), GTK_RESPONSE_ACCEPT, NULL );
		gtk_window_set_default_size( GTK_WINDOW( window ), 700, 440 );
		GtkWidget *content = GTK_DIALOG( window )->vbox;
		gtk_container_set_border_width( GTK_CONTAINER( content ), 8 );
		const std::string scope = ( bundle.empty() ? std::string( _( "Bundle: none (standalone files)." ) ) :
			std::string( _( "Bundle: " ) ) + bundle ) +
			_( "\nModel reference: max 31 ASCII bytes." );
		GtkWidget *label = gtk_label_new( NodModelPicker_Text( scope ).c_str() );
		gtk_label_set_line_wrap( GTK_LABEL( label ), TRUE );
		gtk_label_set_line_wrap_mode( GTK_LABEL( label ), PANGO_WRAP_WORD_CHAR );
		gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
		gtk_box_pack_start( GTK_BOX( content ), label, FALSE, FALSE, 4 );
		search = gtk_entry_new();
		gtk_widget_set_tooltip_text( search, _( "Search reference, path or frame." ) );
		gtk_box_pack_start( GTK_BOX( content ), search, FALSE, FALSE, 4 );
		store = gtk_list_store_new( 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT );
		gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( store ), 0, GTK_SORT_ASCENDING );
		view = gtk_tree_view_new_with_model( GTK_TREE_MODEL( store ) );
		g_object_unref( store );
		const char *titles[] = { _( "Model reference" ), _( "Source / member" ) };
		for ( int i = 0; i < 2; ++i ) {
			GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
			g_object_set( renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
			GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes( titles[i], renderer, "text", i, (char *)NULL );
			gtk_tree_view_column_set_resizable( column, TRUE );
			gtk_tree_view_column_set_sizing( column, GTK_TREE_VIEW_COLUMN_FIXED );
			gtk_tree_view_column_set_fixed_width( column, i == 0 ? 220 : 400 );
			gtk_tree_view_append_column( GTK_TREE_VIEW( view ), column );
		}

		gtk_tree_view_set_tooltip_column( GTK_TREE_VIEW( view ), 1 );
		GtkWidget *scroll = gtk_scrolled_window_new( NULL, NULL );
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
		gtk_container_add( GTK_CONTAINER( scroll ), view );
		gtk_box_pack_start( GTK_BOX( content ), scroll, TRUE, TRUE, 4 );
		status = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( status ), 0, 0.5 );
		gtk_label_set_selectable( GTK_LABEL( status ), TRUE );
		gtk_label_set_line_wrap( GTK_LABEL( status ), TRUE );
		gtk_label_set_line_wrap_mode( GTK_LABEL( status ), PANGO_WRAP_WORD_CHAR );
		gtk_box_pack_start( GTK_BOX( content ), status, FALSE, FALSE, 4 );
		g_signal_connect( G_OBJECT( search ), "changed", G_CALLBACK( SearchChanged ), this );
		g_signal_connect( G_OBJECT( gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) ) ), "changed", G_CALLBACK( Changed ), this );
		g_signal_connect( G_OBJECT( view ), "row-activated", G_CALLBACK( Activated ), this );
		gtk_dialog_set_response_sensitive( GTK_DIALOG( window ), GTK_RESPONSE_ACCEPT, FALSE );
		return window;
	}
};

#endif
