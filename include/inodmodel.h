/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: inodmodel.h
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

#ifndef RADIANT_INODMODEL_H
#define RADIANT_INODMODEL_H

#include <glib.h>

#define NOD_MODEL_MAJOR "nod-model-resources"
#define NOD_MODEL_VERSION "1"

// Callback strings are borrowed for the duration of the call; frame -1 denotes an uninspected NMB file.
typedef void ( *PFN_NOD_MODEL_RESOURCE )( void *data, const char *reference, const char *asset, int frame );
typedef bool ( *PFN_NOD_MODEL_CATALOG )( const char *bundle, PFN_NOD_MODEL_RESOURCE emit, void *data, GError **error );
typedef bool ( *PFN_NOD_MODEL_CHECK )( const char *bundle, const char *reference,
	PFN_NOD_MODEL_RESOURCE emit, void *data, GError **error );

struct _QERNodModelTable {
	int m_nSize;
	PFN_NOD_MODEL_CATALOG m_pfnCatalog;
	PFN_NOD_MODEL_CHECK m_pfnCheck;
};

#endif
