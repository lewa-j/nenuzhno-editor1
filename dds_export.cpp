
#include <fstream>
#include "system/FileSystem.h"
#include "system/neArray.h"
#include "graphics/texture.h"
#include "resource/dds.h"
#include "log.h"

using std::string;
using std::ofstream;

bool ExportDDS(const char *name, neArray<char> &data, int width, int height, int format)
{
	char path[1024];
	g_fs.GetFilePath((string("export/textures/")+string(name)+".dds").c_str(),path,true);

	ofstream file(path,std::ios_base::binary);
	if(!file){
		Log( "Can't open output file %s\n", path);
		return false;
	}

	int ident=DDS_IDENT;
	file.write((char*)&ident,4);

	DDS_HEADER header={};
	header.dwSize = 124;
	header.dwFlags = 1+2+4+0x1000;
	header.dwWidth = width;
	header.dwHeight = height;
	//header.dwPitchOrLinearSize
	header.ddspf={32,0,0,0,0xff000000,0x00ff0000,0x0000ff00,0x000000ff};
	header.dwCaps=0x1000;

	if(format==GL_COMPRESSED_RGB_S3TC_DXT1){
		header.ddspf.dwFourCC = DDS_DXT1;
		header.ddspf.dwFlags|=4;
	}else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5){
		header.ddspf.dwFourCC = DDS_DXT5;
		header.ddspf.dwFlags|=4;
	}else{
		header.ddspf.dwFourCC = 0;
		//header.ddspf.dwFlags|=0x40;
		if(format==GL_RGBA){
			header.ddspf.dwRGBBitCount=32;
		}else if(format==GL_RGB){
			header.ddspf.dwRGBBitCount=24;
			header.ddspf.dwRBitMask = 0x000000ff;
			header.ddspf.dwGBitMask = 0x0000ff00;
			header.ddspf.dwBBitMask = 0x00ff0000;
			header.ddspf.dwABitMask = 0;
			ResampleBGR((uint8_t*)data.data,data.size);
		}else if(format == FMT_BGR8){
			header.ddspf.dwRGBBitCount=24;
		}else{
			Log("ExportDDS: Unsuported format %X\n",format);
			return false;
		}
	}

	file.write((char*)&header, sizeof(DDS_HEADER));
	file.write(data.data,data.size);
	file.close();

	return true;
}
