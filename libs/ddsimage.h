#ifndef DDSIMAGE_H
#define DDSIMAGE_H

#include <cstddef>
#include <string>
#include <vector>

struct DDSImage {
	int width;
	int height;
	std::vector<unsigned char> pixels;
	DDSImage() : width( 0 ), height( 0 ) {}
};

bool DDSDecodeImage( const unsigned char *data, size_t length, DDSImage &image, std::string &error );

#endif
