/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodenvironmentdialog.h
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

#ifndef RADIANT_NODENVIRONMENTDIALOG_H
#define RADIANT_NODENVIRONMENTDIALOG_H

#include <gtk/gtk.h>
#include "nodenvironment.h"

/*
 */
struct NodEnvironmentDialog {
	
	/*
	*/
	std::array<GtkWidget *, 10> entries;
	NodEnvironmentValues original;

	/*
	*/
	GtkWidget *Create( GtkWidget *parent, const NodEnvironmentValues &values ){
		original = values;
		GtkWidget *dialog = gtk_dialog_new_with_buttons( _( "NOD scene environment" ), GTK_WINDOW( parent ),
			static_cast<GtkDialogFlags>( GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT, NULL );
		
		GtkWidget *box = gtk_vbox_new( FALSE, 6 );
		gtk_container_set_border_width( GTK_CONTAINER( box ), 10 );
		gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area( GTK_DIALOG( dialog ) ) ), box, TRUE, TRUE, 0 );
		
		GtkWidget *note = gtk_label_new( _( "Saved in worldspawn; exported by MAPtoNSD (updated SDK required).\nBlank removes a setting. Hover over each field for its native limits.\nReferences do not deploy assets or guarantee runtime playback/rendering." ) );
		gtk_misc_set_alignment( GTK_MISC( note ), 0, 0.5 );
		gtk_box_pack_start( GTK_BOX( box ), note, FALSE, FALSE, 0 );
		
		const auto &fields = NodEnvironment_Fields();
		for ( size_t i = 0; i != fields.size(); ++i ) {
			GtkWidget *row = gtk_hbox_new( FALSE, 8 );
			gtk_box_pack_start( GTK_BOX( box ), row, FALSE, FALSE, 0 );
		
			GtkWidget *label = gtk_label_new( _( fields[i].name ) );
			gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			gtk_widget_set_size_request( label, 130, -1 );
			gtk_box_pack_start( GTK_BOX( row ), label, FALSE, FALSE, 0 );
		
			entries[i] = gtk_entry_new();
			gtk_entry_set_width_chars( GTK_ENTRY( entries[i] ), 36 );
		
			const auto found = values.find( fields[i].key );
			gtk_entry_set_text( GTK_ENTRY( entries[i] ), found == values.end() ? "" : found->second.c_str() );
			gtk_widget_set_tooltip_text( entries[i], _( fields[i].hint ) );
			gtk_box_pack_start( GTK_BOX( row ), entries[i], TRUE, TRUE, 0 );
		}

		gtk_widget_show_all( dialog );
		return dialog;
	}

	/*
	*/
	NodEnvironmentValues Read() const {

		NodEnvironmentValues values = original;
		const auto &fields = NodEnvironment_Fields();

		for ( size_t i = 0; i != fields.size(); ++i ) {
			const std::string value = gtk_entry_get_text( GTK_ENTRY( entries[i] ) );
			const auto found = original.find( fields[i].key );
			const bool emptyReference = NodEnvironment_IsReference( fields[i].key ) && value.empty();
			
			if ( found != original.end() && found->second == value && ( !value.empty() || emptyReference ) ) {
				continue;
			}

			if ( value.empty() ) {
				values.erase( fields[i].key );
			}
			else {
				values[fields[i].key] = value;
			}
		}

		return values;
	}
};

#endif
