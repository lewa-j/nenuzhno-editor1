
#include <cstring>
#include "log.h"
#include "graphics/ArrayBuffer.h"
#include "pcc.h"
#include "PCCStaticMesh.h"
#include "PCCProperties.h"
using namespace std;

PCCStaticMesh::PCCStaticMesh(int id)
{
	objId = id;
	numLODs = 0;
	LODs = 0;
	materials = 0;
}

PCCStaticMesh::~PCCStaticMesh()
{
	if(LODs)
		delete[] LODs;
	if(materials)
		delete[] materials;
}

void PCCStaticMesh::LoadMaterials(PCCFile *pcc,PCCMaterial **mats)
{
	if(materials)
		return;

	//if(!materials){
		materials = new PCCMaterial*[LODs[0].sectionCount];
		memset(materials,0,LODs[0].sectionCount*sizeof(PCCMaterial*));
	//}
	for(int i=0;i<LODs[0].sectionCount;i++){
		if(LODs[0].sections[i].matExpId>0){
			materials[i] = pcc->GetMaterial(pcc->exports+LODs[0].sections[i].matExpId-1);
		}else{
			if(!mats||!mats[i])
				Log("StaticMesh %d: section %d Material id %d\n",objId,i,LODs[0].sections[i].matExpId);
			materials[i] = 0;
		}
	}
}

void PCCStaticMesh::InitBuffers(){
	if(LODs[0].vbo)
		return;
	
	int i=0;
	//for(i=0;i<numLODs;i++)
	{
		StaticMeshLod_t &lod=LODs[i];
		
		int len = (lod.vertSize+lod.UVStrip)*lod.numVerts;
		lod.vboUVOffs = lod.vertSize*lod.numVerts;
		lod.vbo = new VertexBufferObject();
		lod.vbo->Create();
		lod.vbo->Upload(len,NULL);
		lod.vbo->Update(0,lod.vertSize*lod.numVerts, lod.verts);
		lod.vbo->Update(lod.vboUVOffs, lod.UVStrip*lod.numVerts, lod.UVs);
		
		lod.ibo = new IndexBufferObject();
		lod.ibo->Create();
		lod.ibo->Upload(lod.numIndices*2,lod.indices);
	}
}

#include "renderer/Model.h"

Model *PCCStaticMesh::ToModel(PCCFile *pcc)
{
	Model *mdl = new Model();

	mdl->bbox = BoundingBox(bounding.pos-bounding.size,bounding.pos+bounding.size);

	StaticMeshLod_t &lod=LODs[0];

	mdl->vertexCount = lod.numVerts;

	uint32_t uvOffs = lod.vertSize*lod.numVerts;
	mdl->vertexFormat.Resize(5);
	mdl->vertexFormat[0] = {0,3,GL_FLOAT,false,12,0};//pos
	mdl->vertexFormat[1] = {1,4,GL_BYTE,true,lod.UVStrip,uvOffs+4};//nrm
	mdl->vertexFormat[2] = {2,2,GL_HALF_FLOAT,false,lod.UVStrip,uvOffs+8};//uv
	mdl->vertexFormat[3] = {3,2,GL_HALF_FLOAT,false,lod.UVStrip,uvOffs+12};//uv2
	mdl->vertexFormat[4] = {4,3,GL_BYTE,true,lod.UVStrip,uvOffs};//tan

	char *newVerts = new char[(lod.vertSize+lod.UVStrip)*lod.numVerts];
	memcpy(newVerts,lod.verts,lod.vertSize*lod.numVerts);
	memcpy(newVerts+uvOffs,lod.UVs,lod.UVStrip*lod.numVerts);
	mdl->verts = newVerts;
	//TODO free

	mdl->indexCount = lod.numIndices;
	mdl->indexSize = 2;
	mdl->indexType = GL_UNSIGNED_SHORT;
	mdl->inds = (char*)lod.indices;

	mdl->submeshes.Resize(lod.sectionCount);
	for(int i=0; i<lod.sectionCount; i++){
		mdl->submeshes[i] = {(uint32_t)lod.sections[i].firstIdx,lod.sections[i].numFaces*3,i};
	}

	mdl->materials.Resize(lod.sectionCount);
	for(int i=0; i<lod.sectionCount; i++){
		mdl->materials[i].name = pcc->GetObjNameS(materials[i]->objId);
	}

	return mdl;
}

PCCStaticMesh *PCCFile::GetStaticMesh(pccExport_t *exp)
{
	if(exp->pData)
		return (PCCStaticMesh*)exp->pData;
	
	LOG("GetStaticMesh: export %d name: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), exp->dataSize,exp->dataOffset);
	//in->seekg(exp->dataOffset);

	//if(!PrintProperties(exp))
	//	return 0;
	PCCProperties props(this,exp);

	PCCStaticMesh *mesh = new PCCStaticMesh((int)(exp-exports)+1);

	//Bounding
	//Log("sizeof(Bounding_t) %d\n",sizeof(Bounding_t));
	in->read((char*)&mesh->bounding,sizeof(Bounding_t));
	//Log("Bounding box: p (%f, %f, %f), s (%f, %f, %f), r %f\n",mesh->bounding.pos.x,mesh->bounding.pos.y,mesh->bounding.pos.z,mesh->bounding.size.x,mesh->bounding.size.y,mesh->bounding.size.z,mesh->bounding.radius);
	
	//BodySetup
	in->seekg(28,ios::cur);

	int lsize = 0;
	int lcount = 0;
	//kDOP Bounds & nodes
	{
		//Log("cur pos %X\n",in->tellg()-startPos);
		in->read((char*)&lsize,4);
		in->read((char*)&lcount,4);
		int len = lsize*lcount;
		//Log("DOP size %d, count %d, len %d\n",lsize, lcount,len);
		in->seekg(len,ios::cur);
	}

	//kDOP tris
	{
		in->read((char*)&lsize,4);
		in->read((char*)&lcount,4);
		int len = lsize*lcount;
		//Log("Raw tris size %d, count %d, len %d\n",lsize, lcount,len);

		/*for(int i = 0; i < lcount; i++){
			uint16_t v1, v2, v3, mat;
		}*/
		in->seekg(len,ios::cur);
	}

	int internalVersion=0;
	in->read((char*)&internalVersion,4);
	//LODs
	{
	//Log("sizeof(LodSection_t) %d\n",sizeof(LodSection_t));
	in->read((char*)&mesh->numLODs,4);
	//Log("Internal version %d, LOD count %d\n",internalVersion,mesh->numLODs);

	mesh->LODs = new StaticMeshLod_t[mesh->numLODs];
	for(int i=0; i<mesh->numLODs; i++){
		StaticMeshLod_t *lod = mesh->LODs+i;
		{//bulk data
			int flags;
			int elemsCount;
			int bsize;
			int boffs;
			in->read((char*)&flags,4);
			in->read((char*)&elemsCount,4);
			in->read((char*)&bsize,4);
			in->read((char*)&boffs,4);
			if(bsize)
				Log("bulk data: flags %d, count %d,size %d, offs %d\n",flags,elemsCount,bsize,boffs);
			if(flags&0x20)
				Log("bulk data unused flag\n");
		}
		//in->seekg(16,ios::cur);
		in->read((char*)&lod->sectionCount,4);
		//Log("lod %d Section count %d\n",i,lod->sectionCount);
		lod->sections = new LodSection_t[lod->sectionCount];
		//in->read((char*)lod->sections,lod->sectionCount*sizeof(LodSection_t));
		for(int j=0; j<lod->sectionCount; j++){
			LodSection_t *pSection = lod->sections+j;
			in->read((char*)pSection,40);
			//in->seekg(9,ios::cur);
			if(pSection->arrSize>1)
				Log("lod %d sec %d: arrSize %d\n",i,j,pSection->arrSize);
			in->seekg(pSection->arrSize*8,ios::cur);
			in->read(&pSection->unk3,1);
			if(pSection->unk3)
				Log("lod %d sec %d: unk3 %d\n",i,j,pSection->unk3);
			//Log("LOD %d Section %d: material (%d) %s, firstIdx %d, numFaces %d\n",i,j,pSection->matExpId,GetObjName(pSection->matExpId),pSection->firstIdx,pSection->numFaces);
		}

		in->read((char*)&lod->vertSize,4);
		in->read((char*)&lod->numVerts,4);
		in->seekg(4,ios::cur);

		//Log("LOD %d: section count %d, vert size %d, unk3 %d, num verts %d\n",i,lod->sectionCount,lod->vertSize,lod->sections[0].unk3,lod->numVerts);

		//verts
		{
			in->seekg(8,ios::cur);//elemSize,elemCount
			int len = lod->vertSize*lod->numVerts;
			//Log("Verts size %d, count %d, len %d\n",lod->vertSize, lod->numVerts, len);
			lod->verts = new glm::vec3[lod->numVerts];
			in->read((char*)lod->verts,len);

			//for(int i=0;i<min(6,mesh->numVerts);i++)
			//	Log("vert %d: %f %f %f\n",i,mesh->verts[i].x,mesh->verts[i].y,mesh->verts[i].z);
		}
		//uvs
		{
			in->read((char*)&lod->numUVsets,4);
			in->read((char*)&lod->UVStrip,4);
			in->read((char*)&lcount,4);
			int bUseFullPrecisionUVs=0;
			in->read((char*)&bUseFullPrecisionUVs,4);
			in->seekg(4,ios::cur);//unk

			in->seekg(8,ios::cur);//elemSize,elemCount
			int len = lod->UVStrip*lcount;
			//Log("UV size %d, sets %d, lcount %d, len %d, bUseFullPrecisionUVs %d\n",lod->UVStrip, lod->numUVsets, lcount, len,bUseFullPrecisionUVs);
			lod->UVs = new uint8_t[len];
			in->read((char*)lod->UVs,len);
			for(int n = 0; n < lcount; n++){
				//xyzw1, xyzw2 4bytes packet normals
				//uv1, uv2 16bit floats
				(*(uint32_t*)(lod->UVs+n*lod->UVStrip))^=0x80808080;//tan
				(*(uint32_t*)(lod->UVs+n*lod->UVStrip+4))^=0x80808080;//norm
			}
		}
		//in->seekg(28,ios::cur);
		//Colors
		{
			in->read((char*)&lsize,4);
			in->read((char*)&lcount,4);
			int len = lsize*lcount;
			//Log("Colors size %d, count %d, len %d\n",lsize, lcount,len);
			if(lcount){
				in->seekg(8,ios::cur);
				in->seekg(len,ios::cur);
			}
		}
		//Colors 2
		{
			in->read((char*)&lsize,4);
			in->read((char*)&lcount,4);
			int len = lsize*lcount;
			//Log("Colors 2 size %d, count %d, len %d\n",lsize, lcount,len);
			in->seekg(8,ios::cur);
			in->seekg(len,ios::cur);
		}
		int unkNumVerts=0;
		in->read((char*)&unkNumVerts,4);
		//LOG("NumVerts: %d\n",unkNumVerts);
		//Index buffer
		{
			in->read((char*)(&lsize),4);
			in->read((char*)(&lod->numIndices),4);
			int len = lsize*lod->numIndices;
			//Log("Index buffer size %d, count %d, len %d\n",lsize, lod->numIndices, len);
			if(lod->numIndices>0){
				lod->indices = new uint16_t[lod->numIndices];
				in->read((char*)lod->indices,len);
			}
		}
	}
	}//LODs

	//Log("cur pos %X\n",in->tellg()-startPos);

	//unknown
	//in->seekg( ? ,ios::cur);

	if(!mesh->LODs[0].numIndices)
		Log("Mesh: verts %d, inds %d, version %d, lods %d, mats %d\n",mesh->LODs[0].numVerts,mesh->LODs[0].numIndices,internalVersion,mesh->numLODs,mesh->LODs[0].sectionCount);

	exp->pData = mesh;
	return mesh;
}

