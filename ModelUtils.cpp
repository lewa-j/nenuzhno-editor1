
#include <fstream>
#include <string>
using namespace std;
#include "log.h"
#include "graphics/platform_gl.h"
#include "renderer/Model.h"
#include "renderer/mesh.h"
#include "system/FileSystem.h"
#include "resource/nmf.h"

Model *MeshToModel(Mesh *msh, const char* mat){
	Model *mdl = new Model();
	mdl->vertexCount = msh->numVerts;
	//mdl->vertexStride = 8*4;
	mdl->vertexFormat.Resize(3);
	mdl->vertexFormat[0]={0,3,GL_FLOAT,false,8*4,0};//pos
	mdl->vertexFormat[1]={1,3,GL_FLOAT,false,8*4,12};//norm
	mdl->vertexFormat[2]={2,2,GL_FLOAT,false,8*4,24};//uv

	mdl->verts = (char *)msh->verts;
	mdl->indexCount = msh->numInds;
	mdl->indexType = GL_UNSIGNED_SHORT;
	mdl->indexSize = 2;
	mdl->inds = (char *)msh->inds;

	mdl->submeshes.Resize(1);
	mdl->submeshes[0] = {0,mdl->indexCount ? mdl->indexCount : mdl->vertexCount,0};
	mdl->materials.Resize(1);
	mdl->materials[0].name = mat;

	return mdl;
}

void ExportMesh(const char *name, Mesh *mesh){
	char path[256];
	g_fs.GetFilePath((string("models/")+name).c_str(),path,true);
	ofstream out(path,ios::binary);
	if(!out){
		Log("Can't create mesh file (%s)!\n",name);
		return;
	}
	int numTris=mesh->numVerts/3;
	Log("Export mesh %s, %d tris\n",name,numTris);
	out.write((char*)&numTris,4);
	out.write((char*)mesh->verts,mesh->numVerts*8*sizeof(float));

	out.close();
}

void ExportNMF(const char *name, Model *mdl){
	char path[256];
	g_fs.GetFilePath((string("models/")+name+".nmf").c_str(),path,true);
	ofstream out(path,ios::binary);
	if(!out){
		Log("Can't create nmf file (%s)!\n",name);
		return;
	}
	Log("Export model %s\n",name);

	int vertexStride=mdl->vertexFormat[0].stride;

	nmfHeaderBase_t headerBase;
	headerBase.magic = NMF_MAGIC;
	headerBase.version[0] = 1;
	headerBase.version[1] = 0;
	//header.hash = //TODO
	headerBase.flags = 0;

	nmfHeader1_t header;
	header.vertexFormat = 7;//pos,norm,uv
	header.vertexStride = vertexStride;
	header.vertexCount = mdl->vertexCount;
	header.indexSize = mdl->indexSize;
	header.indexCount = mdl->indexCount;
	header.submeshCount = mdl->submeshes.size;
	header.materialsCount = mdl->materials.size;

	Log("sizeof(numHeader) %d\n",sizeof(nmfHeader1_t));
	int dataLength = sizeof(header);
	header.vertexOffset = dataLength;
	dataLength += header.vertexCount*header.vertexStride;
	header.indexOffset = dataLength;
	//Log("Export NMF: indexOffset %d\n",header.indexOffset);
	dataLength += header.indexCount*header.indexSize;
	//Log("Export NMF: index len %d\n",header.indexCount*indSize);
	header.submeshOffset = dataLength;
	//Log("Export NMF: submeshOffset %d\n",header.submeshOffset);
	dataLength += sizeof(submesh_t)*header.submeshCount;
	header.materialsOffset = dataLength;
	dataLength += sizeof(int)*header.materialsCount;

	int *matOffsets = new int[header.materialsCount];
	//int matNamesLength = 0;
	for(int i=0;i<header.materialsCount;i++){
		matOffsets[i] = dataLength;
		dataLength += mdl->materials[i].name.length()+5;
	//	matNamesLength += mdl->materials[i].name.length()+5;
	}

	headerBase.length = dataLength;

	out.write((char*)&headerBase,sizeof(nmfHeaderBase_t));
	out.write((char*)&header,sizeof(header));
	out.write(mdl->verts,header.vertexCount*header.vertexStride);
	if(mdl->inds)
		out.write(mdl->inds,header.indexCount+header.indexSize);
	out.write((char*)mdl->submeshes.data,sizeof(submesh_t)*header.submeshCount);
	out.write((char*)matOffsets,sizeof(int)*header.materialsCount);
	for(int i=0;i<header.materialsCount;i++){
		int len = mdl->materials[i].name.length()+1;
		out.write((const char*)&len,4);
		out.write(mdl->materials[i].name.c_str(),len);
	}

	delete[] matOffsets;
	out.close();
}


