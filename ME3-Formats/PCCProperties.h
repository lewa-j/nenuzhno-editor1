
#pragma once

#include "pcc.h"
#include <vector>
#include <string>

enum ePropType
{
	eNone,
	eArrayProperty,
	eObjectProperty,
	eStructProperty,
	eNameProperty,
	eIntProperty,
	eFloatProperty,
	eByteProperty,
	eBoolProperty
};

class PCCProperty
{
public:
	PCCProperty(){
		data=0;
		//Log("PCCProperty()\n");
	}
	~PCCProperty(){
		//Log("~PCCProperty()\n");
	}
	void Remove()
	{
		if(data)
			delete[] data;
		data=0;
	}
	std::string *name;
	ePropType type;
	int size;
	char *data;

	int GetInt(int offs=0){
		return *(int*)(data+offs);
	}
	float GetFloat(int offs=0){
		return *(float*)(data+offs);
	}
	bool GetBool(){
		return *(char*)data;
	}
	std::string *GetName(PCCFile *pcc, int offs=0){
		return pcc->GetNameSP(*(int*)(data+offs));
	}
	glm::vec3 GetVec3(int offs=0){
		return *(glm::vec3*)(data+offs);
	}
	glm::vec3 GetRot3(int offs=0){
		float f = 0.000095873799242;//852576857380474343247;// 2*Pi/65536
		//float f = 0.0000152587890625;// 1/65536
		return glm::vec3((GetInt(offs)%65536)*f,(GetInt(offs+4)%65536)*f,(GetInt(offs+8)%65536)*f);
	}
	void GetRaw(void *dst, int len, int offs){
		memcpy(dst,data+offs,len);
	}
};

class PCCProperties
{
public:
	PCCProperties();
	PCCProperties(PCCFile *pcc, pccExport_t *exp, bool print = false);
	PCCProperties(PCCFile *pcc, char *data, int size, bool print = false);
	~PCCProperties();

	std::vector<PCCProperty> props;
	
	int GetReaded(){return readed;}
	void FromData(PCCFile *pcc, char *data, int size, bool print = false);
private:
	bool Read(PCCFile *pcc, std::istream *in, bool print);
	int readed;
};
