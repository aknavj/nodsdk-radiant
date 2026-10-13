#ifndef NOD_MODEL_H
#define NOD_MODEL_H

#include "picomodel.h"
#include <cstddef>
#include <string>

bool NodModelExtensionSupported( const char *extension );
bool NodModelIsSupported( const char *name );
picoModel_t *NodModelLoadBuffer( const char *name, int frame,
	const unsigned char *data, std::size_t size, std::string &error );

#endif
