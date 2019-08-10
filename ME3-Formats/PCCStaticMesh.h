
#pragma once

#include "stdint.h"
#include <vec3.hpp>
#include "graphics/ArrayBuffer.h"
#include "PCCObject.h"
#include "PCCMaterial.h"

class Model;

#pragma pack(push,1)
struct Bounding_t
{
	glm::vec3 pos;
	glm::vec3 size;
	float radius;
};

struct LodSection_t
{
	int matExpId;
	int unk[2];
	int bEnableShadowCasting;
	int firstIdx;
	int numFaces;
	//int matEff1;
	//int matEff2;
	int ink2[2];
	int index;
	//array of 2 int
	int arrSize;
	//40b
	//int firstIdx2;
	//int numFaces2;

	char unk3;
	//if(unk3)
	//ps3data
};

struct StaticMeshLod_t
{
	//char GUID[16];
	int sectionCount;
	LodSection_t *sections;
	int vertSize;
	int numVerts;
	glm::vec3 *verts;
	int numUVsets;
	int UVStrip;
	uint8_t *UVs;
	uint16_t *indices;
	int numIndices;

	VertexBufferObject *vbo;
	int vboUVOffs;
	IndexBufferObject *ibo;

	StaticMeshLod_t(){
		sectionCount = 0;
		sections = 0;
		verts = 0;
		UVs = 0;
		indices = 0;
		vbo=0;
		vboUVOffs=0;
		ibo=0;
	}
	~StaticMeshLod_t(){
		if(sections)
			delete[] sections;
		if(verts)
			delete[] verts;
		if(UVs)
			delete[] UVs;
		if(indices)
			delete[] indices;
		if(vbo)
			delete vbo;
		if(ibo)
			delete ibo;
	}
};

#pragma pack(pop)

class PCCStaticMesh: public PCCObject
{
public:
	PCCStaticMesh(int id);
	~PCCStaticMesh();

	void LoadMaterials(PCCFile *pcc,PCCMaterial **mats=0);
	void InitBuffers();
	Model *ToModel(PCCFile *pcc);

	Bounding_t bounding;
	int numLODs;
	StaticMeshLod_t *LODs;
	
	PCCMaterial **materials;//lod[0].numSections

/*
BodySetup ObjectProperty
LightMapCoordinateIndex IntProperty
LightMapResolution IntProperty
UseSimpleBoxCollision BoolProperty
*/
};
