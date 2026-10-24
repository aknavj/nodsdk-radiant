/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodexportdialog.h
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

#ifndef RADIANT_NODEXPORTDIALOG_H
#define RADIANT_NODEXPORTDIALOG_H

#include "nodcompile.h"

struct NodExportColor {
	GtkWidget *rgb[3], *button;
	static void ToButton( GtkSpinButton *, gpointer data ){
		NodExportColor &field = *static_cast<NodExportColor *>( data );
		GdkColor color;
		color.pixel = 0;
		color.red = static_cast<guint16>( gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( field.rgb[0] ) ) * 257 );
		color.green = static_cast<guint16>( gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( field.rgb[1] ) ) * 257 );
		color.blue = static_cast<guint16>( gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( field.rgb[2] ) ) * 257 );
		gtk_color_button_set_color( GTK_COLOR_BUTTON( field.button ), &color );
	}

	static void FromButton( GtkColorButton *button, gpointer data ){
		NodExportColor &field = *static_cast<NodExportColor *>( data );
		GdkColor color;
		gtk_color_button_get_color( button, &color );
		const guint16 values[] = { color.red, color.green, color.blue };
		for ( int i = 0; i != 3; ++i ) {
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( field.rgb[i] ), values[i] / 257.0 );
	}
}
};

struct NodExportDialog {
	GtkWidget *mapName;
	GtkWidget *panel, *geometry, *lighting, *objects, *pathing, *oversample, *shadows;
	GtkWidget *spacingOverride, *spacing, *resolution, *phong, *sun, *ambient, *sunIntensity, *ambientIntensity;
	GtkWidget *direction[3], *lightingBody, *sunBody, *ambientBody;
	NodExportColor sunColor, ambientColor;
	bool adjustingResolution = false;
	static void Resolution( GtkRange *range, gpointer data ){
		NodExportDialog &ui = *static_cast<NodExportDialog *>( data );
		if ( ui.adjustingResolution ) {
			return;
		}

		ui.adjustingResolution = true;
		const double value = std::floor( std::pow( 2.0, gtk_range_get_value( range ) ) * 100 + 0.5 ) / 100;
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ui.spacing ), value );
		ui.adjustingResolution = false;
	}

	static void Spacing( GtkSpinButton *spin, gpointer data ){
		NodExportDialog &ui = *static_cast<NodExportDialog *>( data );
		if ( ui.adjustingResolution ) {
			return;
		}

		ui.adjustingResolution = true;
		gtk_range_set_value( GTK_RANGE( ui.resolution ), std::log( gtk_spin_button_get_value( spin ) ) / std::log( 2.0 ) );
		ui.adjustingResolution = false;
	}

	static GtkWidget *Box( GtkWidget *parent, const char *title ){
		GtkWidget *frame = gtk_frame_new( title );
		gtk_box_pack_start( GTK_BOX( parent ), frame, FALSE, FALSE, 0 );
		GtkWidget *box = gtk_vbox_new( FALSE, 5 );
		gtk_container_set_border_width( GTK_CONTAINER( box ), 8 );
		gtk_container_add( GTK_CONTAINER( frame ), box );
		return box;
	}

	static GtkWidget *Check( GtkWidget *parent, const char *text ){
		GtkWidget *check = gtk_check_button_new_with_label( text );
		gtk_box_pack_start( GTK_BOX( parent ), check, FALSE, FALSE, 0 );
		return check;
	}

	static GtkWidget *Row( GtkWidget *parent, const char *text ){
		GtkWidget *row = gtk_hbox_new( FALSE, 6 );
		gtk_box_pack_start( GTK_BOX( parent ), row, FALSE, FALSE, 0 );
		GtkWidget *label = gtk_label_new( text );
		gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
		gtk_widget_set_size_request( label, 110, -1 );
		gtk_box_pack_start( GTK_BOX( row ), label, FALSE, FALSE, 0 );
		return row;
	}

	static GtkWidget *Spin( GtkWidget *row, double low, double high, double step, unsigned digits ){
		GtkWidget *spin = gtk_spin_button_new_with_range( low, high, step );
		gtk_spin_button_set_digits( GTK_SPIN_BUTTON( spin ), digits );
		gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );
		gtk_entry_set_width_chars( GTK_ENTRY( spin ), 7 );
		gtk_box_pack_start( GTK_BOX( row ), spin, FALSE, FALSE, 0 );
		return spin;
	}

	static void Color( GtkWidget *parent, NodExportColor &field ){
		GtkWidget *row = Row( parent, _( "Color (RGB)" ) );
		for ( int i = 0; i != 3; ++i ) {
			field.rgb[i] = Spin( row, 0, 255, 1, 0 );
		}

		field.button = gtk_color_button_new();
		gtk_box_pack_start( GTK_BOX( row ), field.button, FALSE, FALSE, 0 );
		for ( int i = 0; i != 3; ++i ) {
			g_signal_connect( field.rgb[i], "value-changed", G_CALLBACK( NodExportColor::ToButton ), &field );
		}

		g_signal_connect( field.button, "color-set", G_CALLBACK( NodExportColor::FromButton ), &field );
	}

	static void Toggle( GtkToggleButton *, gpointer data ){
		static_cast<NodExportDialog *>( data )->Sensitivity();
	}

	void Sensitivity(){
		const bool enabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( lighting ) ) != FALSE;
		gtk_widget_set_sensitive( lightingBody, enabled );
		gtk_widget_set_sensitive( spacing, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( spacingOverride ) ) );
		gtk_widget_set_sensitive( resolution, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( spacingOverride ) ) );
		gtk_widget_set_sensitive( sunBody, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( sun ) ) );
		gtk_widget_set_sensitive( ambientBody, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ambient ) ) );
	}

	static void Preset( GtkButton *button, gpointer data ){
		NodExportDialog &ui = *static_cast<NodExportDialog *>( data );
		const int preset = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), "nod-preset" ) );
		NodCompileOptions options;
		ui.Read( options );
		options.geometry = preset != 3;
		options.lighting = preset == 0 || preset == 1;
		options.objects = preset == 0 || preset == 3;
		options.pathing = preset == 0;
		options.oversample = preset == 0;
		options.shadows = true;
		options.phong = preset == 0 ? 89 : 0;
		if ( preset == 1 ) { options.spacingOverride = true; options.spacing = 8; }
		if ( preset == 0 ) {
			options.spacingOverride = false;
		}

		ui.Load( options );
	}

	void Build( GtkWidget *parent, const NodCompileOptions &options ){
		panel = gtk_vbox_new( FALSE, 8 );
		GtkWidget *frame = gtk_frame_new( _( "Export Options" ) );
		gtk_box_pack_start( GTK_BOX( parent ), frame, FALSE, FALSE, 0 );
		gtk_container_set_border_width( GTK_CONTAINER( panel ), 8 );
		gtk_container_add( GTK_CONTAINER( frame ), panel );
		GtkWidget *nameRow = Row( panel, _( "Override Map Name" ) );
		mapName = gtk_entry_new();
		gtk_box_pack_start( GTK_BOX( nameRow ), mapName, TRUE, TRUE, 0 );
		gtk_widget_set_tooltip_text( mapName, _( "Blank uses the current MAP filename. Enter a filename stem, without an extension or folders. Renames exported files only, not the source MAP." ) );
		GtkWidget *columns = gtk_hbox_new( FALSE, 8 );
		gtk_box_pack_start( GTK_BOX( panel ), columns, FALSE, FALSE, 0 );
		GtkWidget *left = gtk_vbox_new( FALSE, 6 );
		gtk_box_pack_start( GTK_BOX( columns ), left, TRUE, TRUE, 0 );
		GtkWidget *geo = Box( left, _( "Geometry" ) );
		geometry = Check( geo, _( "Export geometry (NIL)" ) );
		GtkWidget *note = gtk_label_new( _( "Current native compiler: one sector, unified brush collision." ) );
		gtk_misc_set_alignment( GTK_MISC( note ), 0, 0.5 );
		gtk_box_pack_start( GTK_BOX( geo ), note, FALSE, FALSE, 0 );
		pathing = Check( geo, _( "Pathing info (NPD)" ) );
		GtkWidget *light = Box( left, _( "Lighting" ) );
		lighting = Check( light, _( "Compute lighting (rebuilds NIL)" ) );
		lightingBody = gtk_vbox_new( FALSE, 5 );
		gtk_box_pack_start( GTK_BOX( light ), lightingBody, FALSE, FALSE, 0 );
		GtkWidget *checks = gtk_hbox_new( FALSE, 10 );
		gtk_box_pack_start( GTK_BOX( lightingBody ), checks, FALSE, FALSE, 0 );
		oversample = Check( checks, _( "Oversample (4 samples)" ) );
		shadows = Check( checks, _( "Brush shadows" ) );
		spacingOverride = Check( lightingBody, _( "Override MAP light resolution" ) );
		GtkWidget *resolutionRow = Row( lightingBody, _( "Light resolution" ) );
		resolution = gtk_hscale_new_with_range( std::log( 0.01 ) / std::log( 2.0 ), 10, 0.25 );
		gtk_scale_set_draw_value( GTK_SCALE( resolution ), FALSE );
		gtk_box_pack_start( GTK_BOX( resolutionRow ), resolution, TRUE, TRUE, 0 );
		spacing = Spin( resolutionRow, 0.01, 1024, 0.25, 2 );
		g_signal_connect( resolution, "value-changed", G_CALLBACK( Resolution ), this );
		g_signal_connect( spacing, "value-changed", G_CALLBACK( Spacing ), this );
		gtk_widget_set_tooltip_text( spacing, _( "World units. Smaller values improve resolution but increase triangles and bake cost." ) );
		phong = Spin( Row( lightingBody, _( "Phong angle" ) ), 0, 180, 1, 0 );
		gtk_widget_set_tooltip_text( phong, _( "Normal smoothing threshold in degrees; 0 preserves hard normals. No specular shader is added." ) );
		sun = Check( lightingBody, _( "Moon / Sun light" ) );
		sunBody = gtk_vbox_new( FALSE, 4 );
		gtk_box_pack_start( GTK_BOX( lightingBody ), sunBody, FALSE, FALSE, 0 );
		sunIntensity = Spin( Row( sunBody, _( "Intensity (%)" ) ), 0.01, 102400, 5, 2 );
		Color( sunBody, sunColor );
		GtkWidget *row = Row( sunBody, _( "Direction" ) );
		for ( int i = 0; i != 3; ++i ) {
			direction[i] = Spin( row, -1000000, 1000000, 0.1, 3 );
		}

		gtk_widget_set_tooltip_text( row, _( "Direction light travels, normalized by the compiler. Must not be zero." ) );
		ambient = Check( lightingBody, _( "Ambient light" ) );
		ambientBody = gtk_vbox_new( FALSE, 4 );
		gtk_box_pack_start( GTK_BOX( lightingBody ), ambientBody, FALSE, FALSE, 0 );
		ambientIntensity = Spin( Row( ambientBody, _( "Intensity (%)" ) ), 0.01, 102400, 5, 2 );
		Color( ambientBody, ambientColor );
		objects = Check( left, _( "Export static objects / scenes (NSD placements)" ) );
		GtkWidget *presets = Box( columns, _( "Presets" ) );
		const char *names[] = { _( "Full Export" ), _( "Fast Lighting" ), _( "Geometry Only" ), _( "Objects Only" ) };
		for ( int i = 0; i != 4; ++i ) {
			GtkWidget *button = gtk_button_new_with_label( names[i] );
			gtk_box_pack_start( GTK_BOX( presets ), button, FALSE, FALSE, 0 );
			g_object_set_data( G_OBJECT( button ), "nod-preset", GINT_TO_POINTER( i ) );
			g_signal_connect( button, "clicked", G_CALLBACK( Preset ), this );
		}

		GtkWidget *toggles[] = { lighting, spacingOverride, sun, ambient };
		for ( size_t i = 0; i != G_N_ELEMENTS( toggles ); ++i ) {
			g_signal_connect( toggles[i], "toggled", G_CALLBACK( Toggle ), this );
		}

		Load( options );
	}

	void Load( const NodCompileOptions &options ){
		gtk_entry_set_text( GTK_ENTRY( mapName ), options.mapName.c_str() );
		GtkWidget *checks[] = { geometry, objects, pathing, lighting, oversample, shadows, spacingOverride, sun, ambient };
		const bool values[] = { options.geometry, options.objects, options.pathing, options.lighting, options.oversample,
			options.shadows, options.spacingOverride, options.sun, options.ambient };
		for ( size_t i = 0; i != G_N_ELEMENTS( checks ); ++i ) {
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checks[i] ), values[i] );
		}

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( spacing ), options.spacing );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( phong ), options.phong );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( sunIntensity ), options.sunIntensity * 100 );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( ambientIntensity ), options.ambientIntensity * 100 );
		for ( int i = 0; i != 3; ++i ) {
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( direction[i] ), options.sunDirection[i] );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( sunColor.rgb[i] ), options.sunColor[i] * 255 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( ambientColor.rgb[i] ), options.ambientColor[i] * 255 );
		}

		Sensitivity();
	}

	void Read( NodCompileOptions &options ){
		options.mapName = gtk_entry_get_text( GTK_ENTRY( mapName ) );
		GtkWidget *checks[] = { geometry, objects, pathing, lighting, oversample, shadows, spacingOverride, sun, ambient };
		bool *values[] = { &options.geometry, &options.objects, &options.pathing, &options.lighting, &options.oversample,
			&options.shadows, &options.spacingOverride, &options.sun, &options.ambient };
		for ( size_t i = 0; i != G_N_ELEMENTS( checks ); ++i ) {
			*values[i] = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checks[i] ) ) != FALSE;
		}

		options.spacing = gtk_spin_button_get_value( GTK_SPIN_BUTTON( spacing ) );
		options.phong = gtk_spin_button_get_value( GTK_SPIN_BUTTON( phong ) );
		options.sunIntensity = gtk_spin_button_get_value( GTK_SPIN_BUTTON( sunIntensity ) ) / 100;
		options.ambientIntensity = gtk_spin_button_get_value( GTK_SPIN_BUTTON( ambientIntensity ) ) / 100;
		for ( int i = 0; i != 3; ++i ) {
			options.sunDirection[i] = gtk_spin_button_get_value( GTK_SPIN_BUTTON( direction[i] ) );
			options.sunColor[i] = gtk_spin_button_get_value( GTK_SPIN_BUTTON( sunColor.rgb[i] ) ) / 255;
			options.ambientColor[i] = gtk_spin_button_get_value( GTK_SPIN_BUTTON( ambientColor.rgb[i] ) ) / 255;
		}
	}
};

#endif
