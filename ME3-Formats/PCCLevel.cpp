
#include "log.h"
#include "pcc.h"
#include "PCCLevel.h"
#include "PCCProperties.h"
using namespace std;
#include <common.hpp>
#include <gtc/matrix_transform.hpp>

PCCLevel::PCCLevel(PCCFile *p, int id)
{
	pcc = p;
	objId = id;
	objectsCount = 0;
	objectsIdxs = 0;
	objects = 0;
}

PCCLevel::~PCCLevel()
{
	if(objectsIdxs)
		delete[] objectsIdxs;
	
	if(objects){
		//for(int i=0;i<objectsCount;i++){
		//	if(objects[i])
		//		delete objects[i];
		//}
		delete[] objects;
	}
}

void PCCLevel::Loaded()
{
	for(int i=0;i<objectsCount;i++){
		if(objects[i] && objects[i]->type==ePrefabInstance){
			PrefabInstance *prIn = (PrefabInstance*)objects[i];
			for(int j=0;j<prIn->objectsCount;j++){
				LevelObject *lo=pcc->GetLevelObject(prIn->objectsIds[j*2+1]);
				if(lo&&lo->type==eStaticMeshActor){
					//Log("MergePrefab: %d %d\n",prIn->objectsIds[j*2],prIn->objectsIds[j*2+1]);
					((StaticMeshActor*)lo)->MergePrefab((StaticMeshActor*)pcc->GetLevelObject(prIn->objectsIds[j*2]));
				}
			}
		}
	}

	bbmin=glm::vec3(999999);
	bbmax=glm::vec3(-999999);

	for(int i=0;i<objectsCount;i++){
		if(objects[i]){
			objects[i]->AddToBB(bbmin,bbmax);
		}
	}
	Log("Level bounds (%f, %f, %f) (%f, %f, %f)\n",bbmin.x,bbmin.y,bbmin.z, bbmax.x,bbmax.y,bbmax.z);
}

void StaticMeshActor::AddToBB(glm::vec3 &bbmin, glm::vec3 &bbmax)
{
	bbmin.x = glm::min(bbmin.x,location.x);
	bbmin.y = glm::min(bbmin.y,location.y);
	bbmin.z = glm::min(bbmin.z,location.z);
	bbmax.x = glm::max(bbmax.x,location.x);
	bbmax.y = glm::max(bbmax.y,location.y);
	bbmax.z = glm::max(bbmax.z,location.z);
}

void StaticMeshActor::CalcMtx()
{
	modelMtx = glm::mat4(1.0);
	modelMtx = glm::translate(modelMtx,location);
	modelMtx = glm::rotate(modelMtx,rotation.y,glm::vec3(0,0,1));
	modelMtx = glm::rotate(modelMtx,rotation.x,glm::vec3(0,-1,0));
	modelMtx = glm::rotate(modelMtx,rotation.z,glm::vec3(-1,0,0));
	modelMtx = glm::scale(modelMtx,scale);
}

void StaticMeshActor::MergePrefab(StaticMeshActor *sma)
{
	/*if(!pStatMeshComp||!pStatMeshComp->pMesh){
		pStatMeshComp = sma->pStatMeshComp;
	}*/
	if(!pStatMeshComp->pMesh){
		pStatMeshComp->pMesh = sma->pStatMeshComp->pMesh;
	}
	if(rotation==glm::vec3(0.0f)){
		rotation = sma->rotation;
	}
	if(scale==glm::vec3(1.0f)){
		scale = sma->scale;
	}
	CalcMtx();
}

void StaticMeshCollectionActor::AddToBB(glm::vec3 &bbmin, glm::vec3 &bbmax)
{
	for(int i=0;i<objectsCount;i++){
		glm::vec4 location = matrices[i][3];
		bbmin.x = glm::min(bbmin.x,location.x);
		bbmin.y = glm::min(bbmin.y,location.y);
		bbmin.z = glm::min(bbmin.z,location.z);
		bbmax.x = glm::max(bbmax.x,location.x);
		bbmax.y = glm::max(bbmax.y,location.y);
		bbmax.z = glm::max(bbmax.z,location.z);
	}
}

PCCLevel *PCCFile::GetLevel(int idx)
{
	if(idx<=0||idx>header.exportCount){
		Log("GetLevel: invalid index %d\n",idx);
		return nullptr;
	}
	
	pccExport_t *pExp=exports+idx-1;
	if(GetObjNameS(pExp->class_)!="Level"){
		Log("GetLevel: %d invalid class %s\n",idx,GetObjName(pExp->class_));
		return nullptr;
	}
	
	Log("GetLevel: export %d name: %s, data: len %d, offs %d\n",pExp-exports,GetName(pExp->objectName), pExp->dataSize,pExp->dataOffset);

	PCCProperties props(this,pExp,true);
	
	PCCLevel *pLvl = new PCCLevel(this,idx);
	
	in->seekg(4,ios::cur);//unk
	
	in->read((char*)&pLvl->objectsCount,4);
	Log("Level objects count %d\n",pLvl->objectsCount);
	pLvl->objectsIdxs=new int[pLvl->objectsCount];
	pLvl->objects=new LevelObject*[pLvl->objectsCount];
	in->read((char*)pLvl->objectsIdxs,pLvl->objectsCount*4);
	for(int i=0;i<pLvl->objectsCount;i++){
		//Log("Object %d: %d\n",i,pLvl->objectsIdxs[i]);
		if(pLvl->objectsIdxs[i]>0)
			Log("Object %d: exp %d %s(%s)\n",i,pLvl->objectsIdxs[i]-1,GetObjName(pLvl->objectsIdxs[i]),GetObjName((exports+pLvl->objectsIdxs[i]-1)->class_));
		else
			Log("Object %d: imp %d %s\n",i,-pLvl->objectsIdxs[i]-1,GetObjName(pLvl->objectsIdxs[i]));
		pLvl->objects[i] = GetLevelObject(pLvl->objectsIdxs[i]);
	}

	pLvl->Loaded();

	return pLvl;
}

LevelObject *PCCFile::GetLevelObject(int idx)
{
	if(idx<=0||idx>header.exportCount)
		return 0;
	
	pccExport_t *pExp = exports+idx-1;
	string &className = GetObjNameS(pExp->class_);
	
	if(className=="StaticMeshActor"){
		return GetStaticMeshActor(pExp);
	}else if(className=="StaticMeshCollectionActor"){
		return GetStaticMeshCollectionActor(pExp);
	}else if(className=="PrefabInstance"){
		return GetPrefabInstance(pExp);
	}
	//TODO StaticLightCollectionActor
	//SFXDoor
	//LensFlareSource
	
	return 0;
}

StaticMeshActor *PCCFile::GetStaticMeshActor(pccExport_t *exp)
{
	if(exp->pData)
		return (StaticMeshActor*)exp->pData;

	//Log("GetStaticMeshActor: export %d name: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), exp->dataSize,exp->dataOffset);

	PCCProperties props(this,exp);
	//Log("GetStaticMeshActor: props %d\n",props.props.size());

	StaticMeshActor *sma = new StaticMeshActor((int)(exp-exports)+1);
	sma->scale = glm::vec3(1.0f);

	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eStructProperty){
			if(*p->name=="location"){
				sma->location = p->GetVec3(8);
			}else if(*p->name=="Rotation"){
				sma->rotation = p->GetRot3(8);
			}else if(*p->name=="DrawScale3D"){
				sma->scale *= p->GetVec3(8);
			}
		}else if(p->type==eObjectProperty && *p->name=="StaticMeshComponent"){
			if(p->GetInt()>0)
				sma->pStatMeshComp = GetStaticMeshComponent(exports+p->GetInt()-1);
			else
				Log("GetStaticMeshActor %d: StaticMeshComponent is %d\n",exp-exports,p->GetInt());
		}else if(p->type==eFloatProperty && *p->name=="DrawScale"){
			sma->scale *= p->GetFloat();
			//Log("sma %d: scale %f\n",exp-exports,p->GetFloat());
		}
	}
	sma->scale *= sma->pStatMeshComp->scale;
	sma->CalcMtx();
	
	exp->pData = sma;
	return sma;
}

StaticMeshCollectionActor *PCCFile::GetStaticMeshCollectionActor(pccExport_t *exp)
{
	if(exp->pData)
		return (StaticMeshCollectionActor*)exp->pData;

	PCCProperties props(this,exp);

	//Log("StaticMeshCollectionActor: props %d\n",props.props.size());

	StaticMeshCollectionActor *smca = new StaticMeshCollectionActor((int)(exp-exports)+1);

	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eArrayProperty && *p->name=="StaticMeshComponents"){
			smca->objectsCount = p->GetInt();
			Log("GetStaticMeshCollectionActor: objectsCount %d\n",smca->objectsCount);
			smca->objectsIdxs = new int[smca->objectsCount];
			memcpy(smca->objectsIdxs,p->data+4,smca->objectsCount*4);
		}
	}

	//matrices
	smca->matrices=new glm::mat4[smca->objectsCount];
	in->read((char*)smca->matrices,smca->objectsCount*64);

	//Log("GetStaticMeshCollectionActor: %lld bytes left\n",exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset));

	smca->pMeshes = new StaticMeshComponent*[smca->objectsCount];
	for(int i=0;i<smca->objectsCount;i++){
		if(smca->objectsIdxs[i]>0){
			if((smca->pMeshes[i] = GetStaticMeshComponent(exports+smca->objectsIdxs[i]-1))!=0){
				smca->matrices[i] = glm::scale(smca->matrices[i],smca->pMeshes[i]->scale);
			}
		}
	}

	exp->pData = smca;
	return smca;
}

PrefabInstance *PCCFile::GetPrefabInstance(pccExport_t *exp)
{
	if(exp->pData)
		return (PrefabInstance*)exp->pData;

	Log("GetPrefabInstance: export %d name: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), exp->dataSize,exp->dataOffset);

	PCCProperties props(this,exp);
	
	PrefabInstance *prIn = new PrefabInstance((int)(exp-exports)+1);

	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eObjectProperty && *p->name=="TemplatePrefab"){
			prIn->prefId = p->GetInt();
			Log("GetPrefabInstance: TemplatePrefab %d %s(%s)\n",prIn->prefId,GetObjName(prIn->prefId),GetObjName((exports+prIn->prefId-1)->class_));
		}else if(p->type==eStructProperty){
			if(*p->name=="location"){
				prIn->location = p->GetVec3(8);
				Log("GetPrefabInstance: location (%f %f %f)\n",prIn->location.x,prIn->location.y,prIn->location.z);
			}
		}
	}


	in->read((char*)&prIn->objectsCount,4);
	Log("GetPrefabInstance: objects count %d\n",prIn->objectsCount);
	prIn->objectsIds = new int[prIn->objectsCount*2];//prefab obj and level obj
	in->read((char*)prIn->objectsIds,prIn->objectsCount*8);

	//for(int i=0;i<prIn->objectsCount;i++){
	//	Log("Prefab obj %d: %d %s, %d %s\n",i,prIn->objectsIds[i*2],GetObjName(prIn->objectsIds[i*2]),prIn->objectsIds[i*2+1],GetObjName(prIn->objectsIds[i*2+1]));
	//}

	exp->pData = prIn;
	return prIn;
}

