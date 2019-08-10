
#pragma once

#include <map>
#include "pcc.h"

typedef std::map<std::string,PCCFile*>::iterator PCCMapIt;

class PCCSystem
{
public:
	PCCSystem();
	~PCCSystem();

	PCCFile *GetPCC(std::string fileName);
	bool FindImport(PCCFile *pcc,pccImport_t *imp,PCCFile *&outPcc, pccExport_t *&outExp);

	std::map<std::string,PCCFile*> PCCs;
};
