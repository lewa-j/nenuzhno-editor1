
#include <fstream>
#include "system/neArray.h"
#include "system/FileSystem.h"
#include "log.h"
#include "resource/vtf.h"

using std::string;
using std::ofstream;

bool ExportVTF(const char *name, neArray<char> &data, int width, int height, int format)
{

	char path[1024];
	g_fs.GetFilePath((string("export/textures/")+string(name)+".vtf").c_str(),path,true);

	ofstream file(path,std::ios_base::binary);
	if(!file){
		Log( "Can't open output file %s\n", path);
		return false;
	}

	VTFFileHeader_t header;
	//TODO

	file.close();
	return true;
}
