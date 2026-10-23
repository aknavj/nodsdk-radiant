/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: eclassmodel.cpp
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

#include <stdlib.h>

#include "entity_entitymodel.h"
#include "entity.h"
#include "../../radiant/nodbounds.h"
#include "../../radiant/nodclass.h"
#include "../../radiant/nodmodelreference.h"

static bool NodModel_Value( entity_t *entity, const char *key, const char *storage, std::string &value ){
	for ( epair_t *pair = entity->epairs; pair; pair = pair->next ) {
		if ( strcmp( pair->key, key ) == 0 ) { value = pair->value; return true; }
	}

	std::string stream, error;
	if ( !NodClass_Modifiers( ValueForKey( entity, "nod_modifier_count" ),
		[entity]( const char *name ){ return ValueForKey( entity, name ); }, stream, error ) ) {
		Sys_FPrintf( SYS_WRN, "NOT representation: %s\n", error.c_str() ); return false;
	}

	if ( !NodClass_ResolveValue( entity->eclass->comments, stream, key, storage,
		[]( const char *name ){ return Eclass_ForName( name, false )->comments; }, value ) ) {
		Sys_FPrintf( SYS_WRN, "NOT representation: cannot resolve %s on %s.\n", key, entity->eclass->name ); return false;
	}

	return true;
}

//
// CEntityEclassModel implementation
//

CEntityEclassModel::CEntityEclassModel ( entity_t *entity ){
	refCount = 1;
	m_eclass = NULL;
	m_model = NULL;
	m_entity = entity;
	m_nativePoint = false;
	m_template = false;
	m_lightRadius = 0;
	VectorSet( m_translate, 0,0,0 );
	VectorSet( m_euler, 0,0,0 );
	VectorSet( m_scale, 1,1,1 );
	VectorSet( m_pivot, 0,0,0 );
	m4x4_identity( m_transform );
	m4x4_identity( m_inverse_transform );
}

CEntityEclassModel::~CEntityEclassModel (){
	if ( m_name.c_str()[0] != '\0'
		 && m_version.c_str()[0] != '\0' ) {
		GetModelCache()->DeleteByID( m_name.c_str(), m_version.c_str() );
	}
}


// IRender

void CEntityEclassModel::Draw( int state, int rflags ) const {
	if ( m_nativePoint || ( m_template && !( m_model && m_model->pRender && m_model->pRender->IsModelNotNull() ) ) ) {
		g_QglTable.m_pfn_qglBegin( GL_LINES );
		for ( int corner = 0; corner < 8; ++corner ) {
			for ( int axis = 0; axis < 3; ++axis ) {
				if ( corner & ( 1 << axis ) ) {
					continue;
				}

				vec3_t first, second;
				for ( int component = 0; component < 3; ++component ) {
					first[component] = m_BBox.origin[component] +
						( ( corner & ( 1 << component ) ) ? m_BBox.extents[component] : -m_BBox.extents[component] );
					second[component] = first[component];
				}

				second[axis] += 2 * m_BBox.extents[axis];
				g_QglTable.m_pfn_qglVertex3fv( first );
				g_QglTable.m_pfn_qglVertex3fv( second );
			}
		}

		if ( m_lightRadius > 0 ) {
			const int segments = 64;
			for ( int plane = 0; plane < 3; ++plane ) {
				const int firstAxis = ( plane + 1 ) % 3, secondAxis = ( plane + 2 ) % 3;
				for ( int segment = 0; segment < segments; ++segment ) {
					for ( int end = 0; end < 2; ++end ) {
						const double angle = ( ( segment + end ) % segments ) * 2 * Q_PI / segments;
						vec3_t point;
						VectorCopy( m_translate, point );
						point[firstAxis] += static_cast<float>( m_lightRadius * cos( angle ) );
						point[secondAxis] += static_cast<float>( m_lightRadius * sin( angle ) );
						g_QglTable.m_pfn_qglVertex3fv( point );
					}
				}
			}
		}

		g_QglTable.m_pfn_qglEnd();
		return;
	}

	// push the current modelview matrix
	// FIXME: put in a check for stack recursion depth..
	// or avoid recursion of opengl matrix stack
	g_QglTable.m_pfn_qglPushMatrix();
	// apply the parent-to-local transform
	g_QglTable.m_pfn_qglMultMatrixf( m_transform );

	// draw children
	if ( m_model && m_model->pRender ) {
		m_model->pRender->Draw( state, rflags );
	}

	g_QglTable.m_pfn_qglPopMatrix();
}

// ISelect

bool CEntityEclassModel::TestRay( const ray_t *ray, vec_t *dist ) const {
	vec_t dist_start = *dist;
	vec_t dist_local = *dist;
	ray_t ray_local = *ray;

	if ( aabb_intersect_ray( &m_BBox, &ray_local, &dist_local ) ) {
		*dist = dist_local;
	}

	return *dist < dist_start;
}


//IEdit

void CEntityEclassModel::Translate( const vec3_t translation ){
	VectorIncrement( translation, m_translate );
	UpdateCachedData();
}

void CEntityEclassModel::Rotate( const vec3_t pivot, const vec3_t rotation ){
	m4x4_t rotation_matrix;

	m4x4_identity( rotation_matrix );
	m4x4_pivoted_rotate_by_vec3( rotation_matrix, rotation, eXYZ, pivot );
	m4x4_transform_point( rotation_matrix, m_translate );

	VectorIncrement( rotation, m_euler );

	UpdateCachedData();
}

void CEntityEclassModel::OnKeyValueChanged( entity_t *e, const char *key, const char* value ){
	if ( m_nativePoint && strcmp( m_eclass->name, "nod_static_light" ) == 0 && strcmp( key, "radius" ) == 0 ) {
		float radius[3] = { 10, 0, 0 };
		m_lightRadius = 0;
		if ( ( *value && !ModelTransform_Parse( value, 1, radius, true ) ) ||
			radius[0] < 0.01f || radius[0] > 65536 ) {
			Sys_FPrintf( SYS_WRN, "WARNING: invalid NOD light radius '%s' (expected 0.01 through 65536); radius preview disabled.\n", value );
			return;
		}

		m_lightRadius = radius[0];
	}
	else if ( strcmp( key,"origin" ) == 0 ) {
		if ( m_nativePoint ) {
			float position[3];
			if ( !ModelTransform_Parse( value, 3, position, true ) ) {
				Sys_FPrintf( SYS_WRN, "WARNING: invalid native entity origin: %s\n", value );
				return;
			}

			VectorCopy( position, m_translate );
		}
		else {
			sscanf( value, "%f %f %f", &m_translate[0], &m_translate[1], &m_translate[2] );
		}

		UpdateCachedData();
	}
	else if ( strcmp( key,"angle" ) == 0 ) {
		VectorSet( m_euler, 0, 0, (float) atof( value ) );
		UpdateCachedData();
	}
	else if ( m_template && strcmp( key, "angles" ) == 0 ) {
		vec3_t angles = { 0, 0, 0 };
		if ( *value && !ModelTransform_Parse( value, 3, angles, true ) ) {
			Sys_FPrintf( SYS_WRN, "NOT representation: invalid editor angles: %s\n", value ); return;
		}

		VectorSet( m_euler, angles[2], angles[0], angles[1] );
		UpdateCachedData();
	}
	else if ( m_template && ( strncmp( key, "nod_prop_", 9 ) == 0 || strncmp( key, "nod_modifier_", 13 ) == 0 ||
		strcmp( key, "nod_model_bundle" ) == 0 ) ) {
		UpdateCachedData();
	}
	else if ( m_nativePoint && ( strcmp( key, "nod_bounds_min" ) == 0 ||
		strcmp( key, "nod_bounds_max" ) == 0 || strcmp( key, "nod_rotation" ) == 0 ) ) {
		UpdateCachedData();
	}
}

void CEntityEclassModel::SetEclass( const eclass_t* eclass ){
	m_eclass = eclass;
	m_nativePoint = m_entity && eclass && NodEntity_IsPoint( eclass->name );
	m_template = m_entity && eclass && NodClass_Registered( eclass->comments );
	m_lightRadius = 0;
	if ( m_nativePoint && strcmp( eclass->name, "nod_static_light" ) == 0 ) {
		OnKeyValueChanged( m_entity, "radius", ValueForKey( m_entity, "radius" ) );
	}

	if ( m_nativePoint || m_template ) {
		UpdateCachedData();
}
}

void CEntityEclassModel::SetName( const char *name ){
	if ( strcmp( m_name.c_str(), name ) == 0 ) {
		return;
	}

	if ( m_name.c_str()[0] != '\0'
		 && m_version.c_str()[0] != '\0' ) {
		GetModelCache()->DeleteByID( m_name.c_str(), m_version.c_str() );
	}

	m_model = NULL;
	m_name = name;

	if ( m_name.c_str()[0] != '\0' ) {
		if ( NodModelReference_IsRequest( m_name.c_str() ) ) {
			m_version = "nod";
			m_model = GetModelCache()->GetByID( m_name.c_str(), m_version.c_str() );
			UpdateCachedData();
			return;
		}

		const char* dot = strrchr( m_name.c_str(), '.' );
		if ( dot != NULL ) {
			m_version = ++dot;
			m_model = GetModelCache()->GetByID( m_name.c_str(), m_version.c_str() );
		}
	}

	UpdateCachedData();
}

//
// CEntityEclassModel
//

// private:

void CEntityEclassModel::UpdateCachedData(){
	vec3_t translated, scaled;
	VectorCopy( m_translate, translated ); VectorCopy( m_scale, scaled );
	vec3_t editorMins, editorMaxs;
	if ( m_template ) {
		std::string model, minimum, maximum, scale, offset;
		if ( !NodModel_Value( m_entity, "nod_prop_4", "string32", model ) ||
			!NodModel_Value( m_entity, "nod_prop_6", "vector3", minimum ) ||
			!NodModel_Value( m_entity, "nod_prop_7", "vector3", maximum ) ||
			!NodModel_Value( m_entity, "nod_prop_143", "float32", scale ) ||
			!NodModel_Value( m_entity, "nod_prop_175", "vector3", offset ) ) {
			return;
		}

		if ( !ModelTransform_Native( scale.c_str(), offset.c_str(), m_translate, m_scale, translated, scaled ) ) {
			Sys_FPrintf( SYS_WRN, "NOT representation: invalid model scale/offset on %s.\n", m_eclass->name ); return;
		}

		if ( scaled[0] == 0 ) { model.clear(); VectorSet( scaled, 1, 1, 1 ); }
		if ( !NodClass_Bounds( minimum.c_str(), maximum.c_str(), editorMins, editorMaxs ) ) {
			if ( !minimum.empty() || !maximum.empty() ) {
				Sys_FPrintf( SYS_WRN, "NOT representation: invalid editor bounds on %s; using class bounds.\n", m_eclass->name );
			}

			VectorCopy( m_eclass->mins, editorMins ); VectorCopy( m_eclass->maxs, editorMaxs );
		}

		for ( size_t i = 0; i < model.size(); ++i ) {
			if ( model[i] == '\\' ) {
				model[i] = '/';
			}
		}

		if ( !model.empty() ) {
			entity_t *world = static_cast<entity_t *>( g_FuncTable.m_pfnGetEntityHandle( 0 ) );
			const std::string bundle = world ? ValueForKey( world, "nod_model_bundle" ) : "";
			if ( !NodModelReference_Path( model ) || !NodModelReference_Bundle( bundle ) ) {
				Sys_FPrintf( SYS_WRN, "NOT representation: invalid model reference or per-map bundle on %s.\n", m_eclass->name );
				model.clear();
			}
			else {
				const size_t dot = model.find_last_of( '.' ), slash = model.find_last_of( '/' );
				const bool native = dot == std::string::npos || ( slash != std::string::npos && dot < slash ) ||
					g_ascii_strcasecmp( model.c_str() + dot, ".nod" ) == 0 || g_ascii_strcasecmp( model.c_str() + dot, ".nmb" ) == 0;
				if ( native ) {
					model = NodModelReference_Request( model, bundle );
				} else if ( slash == std::string::npos ) {
					model = "3D/Models/" + model;
			}
		}
		}

		if ( model != m_name.c_str() ) { SetName( model.c_str() ); return; }
	}

	if ( m_nativePoint ) {
		if ( !NodEntity_ComputeBounds( ValueForKey( m_entity, "nod_bounds_min" ),
			ValueForKey( m_entity, "nod_bounds_max" ), ValueForKey( m_entity, "nod_rotation" ),
			m_translate, m_BBox ) ) {
			Sys_FPrintf( SYS_WRN, "WARNING: invalid native bounds on %s; using point marker\n",
				ValueForKey( m_entity, "nod_id" ) );
			NodEntity_ComputeBounds( "", "", "", m_translate, m_BBox );
		}

		return;
	}

	aabb_t aabb_temp;

	aabb_clear( &aabb_temp );

	m4x4_identity( m_transform );
	m4x4_pivoted_transform_by_vec3( m_transform, translated, m_euler, eXYZ, scaled, m_pivot );
	memcpy( m_inverse_transform, m_transform, sizeof( m4x4_t ) );
	if ( m4x4_invert( m_inverse_transform ) == 1 ) {
		Sys_FPrintf( SYS_ERR, "ERROR: Singular Matrix, cannot invert" );
	}

	if ( m_template ) {
		m4x4_t editorTransform;
		m4x4_identity( editorTransform );
		const vec3_t unit = { 1, 1, 1 };
		m4x4_pivoted_transform_by_vec3( editorTransform, m_translate, m_euler, eXYZ, unit, m_pivot );
		aabb_construct_for_vec3( &aabb_temp, editorMins, editorMaxs );
		aabb_for_transformed_aabb( &m_BBox, &aabb_temp, editorTransform );
		return;
	}

	if ( m_eclass ) {
		aabb_construct_for_vec3( &aabb_temp, m_eclass->mins, m_eclass->maxs );
	}
	else{
		VectorSet( aabb_temp.extents, 8, 8, 8 );
	}

	aabb_for_transformed_aabb( &m_BBox, &aabb_temp, m_transform );
}
