
#include "pcc.h"
#include "PCCSystem.h"
#include <cstring>

char gamedir[32] = "ME3";

int main(int argc, char* argv[])
{
	char fileName[256] = {0};
#ifndef ANDROID
	if(argc<2){	
		printf("usage:\n%s <file name>\n",argv[0]);
		return 1;
	}
	strcpy(fileName, argv[1]);
#else
	strcpy(fileName, 
	//"SFXWeapon_Pistol_Predator.pcc");
	//"Core.pcc");
	//"Engine.pcc");
	//"BioA_Nor_205Bridge.pcc");
	"AreaMap_NorCIC.pcc");
	//"SFXWeaponImages_HeavyPistols.pcc");
#endif

	//printf("loading: %s\n", fileName);
	PCCSystem pccSystem;
	PCCFile *pcc;

	pcc = pccSystem.GetPCC(fileName);

	pccSystem.GetPCC("BioP_Nor.pcc");

	if(!pcc){
		return -1;
	}

	printf("imports:\n");
	for(int i=0; i<pcc->header.importCount; i++)
	{
		printf("%d: package name %s %d, class name %s %d, link %d, import name %s %d\n",i, pcc->GetName(pcc->imports[i].packageName),pcc->imports[i].packageNameNumber,
			 pcc->GetName(pcc->imports[i].className),pcc->imports[i].classNameNumber, pcc->imports[i].link, pcc->GetName(pcc->imports[i].importName),pcc->imports[i].importNameNumber);
	}

	printf("exports:\n");
	for(int i=0; i<pcc->header.exportCount; i++)
	{
		printf("%d: class %d, super class %d, link %d, object name %s %d, archetype %d, size %d, offset %d\n",
			i,pcc->exports[i].class_, pcc->exports[i].superClass, pcc->exports[i].link,  pcc->GetName(pcc->exports[i].objectName),pcc->exports[i].objectNameNumber,pcc->exports[i].archetype,pcc->exports[i].dataSize,pcc->exports[i].dataOffset);
	}
	
	return 0;
}



