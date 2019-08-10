
#include "system/FileSystem.h"
#include "PCCSystem.h"
#include "log.h"
using namespace std;

PCCSystem::PCCSystem()
{

}

PCCSystem::~PCCSystem()
{
	for(PCCMapIt it=PCCs.begin();it!=PCCs.end();it++){
		delete it->second;
	}
	PCCs.clear();
}

PCCFile *PCCSystem::GetPCC(string fileName)
{
	if(PCCs.find(fileName)!=PCCs.end()){
		return PCCs[fileName];
	}
	PCCFile *tpcc = new PCCFile(this);

	char path[256];
	g_fs.GetFilePath(fileName.c_str(),path);
	if(!tpcc->Load(path)){
		Log("Can't load PCC file %s\n",fileName.c_str());
		delete tpcc;
		return 0;
	}
	//Log("Loaded %s\n",fileName.c_str());

	PCCs.insert(pair<string,PCCFile*>(fileName,tpcc));
	return tpcc;
}

bool PCCSystem::FindImport(PCCFile *pcc, pccImport_t *imp, PCCFile *&outPcc, pccExport_t *&outExp)
{
	for(PCCMapIt it=PCCs.begin();it!=PCCs.end();it++){
		PCCFile *p = it->second;
		if(p==pcc)
			continue;

		string name = pcc->GetNameS(imp->importName);
		for(int i=0;i<p->header.exportCount;i++){
			if(p->GetNameS(p->exports[i].objectName)==name&&p->exports[i].objectNameNumber==imp->importNameNumber){
				outPcc = p;
				outExp = p->exports+i;
				//Log("FindImport: %s found in %s\n",name.c_str(),it->first.c_str());
				return true;
			}
		}
	}

	imp->pData = (PCCObject*)-1;
	return false;
}
