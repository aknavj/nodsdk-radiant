/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: eclass.cpp
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

#include "stdafx.h"
#include <sys/stat.h>
#if defined( __linux__ ) || defined( __BSD__ ) || defined( __APPLE__ )
#include <dirent.h>
#endif
#include "assert.h"
#include "nodtemplates.h"
#include "nodclass.h"

/*
 */
static NodTemplateCatalog nodClasses;
static unsigned nodClassesGeneration;

/*
 */
unsigned NodClasses_Generation(){ 
	return nodClassesGeneration; 
}

/*
 */
const char *NodClasses_Context(){
	return nodClasses.archives.c_str();
}

/*
 */
const NodTemplateChoice *NodClasses_Find( const char *name ){
	for ( size_t i = 0; i < nodClasses.choices.size(); ++i ) {
		if ( g_ascii_strcasecmp( name, nodClasses.choices[i].name.c_str() ) == 0 ) { 
			return &nodClasses.choices[i];
		}
	}

	return NULL;
}

/*
 */
bool NodTemplate_Query( const char *input, const char *name, NodTemplateCatalog &catalog, std::string &error, const char *modifiers ) {
	
	std::vector<std::string> archives;
	if ( !NodTemplate_Archives( input, archives, error ) ) {
		return false;
	} 
	
	Str executable = g_pGameDescription->mNodEditorsPath;
	if ( !executable.GetLength() ) { 
		executable = g_strAppPath; 
		executable += "nod-editors"; 
	}

	AddSlash( executable );
	executable += "NODTemplates";
#ifdef _WIN32
	executable += ".exe";
#endif
	
	if ( !g_file_test( executable, G_FILE_TEST_IS_REGULAR ) ) {
		error = std::string( "Cannot find " ) + executable.GetBuffer() + ". Configure SDK editor binaries in Project Settings.";
		return false;
	}
	
	Str workspace = g_pGameDescription->mEnginePath;
	AddSlash( workspace ); workspace += g_pGameDescription->mBaseGame;
	
	std::vector<std::string> args;
	args.push_back( executable.GetBuffer() );
	args.push_back( "--workspace" ); 
	args.push_back( workspace.GetBuffer() );
	if ( g_pGameDescription->mNodAssetPath.GetLength() ) {
		args.push_back( "--asset-root" ); 
		args.push_back( g_pGameDescription->mNodAssetPath.GetBuffer() );
	}
	
	for ( size_t i = 0; i < archives.size(); ++i ) { 
		args.push_back( "--archive" ); 
		args.push_back( archives[i] ); 
	}
	
	if ( name && *name ) { 
		args.push_back( "--template" ); 
		args.push_back( name ); 
	} else { 
		args.push_back( "--details" ); 
		args.push_back( "1" ); 
	}
	
	if ( modifiers && *modifiers ) { 
		args.push_back( "--modifiers" ); 
		args.push_back( modifiers ); 
	}
	
	std::vector<gchar *> argv;
	for ( size_t i = 0; i < args.size(); ++i ) { 
		argv.push_back( g_strdup( args[i].c_str() ) );
	}
	
	argv.push_back( NULL );
	
	gchar *output = NULL, *diagnostics = NULL;
	gint status = 0;
	GError *failure = NULL;
	
	const bool launched = g_spawn_sync( workspace, &argv[0], NULL, static_cast<GSpawnFlags>( 0 ),
		NULL, NULL, &output, &diagnostics, &status, &failure ) != FALSE;
	
	for ( size_t i = 0; i < args.size(); ++i ) { 
		g_free( argv[i] );
	}

	
	bool success = false;
	if ( !launched || status != 0 ) {
		error = failure ? failure->message :
			diagnostics && *diagnostics ? diagnostics : "SDK template database query failed.";
	} else  {
		success = NodTemplate_ParseCatalog( output, output ? strlen( output ) : 0, catalog, error );
	}

	if ( launched && diagnostics && *diagnostics ) {
		Sys_FPrintf( SYS_WRN, "NOD SDK diagnostics:\n%s", diagnostics );
	}

	if ( failure ) {
		g_error_free( failure );
	}

	g_free( output ); 
	g_free( diagnostics );
	
	return success;
}

/*
 */
bool NodClasses_Load( const char *archives, std::string &error ){

	if ( !g_pGameDescription || g_pGameDescription->mGameFile != SCGHOST_GAME ) {
		return true;
	}

	NodTemplateCatalog next;
	if ( !NodTemplate_Query( archives, NULL, next, error ) ) { 
		return false;
	}

	for ( size_t i = 0; i < next.choices.size(); ++i ) {
		const NodTemplateChoice &choice = next.choices[i];
		if ( !choice.detailed ) { 
			error = "Rebuild NODTemplates: live registration requires detailed catalog metadata."; 
			return false; 
		}
		
		const char *reserved[] = { 
			"worldspawn", "func_group", "misc_model", "misc_gamemodel",
			"model_static", "nod_scene", "nod_template_instance", "nod_nsd_entity",
			"nod_nil_sector", "nod_npd_waypoint", "nod_npd_link", "nod_static_light"
		};
		
		bool collision = false;
		for ( size_t r = 0; r < G_N_ELEMENTS( reserved ); ++r ) {
			if ( g_ascii_strcasecmp( choice.name.c_str(), reserved[r] ) == 0 ) {
				collision = true;
			}
		}

		if ( collision ) {
			error = "A NOT template collides with a reserved Radiant class: " + choice.name; 
			return false;
		}
	}

	// Keep eclass allocations stable: entities and renderers retain these pointers.
	for ( eclass_t *type = eclass; type; type = type->next ) {
		if ( !NodClass_Registered( type->comments ) ) {
			continue;
		}

		free( type->comments );
		type->comments = strdup( "NOD template database v1\nNOD placeable: 0\nTemplate unavailable in the active NOT context.\n" );
	}

	nodClasses = next;
	++nodClassesGeneration;

	for ( size_t i = 0; i < nodClasses.choices.size(); ++i ) {
		const NodTemplateChoice &choice = nodClasses.choices[i];
		if ( !NodTemplate_MapString( choice.name, 31 ) || choice.name == "[unsupported template name]" ) {
			continue;
		}

		std::string comments = "NOD template database v1\nNOD placeable: " + std::string( choice.placeable ? "1" : "0" ) +
			"\nNOD archive: " + choice.archive + "\nNOD archives: " + choice.context + "\nNOD type: " + choice.type +
			"\nNOD parent: " + choice.parent + "\n" + choice.problem + "\n";
		
		vec3_t mins = { -8, -8, -8 }, maxs = { 8, 8, 8 }, color = { 1, 0.6f, 0.2f };

		for ( size_t p = 0; p < choice.effective.size(); ++p ) {
			const NodTemplateProperty &property = choice.effective[p];
			const std::string key = "nod_prop_" + property.id;
			comments += "\"" + key + "\" - " + property.name + "; " + property.storage + "; " +
				( property.editable ? "editable instance override" : property.problem ) + "\nNOD default " + key + ": " + property.value + "\n";
		}
		
		const std::string low = NodClass_Default( comments.c_str(), "nod_prop_6" ), high = NodClass_Default( comments.c_str(), "nod_prop_7" );
		if ( ( !low.empty() || !high.empty() ) && !NodClass_Bounds( low.c_str(), high.c_str(), mins, maxs ) ) {
			VectorSet( mins, -8, -8, -8 ); VectorSet( maxs, 8, 8, 8 );
			Sys_FPrintf( SYS_WRN, "NOT %s: invalid/incomplete editor bounds; using an editor marker.\n", choice.name.c_str() );
		}

		const std::string colour = NodClass_Default( comments.c_str(), "nod_prop_8" );
		if ( !colour.empty() && !ModelTransform_Parse( colour.c_str(), 3, color, true ) ) {
			VectorSet( color, 1, 0.6f, 0.2f );
			Sys_FPrintf( SYS_WRN, "NOT %s: invalid editor colour; using the default colour.\n", choice.name.c_str() );
		}

		eclass_t *type = NULL;
		for ( eclass_t *existing = eclass; existing; existing = existing->next ) {
			if ( g_ascii_strcasecmp( existing->name, choice.name.c_str() ) == 0 ) { 
				type = existing; break; 
			}
		}

		if ( !type ) {
			type = EClass_Create( choice.name.c_str(), color[0], color[1], color[2], &mins, &maxs, comments.c_str() );
			Eclass_InsertAlphabetized( type );
		} else {
			free( type->comments ); 
			type->comments = strdup( comments.c_str() );
			type->fixedsize = true;

			VectorCopy( mins, type->mins ); 
			VectorCopy( maxs, type->maxs ); 
			VectorCopy( color, type->color );

			char texture[128];
			sprintf( texture, "(%f %f %f)", color[0], color[1], color[2] ); 
			type->texdef.SetName( texture );
		}

		type->unknown = false;
		type->nShowFlags = 0;
		
		memset( type->flagnames, 0, sizeof( type->flagnames ) );

		free( type->modelpath ); 
		type->modelpath = NULL;

		free( type->skinpath ); 
		type->skinpath = NULL;
		
		type->nFrame = 0;
	}

	if ( !next.diagnostics.empty() ) {
		Sys_FPrintf( SYS_WRN, "NOT database:\n%s", next.diagnostics.c_str() );
	}

	Sys_Printf( "Registered %u live NOT templates (Global first).\n", static_cast<unsigned>( next.choices.size() ) );
	return true;
}

/*
 */
eclass_t    *eclass = NULL;
eclass_t    *eclass_bad = NULL;
const vec3_t smallbox[2] = {{-8,-8,-8},{8,8,8}};
char eclass_directory[1024];

qboolean parsing_single = false;
eclass_t *eclass_e;

/*!
   implementation of the EClass manager API
 */
eclass_t** Get_EClass_E(){
	return &eclass_e;
}

void Set_Eclass_Found( qboolean b ){
	eclass_found = b;
}

qboolean Get_Parsing_Single(){
	return parsing_single;
}


// md3 cache for misc_models
//eclass_t *g_md3Cache = NULL;

/*

   the classname, color triple, and bounding box are parsed out of comments
   A ? size means take the exact brush size.

   / *QUAKED <classname> (0 0 0) ?
   / *QUAKED <classname> (0 0 0) (-8 -8 -8) (8 8 8)

   Flag names can follow the size description:

   / *QUAKED func_door (0 .5 .8) ? START_OPEN STONE_SOUND DOOR_DONT_LINK GOLD_KEY SILVER_KEY

 */

void CleanEntityList( eclass_t *&pList ){
	while ( pList )
	{
		eclass_t* pTemp = pList->next;

		entitymodel *model = pList->model;
		while ( model != NULL )
		{
			delete []model->pTriList;
			if ( model->strSkin ) {
				g_string_free( (GString *)model->strSkin, TRUE );
			}

			model->strSkin = NULL;
			model = model->pNext;
		}

		if ( pList->modelpath ) {
			free( pList->modelpath );
			pList->modelpath = NULL;
		}

		if ( pList->skinpath ) {
			free( pList->skinpath );
			pList->skinpath = NULL;
		}

		free( pList->name );
		free( pList->comments );
		free( pList );
		pList = pTemp;
	}

	pList = NULL;

}


void CleanUpEntities(){
	nodClasses = NodTemplateCatalog();
	// NOTE: maybe some leak checks needed .. older versions of Radiant looked like they were freezing more stuff
	CleanEntityList( eclass );
	//CleanEntityList(g_md3Cache);
	if ( eclass_bad ) {
		free( eclass_bad->name );
		free( eclass_bad->comments );
		free( eclass_bad );
		eclass_bad = NULL;
	}
}

void EClass_InsertSortedList( eclass_t *&pList, eclass_t *e ){
	eclass_t    *s;

	if ( !pList ) {
		pList = e;
		return;
	}


	s = pList;
	if ( stricmp( e->name, s->name ) < 0 ) {
		e->next = s;
		pList = e;
		return;
	}

	do
	{
		if ( !s->next || stricmp( e->name, s->next->name ) < 0 ) {
			e->next = s->next;
			s->next = e;
			return;
		}

		s = s->next;
	} while ( 1 );
}

/*
   =================
   Eclass_InsertAlphabetized
   =================
 */
void Eclass_InsertAlphabetized( eclass_t *e ){
#if 1
	EClass_InsertSortedList( eclass, e );
#else
	eclass_t    *s;

	if ( !eclass ) {
		eclass = e;
		return;
	}


	s = eclass;
	if ( stricmp( e->name, s->name ) < 0 ) {
		e->next = s;
		eclass = e;
		return;
	}

	do
	{
		if ( !s->next || stricmp( e->name, s->next->name ) < 0 ) {
			e->next = s->next;
			s->next = e;
			return;
		}

		s = s->next;
	} while ( 1 );
#endif
}

/*!
   This looks at each eclass_t, if it has a "modelpath" set then it leaves it alone
   if it's not set it checks to see if a file called "sprites/<eclassname>.*" exists, and
   if it does exist then it sets the "modelpath" to "sprites/<eclassname>.spr"
 */
void Eclass_CreateSpriteModelPaths(){
	int Counts[4] = { 0, 0, 0, 0 };
	char filename[512]; // should be big enough, ExtractFileBase doesn't take a buffer size...
	eclass_t *e;

	// get a list of all sprite/*>* files in all sprite/ directories
	Sys_Printf( "Searching VFS for files in sprites/*.* that match entity names...\n" );
	GSList *pFiles = vfsGetFileList( "sprites", NULL );
	GSList *pFile;

	if ( pFiles ) {

		// find an eclass without a modelpath.
		for ( e = eclass ; e ; e = e->next )
		{
			Counts[0]++;
			if ( e->modelpath ) {
#ifdef _DEBUG
				Sys_Printf( "Ignoring sprite for entity %s (modelpath: \"%s\")\n",e->name,e->modelpath );
#endif
				Counts[1]++;
				continue; // ignore this eclass, it's already got a model
			}

			// TODO: remove this check when we can have sprites for non-fixed size entities.
			if ( !e->fixedsize ) {
#ifdef _DEBUG
				Sys_Printf( "Ignoring sprite for non-fixed-size entity %s\n",e->name );
#endif
				Counts[2]++;
				continue; // can't have sprites for non-fixed size entities (yet!)
			}


			Sys_Printf( "Searching for sprite for fixed-size entity %s...",e->name );

			pFile = pFiles; // point to start of list

			// look for a file that has the same name, with any extension.
			bool Found = FALSE;
			while ( pFile )
			{

				// strip the path/ and the .extension.
				ExtractFileBase( (char *)pFile->data,filename );

				// does the eclass name match the filename?
				if ( stricmp( e->name,filename ) == 0 ) {
					// yes, so generate a sprite filename using the all-encompasing .spr extension
					// so that the model wrapper knows the sprite model plugin will be the model
					// plugin used to render it.
					CString strSpriteName;
					strSpriteName.Format( "sprites/%s.spr",e->name );
					e->modelpath = strdup( strSpriteName.GetBuffer() );
					Sys_Printf( "Found! (\"%s\")\n",(char *)pFile->data );
					Counts[3]++;
					Found = TRUE;
				}

				pFile = pFile->next;
			}

			if ( !Found ) {
				Sys_Printf( "not found\n" );
			}

		}

		vfsClearFileDirList( &pFiles );
	}

	Sys_Printf( "%d entities were scanned\n"
				"%d entities that already had models/sprites were ignored\n"
				"%d non-fixed-size entities were ignored\n"
				"%d entities did not have matching sprite files\n"
				"%d entities had sprite files and have been attached\n",
				Counts[0],Counts[1],Counts[2],Counts[0] - Counts[3],Counts[3] );

}

void EClass_InitForFileList( GSList *pFiles, _EClassTable *pTable ){
	GSList *pFile = pFiles;
	while ( pFile )
	{
		// for a given name, we grab the first .def in the vfs
		// this allows to override baseq3/scripts/entities.def for instance
		char relPath[PATH_MAX];
		strcpy( relPath, "scripts/" );
		strcat( relPath, (char*)pFile->data );
		if ( !vfsGetFullPath( relPath, 0, 0 ) ) {
			Sys_FPrintf( SYS_ERR, "Failed to find the full path for '%s' in the VFS\n", relPath );
		}
		else{
			pTable->m_pfnScanFile( vfsGetFullPath( relPath, 0, 0 ) );
		}

		pFile = pFile->next;
	}
}

/*!
   Manually create an eclass_t, for when no modules exist.
   this replaces and centralizes the eclass_t allocation
 */
eclass_t * EClass_Create( const char *name, float col1, float col2, float col3, const vec3_t *mins, const vec3_t *maxs, const char *comments ){
	eclass_t *e;
	char color[128];

	e = (eclass_t*)malloc( sizeof( *e ) );
	memset( e, 0, sizeof( *e ) );

	e->name = strdup( name );

	// grab the color, reformat as texture name
	e->color[0] = col1;
	e->color[1] = col2;
	e->color[2] = col3;
	sprintf( color, "(%f %f %f)", e->color[0], e->color[1], e->color[2] );
	e->texdef.SetName( color );

	// supplied size ?
	if ( mins && maxs ) {
		// Hydra:
		// If we set worldspawn to be a fixed-size all the textures are
		// displayed as flat-shaded.  This is a KLUDGE now that we have
		// multiple game support as the worldspawn entity is game specific.
		// Note that this is only ever fixed for the user if a definition
		// for the worldspawn entity was not loaded, this can happen for
		// several reasons:
		// a) no entity definition plugin exists
		// b) no entity definition files were found
		// c) no entity definition file contained an entry for worldspawn.

		if ( stricmp( name, "worldspawn" ) != 0 ) {
			e->fixedsize = true;
		}

		// copy the sizes..
		memcpy( e->mins, mins, sizeof( vec3_t ) );
		memcpy( e->maxs, maxs, sizeof( vec3_t ) );
	}

	if ( comments ) {
		e->comments = strdup( comments );
	}
	else
	{
		e->comments = (char*)malloc( 1 );
		e->comments[0] = '\0';
	}

	return e;
}

void Eclass_Init(){
	GSList *pFiles;

	// start by creating the default unknown eclass
	eclass_bad = EClass_Create( "UNKNOWN_CLASS", 0, 0.5, 0,NULL,NULL,NULL );

	// now scan the definitions
	_EClassTable *pTable = &g_EClassDefTable;
	while ( pTable )
	{
		// read in all scripts/*.<extension>
		pFiles = vfsGetFileList( "scripts", pTable->m_pfnGetExtension() );
		if ( pFiles ) {
			GSList *pFile = pFiles;
			while ( pFile )
			{
				/*!
				   \todo the MP/SP filtering rules need to be CLEANED UP and SANITIZED
				 */
				// HACK
				// JKII SP/MP mapping mode
				if ( g_pGameDescription->mGameFile == "jk2.game" || g_pGameDescription->mGameFile == "ja.game" ) {
					if ( !strcmp( ValueForKey( g_qeglobals.d_project_entity, "gamemode" ), "sp" ) ) {
						// SP mapping, ignore mp_*.def
						char *name = (char *)pFile->data;
						if ( name[0] == 'm' && name[1] == 'p' && name[2] == '_' ) {
							Sys_Printf( "Single Player mapping mode. Ignoring '%s'\n", name );
							pFile = pFile->next;
							continue;
						}
					}
					else
					{
						// MP mapping, ignore sp_*.def
						char *name = (char *)pFile->data;
						if ( name[0] == 's' && name[1] == 'p' && name[2] == '_' ) {
							Sys_Printf( "Multiplayer mapping mode. Ignoring '%s'\n", name );
							pFile = pFile->next;
							continue;
						}
					}
				}

				// RIANT
				// STVEF SP/MP mapping mode
				else if ( g_pGameDescription->mGameFile == "stvef.game" ) {
					if ( !strcmp( ValueForKey( g_qeglobals.d_project_entity, "gamemode" ), "sp" ) ) {
						// SP mapping, ignore mp_*.def
						char *name = (char *)pFile->data;
						if ( ( name[0] == 'm' && name[1] == 'p' && name[2] == '_' )
							|| ( name[0] == 'h' && name[1] == 'm' && name[2] == '_' ) ) {
							Sys_Printf( "Single Player mapping mode. Ignoring '%s'\n", name );
							pFile = pFile->next;
							continue;
						}
					}
					else
					{
						// HM mapping, ignore sp_*.def
						char *name = (char *)pFile->data;
						if ( name[0] == 's' && name[1] == 'p' && name[2] == '_' ) {
							Sys_Printf( "HoloMatch mapping mode. Ignoring '%s'\n", name );
							pFile = pFile->next;
							continue;
						}
					}
				}

				// for a given name, we grab the first .def in the vfs
				// this allows to override baseq3/scripts/entities.def for instance
				char relPath[PATH_MAX];
				strcpy( relPath, "scripts/" );
				strcat( relPath, (char*)pFile->data );
				char *fullpath = vfsGetFullPath( relPath, 0, 0 );
				if ( !fullpath ) {
					Sys_FPrintf( SYS_ERR, "Failed to find the full path for \"%s\" in the VFS\n", relPath );
				}
				else{
					pTable->m_pfnScanFile( fullpath );
				}

				if ( g_pGameDescription->mEClassSingleLoad ) {
					break;
				}

				pFile = pFile->next;
			}

			vfsClearFileDirList( &pFiles );
			pFiles = NULL;
		}
		else{
			Sys_FPrintf( SYS_ERR, "Didn't find any scripts/*.%s files to load EClass information\n", pTable->m_pfnGetExtension() );
		}

		// we deal with two formats max, if the other table exists, loop again
		if ( g_bHaveEClassExt && pTable == &g_EClassDefTable ) {
			pTable = &g_EClassExtTable;
		}
		else{
			pTable = NULL; // done, exit
		}
	}

	Eclass_CreateSpriteModelPaths();
	if ( g_pGameDescription->mGameFile == SCGHOST_GAME ) {
		std::string error;
		if ( !NodClasses_Load( "", error ) ) {
			Sys_FPrintf( SYS_ERR, "NOT entity registration failed: %s\n", error.c_str() );
	}
}
}

eclass_t *Eclass_ForName( const char *name, qboolean has_brushes ){
	eclass_t    *e;

	if ( !name || *name == '\0' ) {
		return eclass_bad;
	}

#ifdef _DEBUG
	// grouping stuff, not an eclass
	if ( strcmp( name, "group_info" ) == 0 ) {
		Sys_FPrintf( SYS_WRN, "WARNING: unexpected group_info entity in Eclass_ForName\n" );
	}
#endif

	for ( e = eclass ; e ; e = e->next ) {
		if ( !strcmp( name, e->name ) || ( NodClass_Registered( e->comments ) && !g_ascii_strcasecmp( name, e->name ) ) ) {
			return e;
		}
	}

	// create a new class for it
	if ( has_brushes ) {
		e = EClass_Create( name, 0, 0.5, 0,NULL,NULL,"Not found in source." );
	}
	else
	{
		e = EClass_Create( name, 0, 0.5, 0,&smallbox[0],&smallbox[1],"Not found in source." );
	}

	Eclass_InsertAlphabetized( e );

	return e;
}
