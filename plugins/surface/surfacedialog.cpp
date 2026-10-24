/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: surfacedialog.cpp
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
// Surface Dialog Module
//

//
// Nurail: Implemented to Module from the main Radiant Surface Dialog code
//


#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "surfdlg_plugin.h"
#include "nodsurface.h"
#include "nodsurfacepresets.h"

#ifdef _DEBUG
//  #define DBG_SI 1
#endif

#include "gtkr_vector.h"

std::vector<texdef_to_face_t> g_texdef_face_vector;

inline texdef_to_face_t* get_texdef_face_list(){
	return &( *g_texdef_face_vector.begin() );
}

inline unsigned int texdef_face_list_empty(){
	return g_texdef_face_vector.empty();
}

inline unsigned int texdef_face_list_size(){
	return g_texdef_face_vector.size();
}

// For different faces having different values
bool is_HShift_conflicting;
bool is_VShift_conflicting;
bool is_HScale_conflicting;
bool is_VScale_conflicting;
bool is_Rotate_conflicting;
bool is_TextureName_conflicting;

void ShowDlg();
void HideDlg();
void CancelDlg();
void SetTexMods();
void GetTexMods( bool b_SetUndoPoint = FALSE );
void BuildDialog();
void FitAll();
void InitDefaultIncrement( texdef_t * );
void DoSnapTToGrid( float hscale, float vscale );
// called to perform a fitting from the outside (shortcut key)
void SurfaceDialogFitAll();


// Dialog Data
int m_nHeight;
int m_nWidth;

// 0 is invalid, otherwise it's the Id of the last 'do' we are responsible for
int m_nUndoId;


texturewin_t *texturewin;
texdef_t *l_pIncrement;
texdef_t texdef_offset;
texdef_t texdef_SI_values;

// For Texture Entry, activate only on entry change
char old_texture_entry[128];

// the texdef to switch back to when the OnCancel is called
texdef_t g_old_texdef;

// when TRUE, this thing means the surface inspector is currently being displayed
bool g_surfwin = FALSE;
// turn on/off processing of the "changed" "value-changed" messages
// (need to turn off when we are feeding data in)
bool g_bListenChanged = true;
// turn on/off listening of the update messages
bool g_bListenUpdate = true;

extern void *g_pMainWidget;

GtkWidget* create_SurfaceInspector( void );
GtkWidget *SurfaceInspector = NULL;

GtkWidget *m_pWidget;
GtkWidget *GetWidget() { return SurfaceInspector; }
GtkWidget *Get_SI_Module_Widget() { return SurfaceInspector; }
void SetWidget( GtkWidget *new_widget ) { m_pWidget = new_widget; }
GtkWidget *GetDlgWidget( const char* name )
{ return GTK_WIDGET( g_object_get_data( G_OBJECT( SurfaceInspector ), name ) ); }

// Spins for FitTexture
GtkWidget *spin_width;
GtkWidget *spin_height;


GtkWidget *texture_combo;
GtkWidget *texture_combo_entry;

GtkWidget *match_grid_button;

GtkAdjustment *hshift_value_spinbutton_adj;
GtkWidget *hshift_value_spinbutton;
GtkAdjustment *vshift_value_spinbutton_adj;
GtkWidget *vshift_value_spinbutton;
GtkAdjustment *hscale_value_spinbutton_adj;
GtkWidget *hscale_value_spinbutton;
GtkAdjustment *vscale_value_spinbutton_adj;
GtkWidget *vscale_value_spinbutton;
GtkAdjustment *rotate_value_spinbutton_adj;
GtkWidget *rotate_value_spinbutton;

GtkAdjustment *hshift_step_spinbutton_adj;
GtkWidget *hshift_step_spinbutton;
GtkAdjustment *vshift_step_spinbutton_adj;
GtkWidget *vshift_step_spinbutton;
GtkAdjustment *hscale_step_spinbutton_adj;
GtkWidget *hscale_step_spinbutton;
GtkAdjustment *vscale_step_spinbutton_adj;
GtkWidget *vscale_step_spinbutton;
GtkAdjustment *rotate_step_spinbutton_adj;
GtkWidget *rotate_step_spinbutton;

GtkAdjustment *fit_width_spinbutton_adj;
GtkWidget *fit_width_spinbutton;
GtkAdjustment *fit_height_spinbutton_adj;
GtkWidget *fit_height_spinbutton;
GtkWidget *fit_button;
GtkWidget *axial_button;

static GtkWidget *nod_surface_status;
static GtkWidget *nod_surface_preset;
static GtkWidget *nod_surface_capture;
static GtkWidget *nod_surface_apply;
static GtkWidget *nod_surface_confirm;
static NodSurfacePreset nod_captured_surface;
static bool nod_has_capture = false;
static GtkWidget *nod_saved_presets, *nod_preset_name;
static NodSurfacePresetLibrary nod_preset_library;
static bool nod_preset_load_failed = false;

static std::string NodSurface_LibraryPath(){
	gchar *path = g_build_filename( g_FuncTable.m_pfnGetGamePath(), "nod", "editor", "surface-presets.xml", NULL );
	const std::string result = path;
	g_free( path );
	return result;
}

static bool NodSurface_Enabled(){
	return NodSurface_IsProfile( g_FuncTable.m_pfnGetGameFile() );
}

static void NodSurface_Update(){
	if ( !nod_surface_status ) {
		return;
	}

	char text[128];
	g_snprintf( text, sizeof( text ), _( "%u faces; session capture: %s." ),
			 texdef_face_list_size(), nod_has_capture ? _( "available" ) : _( "none" ) );
	gtk_label_set_text( GTK_LABEL( nod_surface_status ), text );
	gtk_widget_set_sensitive( nod_surface_capture, texdef_face_list_size() == 1 );
	const int operation = gtk_combo_box_get_active( GTK_COMBO_BOX( nod_surface_preset ) );
	gtk_widget_set_sensitive( nod_surface_apply, !texdef_face_list_empty() && operation >= 0
		&& ( !NodSurface_UsesCapture( static_cast<NodSurfaceOperation>( operation ) ) || nod_has_capture ) );
}

// Callbacks
gboolean on_texture_combo_entry_key_press_event( GtkWidget *widget, GdkEventKey *event, gpointer user_data );
void on_texture_combo_entry_activate( GtkEntry *entry, gpointer user_data );

static void on_match_grid_button_clicked( GtkButton *button, gpointer user_data );

static void on_hshift_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_vshift_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_hscale_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_vscale_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_rotate_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );

static void on_hshift_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_vshift_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_hscale_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_vscale_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_rotate_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );

static void on_fit_width_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_fit_height_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data );
static void on_fit_button_clicked( GtkButton *button, gpointer user_data );
static void on_axial_button_clicked( GtkButton *button, gpointer user_data );

/*
   ===================================================

   SURFACE INSPECTOR

   ===================================================
 */


void IsFaceConflicting(){
	texdef_t* tmp_texdef;
	texdef_to_face_t* temp_texdef_face_list;
	char texture_name[128];

	if ( texdef_face_list_empty() ) {
		gtk_entry_set_text( GTK_ENTRY( hshift_value_spinbutton ), "" );
		gtk_entry_set_text( GTK_ENTRY( vshift_value_spinbutton ), "" );
		gtk_entry_set_text( GTK_ENTRY( hscale_value_spinbutton ), "" );
		gtk_entry_set_text( GTK_ENTRY( vscale_value_spinbutton ), "" );
		gtk_entry_set_text( GTK_ENTRY( rotate_value_spinbutton ), "" );
		gtk_entry_set_text( GTK_ENTRY( texture_combo_entry ), "" );
		return;
	}

	g_bListenChanged = FALSE;

	tmp_texdef = &get_texdef_face_list()->texdef;

	strcpy( texture_name, tmp_texdef->GetName() );

	texdef_SI_values.shift[0] = tmp_texdef->shift[0];
	texdef_SI_values.shift[1] = tmp_texdef->shift[1];
	texdef_SI_values.scale[0] = tmp_texdef->scale[0];
	texdef_SI_values.scale[1] = tmp_texdef->scale[1];
	texdef_SI_values.rotate = tmp_texdef->rotate;
	texdef_SI_values.SetName( texture_name );

	is_HShift_conflicting = FALSE;
	is_VShift_conflicting = FALSE;
	is_HScale_conflicting = FALSE;
	is_VScale_conflicting = FALSE;
	is_Rotate_conflicting = FALSE;
	is_TextureName_conflicting = FALSE;

	if ( texdef_face_list_size() > 1 ) {
		temp_texdef_face_list = get_texdef_face_list()->next;

		for (; temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = &temp_texdef_face_list->texdef;
			if ( texdef_SI_values.shift[0] != tmp_texdef->shift[0] ) {
				is_HShift_conflicting = TRUE;
			}

			if ( texdef_SI_values.shift[1] != tmp_texdef->shift[1] ) {
				is_VShift_conflicting = TRUE;
			}

			if ( texdef_SI_values.scale[0] != tmp_texdef->scale[0] ) {
				is_HScale_conflicting = TRUE;
			}

			if ( texdef_SI_values.scale[1] != tmp_texdef->scale[1] ) {
				is_VScale_conflicting = TRUE;
			}

			if ( texdef_SI_values.rotate != tmp_texdef->rotate ) {
				is_Rotate_conflicting = TRUE;
			}

			if ( strcmp( texture_name, tmp_texdef->GetName() ) ) {
				is_TextureName_conflicting = TRUE;
			}
		}
	}

	if ( is_HShift_conflicting ) {
		gtk_entry_set_text( GTK_ENTRY( hshift_value_spinbutton ), "" );
	} else {
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( hshift_value_spinbutton ), texdef_SI_values.shift[0] );
	}

	if ( is_VShift_conflicting ) {
		gtk_entry_set_text( GTK_ENTRY( vshift_value_spinbutton ), "" );
	} else {
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( vshift_value_spinbutton ), texdef_SI_values.shift[1] );
	}

	if ( is_HScale_conflicting ) {
		gtk_entry_set_text( GTK_ENTRY( hscale_value_spinbutton ), "" );
	} else {
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( hscale_value_spinbutton ), texdef_SI_values.scale[0] );
	}

	if ( is_VScale_conflicting ) {
		gtk_entry_set_text( GTK_ENTRY( vscale_value_spinbutton ), "" );
	} else {
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( vscale_value_spinbutton ), texdef_SI_values.scale[1] );
	}

	if ( is_Rotate_conflicting ) {
		gtk_entry_set_text( GTK_ENTRY( rotate_value_spinbutton ), "" );
	} else {
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( rotate_value_spinbutton ), texdef_SI_values.rotate );
	}

	g_bListenChanged = TRUE;
}

#define MAX_NUM_LIST_ITEMS 15
static void PopulateTextureComboList(){
	texdef_t* tmp_texdef;
	texdef_to_face_t* temp_texdef_face_list;
	char blank[1];
	GList *items = NULL;
	GList *lst;
	int num_of_list_items = 0;

	blank[0] = 0;

	//clear combo box
#if GTK_CHECK_VERSION( 3, 0, 0 )
	gtk_combo_box_text_remove_all( GTK_COMBO_BOX_TEXT( texture_combo ) );
#else
	GtkListStore *store;
	store = GTK_LIST_STORE( gtk_combo_box_get_model( GTK_COMBO_BOX( texture_combo ) ) );
	gtk_list_store_clear( store );
#endif

	if ( texdef_face_list_empty() ) {
		items = g_list_append( items, (gpointer) blank );
		// For Texture Entry, activate only on entry change
		strcpy( old_texture_entry, blank );
	}
	else if ( !is_TextureName_conflicting ) {
		temp_texdef_face_list = get_texdef_face_list();
		tmp_texdef = (texdef_t *) &get_texdef_face_list()->texdef;
		items = g_list_append( items, (gpointer) tmp_texdef->GetName() );
		// For Texture Entry, activate only on entry change
		strcpy( old_texture_entry, tmp_texdef->GetName() );
	}
	else
	{
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			// Need to do a string compare, hence the custom search
			if ( !( g_list_find_custom( items, tmp_texdef->GetName(), (GCompareFunc) strcmp ) ) ) {
				items = g_list_append( items, (gpointer) tmp_texdef->GetName() );
				num_of_list_items++;
			}

			// Make sure the combo list isn't too long
			if ( num_of_list_items >= MAX_NUM_LIST_ITEMS ) {
				break;
			}
		}

		// If this isn't added last (to the top of the list), g_list_find freaks.
		items = g_list_prepend( items, (gpointer) blank );
		// For Texture Entry, activate only on entry change
		strcpy( old_texture_entry, blank );
	}

	for( lst = items; lst != NULL; lst = g_list_next( lst ) )
	{
		gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( texture_combo ), (const char *)lst->data );
	}

	g_list_free( items );

	gtk_combo_box_set_active( GTK_COMBO_BOX( GTK_COMBO_BOX_TEXT( texture_combo ) ), 0 );
}

static void ZeroOffsetValues(){
	texdef_offset.shift[0] = 0.0;
	texdef_offset.shift[1] = 0.0;
	texdef_offset.scale[0] = 0.0;
	texdef_offset.scale[1] = 0.0;
	texdef_offset.rotate = 0.0;
}

static void GetTexdefInfo_from_Radiant(){
	g_texdef_face_vector.clear();

	unsigned int count = GetSelectedFaceCountfromBrushes();
	if ( count == 0 ) {
		count = GetSelectedFaceCount();
	}

	g_texdef_face_vector.resize( count );

	if ( !texdef_face_list_empty() ) {
//    texdef_to_face_t* p = get_texdef_face_list();
		GetSelFacesTexdef( get_texdef_face_list() );
	}

	IsFaceConflicting();
	PopulateTextureComboList();
	ZeroOffsetValues();
	NodSurface_Update();
}

static gint apply_and_hide( GtkWidget *widget, GdkEvent *event, gpointer data ) {
  if ( !texdef_face_list_empty() ) {
    GetTexMods( TRUE );
    Sys_UpdateWindows( W_CAMERA );
    GetTexdefInfo_from_Radiant();
    SetTexMods();
  }

  HideDlg();
  return TRUE;
}

// Listen for 'Esc' globally and apply+hide - that's all we can really do (same as closing the dialog)
static gint surface_dialog_key_press( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
  if ( event->keyval != GDK_KEY_Escape ) {
    return FALSE;
  }

  return apply_and_hide( widget, (GdkEvent*)event, data );
}

// make the shift increments match the grid settings
// the objective being that the shift+arrows shortcuts move the texture by the corresponding grid size
// this depends on a scale value if you have selected a particular texture on which you want it to work:
// we move the textures in pixels, not world units. (i.e. increment values are in pixel)
// depending on the texture scale it doesn't take the same amount of pixels to move of g_qeglobals.d_gridsize
// increment * scale = gridsize
// hscale and vscale are optional parameters, if they are zero they will be set to the default scale
// NOTE: the default scale depends if you are using BP mode or regular.
// For regular it's 0.5f (128 pixels cover 64 world units), for BP it's simply 1.0f
// see fenris #2810
void DoSnapTToGrid( float hscale, float vscale ){
	l_pIncrement = Get_SI_Inc();

	if ( hscale == 0.0f ) {
		hscale = 0.5f;
	}

	if ( vscale == 0.0f ) {
		vscale = 0.5f;
	}
#ifdef _DEBUG
	Sys_Printf( "DoSnapTToGrid: hscale %g vscale %g\n", hscale, vscale );
#endif
	l_pIncrement->shift[0] = GridSize() / hscale;
	l_pIncrement->shift[1] = GridSize() / vscale;
	// now some update work
	// FIXME: doesn't look good here, seems to be called several times
	SetTexMods();
}

void UpdateSurfaceDialog(){
	if ( !g_bListenUpdate ) {
		return;
	}

	if ( SurfaceInspector == NULL ) {
		return;
	}

	// avoid long delays on slow computers
	while ( gtk_events_pending() ) {
		gtk_main_iteration();
        }

	if ( g_surfwin ) {
#ifdef DBG_SI
		Sys_Printf( "UpdateSurfaceDialog\n" );
#endif
		GetTexdefInfo_from_Radiant();
		SetTexMods();
	}

}

// DoSurface will always try to show the surface inspector
// or update it because something new has been selected
void DoSurface( void ) {
#ifdef DBG_SI
  Sys_Printf( "DoSurface\n" );
#endif
  if ( !SurfaceInspector ) {
    create_SurfaceInspector();
  }

  ShowDlg();
  SetTexMods();
}

void ToggleSurface() {
#ifdef DBG_SI
  Sys_Printf( "ToggleSurface Module\n" );
#endif
  if ( !g_surfwin ) {
    DoSurface();
  } else if ( NodSurface_Enabled() ) {
    apply_and_hide( NULL, NULL, NULL );
  } else {
    CancelDlg();
  }
}

// NOTE: will raise and show the Surface inspector and exec fit for patches and brushes
void SurfaceDlgFitAll(){
	DoSurface();
	FitAll();
}

// =============================================================================
// SurfaceDialog class

void ShowDlg() {
  if ( !SurfaceInspector ) {
    create_SurfaceInspector();
  } else {
    gtk_widget_show( SurfaceInspector );
  }

  GetTexdefInfo_from_Radiant();
  if ( NodSurface_Enabled() ) {
		m_nUndoId = 0;
	} else {
		GetTexMods( TRUE ); // Set Initial Undo Point
	}

	g_surfwin = TRUE;
}

void HideDlg() {
  g_surfwin = FALSE;
  gtk_widget_hide( SurfaceInspector );
}

void CancelDlg() {
  texturewin = Texturewin();
  texturewin->texdef = g_old_texdef;
  // cancel the last do if we own it
  if ( ( m_nUndoId == Undo_GetUndoId() ) && ( m_nUndoId != 0 ) ) {
#ifdef DBG_SI
    Sys_Printf( "CancelDlg calling Undo_Undo\n" );
#endif
    g_bListenUpdate = false;
    Undo_Undo( TRUE );
    g_bListenUpdate = true;
    m_nUndoId = 0;
  }

  HideDlg();
}

// set default values for increments (shift scale and rot)
// this is called by the prefs code if can't find the values
void InitDefaultIncrement( texdef_t *tex ){
	tex->SetName( "foo" );
	tex->shift[0] = 8;
	tex->shift[1] = 8;
	tex->scale[0] = 0.25;
	tex->scale[1] = 0.25;
	tex->rotate = 10;
}

void BuildDialog(){
	if ( !SurfaceInspector ) {
		create_SurfaceInspector();
	}
}

/*
   ==============
   SetTexMods

   Set the fields to the current texdef (i.e. map/texdef -> dialog widgets)
   ===============
 */

void SetTexMods(){
	texdef_t *pt;
	GtkSpinButton *spin;
	GtkAdjustment *adjust;

	texturewin = Texturewin();
	l_pIncrement = Get_SI_Inc();

#ifdef DBG_SI
	Sys_Printf( "SurfaceDlg SetTexMods\n" );
#endif

	if ( !g_surfwin ) {
		return;
	}

	pt = &texturewin->texdef;

	g_bListenChanged = false;

	if ( strncmp( pt->GetName(), "textures/", 9 ) != 0 ) {
		texdef_offset.SetName( SHADER_NOT_FOUND );
	}

	spin = GTK_SPIN_BUTTON( hshift_value_spinbutton );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( spin ) );
	gtk_adjustment_set_step_increment( adjust, l_pIncrement->shift[0] );
	spin = GTK_SPIN_BUTTON( hshift_step_spinbutton );
	gtk_spin_button_set_value( spin, l_pIncrement->shift[0] );
	gtk_spin_button_set_increments( spin, l_pIncrement->shift[0], l_pIncrement->shift[0] );

	spin = GTK_SPIN_BUTTON( vshift_value_spinbutton );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( spin ) );
	gtk_adjustment_set_step_increment( adjust, l_pIncrement->shift[1] );
	spin = GTK_SPIN_BUTTON( vshift_step_spinbutton );
	gtk_spin_button_set_value( spin, l_pIncrement->shift[1] );
	gtk_spin_button_set_increments( spin, l_pIncrement->shift[1], l_pIncrement->shift[1] );

	spin = GTK_SPIN_BUTTON( hscale_value_spinbutton );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( spin ) );
	gtk_adjustment_set_step_increment( adjust, l_pIncrement->scale[0] );
	spin = GTK_SPIN_BUTTON( hscale_step_spinbutton );
	gtk_spin_button_set_value( spin, l_pIncrement->scale[0] );
	gtk_spin_button_set_increments( spin, l_pIncrement->scale[0], l_pIncrement->scale[0] );

	spin = GTK_SPIN_BUTTON( vscale_value_spinbutton );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( spin ) );
	gtk_adjustment_set_step_increment( adjust, l_pIncrement->scale[1] );
	spin = GTK_SPIN_BUTTON( vscale_step_spinbutton );
	gtk_spin_button_set_value( spin, l_pIncrement->scale[1] );
	gtk_spin_button_set_increments( spin, l_pIncrement->scale[1], l_pIncrement->scale[1] );

	spin = GTK_SPIN_BUTTON( rotate_value_spinbutton );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( spin ) );
	gtk_adjustment_set_step_increment( adjust, l_pIncrement->rotate );
	spin = GTK_SPIN_BUTTON( rotate_step_spinbutton );
	gtk_spin_button_set_value( spin, l_pIncrement->rotate );
	gtk_spin_button_set_increments( spin, l_pIncrement->rotate, l_pIncrement->rotate );

	g_bListenChanged = true;

	// store the current texdef as our escape route if user hits OnCancel
	g_old_texdef = texturewin->texdef;
}

/*
   ==============
   GetTexMods

   Shows any changes to the main Radiant windows
   ===============
 */
void GetTexMods( bool b_SetUndoPoint ) {

#ifdef DBG_SI
	Sys_Printf( "SurfaceDlg GetTexMods\n" );
#endif

	if ( !texdef_face_list_empty() ) {
		g_bListenUpdate = FALSE;
		SetTexdef_FaceList( get_texdef_face_list(), b_SetUndoPoint, false );
		g_bListenUpdate = TRUE;

		if ( b_SetUndoPoint ) {
			m_nUndoId = Undo_GetUndoId();
		}
	}
}

void FitAll(){
	on_fit_button_clicked( NULL, NULL );
}

static void NodSurface_Error( const char *message ){
	Sys_FPrintf( SYS_WRN, "NOD surface preset: %s\n", message );
	g_FuncTable.m_pfnMessageBox( SurfaceInspector, message, _( "NOD Surface Presets" ), MB_OK | MB_ICONWARNING, NULL );
}

static void NodSurface_RefreshLibrary(){
	GtkListStore *store = GTK_LIST_STORE( gtk_combo_box_get_model( GTK_COMBO_BOX( nod_saved_presets ) ) );
	gtk_list_store_clear( store );
	for ( std::map<std::string, NodSurfacePreset>::const_iterator i = nod_preset_library.presets.begin();
		  i != nod_preset_library.presets.end(); ++i ) {
		gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( nod_saved_presets ), i->first.c_str() );
}
}

static void NodSurface_SavedSelected( GtkComboBox *, gpointer ){
	gchar *name = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT( nod_saved_presets ) );
	if ( !name ) {
		return;
	}

	std::map<std::string, NodSurfacePreset>::const_iterator found = nod_preset_library.presets.find( name );
	if ( found != nod_preset_library.presets.end() ) {
		nod_captured_surface = found->second;
		nod_has_capture = true;
		gtk_entry_set_text( GTK_ENTRY( nod_preset_name ), name );
		gtk_combo_box_set_active( GTK_COMBO_BOX( nod_surface_preset ), NOD_SURFACE_CAPTURED );
		NodSurface_Update();
	}

	g_free( name );
}

static void NodSurface_WriteLibrary( GtkButton *, gpointer data ){
	const std::string name = gtk_entry_get_text( GTK_ENTRY( nod_preset_name ) );
	const char *invalid = NodPreset_ValidateName( name );
	if ( !invalid && !GPOINTER_TO_INT( data ) && !nod_has_capture ) {
		invalid = "Capture or select a surface preset first.";
	}

	if ( invalid ) { NodSurface_Error( invalid ); return; }
	const bool exists = nod_preset_library.presets.find( name ) != nod_preset_library.presets.end();
	if ( GPOINTER_TO_INT( data ) && !exists ) { NodSurface_Error( "Select an existing named preset to delete." ); return; }
	if ( ( exists || nod_preset_load_failed ) && g_FuncTable.m_pfnMessageBox( SurfaceInspector,
		GPOINTER_TO_INT( data ) ? _( "Delete this saved surface preset?" ) :
		_( "Replace the existing preset or invalid preset library?" ), _( "Surface presets" ),
		MB_YESNO | MB_ICONWARNING, NULL ) != IDYES ) {
		return;
	}

	NodSurfacePresetLibrary next = nod_preset_library;
	if ( GPOINTER_TO_INT( data ) ) {
		next.presets.erase( name );
	} else {
		next.presets[name] = nod_captured_surface;
	}

	std::string error;
	if ( !next.Save( NodSurface_LibraryPath().c_str(), error ) ) { NodSurface_Error( error.c_str() ); return; }
	nod_preset_library = next;
	nod_preset_load_failed = false;
	NodSurface_RefreshLibrary();
}

static void NodSurface_CaptureClicked( GtkButton *, gpointer ){
	if ( texdef_face_list_size() != 1 ) {
		NodSurface_Error( _( "Select exactly one face to capture its material and UV values." ) );
		return;
	}

	const NodSurfacePreset preset = NodSurface_Capture( get_texdef_face_list()->texdef );
	const char *error = NodSurface_Validate( preset, true, true );
	if ( error ) {
		NodSurface_Error( error );
		return;
	}

	nod_captured_surface = preset;
	nod_has_capture = true;
	NodSurface_Update();
}

static void NodSurface_PresetChanged( GtkComboBox *, gpointer ){
	NodSurface_Update();
}

static void NodSurface_ApplyClicked( GtkButton *, gpointer ){
	if ( texdef_face_list_empty() ) {
		NodSurface_Error( _( "Select one or more brush faces first." ) );
		return;
	}

	const NodSurfaceOperation operation = static_cast<NodSurfaceOperation>(
		gtk_combo_box_get_active( GTK_COMBO_BOX( nod_surface_preset ) ) );
	const NodSurfacePreset *captured = nod_has_capture ? &nod_captured_surface : NULL;
	texdef_t probe = get_texdef_face_list()->texdef;
	const char *error = NodSurface_Apply( probe, operation, captured );
	if ( error ) {
		NodSurface_Error( error );
		return;
	}

	if ( texdef_face_list_size() > 1 && gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( nod_surface_confirm ) )
		 && g_FuncTable.m_pfnMessageBox( SurfaceInspector,
			_( "Apply to all selected faces?\nMaterial/UV edits cannot be compiled back to native." ),
			_( "NOD Surface Presets" ), MB_YESNO | MB_ICONWARNING, NULL ) != IDYES ) {
		return;
	}

	// Commit outstanding live edits before starting the preset's own undo step.
	GetTexMods( TRUE );
	GetTexdefInfo_from_Radiant();
	for ( texdef_to_face_t *face = get_texdef_face_list(); face; face = face->next ) {
		NodSurface_Apply( face->texdef, operation, captured );
	}

	GetTexMods( TRUE );
	GetTexdefInfo_from_Radiant();
	SetTexMods();
}


////////////////////////////////////////////////////////////////////
//
//  GUI Section
//
////////////////////////////////////////////////////////////////////

GtkWidget* create_SurfaceInspector( void ){

	GtkWidget *label;
	GtkWidget *hseparator;
	GtkWidget *eventbox;

	GtkWidget *viewport8;
	GtkWidget *viewport9;
	GtkWidget *viewport2;
	GtkWidget *viewport7;
	GtkWidget *viewport5;
	GtkWidget *viewport6;

	GtkWidget *table1;
	GtkWidget *table4;
	GtkWidget *table5;
	GtkWidget *table7;

	GtkWidget *vbox7;

	GtkWidget *hbox1;

	GtkWidget *hbuttonbox1;
	GtkSizeGroup *size_group;

	SurfaceInspector = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_transient_for( GTK_WINDOW( SurfaceInspector ), GTK_WINDOW( g_pMainWidget ) );
	gtk_container_set_border_width( GTK_CONTAINER( SurfaceInspector ), 4 );
	gtk_window_set_title( GTK_WINDOW( SurfaceInspector ), _( "Surface Inspector" ) );

	SetWinPos_from_Prefs( SurfaceInspector );

	viewport8 = gtk_viewport_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( SurfaceInspector ), viewport8 );
	gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport8 ), GTK_SHADOW_NONE );
	gtk_widget_show( viewport8 );

	vbox7 = gtk_vbox_new( FALSE, 0 );
	gtk_container_add( GTK_CONTAINER( viewport8 ), vbox7 );
	gtk_widget_show( vbox7 );

	if ( NodSurface_Enabled() ) {
		gtk_window_set_title( GTK_WINDOW( SurfaceInspector ), _( "NOD Surfaces (editor only)" ) );
		GtkWidget *frame = gtk_frame_new( _( "Material / UV presets" ) );
		gtk_box_pack_start( GTK_BOX( vbox7 ), frame, FALSE, FALSE, 4 );
		GtkWidget *box = gtk_vbox_new( FALSE, 5 );
		gtk_container_set_border_width( GTK_CONTAINER( box ), 5 );
		gtk_container_add( GTK_CONTAINER( frame ), box );
		label = gtk_label_new( _( "MAP only; native material/UV compilation unsupported.\n"
								 "Native surface flags are unchanged." ) );
		gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
		gtk_box_pack_start( GTK_BOX( box ), label, FALSE, FALSE, 0 );
		nod_surface_status = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( nod_surface_status ), 0.0, 0.5 );
		gtk_box_pack_start( GTK_BOX( box ), nod_surface_status, FALSE, FALSE, 0 );
		nod_surface_preset = gtk_combo_box_text_new();
		const char *names[] = { _( "Reset UV" ),
			_( "Placeholder material" ), _( "Captured material + UV" ),
			_( "Captured material only" ), _( "Captured UV only" ) };
		for ( size_t i = 0; i < G_N_ELEMENTS( names ); ++i ) {
			gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( nod_surface_preset ), names[i] );
		}

		gtk_combo_box_set_active( GTK_COMBO_BOX( nod_surface_preset ), 0 );
		gtk_box_pack_start( GTK_BOX( box ), nod_surface_preset, FALSE, FALSE, 0 );
		GtkWidget *buttons = gtk_hbox_new( FALSE, 5 );
		gtk_box_pack_start( GTK_BOX( box ), buttons, FALSE, FALSE, 0 );
		nod_surface_capture = gtk_button_new_with_label( _( "Capture selected face" ) );
		nod_surface_apply = gtk_button_new_with_label( _( "Apply preset" ) );
		gtk_box_pack_start( GTK_BOX( buttons ), nod_surface_capture, TRUE, TRUE, 0 );
		gtk_box_pack_start( GTK_BOX( buttons ), nod_surface_apply, TRUE, TRUE, 0 );
		nod_surface_confirm = gtk_check_button_new_with_label( _( "Confirm multiple faces" ) );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( nod_surface_confirm ), TRUE );
		gtk_box_pack_start( GTK_BOX( box ), nod_surface_confirm, FALSE, FALSE, 0 );
		nod_saved_presets = gtk_combo_box_text_new();
		gtk_box_pack_start( GTK_BOX( box ), nod_saved_presets, FALSE, FALSE, 0 );
		nod_preset_name = gtk_entry_new();
		gtk_entry_set_max_length( GTK_ENTRY( nod_preset_name ), 63 );
		gtk_entry_set_text( GTK_ENTRY( nod_preset_name ), _( "New preset" ) );
		gtk_box_pack_start( GTK_BOX( box ), nod_preset_name, FALSE, FALSE, 0 );
		buttons = gtk_hbox_new( FALSE, 5 );
		gtk_box_pack_start( GTK_BOX( box ), buttons, FALSE, FALSE, 0 );
		GtkWidget *save = gtk_button_new_with_label( _( "Save preset" ) );
		GtkWidget *remove = gtk_button_new_with_label( _( "Delete preset" ) );
		gtk_box_pack_start( GTK_BOX( buttons ), save, TRUE, TRUE, 0 );
		gtk_box_pack_start( GTK_BOX( buttons ), remove, TRUE, TRUE, 0 );
		g_signal_connect( save, "clicked", G_CALLBACK( NodSurface_WriteLibrary ), GINT_TO_POINTER( 0 ) );
		g_signal_connect( remove, "clicked", G_CALLBACK( NodSurface_WriteLibrary ), GINT_TO_POINTER( 1 ) );
		g_signal_connect( nod_saved_presets, "changed", G_CALLBACK( NodSurface_SavedSelected ), NULL );
		g_signal_connect( nod_surface_capture, "clicked", G_CALLBACK( NodSurface_CaptureClicked ), NULL );
		g_signal_connect( nod_surface_apply, "clicked", G_CALLBACK( NodSurface_ApplyClicked ), NULL );
		g_signal_connect( nod_surface_preset, "changed", G_CALLBACK( NodSurface_PresetChanged ), NULL );
		std::string error;
		nod_preset_load_failed = !nod_preset_library.Load( NodSurface_LibraryPath().c_str(), error );
		if ( nod_preset_load_failed ) {
			NodSurface_Error( error.c_str() );
		}

		NodSurface_RefreshLibrary();
		gtk_widget_show_all( frame );
	}

	viewport9 = gtk_viewport_new( NULL, NULL );
	gtk_box_pack_start( GTK_BOX( vbox7 ), viewport9, FALSE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( viewport9 ), 2 );
	gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport9 ), GTK_SHADOW_ETCHED_IN );
	gtk_widget_show( viewport9 );

	hbox1 = gtk_hbox_new( FALSE, 0 );
	gtk_container_add( GTK_CONTAINER( viewport9 ), hbox1 );
	gtk_container_set_border_width( GTK_CONTAINER( hbox1 ), 4 );
	gtk_widget_show( hbox1 );

	label = gtk_label_new( _( "Texture: " ) );
	gtk_box_pack_start( GTK_BOX( hbox1 ), label, FALSE, FALSE, 0 );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	texture_combo = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start( GTK_BOX( hbox1 ), texture_combo, TRUE, TRUE, 0 );
	gtk_widget_show( texture_combo );

	texture_combo_entry = gtk_bin_get_child( GTK_BIN( texture_combo ) );
	gtk_entry_set_max_length( GTK_ENTRY( texture_combo_entry ), 128 );
	gtk_widget_show( texture_combo_entry );

	viewport2 = gtk_viewport_new( NULL, NULL );
	gtk_box_pack_start( GTK_BOX( vbox7 ), viewport2, FALSE, TRUE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( viewport2 ), 2 );
	gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport2 ), GTK_SHADOW_ETCHED_IN );
	gtk_widget_show( viewport2 );

	table1 = gtk_table_new( 13, 4, FALSE );
	gtk_container_add( GTK_CONTAINER( viewport2 ), table1 );
	gtk_widget_show( table1 );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 2, 3, 1, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 3, 4, 1, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 1, 2, 3, 4,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 2, 3, 3, 4,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 3, 4, 3, 4,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 1, 2, 5, 6,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 2, 3, 5, 6,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 3, 4, 5, 6,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 1, 2, 7, 8,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 2, 3, 7, 8,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 3, 4, 7, 8,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 1, 2, 9, 10,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 2, 3, 9, 10,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 3, 4, 9, 10,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 1, 2, 11, 12,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 2, 3, 11, 12,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 3, 4, 11, 12,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	label = gtk_label_new( _( "Step" ) );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_table_attach( GTK_TABLE( table1 ), label, 3, 4, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( label );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 3, 4, 12, 13,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	match_grid_button = gtk_button_new_with_mnemonic( _( "Match Grid" ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), match_grid_button );
	gtk_widget_show( match_grid_button );

	label = gtk_label_new( _( "Value" ) );
	gtk_table_attach( GTK_TABLE( table1 ), label, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 0, 1, 3, 4,
					  (GtkAttachOptions) ( GTK_SHRINK | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 0, 1, 5, 6,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 0, 1, 7, 8,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 0, 1, 9, 10,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 0, 1, 11, 12,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 4, 5,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	label = gtk_label_new( _( "Vertical Shift: " ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 6, 7,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	label = gtk_label_new( _( "Horizontal Scale: " ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 8, 9,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	label = gtk_label_new( _( "Vertical Scale: " ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 10, 11,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	label = gtk_label_new( _( "Rotate: " ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 2, 3,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	label = gtk_label_new( _( "Horizontal Shift: " ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), label );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	hseparator = gtk_hseparator_new();
	gtk_table_attach( GTK_TABLE( table1 ), hseparator, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( hseparator );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 1, 2, 12, 13,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	// Value Spins
	hshift_value_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -8192.0, 8192.0, 2.0, 8.0, 0.0 ) );
	hshift_value_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( hshift_value_spinbutton_adj ), 1, 2 );
	gtk_table_attach( GTK_TABLE( table1 ), hshift_value_spinbutton, 1, 2, 2, 3,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( hshift_value_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( hshift_value_spinbutton ), TRUE );
	gtk_spin_button_set_wrap( GTK_SPIN_BUTTON( hshift_value_spinbutton ), TRUE );
	gtk_widget_set_sensitive( GTK_WIDGET( hshift_value_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( hshift_value_spinbutton ), 1.0 ); //right
	gtk_widget_show( hshift_value_spinbutton );

	vshift_value_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -8192.0, 8192.0, 2.0, 8.0, 0.0 ) );
	vshift_value_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( vshift_value_spinbutton_adj ), 1, 2 );
	gtk_table_attach( GTK_TABLE( table1 ), vshift_value_spinbutton, 1, 2, 4, 5,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( vshift_value_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( vshift_value_spinbutton ), TRUE );
	gtk_spin_button_set_wrap( GTK_SPIN_BUTTON( vshift_value_spinbutton ), TRUE );
	gtk_widget_set_sensitive( GTK_WIDGET( vshift_value_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( vshift_value_spinbutton ), 1.0 ); //right
	gtk_widget_show( vshift_value_spinbutton );

	hscale_value_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -1024.0, 1024.0, 1.0, 4.0, 0.0 ) );
	hscale_value_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( hscale_value_spinbutton_adj ), 1, 4 );
	gtk_table_attach( GTK_TABLE( table1 ), hscale_value_spinbutton, 1, 2, 6, 7,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( hscale_value_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( hscale_value_spinbutton ), TRUE );
	gtk_spin_button_set_wrap( GTK_SPIN_BUTTON( hscale_value_spinbutton ), TRUE );
	gtk_widget_set_sensitive( GTK_WIDGET( hscale_value_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( hscale_value_spinbutton ), 1.0 ); //right
	gtk_widget_show( hscale_value_spinbutton );

	vscale_value_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -1024.0, 1024.0, 1.0, 4.0, 0.0 ) );
	vscale_value_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( vscale_value_spinbutton_adj ), 1, 4 );
	gtk_table_attach( GTK_TABLE( table1 ), vscale_value_spinbutton, 1, 2, 8, 9,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( vscale_value_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( vscale_value_spinbutton ), TRUE );
	gtk_spin_button_set_wrap( GTK_SPIN_BUTTON( vscale_value_spinbutton ), TRUE );
	gtk_widget_set_sensitive( GTK_WIDGET( vscale_value_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( vscale_value_spinbutton ), 1.0 ); //right
	gtk_widget_show( vscale_value_spinbutton );

	rotate_value_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -360.0, 360.0, 1.0, 10.0, 0.0 ) );
	rotate_value_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( rotate_value_spinbutton_adj ), 1, 4 );
	gtk_table_attach( GTK_TABLE( table1 ), rotate_value_spinbutton, 1, 2, 10, 11,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( rotate_value_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( rotate_value_spinbutton ), TRUE );
	gtk_spin_button_set_wrap( GTK_SPIN_BUTTON( rotate_value_spinbutton ), TRUE );
	gtk_widget_set_sensitive( GTK_WIDGET( rotate_value_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( rotate_value_spinbutton ), 1.0 ); //right
	gtk_widget_show( rotate_value_spinbutton );

	// Step Spins
	hshift_step_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -8192.0, 8192.0, 2.0, 8.0, 0.0 ) );
	hshift_step_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( hshift_step_spinbutton_adj ), 1, 2 );
	gtk_table_attach( GTK_TABLE( table1 ), hshift_step_spinbutton, 3, 4, 2, 3,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( hshift_step_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( hshift_step_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( hshift_step_spinbutton ), 1.0 ); //right
	gtk_widget_show( hshift_step_spinbutton );

	vshift_step_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -8192.0, 8192.0, 2.0, 8.0, 0.0 ) );
	vshift_step_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( vshift_step_spinbutton_adj ), 1, 2 );
	gtk_table_attach( GTK_TABLE( table1 ), vshift_step_spinbutton, 3, 4, 4, 5,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( vshift_step_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( vshift_step_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( vshift_step_spinbutton ), 1.0 ); //right
	gtk_widget_show( vshift_step_spinbutton );

	hscale_step_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -1024.0, 1024.0, 1.0, 4.0, 0.0 ) );
	hscale_step_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( hscale_step_spinbutton_adj ), 1, 4 );
	gtk_table_attach( GTK_TABLE( table1 ), hscale_step_spinbutton, 3, 4, 6, 7,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( hscale_step_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( hscale_step_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( hscale_step_spinbutton ), 1.0 ); //right
	gtk_widget_show( hscale_step_spinbutton );

	vscale_step_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -1024.0, 1024.0, 1.0, 4.0, 0.0 ) );
	vscale_step_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( vscale_step_spinbutton_adj ), 1, 4 );
	gtk_table_attach( GTK_TABLE( table1 ), vscale_step_spinbutton, 3, 4, 8, 9,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( vscale_step_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( vscale_step_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( vscale_step_spinbutton ), 1.0 ); //right
	gtk_widget_show( vscale_step_spinbutton );

	rotate_step_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0.0, -360.0, 360.0, 1.0, 10.0, 0.0 ) );
	rotate_step_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( rotate_step_spinbutton_adj ), 1, 4 );
	gtk_table_attach( GTK_TABLE( table1 ), rotate_step_spinbutton, 3, 4, 10, 11,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( rotate_step_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( rotate_step_spinbutton ), TRUE );
	gtk_entry_set_alignment( GTK_ENTRY( rotate_step_spinbutton ), 1.0 ); //right
	gtk_widget_show( rotate_step_spinbutton );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 2, 3, 12, 13,
					  (GtkAttachOptions) ( 0 ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( eventbox );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 12, 13,
					  (GtkAttachOptions) ( 0 ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( eventbox );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table1 ), eventbox, 0, 1, 0, 1,
					  (GtkAttachOptions) ( 0 ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( eventbox );

	viewport7 = gtk_viewport_new( NULL, NULL );
	gtk_box_pack_start( GTK_BOX( vbox7 ), viewport7, FALSE, TRUE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( viewport7 ), 2 );
	gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport7 ), GTK_SHADOW_ETCHED_IN );
	gtk_widget_show( viewport7 );

	table4 = gtk_table_new( 4, 7, FALSE );
	gtk_container_add( GTK_CONTAINER( viewport7 ), table4 );
	gtk_widget_show( table4 );

	viewport5 = gtk_viewport_new( NULL, NULL );
	gtk_table_attach( GTK_TABLE( table4 ), viewport5, 1, 7, 0, 4,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( viewport5 ), 6 );
	gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport5 ), GTK_SHADOW_ETCHED_OUT );
	gtk_widget_show( viewport5 );

	table5 = gtk_table_new( 2, 3, FALSE );
	gtk_container_add( GTK_CONTAINER( viewport5 ), table5 );
	gtk_container_set_border_width( GTK_CONTAINER( table5 ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table5 ), 2 );
	gtk_widget_show( table5 );

	label = gtk_label_new( _( "Height" ) );
	gtk_table_attach( GTK_TABLE( table5 ), label, 2, 3, 0, 1,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	label = gtk_label_new( _( "Width" ) );
	gtk_table_attach( GTK_TABLE( table5 ), label, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_show( label );

	fit_width_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 1, 1, 32, 1, 10, 0 ) );
	fit_width_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( fit_width_spinbutton_adj ), 1, 0 );
	gtk_table_attach( GTK_TABLE( table5 ), fit_width_spinbutton, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( fit_width_spinbutton ), TRUE );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( fit_width_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_entry_set_alignment( GTK_ENTRY( fit_width_spinbutton ), 1.0 ); //right
	gtk_widget_show( fit_width_spinbutton );

	fit_height_spinbutton_adj = GTK_ADJUSTMENT( gtk_adjustment_new( 1, 1, 32, 1, 10, 0 ) );
	fit_height_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT( fit_height_spinbutton_adj ), 1, 0 );
	gtk_table_attach( GTK_TABLE( table5 ), fit_height_spinbutton, 2, 3, 1, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 3, 0 );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( fit_height_spinbutton ), TRUE );
	gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON( fit_height_spinbutton ), GTK_UPDATE_IF_VALID );
	gtk_entry_set_alignment( GTK_ENTRY( fit_height_spinbutton ), 1.0 ); //right
	gtk_widget_show( fit_height_spinbutton );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table5 ), eventbox, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table5 ), eventbox, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_FILL ), 4, 0 );
	gtk_widget_show( eventbox );

	fit_button = gtk_button_new_with_mnemonic( _( "Fit" ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), fit_button );
	gtk_widget_show( fit_button );

	viewport6 = gtk_viewport_new( NULL, NULL );
	gtk_table_attach( GTK_TABLE( table4 ), viewport6, 0, 1, 0, 4,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( viewport6 ), 4 );
	gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport6 ), GTK_SHADOW_NONE );
	gtk_widget_show( viewport6 );

	table7 = gtk_table_new( 2, 1, FALSE );
	gtk_container_add( GTK_CONTAINER( viewport6 ), table7 );
	gtk_widget_show( table7 );

	eventbox = gtk_event_box_new();
	gtk_table_attach( GTK_TABLE( table7 ), eventbox, 0, 1, 0, 2,
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
					  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ), 0, 0 );
	gtk_widget_show( eventbox );

	axial_button = gtk_button_new_with_mnemonic( _( "Axial" ) );
	gtk_container_add( GTK_CONTAINER( eventbox ), axial_button );
	gtk_widget_set_size_request( axial_button, 56, 29 );
	gtk_container_set_border_width( GTK_CONTAINER( axial_button ), 4 );
	gtk_widget_show( axial_button );

	size_group = gtk_size_group_new( GTK_SIZE_GROUP_BOTH );
	gtk_size_group_add_widget( size_group, axial_button );
	gtk_size_group_add_widget( size_group, fit_button );
	g_object_unref( size_group );

	hbuttonbox1 = gtk_hbox_new( FALSE, 5 );
	gtk_box_pack_start( GTK_BOX( vbox7 ), hbuttonbox1, TRUE, FALSE, 0 );
	gtk_widget_show( hbuttonbox1 );

        // closing the window (upper right window manager click)
	g_signal_connect( (gpointer) SurfaceInspector,
					  "delete-event",
					  G_CALLBACK( apply_and_hide ),
					  NULL );

	g_signal_connect( (gpointer) SurfaceInspector, "destroy",
					  G_CALLBACK( gtk_widget_destroy ),
					  NULL );

	g_signal_connect( (gpointer) texture_combo_entry, "key-press-event",
					  G_CALLBACK( on_texture_combo_entry_key_press_event ),
					  NULL );

	g_signal_connect( (gpointer) SurfaceInspector, "key-press-event",
            G_CALLBACK( surface_dialog_key_press ),
            NULL );

	g_signal_connect( (gpointer) texture_combo_entry, "activate",
					  G_CALLBACK( on_texture_combo_entry_activate ),
					  NULL );

	g_signal_connect( (gpointer) hshift_value_spinbutton, "value-changed",
					  G_CALLBACK( on_hshift_value_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) vshift_value_spinbutton, "value-changed",
					  G_CALLBACK( on_vshift_value_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) hscale_value_spinbutton, "value-changed",
					  G_CALLBACK( on_hscale_value_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) vscale_value_spinbutton, "value-changed",
					  G_CALLBACK( on_vscale_value_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) rotate_value_spinbutton, "value-changed",
					  G_CALLBACK( on_rotate_value_spinbutton_value_changed ),
					  NULL );

	g_signal_connect( (gpointer) hshift_step_spinbutton, "value-changed",
					  G_CALLBACK( on_hshift_step_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) vshift_step_spinbutton, "value-changed",
					  G_CALLBACK( on_vshift_step_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) hscale_step_spinbutton, "value-changed",
					  G_CALLBACK( on_hscale_step_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) vscale_step_spinbutton, "value-changed",
					  G_CALLBACK( on_vscale_step_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) rotate_step_spinbutton, "value-changed",
					  G_CALLBACK( on_rotate_step_spinbutton_value_changed ),
					  NULL );

	g_signal_connect( (gpointer) match_grid_button, "clicked",
					  G_CALLBACK( on_match_grid_button_clicked ),
					  NULL );

	g_signal_connect( (gpointer) fit_width_spinbutton, "value-changed",
					  G_CALLBACK( on_fit_width_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) fit_height_spinbutton, "value-changed",
					  G_CALLBACK( on_fit_height_spinbutton_value_changed ),
					  NULL );
	g_signal_connect( (gpointer) fit_button, "clicked",
					  G_CALLBACK( on_fit_button_clicked ),
					  NULL );

	g_signal_connect( (gpointer) axial_button, "clicked",
					  G_CALLBACK( on_axial_button_clicked ),
					  NULL );

	return SurfaceInspector;
}


// Texture Combo
gboolean on_texture_combo_entry_key_press_event( GtkWidget *widget, GdkEventKey *event, gpointer user_data ){
	// Have Tab activate selection as well as Return
	if ( event->keyval == GDK_KEY_Tab ) {
		g_signal_emit_by_name( texture_combo_entry, "activate" );
	}

	return FALSE;
}

void on_texture_combo_entry_activate( GtkEntry *entry, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_t* tmp_orig_texdef;
	texdef_to_face_t* temp_texdef_face_list;
	char text[128] = { 0 };

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		// activate only on entry change
		strcpy( text, gtk_entry_get_text( entry ) );
		if ( strcmp( old_texture_entry, text ) ) {
			// Check for spaces in shader name
			if ( text[0] <= ' ' || strchr( text, ' ' ) ) {
				Sys_FPrintf( SYS_WRN, "WARNING: spaces in shader names are not allowed, ignoring '%s'\n", text );
			}
			else
			{
				for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
				{
					tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
					tmp_orig_texdef = (texdef_t *) &temp_texdef_face_list->orig_texdef;
					strcpy( old_texture_entry, text );
					tmp_texdef->SetName( text );
				}

				GetTexMods();
			}
		}
	}
}

// Match Grid
static void on_match_grid_button_clicked( GtkButton *button, gpointer user_data ){
	float hscale, vscale;

	if ( !strcmp( gtk_entry_get_text( GTK_ENTRY( hscale_value_spinbutton ) ), "" ) ) {
		hscale = 0.0;
	} else {
		hscale = gtk_spin_button_get_value( GTK_SPIN_BUTTON( hscale_value_spinbutton ) );
	}

	if ( !strcmp( gtk_entry_get_text( GTK_ENTRY( vscale_value_spinbutton ) ), "" ) ) {
		vscale = 0.0;
	} else {
		vscale = gtk_spin_button_get_value( GTK_SPIN_BUTTON( vscale_value_spinbutton ) );
	}

	DoSnapTToGrid( hscale, vscale );
}

// Value Spins
static void on_hshift_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_t* tmp_orig_texdef;
	texdef_to_face_t* temp_texdef_face_list;

	texdef_SI_values.shift[0] = gtk_spin_button_get_value( GTK_SPIN_BUTTON( hshift_value_spinbutton ) );

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			tmp_orig_texdef = (texdef_t *) &temp_texdef_face_list->orig_texdef;
			tmp_texdef->shift[0] = texdef_SI_values.shift[0] + texdef_offset.shift[0];
			is_HShift_conflicting = FALSE;
		}

		GetTexMods();
	}
}

static void on_vshift_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_t* tmp_orig_texdef;
	texdef_to_face_t* temp_texdef_face_list;

	texdef_SI_values.shift[1]  = gtk_spin_button_get_value( GTK_SPIN_BUTTON( vshift_value_spinbutton ) );

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			tmp_orig_texdef = (texdef_t *) &temp_texdef_face_list->orig_texdef;
			tmp_texdef->shift[1] = texdef_SI_values.shift[1] + texdef_offset.shift[1];
			is_VShift_conflicting = FALSE;
		}

		GetTexMods();
	}
}

static void on_hscale_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_t* tmp_orig_texdef;
	texdef_to_face_t* temp_texdef_face_list;

	texdef_SI_values.scale[0] = gtk_spin_button_get_value( GTK_SPIN_BUTTON( hscale_value_spinbutton ) );

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			tmp_orig_texdef = (texdef_t *) &temp_texdef_face_list->orig_texdef;
			tmp_texdef->scale[0] = texdef_SI_values.scale[0] + texdef_offset.scale[0];
			is_HScale_conflicting = FALSE;
		}

		GetTexMods();
	}
}

static void on_vscale_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_t* tmp_orig_texdef;
	texdef_to_face_t* temp_texdef_face_list;

	texdef_SI_values.scale[1] = gtk_spin_button_get_value( GTK_SPIN_BUTTON( vscale_value_spinbutton ) );

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			tmp_orig_texdef = (texdef_t *) &temp_texdef_face_list->orig_texdef;
			tmp_texdef->scale[1] = texdef_SI_values.scale[1] + texdef_offset.scale[1];
			is_VScale_conflicting = FALSE;
		}

		GetTexMods();
	}
}

static void on_rotate_value_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_t* tmp_orig_texdef;
	texdef_to_face_t* temp_texdef_face_list;

	texdef_SI_values.rotate = gtk_spin_button_get_value( GTK_SPIN_BUTTON( rotate_value_spinbutton ) );

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			tmp_orig_texdef = (texdef_t *) &temp_texdef_face_list->orig_texdef;
			tmp_texdef->rotate = texdef_SI_values.rotate + texdef_offset.rotate;
			is_Rotate_conflicting = FALSE;
		}

		GetTexMods();
	}
}


// Step Spins
static void on_hshift_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	gfloat val;
	GtkAdjustment * adjust;

	if ( !g_bListenChanged ) {
		return;
	}

	l_pIncrement = Get_SI_Inc();

#ifdef DBG_SI
	Sys_Printf( "OnIncrementChanged HShift\n" );
#endif

	val = gtk_spin_button_get_value( GTK_SPIN_BUTTON( hshift_step_spinbutton ) );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( hshift_value_spinbutton ) );
	gtk_adjustment_set_step_increment( adjust, val );
	l_pIncrement->shift[0] = val;
}

static void on_vshift_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	gfloat val;
	GtkAdjustment * adjust;

	if ( !g_bListenChanged ) {
		return;
	}

	l_pIncrement = Get_SI_Inc();

#ifdef DBG_SI
	Sys_Printf( "OnIncrementChanged VShift\n" );
#endif

	val = gtk_spin_button_get_value( GTK_SPIN_BUTTON( vshift_step_spinbutton ) );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( vshift_value_spinbutton ) );
	gtk_adjustment_set_step_increment( adjust, val );
	l_pIncrement->shift[1] = val;
}

static void on_hscale_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	gfloat val;
	GtkAdjustment * adjust;

	if ( !g_bListenChanged ) {
		return;
	}

	l_pIncrement = Get_SI_Inc();

#ifdef DBG_SI
	Sys_Printf( "OnIncrementChanged HShift\n" );
#endif

	val = gtk_spin_button_get_value( GTK_SPIN_BUTTON( hscale_step_spinbutton ) );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( hscale_value_spinbutton ) );
	gtk_adjustment_set_step_increment( adjust, val );
	l_pIncrement->scale[0] = val;
}

static void on_vscale_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	gfloat val;
	GtkAdjustment * adjust;

	if ( !g_bListenChanged ) {
		return;
	}

	l_pIncrement = Get_SI_Inc();

#ifdef DBG_SI
	Sys_Printf( "OnIncrementChanged HShift\n" );
#endif

	val = gtk_spin_button_get_value( GTK_SPIN_BUTTON( vscale_step_spinbutton ) );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( vscale_value_spinbutton ) );
	gtk_adjustment_set_step_increment( adjust, val );
	l_pIncrement->scale[1] = val;
}

static void on_rotate_step_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	gfloat val;
	GtkAdjustment * adjust;

	if ( !g_bListenChanged ) {
		return;
	}

	l_pIncrement = Get_SI_Inc();

#ifdef DBG_SI
	Sys_Printf( "OnIncrementChanged HShift\n" );
#endif

	val = gtk_spin_button_get_value( GTK_SPIN_BUTTON( rotate_step_spinbutton ) );
	adjust = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( rotate_value_spinbutton ) );
	gtk_adjustment_set_step_increment( adjust, val );
	l_pIncrement->rotate = val;
}


// Fit Texture
static void on_fit_width_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	m_nWidth = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( fit_width_spinbutton ) );
}

static void on_fit_height_spinbutton_value_changed( GtkSpinButton *spinbutton, gpointer user_data ){
	m_nHeight = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( fit_height_spinbutton ) );
}

static void on_fit_button_clicked( GtkButton *button, gpointer user_data ){
	FaceList_FitTexture( get_texdef_face_list(), m_nHeight, m_nWidth );
	Sys_UpdateWindows( W_ALL );
}


// Axial Button
static void on_axial_button_clicked( GtkButton *button, gpointer user_data ){
	texdef_t* tmp_texdef;
	texdef_to_face_t* temp_texdef_face_list;

	if ( !texdef_face_list_empty() && g_bListenChanged ) {
		for ( temp_texdef_face_list = get_texdef_face_list(); temp_texdef_face_list; temp_texdef_face_list = temp_texdef_face_list->next )
		{
			tmp_texdef = (texdef_t *) &temp_texdef_face_list->texdef;
			tmp_texdef->shift[0] = 0.0;
			tmp_texdef->shift[1] = 0.0;
			tmp_texdef->scale[0] = 0.5;
			tmp_texdef->scale[1] = 0.5;
			tmp_texdef->rotate = 0.0;
		}
	}

	if ( !texdef_face_list_empty() ) {
		SetTexdef_FaceList( get_texdef_face_list(), FALSE, TRUE );
	}

	Sys_UpdateWindows( W_ALL );
}
