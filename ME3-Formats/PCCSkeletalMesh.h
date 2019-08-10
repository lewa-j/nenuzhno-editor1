
#pragma once

#include "PCCObject.h"
#include "PCCStaticMesh.h"
#include <vec2.hpp>
#include <gtc/quaternion.hpp>

class Bone
{
public:
	int nameIdx;
	int nameNum;
	int flags;
	glm::quat quat;
	glm::vec3 pos;
	int numChildren;
	int parent;
	int unk;//col
};

//skeletal mesh lod section
struct SMLsection_t{
	uint16_t matIdx;
	uint16_t chunkIdx;
	int firstIdx;
	uint16_t numTris;
	uint8_t unk;
	uint8_t flag;
};//12b

#pragma pack(push,1)
struct RigidVertex_t{
	glm::vec3 pos;
	char normals[12];
	glm::vec2 uv;
	uint8_t boneIndex;
};//33

struct SoftVertex_t{
	glm::vec3 pos;
	char normals[12];
	glm::vec2 uv;
	uint8_t boneWeights[8];//index,weight...
};//40
#pragma pack(pop)

struct SMLchunk_t{
	int firstVtx;
	int rigidVertsNumArr;
	RigidVertex_t *rigidVerts;
	int softVertsNumArr;
	SoftVertex_t *softVerts;
	int bonesNum;
	int16_t *bones;
	int rigidVertsNum;
	int softVertsNum;
	int maxInfluences;
	
	SMLchunk_t(){
		rigidVerts = 0;
		softVerts = 0;
		bones = 0;
	}
	~SMLchunk_t(){
		if(rigidVerts)
			delete[] rigidVerts;
		if(softVerts)
			delete[] softVerts;
		if(bones)
			delete[] bones;
	}
};

/*vert
packetnormals[2] - 8b
boneIdx[4] - 4b
boneWeight[4] - 4b
pos - 12b
uv - 4b
*/

struct SkelLod_t
{
	int sectionsCount;
	SMLsection_t *sections;
	int indSize;
	int indCount;
	char *inds;
	int chunksCount;
	SMLchunk_t *chunks;
	int numVerts;
	int numEdges;
	int bUseFullPrecisionUVs;
	int bUsePackedPosition;
	glm::vec3 meshExtension;
	glm::vec3 meshOrigin;
	int vertSize;
	int numVertsArr;
	char *verts;
	
	SkelLod_t(){
		sections=0;
		inds=0;
		chunks=0;
		verts=0;
	}
	
	~SkelLod_t(){
		if(sections)
			delete[] sections;
		if(inds)
			delete[] inds;
		if(chunks)
			delete[] chunks;
		if(verts)
			delete[] verts;
	}
};

class SkeletalMesh: public PCCObject
{
public:
	SkeletalMesh(int id);
	~SkeletalMesh();

	void LoadMaterials(PCCFile *pcc);
	void SetupSkeleton();
	
	Bounding_t bounds;
	int numMaterials;
	int *matIds;
	PCCMaterial **pMaterials;
	glm::vec3 originPos;
	glm::vec3 originRot;
	int numBones;
	Bone *skeleton;
	int skeletalDepth;
	int numLODs;
	SkelLod_t *LODs;
	
	glm::mat4 *skelMats;
};

