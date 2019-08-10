
#pragma once

#include <string>
#include "graphics/texture.h"
#include "PCCObject.h"
#include "system/neArray.h"

enum e_imgStorage
{
	arcCpr = 0x3, // archive compressed
	arcUnc = 0x1, // archive uncompressed (DLC)
	pccSto = 0x0, // pcc local storage
	empty = 0x21  // unused image (void pointer sorta)
};

struct imgInfo_t
{
	e_imgStorage storageType;
	int uncSize;
	int compSize;
	int offset;
	int w;
	int h;
};

class PCCFile;

class PCCTexture2D: public PCCObject
{
public:
	PCCTexture2D(PCCFile *p,int id);
	~PCCTexture2D();

	int width;
	int height;
	int fmt;
	int numMips;
	imgInfo_t *imgInfos;
	bool mip;
	PCCFile *pcc;
	std::string *arcName;
	Texture *tex;
	bool def;
	int firstLod;

	bool GetData(int &lod,neArray<char> &data,int &format);
	Texture *GetGLTexture(Texture *defTex=0,int lod=0);
};
