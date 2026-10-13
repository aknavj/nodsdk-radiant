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

#include "cpicomodel.h"
#include "cpicosurface.h"
#include "nodmodel.h"

static picoModel_t *LoadPicoModel( char *name, int frame ){
	if ( NodModelIsSupported( name ) ) {
		void *buffer = NULL;
		const int size = vfsLoadFile( name, &buffer, 0 );
		std::string error;
		picoModel_t *model = NULL;
		if ( size > 0 && buffer )
			model = NodModelLoadBuffer( name, frame, static_cast<const unsigned char*>( buffer ),
				static_cast<std::size_t>( size ), error );
		else
			error = "Cannot read native model through the VFS";
		if ( buffer ) vfsFreeFile( buffer );
		if ( !model ) Sys_FPrintf( SYS_ERR, "ERROR: %s (_frame %d): %s\n", name, frame, error.c_str() );
		return model;
	}
	picoModel_t *model = PicoLoadModel( name, frame );
	const std::size_t len = strlen( name );
	if ( !model && len >= 4 ) {
		if ( !strcmp( name + len - 4, ".md3" ) ) {
			name[len - 1] = 'c';
			model = PicoLoadModel( name, frame );
		}
		else if ( !strcmp( name + len - 4, ".mdc" ) ) {
			name[len - 1] = '3';
			model = PicoLoadModel( name, frame );
		}
	}
	return model;
}

CPicoModel::CPicoModel( const PicoModelKey& key )
	: m_refcount( 1 ){
	load( key.first.GetBuffer(), key.second );
}

CPicoModel::CPicoModel( const Str& name )
	: m_refcount( 1 ){
	load( name.GetBuffer(), 0 );
}

CPicoModel::CPicoModel( const Str& name, const int frame )
	: m_refcount( 1 ){
	load( name.GetBuffer(), frame );
}

CPicoModel::CPicoModel( const char *name, const int frame )
	: m_refcount( 1 ){
	load( name, frame );
}

void CPicoModel::load( const char *name, const int frame ){
	CPicoSurface *surf;
	picoSurface_t *pSurface;
	int i;

	m_name = new char[strlen( name ) + 1];
	strcpy( m_name,name );

	m_frame = frame;
	m_children = g_ptr_array_new();
	m_pModel = LoadPicoModel( m_name, frame );

	if ( m_pModel ) {
		aabb_clear( &m_BBox );
		for ( i = 0; i < PicoGetModelNumSurfaces( m_pModel ); i++ )
		{
			pSurface = PicoGetModelSurface( m_pModel,i );
			surf = new CPicoSurface( pSurface );
			g_ptr_array_add( m_children, surf );
			aabb_extend_by_aabb( &m_BBox, surf->GetAABB() );
		}
	}
	else
	{
		m_BBox.origin[0] = m_BBox.origin[1] = m_BBox.origin[2] = 0;
		m_BBox.extents[0] = m_BBox.extents[1] = m_BBox.extents[2] = 0;
	}

	m_parents = g_ptr_array_new();
}

CPicoModel::~CPicoModel(){
	for ( unsigned int i = 0; i < m_children->len; i++ )
		( (CPicoSurface*)m_children->pdata[i] )->DecRef();
	g_ptr_array_free( m_children, TRUE );
	PicoFreeModel( m_pModel );
	g_ptr_array_free( m_parents, TRUE );
	delete [] m_name;
}

void CPicoModel::AddParent( CPicoParent *parent ){
	g_ptr_array_add( m_parents, parent );
}

void CPicoModel::RemoveParent( CPicoParent *parent ){
	unsigned int i;
	for ( i = 0; i < m_parents->len; i++ ) {
		if ( parent == (CPicoParent*)m_parents->pdata[i] ) {
			g_ptr_array_remove_index_fast( m_parents, i );
		}
	}
}

void CPicoModel::Reload( void ){
	CPicoSurface *surf;
	picoSurface_t *pSurface;
	int i;
	unsigned int j;

	// Get rid of the old model
	for ( j = 0; j < m_children->len; j++ )
		( (CPicoSurface*)m_children->pdata[j] )->DecRef();
	g_ptr_array_set_size( m_children, 0 );
	PicoFreeModel( m_pModel );

	// And reload it
	m_pModel = LoadPicoModel( m_name, m_frame );

	if ( m_pModel ) {
		aabb_clear( &m_BBox );
		for ( i = 0; i < PicoGetModelNumSurfaces( m_pModel ); i++ )
		{
			pSurface = PicoGetModelSurface( m_pModel,i );
			surf = new CPicoSurface( pSurface );
			g_ptr_array_add( m_children, surf );
			aabb_extend_by_aabb( &m_BBox, surf->GetAABB() );
		}
	}
	else
	{
		m_BBox.origin[0] = m_BBox.origin[1] = m_BBox.origin[2] = 0;
		m_BBox.extents[0] = m_BBox.extents[1] = m_BBox.extents[2] = 0;
	}

	for ( j = 0; j < m_parents->len; j++ ) {
		( (CPicoParent*)m_parents->pdata[j] )->UpdateShaders();
	}
}

void CPicoModel::Draw( int state, std::vector<IShader*> shaders, int rflags ) const {
	if ( m_pModel ) {
		for ( unsigned int i = 0; i < m_children->len; i++ )
			( (CPicoSurface*)m_children->pdata[i] )->Draw( state, shaders[i], rflags );
	}
}

void CPicoModel::Draw( int state, int rflags ) const {
	if ( m_pModel ) {
		for ( unsigned int i = 0; i < m_children->len; i++ )
			( (CPicoSurface*)m_children->pdata[i] )->Draw( state, rflags );
	}
}

bool CPicoModel::TestRay( const ray_t *ray, vec_t *dist ) const {
	vec_t dist_start = *dist;
	vec_t dist_local = *dist;
	ray_t ray_local = *ray;

	if ( !m_pModel ) {
		return false;
	}

	if ( !aabb_intersect_ray( &m_BBox, &ray_local, &dist_local ) ) {
		return false;
	}
	dist_local = dist_start;

	for ( unsigned int i = 0; i < m_children->len; i++ )
	{
		if ( ( (CPicoSurface*)m_children->pdata[i] )->TestRay( &ray_local, &dist_local ) ) {
			*dist = dist_local;
		}
	}

	return *dist < dist_start;
}

int CPicoModel::GetNumSurfaces( void ){
	if ( !m_pModel ) {
		return 0;
	}

	return m_children->len;
}

char *CPicoModel::GetShaderNameForSurface( const unsigned int surf ){
	if ( !m_pModel || surf >= m_children->len ) {
		return 0;
	}

	return ( (CPicoSurface*)m_children->pdata[surf] )->GetShaderName();
}
