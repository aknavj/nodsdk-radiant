#include "ddsimage.h"
#include "ddslib.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {
uint32_t Read32( const unsigned char *data ){
	return uint32_t( data[0] ) | ( uint32_t( data[1] ) << 8 ) |
		   ( uint32_t( data[2] ) << 16 ) | ( uint32_t( data[3] ) << 24 );
}

void Write32( unsigned char *data, uint32_t value ){
	for ( size_t byte = 0; byte < 4; ++byte ) {
		data[byte] = static_cast<unsigned char>( value >> ( byte * 8 ) );
	}
}

bool ValidMask( uint32_t mask ){
	if ( !mask ) {
		return true;
	}
	while ( !( mask & 1 ) ) {
		mask >>= 1;
	}
	return ( mask & ( mask + 1 ) ) == 0;
}

unsigned char Channel( uint32_t value, uint32_t mask, unsigned char absent ){
	if ( !mask ) {
		return absent;
	}
	value &= mask;
	while ( !( mask & 1 ) ) {
		mask >>= 1;
		value >>= 1;
	}
	return static_cast<unsigned char>( ( uint64_t( value ) * 255 + mask / 2 ) / mask );
}
}

bool DDSDecodeImage( const unsigned char *data, size_t length, DDSImage &image, std::string &error ){
	image = DDSImage();
	error.clear();
	if ( !data || length < 128 || memcmp( data, "DDS ", 4 ) ||
		 Read32( data + 4 ) != 124 || Read32( data + 76 ) != 32 ) {
		error = "invalid or truncated DDS header";
		return false;
	}
	const uint32_t width = Read32( data + 16 ), height = Read32( data + 12 );
	if ( !width || !height || width > 16384 || height > 16384 ||
		 uint64_t( width ) * height > 64 * 1024 * 1024 ) {
		error = "DDS dimensions exceed the supported image bounds";
		return false;
	}
	const uint32_t fourCC = Read32( data + 84 );
	std::vector<unsigned char> pixels;
	if ( fourCC ) {
		if ( memcmp( data + 84, "DXT", 3 ) || data[87] < '1' || data[87] > '5' ) {
			error = "unsupported DDS compression (expected DXT1 through DXT5)";
			return false;
		}
		const size_t paddedWidth = ( width + 3 ) & ~size_t( 3 );
		const size_t paddedHeight = ( height + 3 ) & ~size_t( 3 );
		const size_t payload = paddedWidth / 4 * ( paddedHeight / 4 ) * ( data[87] == '1' ? 8 : 16 );
		if ( payload > length - 128 ) {
			error = "truncated DDS compressed pixel data";
			return false;
		}
		// ddslib's in-memory header contains a native pointer; never cast an on-disk header to it.
		std::vector<unsigned char> storage( offsetof( ddsBuffer_t, data ) + payload, 0 );
		ddsBuffer_t *dds = reinterpret_cast<ddsBuffer_t *>( storage.data() );
		memcpy( dds->magic, "DDS ", 4 );
		Write32( reinterpret_cast<unsigned char *>( &dds->size ), 124 );
		Write32( reinterpret_cast<unsigned char *>( &dds->width ), static_cast<uint32_t>( paddedWidth ) );
		Write32( reinterpret_cast<unsigned char *>( &dds->height ), static_cast<uint32_t>( paddedHeight ) );
		memcpy( &dds->pixelFormat.fourCC, data + 84, 4 );
		memcpy( dds->data, data + 128, payload );
		std::vector<unsigned char> padded( paddedWidth * paddedHeight * 4 );
		if ( DDSDecompress( dds, padded.data() ) ) {
			error = "DDS decompression failed";
			return false;
		}
		pixels.resize( size_t( width ) * height * 4 );
		for ( size_t row = 0; row < height; ++row ) {
			memcpy( pixels.data() + row * width * 4, padded.data() + row * paddedWidth * 4, size_t( width ) * 4 );
		}
		if ( data[87] == '2' || data[87] == '4' ) {
			for ( size_t i = 0; i < pixels.size(); i += 4 ) {
				const unsigned int alpha = pixels[i + 3];
				for ( size_t channel = 0; channel < 3; ++channel ) {
					pixels[i + channel] = alpha ? static_cast<unsigned char>(
						std::min( 255u, ( pixels[i + channel] * 255u + alpha / 2 ) / alpha ) ) : 0;
				}
			}
		}
	} else {
		const uint32_t bits = Read32( data + 88 );
		uint32_t masks[4];
		for ( size_t i = 0; i < 4; ++i ) {
			masks[i] = Read32( data + 92 + i * 4 );
		}
		if ( !( Read32( data + 80 ) & 0x40 ) || ( bits != 16 && bits != 24 && bits != 32 ) ) {
			error = "unsupported DDS uncompressed pixel format";
			return false;
		}
		uint32_t used = 0;
		for ( size_t i = 0; i < 4; ++i ) {
			if ( !ValidMask( masks[i] ) || ( used & masks[i] ) ||
				 ( bits < 32 && ( masks[i] >> bits ) ) || ( i < 3 && !masks[i] ) ) {
				error = "invalid DDS channel masks";
				return false;
			}
			used |= masks[i];
		}
		const size_t bytes = bits / 8, tightPitch = size_t( width ) * bytes;
		const size_t pitch = ( Read32( data + 8 ) & 8 ) ? Read32( data + 20 ) : tightPitch;
		if ( pitch < tightPitch || pitch > ( length - 128 ) / height ) {
			error = "truncated DDS pixels or invalid row pitch";
			return false;
		}
		pixels.resize( size_t( width ) * height * 4 );
		for ( size_t row = 0; row < height; ++row ) {
			for ( size_t column = 0; column < width; ++column ) {
				const unsigned char *source = data + 128 + row * pitch + column * bytes;
				uint32_t value = 0;
				for ( size_t byte = 0; byte < bytes; ++byte ) {
					value |= uint32_t( source[byte] ) << ( byte * 8 );
				}
				for ( size_t channel = 0; channel < 4; ++channel ) {
					pixels[( row * width + column ) * 4 + channel] = Channel( value, masks[channel], channel == 3 ? 255 : 0 );
				}
			}
		}
	}
	image.width = static_cast<int>( width );
	image.height = static_cast<int>( height );
	image.pixels.swap( pixels );
	return true;
}
