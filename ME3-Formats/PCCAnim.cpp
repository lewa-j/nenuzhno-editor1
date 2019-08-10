
#include "log.h"
#include "PCCAnim.h"
#include "pcc.h"
#include "PCCProperties.h"

AnimSet::AnimSet()
{
	numSeqs=0;
	animSetData=0;
}

AnimSet *PCCFile::GetAnimSet(int idx)
{
	if(idx<=0||idx>header.exportCount){
		Log("GetAnimSet: invalid index %d\n",idx);
		return 0;
	}
	
	pccExport_t *pExp=exports+idx-1;
	if(GetObjNameS(pExp->class_)!="AnimSet"){
		Log("GetAnimSet: %d invalid class %s\n",idx,GetObjName(pExp->class_));
		return 0;
	}
	if(pExp->pData)
		return (AnimSet*)pExp->pData;

	Log("GetAnimSet: export %d, name %s, data: len %d, offs %d\n",idx-1,GetName(pExp->objectName), pExp->dataSize,pExp->dataOffset);

	PCCProperties props(this,pExp,true);
	AnimSet *as = new AnimSet();
	int asdIdx=0;
	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eArrayProperty){
			if(*p->name=="Sequences"){
				as->numSeqs = p->GetInt();
				Log("AnimSet: seqs count %d\n",as->numSeqs);
				for(int j=0;j<as->numSeqs;j++){
					int s=p->GetInt(j*4+4);
					Log("seq %d: %d %s\n",j,s,GetObjName(s));
				}
			}
		}else if(p->type==eObjectProperty && *p->name=="m_pBioAnimSetData"){
			asdIdx=p->GetInt();
			Log("AnimSet: m_pBioAnimSetData %d %s\n",asdIdx,GetObjName(asdIdx));
		}else if(p->type==eNameProperty && *p->name=="PreviewSkelMeshName"){
			//as->previewSkelMeshName = p->GetName();
			Log("AnimSet: PreviewSkelMeshName %s\n",p->GetName(this)->c_str());
		}
	}
	if(asdIdx)
		as->animSetData = GetBioAnimSetData(asdIdx);
	
	pExp->pData = as;
	return as;
}

BioAnimSetData::BioAnimSetData()
{
	trackBoneNames=0;
	numTrackBoneNames=0;
	bAnimRotationOnly=false;
}

BioAnimSetData *PCCFile::GetBioAnimSetData(int idx)
{
	if(idx<=0||idx>header.exportCount){
		Log("GetBioAnimSetData: invalid index %d\n",idx);
		return 0;
	}
	
	pccExport_t *pExp=exports+idx-1;
	if(GetObjNameS(pExp->class_)!="BioAnimSetData"){
		Log("GetBioAnimSetData: %d invalid class %s\n",idx,GetObjName(pExp->class_));
		return 0;
	}
	if(pExp->pData)
		return (BioAnimSetData*)pExp->pData;

	Log("GetBioAnimSetData: export %d, name %s, data: len %d, offs %d\n",idx-1,GetName(pExp->objectName), pExp->dataSize,pExp->dataOffset);

	PCCProperties props(this,pExp,true);
	BioAnimSetData *asd = new BioAnimSetData();
	
	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eArrayProperty){
			if(*p->name=="TrackBoneNames"){
				asd->numTrackBoneNames = p->GetInt();
				Log("GetBioAnimSetData: TrackBoneNames count %d\n",asd->numTrackBoneNames);
				asd->trackBoneNames = new const char*[asd->numTrackBoneNames];
				for(int j=0;j<asd->numTrackBoneNames;j++){
					asd->trackBoneNames[j]=p->GetName(this,j*8+4)->c_str();
					Log("bone %d: %s\n",j,asd->trackBoneNames[j]);
				}
			}
		}else if(p->type==eBoolProperty && *p->name=="bAnimRotationOnly"){
			asd->bAnimRotationOnly = p->GetBool();
			Log("GetBioAnimSetData: bAnimRotationOnly %d\n",asd->bAnimRotationOnly);
		}
	}
	
	pExp->pData = asd;
	return asd;
}
