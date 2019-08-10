
#include <cstring>
#include <gtc/matrix_transform.hpp>
#include "log.h"
#include "PCCSkeletalMesh.h"
#include "pcc.h"
#include "PCCProperties.h"
using namespace std;

SkeletalMesh::SkeletalMesh(int id){
	objId = id;
	numMaterials = 0;
	matIds = 0;
	pMaterials=0;
	numBones = 0;
	skeleton = 0;
	skeletalDepth = 0;
	numLODs = 0;
	LODs = 0;
	
	skelMats=0;
}

SkeletalMesh::~SkeletalMesh(){
	if(matIds)
		delete[] matIds;
	if(pMaterials)
		delete[] pMaterials;
	if(skeleton)
		delete[] skeleton;
	if(LODs)
		delete[] LODs;
	if(skelMats){
		delete[] skelMats;
	}
}

void SkeletalMesh::LoadMaterials(PCCFile *pcc)
{
	if(pMaterials)
		return;

	pMaterials = new PCCMaterial*[numMaterials];
	memset(pMaterials,0,numMaterials*sizeof(PCCMaterial*));
	for(int i=0;i<numMaterials;i++){
		if(matIds[i]>0){
			pMaterials[i] = pcc->GetMaterial(pcc->exports+matIds[i]-1);
		}else{
			pMaterials[i] = 0;
		}
	}
}

void SkeletalMesh::SetupSkeleton(){
	if(skelMats)
		return;
	
	skelMats = new glm::mat4[numBones];
	
	for(int i=0;i<numBones;i++){
		Bone &b = skeleton[i];
		
		glm::vec3 bonePos = b.pos;
		glm::quat boneQuat = b.quat;
		if(!i) boneQuat = glm::conjugate(boneQuat);
		glm::mat4 &boneMat = skelMats[i];
		boneMat = glm::mat4(1.0f);
		//boneMat = glm::translate(boneMat,bonePos);
		//boneMat = glm::mat4_cast(boneQuat)*boneMat;
		boneMat = boneMat*glm::mat4_cast(boneQuat);
		boneMat[3] = glm::vec4(bonePos,1.0f);
		if(i){
			boneMat = skelMats[b.parent]*boneMat;
		}
	}
}

SkeletalMesh *PCCFile::GetSkeletalMesh(pccExport_t *exp)
{
	if(exp->pData)
		return (SkeletalMesh*)exp->pData;
	
	Log("GetSkeletalMesh: export %d, name %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), exp->dataSize,exp->dataOffset);
	
	SkeletalMesh *skm = new SkeletalMesh((int)(exp-exports)+1);
	
	PCCProperties props(this,exp,true);
	
	//Bounds
	in->read((char*)&skm->bounds,sizeof(Bounding_t));
	skm->bounds.radius *= 0.5f;
	//Log("Bounding box: p (%f, %f, %f), s (%f, %f, %f), r %f\n",skm->bounds.pos.x,skm->bounds.pos.y,skm->bounds.pos.z,skm->bounds.size.x,skm->bounds.size.y,skm->bounds.size.z,skm->bounds.radius);
	
	//Materials
	in->read((char*)&skm->numMaterials,4);
	//Log("Num materials %d\n",skm->numMaterials);
	skm->matIds = new int[skm->numMaterials];
	in->read((char*)skm->matIds,skm->numMaterials*4);
	//for(int i=0;i<skm->numMaterials;i++){
	//	Log("mat %d: %d %s\n",i,skm->matIds[i],GetObjName(skm->matIds[i]));
	//}
	
	//Origin
	in->read((char*)&skm->originPos,12);
	in->read((char*)&skm->originRot,12);
	//Log("Mesh origin: pos (%f,%f,%f), rot (%f,%f,%f)\n",skm->originPos.x,skm->originPos.y,skm->originPos.z,skm->originRot.x,skm->originRot.y,skm->originRot.z);
	
	//Skeleton
	in->read((char*)&skm->numBones,4);
	Log("Num bones %d\n",skm->numBones);
	skm->skeleton = new Bone[skm->numBones];
	in->read((char*)skm->skeleton,skm->numBones*sizeof(Bone));
	for(int i=0;i<skm->numBones;i++){
		Bone *b = skm->skeleton+i;
		//if(i>0){
		//	b->quat.w*=-1;
		//}
		Log("bone %d: %s pos (%f,%f,%f), par %d\n",i,GetName(b->nameIdx),b->pos.x,b->pos.y,b->pos.z,b->parent);
	}
	in->read((char*)&skm->skeletalDepth,4);
	//Log("Skeletal depth: %d\n",skm->skeletalDepth);
	
	//LOD models
	in->read((char*)&skm->numLODs,4);
	//Log("Num LODs: %d\n",skm->numLODs);
	skm->LODs = new SkelLod_t[skm->numLODs];
	
	int tcount=0;
	//int tsize=0;
	for(int i=0;i<skm->numLODs;i++)
	//int i=0;
	{
		//Log("LOD %d\n",i);
		SkelLod_t *lod = skm->LODs+i;
		//sections
		in->read((char*)&lod->sectionsCount,4);
		//Log("Num sections: %d\n",lod->sectionsCount);
		lod->sections=new SMLsection_t[lod->sectionsCount];
		in->read((char*)lod->sections,sizeof(SMLsection_t)*lod->sectionsCount);
		for(int j=0;j<lod->sectionsCount;j++){
			SMLsection_t s=lod->sections[j];
			//Log("Section %d: m %d c %d i %d n %d\n",j,s.matIdx,s.chunkIdx,s.firstIdx,s.numTris);
			if(s.matIdx>=skm->numMaterials){
				Log("Section %d: mat idx > num mats (%d,%d)\n",j,s.matIdx,skm->numMaterials);
				lod->sections[j].matIdx=0;
			}
		}
		//indexBuffer
		in->read((char*)&lod->indSize,4);
		in->read((char*)&lod->indCount,4);
		//Log("Index buffer: size %d, count %d\n",lod->indSize,lod->indCount);
		//in->seekg(lod->indSize*lod->indCount,ios::cur);
		lod->inds=new char[lod->indSize*lod->indCount];
		in->read(lod->inds,lod->indSize*lod->indCount);
		//uint16 array
		in->read((char*)&tcount,4);
		//Log("unk uint16 array: count %d\n",tcount);
		in->seekg(tcount*2,ios::cur);
		//usedBones int16
		in->read((char*)&tcount,4);
		//Log("UsedBones int16 array: count %d\n",tcount);
		in->seekg(tcount*2,ios::cur);
		//byte array
		in->read((char*)&tcount,4);
		//Log("unk byte array: count %d\n",tcount);
		in->seekg(tcount,ios::cur);
		//chunks
		in->read((char*)&lod->chunksCount,4);
		//Log("Chunks count %d\n",lod->chunksCount);
		lod->chunks = new SMLchunk_t[lod->chunksCount];
		for(int j=0;j<lod->chunksCount;j++){
			SMLchunk_t *ch=lod->chunks+j;
			in->read((char*)&ch->firstVtx,4);
			in->read((char*)&ch->rigidVertsNumArr,4);
			in->seekg(ch->rigidVertsNumArr*sizeof(RigidVertex_t),ios::cur);
			in->read((char*)&ch->softVertsNumArr,4);
			in->seekg(ch->softVertsNumArr*sizeof(SoftVertex_t),ios::cur);
			in->read((char*)&ch->bonesNum,4);
			ch->bones=new int16_t[ch->bonesNum];
			in->read((char*)ch->bones,ch->bonesNum*2);
			in->read((char*)&ch->rigidVertsNum,4);
			in->read((char*)&ch->softVertsNum,4);
			in->read((char*)&ch->maxInfluences,4);
			//Log("Chunk %d: firstVtx %d, numRigid %d(%d), numSoft %d(%d), bones %d, maxInfluences %d\n",j,ch->firstVtx,ch->rigidVertsNumArr,ch->rigidVertsNum,ch->softVertsNumArr,ch->softVertsNum,ch->bonesNum,ch->maxInfluences);
		}
		//int (Size?)
		in->seekg(4,ios::cur);
		//numVerts
		in->read((char*)&lod->numVerts,4);
		//Log("Num verts %d\n",lod->numVerts);
		//edges
		in->read((char*)&lod->numEdges,4);
		//Log("Num edges %d\n",lod->numEdges);
		in->seekg(lod->numEdges*16,ios::cur);//ints vert1,vert2,face1,face2
		//byte array (RequiredBones?)
		in->read((char*)&tcount,4);
		//Log("RequiredBones?: count %d\n",tcount);
		in->seekg(tcount,ios::cur);
		//int16 bulk
		//in->read((char*)&tsize,4);
		//in->read((char*)&tcount,4);
		//Log("int16 bulk: size %d, count %d\n",tsize,tcount);
		//in->seekg(tsize*tcount,ios::cur);
		{
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
		//gpu skin
		{
		in->read((char*)&lod->bUseFullPrecisionUVs,32);//int,int,vec3,vec3
		in->read((char*)&lod->vertSize,8);//int,int
		//Log("GPU skin: floatUVs %d, vert size %d num %d\n",lod->bUseFullPrecisionUVs,lod->vertSize,lod->numVertsArr);
		//in->seekg(lod->vertSize*lod->numVertsArr,ios::cur);
		lod->verts = new char[lod->vertSize*lod->numVertsArr];
		in->read(lod->verts,lod->vertSize*lod->numVertsArr);
		
		for(int n=0;n<lod->numVertsArr;n++){
			(*(uint32_t*)(lod->verts+n*lod->vertSize))^=0x80808080;//tan
			(*(uint32_t*)(lod->verts+n*lod->vertSize+4))^=0x80808080;//norm
		}
		}
		//extra vertex influences
		tcount = ReadInt();
		for(int j=0;j<tcount;j++){
			int numInfs = ReadInt();
			Skip(numInfs*8);//int weights,bones
			
			int mapLength=ReadInt();
			for(int k=0;k<mapLength;k++){
				Skip(8);//2 ints key
				int tarrnum=ReadInt();
				Skip(tarrnum*2);//uint16 array
			}
		}
	}
	
	exp->pData = skm;
	return skm;
}

