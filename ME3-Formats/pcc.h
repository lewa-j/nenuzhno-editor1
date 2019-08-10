
#pragma once

#include <stdint.h>
#include <string>
#include <istream>

#include "PCCTexture2D.h"
#include "PCCMaterial.h"
#include "PCCStaticMesh.h"
#include "PCCSkeletalMesh.h"
#include "PCCLevel.h"
#include "PCCAnim.h"

class PCCSystem;

#define PCCIDENT 0x9E2A83C1
#define PCC_FLAG_COMPRESSED 0x02000000

#pragma pack(push,1)
struct pccHeader_t
{
	uint32_t		ident;
	uint16_t		version[2];
	int		expDataBegOffset;
	int		pkgNameSize;
	uint16_t	pkg[5];
	int		packageFlags;
	int		unknown3;
	//34
	int		nameCount;
	int		nameOffset;
	int		exportCount;
	int		exportOffset;
	int		importCount;
	int		importOffset;
	int		unknown4[5];
	int		GUID[4];
	int		generations;
	int		exportCount2;
	int		nameCount2;
	int		unknown5;
	int		engine;
	int		unknown6[3];
	int		compressionFlag;
	int		chunkCount;
};//142?

struct pccImport_t
{
	int packageName;
	int packageNameNumber;
	int className;
	int classNameNumber;
	int link;
	int importName;
	int importNameNumber;
	PCCObject *pData;
};
#define pccImportSize 28

struct pccExport_t
{
	int 	class_;
	int 	superClass;
	int 	link;
	int 	objectName;
	int 	objectNameNumber;
	int 	archetype;
	uint64_t objectFlags;
	int 	dataSize;
	int 	dataOffset;
	int 	unknown;
	int 	count;
	int 	unknown1;
	int 	GUID[4];
	//unknown count*4
	PCCObject *pData;
};
#define pccExportSize 68

struct pccChunks_t
{
	int uncompressedOffset;
	int uncompressedSize;
	int compressedOffset;
	int compressedSize;
};

#define PCC_CHUNK_MAGIC 0x9E2A83C1
//maxSegmentSize 0x20000

struct pccChunk_t
{
	uint32_t magic;
	int blockSize;
	int compressedSize;
	int uncompressedSize;
};

struct pccBlock_t
{
	int compressedSize;
	int uncompressedSize;
};

#pragma pack(pop)

class PCCFile
{
public:
	PCCFile(PCCSystem *sys);
	~PCCFile();
	bool Load(const char *fileName);
	const char *GetName(int i);
	std::string &GetNameS(int i);
	std::string *GetNameSP(int i);
	const char *GetObjName(int i);
	std::string &GetObjNameS(int i);
	std::string *GetObjNameSP(int i);
	const char *GetClassName(int i);

	PCCTexture2D *GetTexture(int idx);
	PCCTexture2D *GetTexture(pccExport_t *exp,bool print=false);//PCCTexture2D.cpp
	PCCStaticMesh *GetStaticMesh(pccExport_t *exp);//PCCStaticMesh.cpp
	SkeletalMesh *GetSkeletalMesh(pccExport_t *exp);
	AnimSet *GetAnimSet(int idx);
	BioAnimSetData *GetBioAnimSetData(int idx);
	PCCMaterial *GetMaterial(pccExport_t *exp,bool print=false);
	PCCMaterial *GetMatInstConst(pccExport_t *exp, bool print=false);

	PCCLevel *GetLevel(int idx);//PCCLevel.cpp
	LevelObject *GetLevelObject(int idx);//PCCLevel.cpp
	StaticMeshActor *GetStaticMeshActor(pccExport_t *exp);//PCCLevel.cpp
	StaticMeshCollectionActor *GetStaticMeshCollectionActor(pccExport_t *exp);//PCCLevel.cpp
	PrefabInstance *GetPrefabInstance(pccExport_t *exp);//PCCLevel.cpp
	StaticMeshComponent *GetStaticMeshComponent(pccExport_t *exp);
	
	std::string pccName;
	PCCSystem *pccSys;
	std::istream *in;
	pccHeader_t header;
	bool compressed;
	char *uncompressedData;
	int uncompressedSize;
	std::string *nameTable;
	pccImport_t *imports;
	pccExport_t *exports;

	bool PrintProperties(pccExport_t *exp);
	int ReadInt();
	void Skip(int l);

private:
	void ReadMaterialResource(PCCMaterial *pMat);
};
