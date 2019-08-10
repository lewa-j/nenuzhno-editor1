
#include <istream>
using namespace std;
#include "PCCProperties.h"
#include "log.h"
#include "MyMembuf.h"

PCCProperties::PCCProperties(){
	readed=0;
}

PCCProperties::PCCProperties(PCCFile *pcc, pccExport_t *exp, bool print)
{
	readed=0;
	pcc->in->seekg(exp->dataOffset);
	if(exp->objectFlags&0x0200000000000000){
		if(print) Log("exp has stack (flags %llX)\n",exp->objectFlags);
		pcc->in->seekg(30,ios::cur);
	}else{
		pcc->in->seekg(4,ios::cur);//unk
		int test = 0;
		pcc->in->read((char*)(&test),4);
		//Log("test %d\n",test);
		if(test>0&&test<pcc->header.nameCount){
			pcc->in->read((char*)(&test),4);
			if(test==0)
				pcc->in->seekg(-8,ios::cur);
			else
				pcc->in->seekg(-4,ios::cur);
		}
	}
	Read(pcc,pcc->in,print);
	if(print) Log("PropertiesRead: %lld bytes left\n",exp->dataSize-(pcc->in->tellg()-(int64_t)exp->dataOffset));
	readed = pcc->in->tellg()-(int64_t)exp->dataOffset;
}

PCCProperties::PCCProperties(PCCFile *pcc, char *data, int size, bool print)
{
	readed=0;
	FromData(pcc,data,size,print);
}

PCCProperties::~PCCProperties()
{
	for(uint32_t i=0;i<props.size();i++){
		props[i].Remove();
	}
	props.clear();
}

void PCCProperties::FromData(PCCFile *pcc, char *data, int size, bool print)
{
	membuf buf(data, size);
	istream in(&buf);
	Read(pcc,&in,print);
	readed = in.tellg();
}

bool PCCProperties::Read(PCCFile *pcc, istream *in, bool print)
{
	if(props.size()!=0||readed){
		Log("PropertiesRead: already loaded\n");
		return false;
	}
	if(!in->good()){
		Log("PropertiesRead: in stream not good (%X)\n",in->rdstate());
		return false;
	}

	bool ret=true;

	int i=0;
	for(i=0; i<32; i++)
	{
		PCCProperty prop;
		int namei = 0;
		in->read((char*)(&namei),4);
		in->seekg(4,ios::cur);
		if(namei<0||namei>pcc->header.nameCount){
			Log("PropertiesRead: %d name idx out of bounds %d\n",i,namei);
			ret=false;
			break;
		}
		prop.name = pcc->GetNameSP(namei);
		if(*prop.name=="None"){
			//Log("None prop\n");
			//prop.type=eNone;
			//props.push_back(prop);
			break;
		}

		int typei = 0;
		in->read((char*)(&typei),4);
		in->seekg(4,ios::cur);
		int size = 0;
		in->read((char*)(&size),4);
		in->seekg(4,ios::cur);//unk
		//24 byte readed
		string &type = pcc->GetNameS(typei);
		if(print) Log("prop %d: name %s, type %s, size %d\n",i,prop.name->c_str(),type.c_str(),size);

		if(type == "ArrayProperty"){
			//int count=0;
			//pcc->in->read((char*)(&count),4);
			//Log("Count %d\n",count);
			//pcc->in->seekg(size-4,ios::cur);
			prop.type=eArrayProperty;
			prop.size=size;
			prop.data=new char[size];
			in->read(prop.data,size);
		}else if(type == "IntProperty"){
			//int val=0;
			//in->read((char*)(&val),4);
			//Log("Value: %d\n",val);
			prop.type=eIntProperty;
			prop.size=4;
			prop.data=new char[4];
			in->read(prop.data,4);
		}else if(type == "FloatProperty"){
			//float fval=0;
			//in->read((char*)(&fval),4);
			//Log("Value: %f\n",fval);
			prop.type=eFloatProperty;
			prop.size=4;
			prop.data=new char[4];
			in->read(prop.data,4);
		}else if(type == "StructProperty"){
			//int snamei = 0;
			//in->read((char*)(&snamei),4);
			//in->seekg(4,ios::cur);
			//Log("Struct name %s\n",GetName(snamei));
			//in->seekg(size,ios::cur);
			prop.type=eStructProperty;
			prop.size=size+8;
			prop.data=new char[size+8];
			in->read(prop.data,size+8);
		}else if(type == "ByteProperty"){
			//int snamei = 0;
			//in->read((char*)(&snamei),4);
			//in->seekg(4,ios::cur);
			//Log("Byte name %s\n",GetName(snamei));
			//in->seekg(size,ios::cur);
			prop.type=eByteProperty;
			prop.size=size+8;
			prop.data=new char[size+8];
			in->read(prop.data,size+8);
		}else if(type == "BoolProperty"){
			//bool val=0;
			//in->read((char*)&val,1);
			//Log("Bool val %d\n",val);
			prop.type=eBoolProperty;
			prop.size=1;
			prop.data=new char[1];
			in->read(prop.data,1);
		}else if(type=="NameProperty"){
			//int val = 0;
			//in->read((char*)(&val),4);
			//in->seekg(4,ios::cur);
			//Log("Value: %s\n",GetName(val));
			prop.type=eNameProperty;
			prop.size=8;
			prop.data=new char[8];
			in->read(prop.data,8);
		}else if(type=="ObjectProperty"){
			//int val = 0;
			//in->read((char*)(&val),4);
			//Log("Value: %d %s\n", val, GetObjName(val));
			prop.type=eObjectProperty;
			prop.size=4;
			prop.data=new char[4];
			in->read(prop.data,4);
		}else{
			Log("Unknown type property %s (%s)\n",prop.name->c_str(),type.c_str());
			ret=false;
		}
		props.push_back(prop);
	}
	if(print) Log("Readed %d properties\n",props.size());

	return ret;
}
