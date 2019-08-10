
#include <cmath>
#include "PCCTexture2D.h"
#include "log.h"
#include "pcc.h"
#include "PCCProperties.h"
#include "system/FileSystem.h"
using namespace std;

//pcc.cpp
int LoadCacheTex(const char *arc,int offs,int cprSize,char *data,int uncSize);

PCCTexture2D::PCCTexture2D(PCCFile *p,int id)
{
	pcc = p;
	objId = id;

	width = 0;
	height = 0;
	fmt = 0;
	mip = false;
	numMips = 0;
	imgInfos = 0;
	arcName = 0;
	tex = 0;
	def = false;
	firstLod=-1;
}

PCCTexture2D::~PCCTexture2D()
{
	if(tex&&!def)
		delete tex;
	if(imgInfos)
		delete[] imgInfos;
}

PCCTexture2D *PCCFile::GetTexture(pccExport_t *exp,bool print)
{
	//Log("GetTexture: %p %p\n",exp,exp->pData);
	if(exp->pData){
		return (PCCTexture2D*)exp->pData;
	}
	if(GetObjNameS(exp->class_)=="TextureCube"){
		Log("Error: Texture %d %s is cubemap\n",exp-exports,GetName(exp->objectName));
		return 0;
	}
	Log("GetTexture: export %d name: %s, class: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), GetObjName(exp->class_), exp->dataSize,exp->dataOffset);

	PCCTexture2D *pTex = new PCCTexture2D(this,(int)(exp-exports)+1);

	PCCProperties props(this,exp,print);

	string *texFmtS=0;

	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eIntProperty){
			if(*p->name=="SizeX")
				pTex->width=p->GetInt();
			if(*p->name=="SizeX")
				pTex->height=p->GetInt();
		}else if(p->type==eByteProperty){
			if(*p->name=="Format" && *p->GetName(this)=="EPixelFormat"){
				texFmtS=p->GetName(this,8);
				//TODO: move to separate function
				if(*texFmtS=="PF_DXT1")
					pTex->fmt = GL_COMPRESSED_RGB_S3TC_DXT1;
				else if(*texFmtS=="PF_DXT5")
					pTex->fmt = GL_COMPRESSED_RGBA_S3TC_DXT5;
				else if(*texFmtS=="PF_G8")
					pTex->fmt = GL_LUMINANCE;
				else if(*texFmtS=="PF_A8R8G8B8")
					pTex->fmt = FMT_ARGB8;
				else if(*texFmtS=="PF_V8U8")
					pTex->fmt = FMT_V8U8;
				else
					pTex->fmt = 0;
			}
		}else if(p->type==eBoolProperty){
			if(*p->name == "CompressionNoMipmaps"){
				pTex->mip = p->GetBool();
			}
		}else if(p->type==eNameProperty){
			if(*p->name == "TextureFileCacheName"){
				pTex->arcName = p->GetName(this);
			}
		}
	}
	if(print) Log("Tex props: size %dx%d, fmt %s, nomips %d, tfc %s\n",pTex->width,pTex->height,texFmtS->c_str(),pTex->mip,pTex->arcName?pTex->arcName->c_str():"null");

	pTex->numMips = 0;
	in->read((char*)&pTex->numMips,4);

	if(!pTex->numMips){
		Log("Tex: num mips is 0\n");
	}
	if(print) Log("NumMips %d\n",pTex->numMips);
	pTex->imgInfos = new imgInfo_t[pTex->numMips];
	pTex->firstLod=-1;
	for(int i=0;i<pTex->numMips;i++){
		in->read((char*)(pTex->imgInfos+i),16);

		if(pTex->imgInfos[i].storageType==pccSto){
			//if(!pTex->dataOffset)
			//	Log("tex: storType=pcc, dataOffs=0\n");
			in->seekg(pTex->imgInfos[i].uncSize,ios::cur);
		}
		in->read((char*)&(pTex->imgInfos[i].w),8);
		if(pTex->imgInfos[i].storageType==pccSto||pTex->imgInfos[i].storageType==arcCpr){
			//Log("Texture mip %d, size %dx%d format %s, mips %d\n",i,w,h,texFmtS->c_str(),pTex->numMips);
			//if(pTex->imgInfo.storageType==arcCpr){
				//Log("Cache file name %s\n",pTex->arcName->c_str());
			//}
			if(pTex->firstLod==-1)
				pTex->firstLod=i;
		}
		if(print) Log("mip %d Image info: storageType %d, uncSize %d, compSize %d, offset %d, %dx%d\n",i,pTex->imgInfos[i].storageType,pTex->imgInfos[i].uncSize,pTex->imgInfos[i].compSize,pTex->imgInfos[i].offset,pTex->imgInfos[i].w,pTex->imgInfos[i].h);
	}
	if(pTex->firstLod==-1)
		pTex->firstLod=0;

	exp->pData = pTex;
	return pTex;
}

void ResampleARGB(uint8_t *data, int size)
{
	int t;
	for(int i=0; i<size; i+=4)
	{
		//TEST
		/*t = data[i];
		data[i]=data[i+1];
		data[i+1]=data[i+2];
		data[i+2]=data[i+3];
		data[i+3]=t;*/
		t = data[i];
		data[i]=data[i+2];
		//data[i+1]=data[i+1];
		data[i+2]=t;
	}
}

void ResampleV8U8(char *data, int size, char *dst)
{
	int8_t *s=(int8_t*)data;
	char *d=dst;
	for(int i=0; i<size; i+=2, s+=2, d+=3)
	{
		d[0] = s[0]+128;
		d[1] = s[1]+128;
		float x=(s[0]/127.5f);
		float y=(s[1]/127.5f);
		d[2] = 255*sqrt(1-x*x-y*y);
	}
}

bool PCCTexture2D::GetData(int &lod,neArray<char> &data, int &format)
{
	if(lod >= numMips)
		lod = numMips-1;
	
	bool arcExist = arcName&&g_fs.FileExists((*arcName+".tfc").c_str());
	for(int i=lod;i<numMips;i++){
		if(imgInfos[i].storageType==empty){
			continue;
		}
		if(imgInfos[i].storageType==arcCpr||imgInfos[i].storageType==arcUnc){
			if(!arcExist)
				continue;
		}
		lod=i;
		break;
	}
	if(lod>=numMips){
		Log("PCCTexture2D %d: can't get data\n",objId);
		return false;
	}
	imgInfo_t &img = imgInfos[lod];

	if(img.storageType!=pccSto&&img.storageType!=arcCpr&&img.storageType!=arcUnc){
		Log("PCCTexture2D %d: Image storage error %d\n",objId,img.storageType);
		return false;
	}

	data.Resize(img.uncSize);
	if(img.storageType==pccSto){
		pcc->in->seekg(img.offset);
		pcc->in->read(data.data, img.uncSize);
		//Log("texData offset %d\n",dataOffset);
	}else{
		//Log("Cache file: name %s, off %d, len %d\n",arcName->c_str(),imgInfo.offset,imgInfo.compSize);
		if(!LoadCacheTex(arcName->c_str(),img.offset,img.compSize,data.data,img.uncSize)){
			Log("Error: Texture2D %d: tex cache not loaded\n",objId);
			data.Clear();
			return false;
		}
	}

	format = fmt;

	if(fmt==FMT_V8U8){
		char *newData = new char[data.size/2*3];
		ResampleV8U8(data.data,img.uncSize,newData);
		format=GL_RGB;
		delete[] data.data;
		data.data = newData;
		data.size = data.size/2*3;
	}

	if(fmt==FMT_ARGB8){
		ResampleARGB((uint8_t*)data.data,img.uncSize);
		format = GL_RGBA;
	}

	return true;
}

Texture *PCCTexture2D::GetGLTexture(Texture *defTex,int lod)
{
	if(tex)
		return tex;

	if(!fmt){
		Log("Texture2D %d: Unknown image format\n",objId);
		return NULL;
	}

	neArray<char> data;
	int format;
	if(!GetData(lod,data,format)){
		Log("Texture: GetData error\n");
		tex = defTex;
		def = true;
		return NULL;
	}
	//Log("GetGLTexture: data len %d, lod %d\n",data.size,lod);
	imgInfo_t &img = imgInfos[lod];

	//Load tex
	tex = new Texture();
	tex->Create(img.w,img.h);
	tex->SetFilter(GL_LINEAR,GL_LINEAR);
	//TODO: Check NPOT
	tex->SetWrap(GL_REPEAT);
	if(format==GL_COMPRESSED_RGB_S3TC_DXT1||format==GL_COMPRESSED_RGBA_S3TC_DXT5){
		tex->UploadCompressed(format,img.uncSize,data.data);
	}else{
		tex->Upload(0,format,(uint8_t*)data.data);
	}

	tex->SetFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
#ifndef ANDROID
	//ext
	glTexParameteri(tex->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);
#endif
	glGenerateMipmap(tex->target);

	data.Clear();
	return tex;
}

