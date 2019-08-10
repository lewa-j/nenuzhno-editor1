
#pragma once

#include <vec3.hpp>
#include <mat4x4.hpp>
#include "PCCObject.h"
#include "PCCStaticMesh.h"

enum eLevelObjectType{
	eUnknown,
	eStaticMeshActor,
	eStaticMeshCollectionActor,
	ePrefabInstance
};

class StaticMeshComponent : public PCCObject
{
public:
	StaticMeshComponent(int id){
		objId = id;
		pMesh = 0;
		pMaterials = 0;
		numMaterials = 0;
		scale = glm::vec3(1.0f);
		hidden = false;
		lightmapType=0;
		lmColTex=0;
		lmColScale = glm::vec4(1.0f);
		lmDirTex=0;
		lmDirScale = glm::vec4(1.0f);
		lmScaleAndBias = glm::vec4(1.0f);
		lm1DVertsNum=0;
		lm1DVerts=0;
	}
	~StaticMeshComponent(){
		if(pMaterials)
			delete[] pMaterials;
	}
	PCCStaticMesh *pMesh;
	PCCMaterial **pMaterials;
	int numMaterials;
	glm::vec3 scale;
	bool hidden;

	//lightmap
	int lightmapType;
	PCCTexture2D *lmColTex;
	glm::vec4 lmColScale;
	glm::vec4 lmColBias;
	PCCTexture2D *lmDirTex;
	glm::vec4 lmDirScale;
	glm::vec4 lmDirBias;
	glm::vec4 lmScaleAndBias;//vec2 vec2
	int lm1DVertsNum;
	char *lm1DVerts;

/*
StaticMesh ObjectProperty
bBioIsReceivingDecals BoolProperty
LightMapEncoding ByteProperty, size 8
Materials ArrayProperty, size 12
ReplacementPrimitive ObjectProperty
LightingChannels StructProperty, size 58
BlockNonZeroExtent BoolProperty
BlockRigidBody BoolProperty
IrrelevantLights ArrayProperty, size 36
Scale3D StructProperty, size 12

bUseAsOccluder BoolProperty
bAcceptsStaticDecals BoolProperty
bAcceptsDynamicDecals BoolProperty
CastShadow BoolProperty
bAcceptsLights BoolProperty
bAcceptsDynamicLights BoolProperty
bUsePrecomputedShadows BoolProperty
CollideActors BoolProperty
MaxDrawDistance 6 4
CachedMaxDrawDistance 6 4
bForceDirectLightMap
bCastDynamicShadow
bSelfShadowOnly
RBChannel 7 16
bCullModulatedShadowOnEmissive 8
bCullModulatedShadowOnBackfaces 8
bAcceptsDynamicDominantLightShadows 8
OverridePhysMat 8
bAllowAmbientOcclusion 8 1
CanBlockCamera 8 1
bAllowShadowFade 8 1
bAcceptsFoliage
bAllowApproximateOcclusion 8
TranslucencySortPriority
LocalTranslucencySortPriority
HiddenGame 8

bCastHiddenShadow 8
*/
};

class LevelObject: public PCCObject
{
public:
	virtual ~LevelObject(){}
	eLevelObjectType type;

	virtual void AddToBB(glm::vec3 &bbmin, glm::vec3 &bbmax)=0;
};

class StaticMeshActor: public LevelObject
{
public:
	StaticMeshActor(int id){
		objId = id;
		type = eStaticMeshActor;
		pStatMeshComp = 0;
		rotation = glm::vec3(0.0f);
		scale = glm::vec3(1.0f);
		modelMtx = glm::mat4(1.0f);
	}
	~StaticMeshActor(){}

	void AddToBB(glm::vec3 &bbmin, glm::vec3 &bbmax);
	void CalcMtx();
	void MergePrefab(StaticMeshActor* sma);
	
	glm::vec3 location;
	glm::vec3 rotation;
	glm::vec3 scale;
	glm::mat4 modelMtx;
	StaticMeshComponent *pStatMeshComp;
	
	/*
	StaticMeshComponent ObjectProperty
	location StructProperty
	Rotation StructProperty
	DrawScale3D StructProperty
	Tag NameProperty
	DrawScale FloatProperty
	CollisionComponent ObjectProperty
	Group NameProperty
	*/
};

class StaticMeshCollectionActor: public LevelObject
{
public:
	StaticMeshCollectionActor(int id){
		objId = id;
		type = eStaticMeshCollectionActor;
		objectsCount = 0;
		objectsIdxs = 0;
		pMeshes = 0;
		matrices = 0;
	}

	~StaticMeshCollectionActor(){
		if(objectsIdxs)
			delete[] objectsIdxs;
		if(pMeshes)
			delete[] pMeshes;
		if(matrices)
			delete[] matrices;
	}

	int objectsCount;
	int *objectsIdxs;
	StaticMeshComponent **pMeshes;
	glm::mat4 *matrices;

	void AddToBB(glm::vec3 &bbmin, glm::vec3 &bbmax);
	
/*
StaticMeshComponents ArrayProperty, size 404
Tag NameProperty
CreationTime FloatProperty
*/
};

class PrefabInstance: public LevelObject
{
public:
	PrefabInstance(int id){
		objId = id;
		type = ePrefabInstance;
		prefId = 0;
		location = glm::vec3(0.0f);
		objectsCount = 0;
		objectsIds = 0;
	}
	~PrefabInstance(){
		if(objectsIds)
			delete[] objectsIds;
	}

	int prefId;
	glm::vec3 location;
	int objectsCount;
	int *objectsIds;//prefab obj and level obj
	
	void AddToBB(glm::vec3 &bbmin, glm::vec3 &bbmax){}
};

class PCCLevel: public PCCObject
{
public:
	PCCLevel(PCCFile *p,int id);
	~PCCLevel();

	void Loaded();
	
	int objectsCount;
	int *objectsIdxs;
	LevelObject **objects;
	PCCFile *pcc;

	glm::vec3 bbmin;
	glm::vec3 bbmax;
};

