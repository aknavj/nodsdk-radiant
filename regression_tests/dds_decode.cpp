#include "ddsimage.h"
#include "../plugins/shaders/nod_materials.h"

#include <fstream>
#include <iostream>
#include <iterator>

int main( int argc, char **argv ){
	if ( argc == 2 && std::string( argv[1] ) == "--index-test" ) {
		NodTextureIndex index;
		index.Add( "Materials/Metal/Panel.dds" );
		index.Add( "Materials/Metal/Panel.tga" );
		if ( index.Resolve( "panel" ) != "Materials/Metal/Panel.dds" ||
			 index.Resolve( "METAL\\PANEL.DDS" ) != "Materials/Metal/Panel.dds" ||
			 index.Resolve( "Materials/Metal/Panel" ) != "Materials/Metal/Panel.dds" ||
			 !index.Resolve( "wrong/panel" ).empty() ) {
			return 1;
		}
		index.Add( "Materials/Stone/Panel.dds" );
		if ( !index.Resolve( "panel.dds" ).empty() ||
			 index.Resolve( "stone/panel" ) != "Materials/Stone/Panel.dds" ) {
			return 1;
		}
		index.Add( "3D/Materials/ModelSkin.dds" );
		if ( index.Resolve( "modelskin" ) != "3D/Materials/ModelSkin.dds" ||
			 index.Resolve( "3D\\Materials\\MODELSKIN.DDS" ) != "3D/Materials/ModelSkin.dds" ) {
			return 1;
		}
		index.Clear();
		return index.Resolve( "metal/panel" ).empty() ? 0 : 1;
	}
	if ( argc != 3 ) {
		std::cerr << "Usage: dds_decode input.dds output.rgba\n";
		return 2;
	}
	std::ifstream input( argv[1], std::ios::binary );
	if ( !input ) {
		std::cerr << "Unable to read DDS input\n";
		return 2;
	}
	std::vector<unsigned char> data( ( std::istreambuf_iterator<char>( input ) ), std::istreambuf_iterator<char>() );
	DDSImage image;
	std::string error;
	if ( !DDSDecodeImage( data.data(), data.size(), image, error ) ) {
		std::cerr << error << '\n';
		return 1;
	}
	std::ofstream output( argv[2], std::ios::binary );
	output.write( reinterpret_cast<const char *>( image.pixels.data() ), image.pixels.size() );
	if ( !output ) {
		std::cerr << "Unable to write RGBA output\n";
		return 2;
	}
	std::cout << image.width << ' ' << image.height << '\n';
	return 0;
}
