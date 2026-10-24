/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodmodel.h
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

#ifndef NOD_MODEL_H
#define NOD_MODEL_H

#include "picomodel.h"
#include <cstddef>
#include <string>
#include <functional>
#include <map>
#include <memory>
#include <vector>

class NMBParser;
struct NodModelSource {
	std::function<bool( std::vector<std::string> &, std::string & )> assets;
	std::function<bool( const std::string &, std::vector<unsigned char> &, std::string & )> load;
	std::function<std::string( const std::string & )> identity;
};

struct NodModelResource {
	std::string reference, asset;
	int frame;
};

class NodModelReferences {
	std::map<std::string, std::shared_ptr<NMBParser> > bundles;
	std::shared_ptr<NMBParser> loadBundle( const std::string &path, const NodModelSource &source, std::string &error );
public:
	void clear(){ bundles.clear(); }
	bool catalog( const std::string &bundle, const NodModelSource &source,
		std::vector<NodModelResource> &resources, std::string &error );
	picoModel_t *load( const std::string &request, const NodModelSource &source, std::string &error );
};

bool NodModelExtensionSupported( const char *extension );
void NodModelClearReferenceCache();
bool NodModelIsSupported( const char *name );
picoModel_t *NodModelLoadBuffer( const char *name, int frame,
	const unsigned char *data, std::size_t size, std::string &error );

#endif
