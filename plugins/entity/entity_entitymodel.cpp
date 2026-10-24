/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: entity_entitymodel.cpp
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
#include "../../radiant/nodclass.h"
#include "entity.h"
#include "../../radiant/nodbounds.h"

void Entity_UpdateClass( entity_t *e, const char* value ){
	if ( e->model.pRender ) {
		e->model.pRender->DecRef();
	}

	if ( e->model.pSelect ) {
		e->model.pSelect->DecRef();
	}

	if ( e->model.pEdit ) {
		e->model.pEdit->DecRef();
	}

	e->model.pRender = NULL;
	e->model.pSelect = NULL;
	e->model.pEdit = NULL;
	if ( ModelTransform_IsEntity( value, ValueForKey( e, "model" ) ) ) {
		CEntityMiscModel *model = new CEntityMiscModel( e );

		e->model.pRender = (IRender*)model;
		e->model.pRender->IncRef();
		e->model.pSelect = (ISelect*)model;
		e->model.pSelect->IncRef();
		e->model.pEdit = (IEdit*)model;
		e->model.pEdit->IncRef();

		model->DecRef();
	}
	else if ( e->eclass && ( e->eclass->modelpath || NodEntity_IsPoint( value ) || NodClass_Registered( e->eclass->comments ) ) ) {
		CEntityEclassModel *model = new CEntityEclassModel( e );

		model->SetEclass( e->eclass );
		if ( e->eclass->modelpath && !NodEntity_IsPoint( value ) ) {
			model->SetName( e->eclass->modelpath );
		}

		e->model.pRender = (IRender*)model;
		e->model.pRender->IncRef();
		e->model.pSelect = (ISelect*)model;
		e->model.pSelect->IncRef();
		e->model.pEdit = (IEdit*)model;
		e->model.pEdit->IncRef();

		model->DecRef();
	}
}

void pivot_draw( const vec3_t pivot ){
	vec3_t vCenter, vMin, vMax;
	VectorCopy( pivot, vCenter );

	g_QglTable.m_pfn_qglPointSize( 4 );

	g_QglTable.m_pfn_qglBegin( GL_POINTS );
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	g_QglTable.m_pfn_qglEnd();

	g_QglTable.m_pfn_qglBegin( GL_LINES );
	vCenter[0] -= 8;
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	vCenter[0] += 16;
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	vCenter[0] -= 8;
	vCenter[1] -= 8;
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	vCenter[1] += 16;
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	vCenter[1] -= 8;
	vCenter[2] -= 8;
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	vCenter[2] += 16;
	g_QglTable.m_pfn_qglVertex3fv( vCenter );
	vCenter[2] -= 8;
	g_QglTable.m_pfn_qglEnd();

	VectorCopy( vCenter, vMin );
	VectorCopy( vCenter, vMax );
	vMin[0] -= 4;
	vMin[1] -= 4;
	vMin[2] -= 4;
	vMax[0] += 4;
	vMax[1] += 4;
	vMax[2] += 4;

	g_QglTable.m_pfn_qglBegin( GL_LINE_LOOP );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMin[1],vMin[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMin[1],vMin[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMax[1],vMin[2] );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMax[1],vMin[2] );
	g_QglTable.m_pfn_qglEnd();

	g_QglTable.m_pfn_qglBegin( GL_LINE_LOOP );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMin[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMin[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMax[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMax[1],vMax[2] );
	g_QglTable.m_pfn_qglEnd();

	g_QglTable.m_pfn_qglBegin( GL_LINES );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMin[1],vMin[2] );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMin[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMax[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMin[0],vMax[1],vMin[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMin[1],vMin[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMin[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMax[1],vMax[2] );
	g_QglTable.m_pfn_qglVertex3f( vMax[0],vMax[1],vMin[2] );
	g_QglTable.m_pfn_qglEnd();
}
