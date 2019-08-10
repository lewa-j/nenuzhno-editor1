
#include <cstdlib>
#include <cmath>
#include <fstream>
using namespace std;

#include <gtc/matrix_transform.hpp>
#include <zlib.h>

#include "PCCSystem.h"
#include "pcc.h"
#include "engine.h"
#include "log.h"
#include "system/FileSystem.h"
#include "PCCProperties.h"
#include "MyMembuf.h"

void UTF16to8(char *str,int l)
{
	for(int i=1; i<l; i++)
	{
		str[i]=str[i*2];
	}
}

int DecompressBlock(char *src, int srcSize, char *dst, int dstSize);
bool ReadChunk(istream *pIn,int offs,char *dst);

//=======================
//=======PCCObject=======
//=======================

PCCFile::PCCFile(PCCSystem *sys)
{
	pccSys = sys;
	in=0;
	compressed=0;
	uncompressedData = 0;
	uncompressedSize = 0;
	nameTable=0;
	imports=0;
	exports=0;
}

PCCFile::~PCCFile()
{
	Log("Destroying %s\n",pccName.c_str());
	if(compressed)
	{
		delete[] uncompressedData;
	}
	if(in){//else not loaded
		//in.close();
		delete in;

		delete[] nameTable;
		delete[] imports;
		for(int i=0;i<header.exportCount;i++){
			if(exports[i].pData){
				delete exports[i].pData;
			}
		}
		delete[] exports;
	}
}

bool PCCFile::Load(const char *fileName)
{
	pccName=fileName;
	in = new ifstream(fileName, ios::binary);
	if(!in||!(*in))
	{
		//Log( "File: %s not found\n", fileName);
		if(in)
			delete in;
		in = 0;
		return false;
	}

	Log("loading pcc: %s\n", fileName);

	//Log("sizeof(pccHeader_t) %d\n",sizeof(pccHeader_t));
	in->read((char *)&header,sizeof(pccHeader_t));

	if(header.ident != PCCIDENT)
	{
		Log("Unknown header ident: %d\n",header.ident);
		return false;
	}
	UTF16to8((char*)header.pkg,5);

	Log("pccheader: ver %d.%d, pkg %s, flags %X, name count %d offset %d, export count %d, import count %d, generations %d, engine %d\n",
				header.version[0],header.version[1], (char*)header.pkg, header.packageFlags, header.nameCount, header.nameOffset, header.exportCount, header.importCount, header.generations, header.engine);

	compressed = header.packageFlags&PCC_FLAG_COMPRESSED;
	uncompressedData = 0;
	uncompressedSize = 0;
	membuf *buf;
	
	if(!(header.packageFlags&8)){
		Log("Error: no flag 8\n");
		//in.close();
		delete in;
		return false;
	}
	
	if(header.generations!=1){
		Log("Error: generations != 1\n");
		//in.close();
		delete in;
		return false;
	}

	if(compressed)
	{
		//Log("pcc compressed %s\n",fileName);
		
		pccChunks_t *chunksHdrs=new pccChunks_t[header.chunkCount];
		in->read((char *)chunksHdrs,sizeof(pccChunks_t)*header.chunkCount);

		in->seekg(8,ios::cur);
		//int headerEnd = in->tellg();
		//int cmprOffs = chunksHdrs[0].compressedOffset-headerEnd;
		//Log("compressed offset %d, header end %d, expDataBegOffset %d\n",cmprOffs,headerEnd,header.expDataBegOffset);
		
		uncompressedSize = chunksHdrs[0].uncompressedOffset;
		for(int i = 0; i<header.chunkCount; i++){
			//LOG("chunks %d: uncompressed offset %d size %d, compressed offset %d size %d\n", i, chunksHdrs[i].uncompressedOffset,chunksHdrs[i].uncompressedSize,chunksHdrs[i].compressedOffset,chunksHdrs[i].compressedSize);
			uncompressedSize += chunksHdrs[i].uncompressedSize;
		}
		Log("Uncompressed size %d\n",uncompressedSize);
		uncompressedData = new char[uncompressedSize];
		int uncPos = chunksHdrs[0].uncompressedOffset;

		for(int c=0;c<header.chunkCount; c++)
		{
			uncPos = chunksHdrs[c].uncompressedOffset;
			if(ReadChunk(in,chunksHdrs[c].compressedOffset,uncompressedData+uncPos))
				Log("Uncompressing chunk %d error\n",c);
		}
		delete[] chunksHdrs;

		buf = new membuf(uncompressedData, uncompressedSize);
		delete in;
		in = new istream(buf);

		//Log("Uncompressing done\n");
	}
	
	//name
	uint16_t buffer[2048]={0};
	nameTable = new string[header.nameCount];
	//LOG("names: ");
	in->seekg(header.nameOffset);
	int nameLength=0;
	for(int i=0; i<header.nameCount; i++)
	{
		in->read((char*)&nameLength, 4);
		nameLength*=-1;
		if(nameLength>=2048)
			nameLength=2047;
		in->read((char*)buffer, nameLength*2);
		UTF16to8((char*)buffer, nameLength);
		//LOG("%d %s, ", nameLength, (char*)buffer);
		nameTable[i]=(char*)buffer;
	}
	//LOG("\n");

	//import
	imports = new pccImport_t[header.importCount];
	memset((char*)imports,0,sizeof(pccImport_t)*header.importCount);
	in->seekg(header.importOffset);
	//in->read((char*)imports,header.importCount*sizeof(pccImport_t));
	//LOG("imports:\n");
	for(int i=0; i<header.importCount; i++)
	{
		in->read((char*)(imports+i),pccImportSize);
		//LOG("%d: package name %s %d, class name %s %d, link %d, import name %s %d\n",i, nameTable[imports[i].packageName].c_str(),imports[i].packageNameNumber,
		//	nameTable[imports[i].className].c_str(),imports[i].classNameNumber, imports[i].link, nameTable[imports[i].importName].c_str(),imports[i].importNameNumber);
	}
	
	//export
	//Log("export size %d\n",sizeof(pccExport_t));
	exports = new pccExport_t[header.exportCount];
	memset((char*)exports,0,sizeof(pccExport_t)*header.exportCount);
	in->seekg(header.exportOffset);
	//LOG("exports:\n");
	for(int i=0; i<header.exportCount; i++)
	{
		in->read((char*)(exports+i),pccExportSize);
		//LOG("%d: class %d, super class %d, link %d, object name %s %d, archetype %d, size %d, offset %d\n",
		//	i,exports[i].class_, exports[i].superClass, exports[i].link, nameTable[exports[i].objectName].c_str(),exports[i].objectNameNumber,exports[i].archetype,exports[i].dataSize,exports[i].dataOffset);
		in->seekg(exports[i].count*4, ios::cur);
	}

	return true;
}

PCCTexture2D *PCCFile::GetTexture(int idx)
{
	if(idx==0)
		return 0;
	if(idx<0){
		if(imports[-idx-1].pData){
			if(imports[-idx-1].pData==(PCCObject*)-1)
				return 0;
			return (PCCTexture2D*)imports[-idx-1].pData;
		}
		PCCFile *outPcc=0;
		pccExport_t *outExp=0;
		if(pccSys->FindImport(this,imports-idx-1,outPcc,outExp)){
			return (PCCTexture2D*)(imports[-idx-1].pData = outPcc->GetTexture(outExp));
		}
		return 0;
	}
	return GetTexture(exports+idx-1);
}

//#define NEWMATLOAD 1
#if NEWMATLOAD
void PCCFile::ReadMaterialResource(PCCMaterial *pMat){
	//CompileErrors
	int count = ReadInt();
	if(count){
		Log("mat: CompileErrors count %d\n",count);
		for(int i=0;i<count;i++){
			char tstr[512];
			int tstrlen=ReadInt();
			if(!tstrlen)
				break;
			in->read(tstr, -tstrlen*2);
			UTF16to8(tstr, -tstrlen);
			Log("mat: err %d len %d (%s)\n",i,tstrlen,tstr);
		}
		//temp
		EngineError("mat CompileErrors");
	}
	//TextureDependencyLength
	count=ReadInt();
	if(count)
		Log("mat: TextureDependencyLength map count %d\n",count);
	for(int i=0;i<count;i++){
		int objid=ReadInt();
		int ival=ReadInt();
		Log("map %d obj %d (%s) int %d\n",i,objid,GetObjName(objid),ival);
	}
	int maxTexDependencyLen=ReadInt();
	Skip(16);//GUID
	int numUserTexCoords=ReadInt();
	if(maxTexDependencyLen)
		Log("mat: maxTexDependencyLen %d numUserTexCoords %d\n",maxTexDependencyLen,numUserTexCoords);
		
	count=ReadInt();
	if(count)
		Log("expression texures count %d\n",count);
	if(pMat){
		pMat->numExpressionTextures = count;
		if(count){
			pMat->expressionTextures = new int[count];
			in->read((char*)pMat->expressionTextures,count*4);
			for(int i=0;i<pMat->numExpressionTextures;i++){
				int objid=pMat->expressionTextures[i];
				Log("tex %d: %d %s\n",i,objid,GetObjName(objid));
			}
		}
	}else{
		for(int i=0;i<count;i++){
			int objid=ReadInt();
			Log("tex %d: %d %s\n",i,objid,GetObjName(objid));
		}
	}
	//bool usesSceneColor, usesSceneDepth
	//bool usesDynamicParameter, usesLmUVs, usesMatVertPosOffs
	Skip(5*4);
	
	int usingTransform = ReadInt();
	//Log("mat: using transform %d\n",usingTransform);
		
	count = ReadInt();
	if(count){
		Log("mat: textureLookups count %d\n",count);
		for(int l=0;l<count;l++){
			int texCoordIndex=ReadInt();
			int texIndex=ReadInt();
			glm::vec2 tileScale;
			in->read((char*)&tileScale,8);
			Log("lookup %d: coord %d tex %d scale %fx%f\n",texCoordIndex,texIndex,tileScale.x,tileScale.y);
		}
	}
	Skip(4);//int
}

PCCMaterial *PCCFile::GetMaterial(pccExport_t *exp,bool print)
{
	if(exp->pData)
		return (PCCMaterial*)exp->pData;
	
	Log("GetMaterial: export %d name: %s, class: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), GetObjName(exp->class_), exp->dataSize,exp->dataOffset);
	PCCMaterial *pMat=0;
	if(GetObjNameS(exp->class_)=="MaterialInstanceConstant"){
		pMat = GetMatInstConst(exp,print);
	}else if(GetObjNameS(exp->class_)=="Material"){
		pMat = new PCCMaterial((int)(exp-exports)+1);
		PCCProperties props(this,exp,print);
		
		int *exprIds=0;
		int exprNum=0;
		
		for(uint32_t i=0; i<props.props.size();i++){
			PCCProperty *p=&props.props[i];
			if(p->type==eArrayProperty&&*p->name=="Expressions"){
				exprNum=p->GetInt();
				exprIds=new int[exprNum];
				p->GetRaw(exprIds,exprNum*4,4);
				//for(int e=0;e<exprNum;e++){
				//	Log("mat: expr %d: %d (%s)\n",e,exprIds[e],GetObjName(exprIds[e]));
				//}
			}
			/*else if(p->type==eByteProperty&&*p->name=="BlendMode"){
				string *bmode=p->GetName(this,8);
				Log("BlendMode: %d %s, %d %s\n",p->GetInt(),p->GetName(this)->c_str(),p->GetInt(8),bmode->c_str());
				if(*bmode=="BLEND_Masked")
					pMat->blendMode=eBlendMasked;
				else if(*bmode=="BLEND_Translucent")
					pMat->blendMode=eBlendTranslucent;
				else if(*bmode=="BLEND_Additive")
					pMat->blendMode=eBlendAdditive;
			}*/
			else if(p->type==eObjectProperty&&*p->name=="Parent"){
				int parId = p->GetInt();
				Log("mat: parent %d (%s)\n",parId,GetObjName(parId));
			}else if(p->type==eStructProperty&&*p->name=="DiffuseColor"){
				int sname = p->GetInt();
				Log("mat: DiffuseColor struct %d(%s)\n",sname,GetName(sname));
				for(int i=8;i<p->size;i+=4){
					Log("struct bytes %d: i %d n %s o %s f %f\n",i,p->GetInt(i),GetName(p->GetInt(i)),GetObjName(p->GetInt(i)),p->GetFloat(i));
				}
			}
		}
		
		//MaterialResource sm3
		ReadMaterialResource(pMat);
		
		//ShaderMap ?
		int hasShaderMap=ReadInt();
		if(hasShaderMap)
			Log("mat: has shader map %d\n",hasShaderMap);
		if(hasShaderMap==1)
		{
			int numShaders = ReadInt();
			if(numShaders){
				Log("shaders count %d\n",numShaders);
				EngineError("ShaderMap doesn't implemented");
			}
		}
		
		//MaterialResource sm2
		//ReadMaterialResource(0);
	
		//if(exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset))
		//	Log("mat: %d left %d bytes\n",exp-exports,exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset));
		//end binary
		
		//expressions
		for(int i=0;i<exprNum;i++){
			Log("mat: expr %d: %d (%s)\n",i,exprIds[i],GetObjName(exprIds[i]));
			PrintProperties(exports+exprIds[i]-1);
		}
	}else{
		Log("GetMaterial: unknown class\n");
	}
	exp->pData = pMat;
	return pMat;
}

PCCMaterial *PCCFile::GetMatInstConst(pccExport_t *exp, bool print)
{
	PCCMaterial *pMat = new PCCMaterial((int)(exp-exports)+1);
	
	PCCProperties props(this,exp,print);
	int texParamsCount=0;
	int texParamsId=-1;
	int diffId = 0;
	int normId = 0;
	int firstId = -1;
	int parId=0;
	for(uint32_t i=0; i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eArrayProperty&&*p->name=="TextureParameterValues"){
			texParamsId=i;
			texParamsCount=p->GetInt();
			if(print) Log("TexParams count %d\n",texParamsCount);
			if(texParamsCount>0){
				int curOffs=4;
				for(int k=0;k<texParamsCount;k++){
					PCCProperties texVals(this,p->data+curOffs,p->size-curOffs);
					curOffs += texVals.GetReaded();
					if(print) Log("param name %s, tex %d %s\n",GetName(texVals.props[1].GetInt()),texVals.props[2].GetInt(),GetObjName(texVals.props[2].GetInt()));
					string &parName=GetNameS(texVals.props[1].GetInt());
					if(parName=="Diffuse"||
							parName=="Texture"||
							parName=="Simple Diffuse"||
							parName=="Diffuse_Map"||
							parName=="Diffuse1"||
							parName=="DiffA"||
							parName=="Diffuse_Stack_Map"||
							parName=="Base_Sky_IMG"||
							parName=="TextGUI_Sheet"||//?
							parName=="TextGUI_Sheet1"||
							parName.find("_Diff")!=string::npos){
						diffId = texVals.props[2].GetInt();
						continue;
					}
					if(parName=="Normal_Map"||
						parName=="NORM"||
						parName=="NormalMap"||
						parName=="Nm"||
						parName.find("_Norm")!=string::npos){
						normId = texVals.props[2].GetInt();
					}else if(firstId==-1){//TODO: check normal, cube, etc
						firstId = texVals.props[2].GetInt();
					}
				}
			}
		}else if(p->type==eObjectProperty&&*p->name=="Parent"){
			parId = p->GetInt();
			Log("mat: parent %d (%s)\n",parId,GetObjName(parId));
		}
	}
	if(normId)
		pMat->normalmapTex = GetTexture(normId);
	
	if(diffId){
		pMat->diffuseTex = GetTexture(diffId);
	}else
	if(parId>0){
		if(GetObjNameS(exports[parId-1].class_)=="Material"){
		PCCMaterial *parentMat=GetMaterial(exports+parId-1);
		//if(parentMat->blendMode!=eBlendOpaque)
		//	pMat->blendMode = parentMat->blendMode;
		if(parentMat->diffuseTex)
			pMat->diffuseTex = parentMat->diffuseTex;
		}
	}else if(parId<0){
		Log("GetMaterial: parent id < 0 (%d)\n",parId);
	}
	
	if(!pMat->diffuseTex){
		Log("mat mic: Diffuse not found: props %d, texParams %d\n",props.props.size(),texParamsCount);
		//
		if(texParamsCount&&!print){
			PCCProperty *p=&props.props[texParamsId];
			int curOffs=4;
			for(int k=0;k<texParamsCount;k++){
				PCCProperties texVals(this,p->data+curOffs,p->size-curOffs);
				curOffs += texVals.GetReaded();
				Log("param %d name %s, tex %d %s\n",k,GetName(texVals.props[1].GetInt()),texVals.props[2].GetInt(),GetObjName(texVals.props[2].GetInt()));
			}
		}
		//
	}
	return pMat;
}
#else
PCCMaterial *PCCFile::GetMaterial(pccExport_t *exp,bool print)
{
	if(exp->pData)
		return (PCCMaterial*)exp->pData;
	
	Log("GetMaterial: export %d name: %s, class: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), GetObjName(exp->class_), exp->dataSize,exp->dataOffset);
	
	//TODO: Expressions

	PCCProperties props(this,exp,print);
	
	//Log("cur offs %lld\n", in->tellg()-(int64_t)exp->dataOffset);
	
	PCCMaterial *pMat = new PCCMaterial((int)(exp-exports)+1);

	int diffId = 0;
	int normId = 0;
	int firstId = -1;
	int parId=0;
	{
		for(uint32_t i=0; i<props.props.size();i++){
			PCCProperty *p=&props.props[i];
			if(p->type==eByteProperty&&*p->name=="BlendMode"){
				string *bmode=p->GetName(this,8);
				Log("BlendMode: %d %s, %d %s\n",p->GetInt(),p->GetName(this)->c_str(),p->GetInt(8),bmode->c_str());
				if(*bmode=="BLEND_Masked")
					pMat->blendMode=eBlendMasked;
				else if(*bmode=="BLEND_Translucent")
					pMat->blendMode=eBlendTranslucent;
				else if(*bmode=="BLEND_Additive")
					pMat->blendMode=eBlendAdditive;
			}else if(p->type==eObjectProperty&&*p->name=="Parent"){
				parId = p->GetInt();
			}
		}
	}

	if(GetObjNameS(exp->class_)=="MaterialInstanceConstant"){
		//Log("MaterialInstanceConstant: props %d\n",props.props.size());
		int texParamsCount=0;
		for(uint32_t i=0; i<props.props.size();i++){
			PCCProperty *p=&props.props[i];
			//if(print) Log("mic props %d: %s %d %d\n",i,p->name->c_str(),p->type, p->size);
			if(p->type==eArrayProperty&&*p->name=="TextureParameterValues"){
				texParamsCount=p->GetInt();
				if(print) Log("TexParams count %d\n",texParamsCount);
				if(texParamsCount>0){
					int curOffs=4;
					for(int k=0;k<texParamsCount;k++){
						PCCProperties texVals(this,p->data+curOffs,p->size-curOffs);
						curOffs += texVals.GetReaded();
						/*
						for(uint32_t j=0;j<texVals.props.size();j++){
							PCCProperty *tp=&texVals.props[j];
							if(tp->type==eNameProperty&&*tp->name=="ParameterName"){
								Log("ParameterName: %s\n",GetName(tp->GetInt()));
							}else if(tp->type==eObjectProperty && *tp->name=="ParameterValue"){
								Log("ParameterValue: %d %s\n",tp->GetInt(),GetObjName(tp->GetInt()));
							}
						}*/
						if(print) Log("param name %s, tex %d %s\n",GetName(texVals.props[1].GetInt()),texVals.props[2].GetInt(),GetObjName(texVals.props[2].GetInt()));
						string &parName=GetNameS(texVals.props[1].GetInt());
						if(parName=="Diffuse"||
							parName=="Texture"||
							parName=="Simple Diffuse"||
							parName=="Diffuse_Map"||
							parName=="Diffuse1"||
							parName=="DiffA"||
							parName=="Diffuse_Stack_Map"||
							parName=="Base_Sky_IMG"||
							parName=="TextGUI_Sheet"||//?
							parName=="TextGUI_Sheet1"||
							parName.find("_Diff")!=string::npos){
							diffId = texVals.props[2].GetInt();
							continue;
						}
						if(parName=="Normal_Map"||
							parName=="NORM"||
							parName.find("_Norm")!=string::npos){
							normId = texVals.props[2].GetInt();
						}else if(firstId==-1)//TODO: check normal, cube, etc
							firstId = texVals.props[2].GetInt();
					}
				}
			}
		}
		if(diffId){
			pMat->diffuseTex = GetTexture(diffId);
			if(normId)
				pMat->normalmapTex = GetTexture(normId);
			if(parId>0){
				PCCMaterial *parentMat=GetMaterial(exports+parId-1);
				if(parentMat->blendMode!=eBlendOpaque)
					pMat->blendMode = parentMat->blendMode;
			}else if(parId<0){
				Log("GetMaterial: parent id < 0 (%d)\n",parId);
			}
			exp->pData = pMat;
			return pMat;
		}
		Log("GetMaterial MIC: Diffuse not found: props %d, texParams %d\n",props.props.size(),texParamsCount);
	}
	
	if(in->tellg()>=(int64_t)exp->dataOffset+exp->dataSize){
		Log("GetMaterial: no additional data\n");
		//TODO fix
		if(!pMat->diffuseTex && firstId!=-1){
			pMat->diffuseTex = GetTexture(firstId);
		}
		exp->pData = pMat;
		return pMat;
	}
	//binary

	//MaterialResource
	{
		//CompileErrors
		int count=ReadInt();
		if(count) Log("mat: CompileErrors count %d\n",count);
		for(int i=0;i<count;i++){
			char tstr[512];
			int tstrlen=ReadInt();
			in->read(tstr, -tstrlen*2);
			UTF16to8(tstr, -tstrlen);
			Log("mat: err %d len %d (%s)\n",i,tstrlen,tstr);
		}
		//TextureDependencyLength
		count=ReadInt();
		if(count) Log("TextureDependencyLength map count %d\n",count);
		for(int i=0;i<count;i++){
			int objid=ReadInt();
			int ival=ReadInt();
			Log("map %d obj %d (%s) int %d\n",i,objid,GetObjName(objid),ival);
		}
		//int maxTexDependencyLen=
		ReadInt();
		in->seekg(16,ios::cur);//GUID
		//int numUserTexCoords=
		ReadInt();
	}
	
	if(!in->good()){
		Log("GetMaterial: in stream not good (%X)\n",in->rdstate());
		delete pMat;
		return 0;
	}
	
	//expression textures
	int count=ReadInt();
	if(print) Log("expression texures count %d\n",count);
	pMat->numExpressionTextures = count;
	if(count){
		pMat->expressionTextures = new int[count];
		in->read((char*)pMat->expressionTextures,count*4);
	}
	//bool usesSceneColor, usesSceneDepth
	//bool usesDynamicParameter, usesLmUVs, usesMatVertPosOffs
	//int usingTransform
	Skip(9);
	//TextureLookupInfo
	count = ReadInt();
	Log("mat: TextureLookup count %d\n",count);
	//int DummyDroppedFallbackComponents
	if(exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset))
		Log("mat: %d left %d bytes\n",exp-exports,exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset));
	//end binary

	if(pMat->numExpressionTextures){
		int first=-1;
		for(int i=0;i<pMat->numExpressionTextures;i++){
			int objid=pMat->expressionTextures[i];
			string &texName = GetObjNameS(objid);
			if(print) Log("tex %d: %d %s\n",i,objid,texName.c_str());
			if(!pMat->diffuseTex){
				//shit!
				if(texName.find("_Diff")!=string::npos||
					texName.find("_diff")!=string::npos||
					texName.find("_DIFF")!=string::npos){
					diffId=objid;
					//Log("Diffuse tex: %d %d %s\n",i,objid,texName.c_str());
				}else if(texName.find("_Norm")!=string::npos){
					normId = objid;
				}else if(first==-1&&
					texName.find("_Cube")==string::npos&&
					texName.find("CUBEMAP")==string::npos){
					first=objid;
				}
			}
		}
		if(!diffId){
			if(first!=-1)
				diffId = first;
			else
				diffId = pMat->expressionTextures[0];
			Log("GetMaterial: Diffuse not founds. Used %d %s\n",diffId,GetObjName(diffId));
		}
	}else{
		Log("GetMaterial: no expressionTextures\n");
	}
	//Log("cur offs %lld\n", in->tellg()-(int64_t)exp->dataOffset);
	if(diffId)
		pMat->diffuseTex = GetTexture(diffId);
	if(normId)
		pMat->normalmapTex = GetTexture(normId);
	exp->pData = pMat;
	return pMat;
}
#endif

StaticMeshComponent *PCCFile::GetStaticMeshComponent(pccExport_t *exp)
{
	if(exp->pData)
		return (StaticMeshComponent*)exp->pData;
	
	//Log("GetStaticMeshComponent: export %d name: %s, data: len %d, offs %d\n",exp-exports,GetName(exp->objectName), exp->dataSize,exp->dataOffset);
	
	PCCProperties props(this,exp);

	StaticMeshComponent *smc = new StaticMeshComponent((int)(exp-exports)+1);
	smc->scale = glm::vec3(1.0f);
	int meshId=0;
	//int primId=0;
	int *matIds=0;
	for(uint32_t i=0;i<props.props.size();i++){
		PCCProperty *p=&props.props[i];
		if(p->type==eObjectProperty){
			if(*p->name=="StaticMesh"){
				meshId = p->GetInt();
			}//else if(*p->name=="ReplacementPrimitive"){
			//	primId = p->GetInt();
			//}
		}else if(p->type==eStructProperty && *p->name=="Scale3D"){
			smc->scale = p->GetVec3(8);
		}else if(p->type==eArrayProperty && *p->name=="Materials"){
			smc->numMaterials = p->GetInt();
			//Log("GetStaticMeshComponent: Materials count %d\n",smc->numMaterials);
			matIds = new int[smc->numMaterials];
			for(int j=0;j<smc->numMaterials;j++){
				matIds[j] = p->GetInt(4+j*4);
			}
		}else if(p->type==eBoolProperty && *p->name=="HiddenGame"){
			smc->hidden = p->GetBool();
		}
		/*if(*p->name!="StaticMesh"&&
			*p->name!="ReplacementPrimitive"&&
			*p->name!="Scale3D"&&
			*p->name!="HiddenGame"&&
			*p->name!="bBioIsReceivingDecals"&&
			*p->name!="LightMapEncoding"&&
			*p->name!="Materials"&&
			*p->name!="LightingChannels"&&
			*p->name!="BlockNonZeroExtent"&&
			*p->name!="BlockRigidBody"&&
			*p->name!="IrrelevantLights"&&
			*p->name!="bUseAsOccluder"&&
			*p->name!="bAcceptsStaticDecals"&&
			*p->name!="bAcceptsDynamicDecals"&&
			*p->name!="CastShadow"&&
			*p->name!="bAcceptsLights"&&
			*p->name!="bAcceptsDynamicLights"&&
			*p->name!="bUsePrecomputedShadows"&&
			*p->name!="CollideActors"&&
			*p->name!="MaxDrawDistance"&&
			*p->name!="CachedMaxDrawDistance"&&
			*p->name!="bForceDirectLightMap"&&
			*p->name!="bCastDynamicShadow"&&
			*p->name!="bSelfShadowOnly"&&
			*p->name!="RBChannel"&&
			*p->name!="bAcceptsDynamicDominantLightShadows"&&
			*p->name!="bCullModulatedShadowOnBackfaces"&&
			*p->name!="bCullModulatedShadowOnEmissive"&&
			*p->name!="OverridePhysMat"&&
			*p->name!="bAllowAmbientOcclusion"&&
			*p->name!="CanBlockCamera"&&
			*p->name!="bAllowShadowFade"&&
			*p->name!="bAcceptsFoliage"&&
			*p->name!="bAllowApproximateOcclusion"&&
			*p->name!="TranslucencySortPriority"&&
			*p->name!="LocalTranslucencySortPriority"){
			Log("smc prop %d: %s %d %d\n",i,p->name->c_str(),p->type,p->size);
		}*/
	}


	//binary part
	int numLods = ReadInt();
	if(numLods != 1){
		Log("smc: lods count %d (%d)\n",numLods,exp-exports);
	}
	int lmColTexId=0;
	int lmDirTexId=0;
	if(numLods){
		int shadowMapsCount = ReadInt();
		if(shadowMapsCount != 0){
			/*Log("smc: ShadowMap count %d (%d)\n",shadowMapsCount,exp-exports);
			for(int i=0; i<shadowMapsCount; i++){
				Log("smc: ShadowMap %d id %d\n",i,ReadInt());
			}*/
			Skip(shadowMapsCount*4);
		}
		int shadowVertexBuffersCount = ReadInt();
		if(shadowVertexBuffersCount){
			/*Log("smc: ShadowVertexBuffers count %d (%d)\n",shadowVertexBuffersCount,exp-exports);
			for(int i=0; i<shadowVertexBuffersCount; i++){
				Log("smc: ShadowVertexBuffer %d id %d\n",i,ReadInt());
			}*/
			Skip(shadowVertexBuffersCount*4);
		}
		//lightmap
		smc->lightmapType = ReadInt();
		if(smc->lightmapType<1||smc->lightmapType>4){
			Log("smc: lightmapType %d (%d)\n",smc->lightmapType,exp-exports);
			//EngineError("smc invalid lightmapType");
			smc->lightmapType=0;
		}else{
			int numGUIDs = ReadInt();
			//Log("smc: GUIDs count %d (%d)\n",numGUIDs,exp-exports);
			Skip(numGUIDs*16);//GUIDs
			if(smc->lightmapType==1){
				//Log("smc: lm1D (%d)\n",exp-exports);
				int owner=ReadInt();//this id
				if(owner!=exp-exports+1){
					Log("smc %d: lm owner invalid %d\n",exp-exports,owner);
				}
				//bulk data (dirs)
				//{
					int flags;
					int elemsCount;
					int bsize;
					int boffs;
					in->read((char*)&flags,4);
					in->read((char*)&elemsCount,4);
					in->read((char*)&bsize,4);
					in->read((char*)&boffs,4);
					if(flags)
						Log("bulk1 data: flags %d, count %d,size %d, offs %d\n",flags,elemsCount,bsize,boffs);
					if(flags&0x20)
						Log("bulk1 data unused flag\n");
					else{
						if(in->tellg()!=boffs){
							Log("bulk1 offs invalid %d %d\n",boffs,in->tellg());
							EngineError("BulkData error");
						}
						//Skip(bsize);
						smc->lm1DVertsNum = elemsCount;
						smc->lm1DVerts = new char[bsize];
						in->read(smc->lm1DVerts, bsize);
					}
				//}
				Skip(12*3);//scale vectors vec3 x 3

				//bulk data colors
				in->read((char*)&flags,4);
				in->read((char*)&elemsCount,4);
				in->read((char*)&bsize,4);
				in->read((char*)&boffs,4);
				if(flags)
					Log("bulk2 data: flags %d, count %d,size %d, offs %d\n",flags,elemsCount,bsize,boffs);
				if(flags&0x20)
					Log("bulk2 data unused flag\n");
				else{
					if(in->tellg()!=boffs){
						Log("bulk2 offs invalid %d %d\n",boffs,in->tellg());
						EngineError("BulkData error");
					}
					Skip(bsize);
				}
			}else if(smc->lightmapType == 2){
				lmColTexId=ReadInt();
				glm::vec3 lmColVec(0);
				in->read((char*)&lmColVec,12);
				//Log("smc: %d lightmap col tex %d(%s) vec (%f %f %f)\n",exp-exports,lmColTex,GetObjName(lmColTex),lmColVec.x,lmColVec.y,lmColVec.z);
				lmDirTexId=ReadInt();
				glm::vec3 lmDirVec(0);
				in->read((char*)&lmDirVec,12);
				//Log("smc: %d lightmap dir tex %d (%s) vec (%f %f %f)\n",exp-exports,lmDirTex,GetObjName(lmDirTex),lmDirVec.x,lmDirVec.y,lmDirVec.z);
				//third lm (null)
				int lmSimpleTex=ReadInt();
				if(lmSimpleTex)
					Log("smc: %d lightmap simple tex %d (%s)\n",exp-exports,lmSimpleTex,GetObjName(lmSimpleTex));
				Skip(12);//vec3
				in->read((char*)&smc->lmScaleAndBias,16);
				//Log("smc: %d lm scale %f %f bias %f %f\n",exp-exports,lmScaleAndBias.x,lmScaleAndBias.y,lmScaleAndBias.z,lmScaleAndBias.w);
			}/*else if(smc->lightmapType==3){
				Log("smc: lightmapType %d (%d)\n",smc->lightmapType,exp-exports);
				Skip(8);//0 0
				lmColTexId=ReadInt();
				Log("smc: LightmapVector: Color %d(%s)\n",lmColTexId,GetObjName(lmColTexId));
				//int count = ReadInt();
				//Log("smc: count %d\n",count);
				//Skip(count);
				//int unk = ReadInt();
				//Log("smc: unk %d\n",unk);
			}*/else if(smc->lightmapType==4){
				Log("smc: lightmapType %d, GUIDs count %d (%d)\n",smc->lightmapType,numGUIDs,exp-exports);
				lmColTexId=ReadInt();
				in->read((char*)&smc->lmColScale,16);
				in->read((char*)&smc->lmColBias,16);
				lmDirTexId=ReadInt();
				//Log("smc: LightmapVector: Color %d(%s), Dir %d(%s)\n",lmColTexId,GetObjName(lmColTexId),lmDirTexId,GetObjName(lmDirTexId));
				in->read((char*)&smc->lmDirScale,16);
				in->read((char*)&smc->lmDirBias,16);
				Skip(4+8*4);//third tex, scale, uv scale, bias
				in->read((char*)&smc->lmScaleAndBias,16);
			}
		}
		bool vertexColorData=false;
		in->read((char*)&vertexColorData,1);
		if(vertexColorData){
			Log("smc %d: vertexColorData\n",exp-exports);
		}
	}
	if(exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset))
		Log("smc: %d left %d bytes\n",exp-exports,exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset));
	//end binary

	if(lmColTexId){
		smc->lmColTex=GetTexture(lmColTexId);
	}
	if(lmDirTexId){
		smc->lmDirTex=GetTexture(lmDirTexId);
	}

	if(meshId>0&&!smc->hidden&&
	   GetObjNameS(meshId)!="Volumetric_Sphere01"&&
	   GetObjNameS(meshId)!="Volumetric_Plane01"&&
	   GetObjNameS(meshId)!="SphereMesh")//HACK
	{
		smc->pMesh = GetStaticMesh(exports+meshId-1);
		if(smc->lightmapType==1&&smc->lm1DVertsNum!=smc->pMesh->LODs[0].numVerts)
			Log("smc %d: 1D lm verts count %d (mesh %d)\n",exp-exports,smc->lm1DVertsNum,smc->pMesh->LODs[0].numVerts);
		if(!smc->numMaterials){
			smc->pMesh->LoadMaterials(this);
		}else{
			if(smc->pMesh->LODs[0].sectionCount<smc->numMaterials){
				int realMatCount=0;
				for(int i=0;i<smc->numMaterials;i++)
					if(matIds[i]) realMatCount++;
				//if(smc->pMesh->LODs[0].sectionCount!=realMatCount)
				//	Log("GetSMC: export %d, mats count != mesh mats count (%d, %d)\n",exp-exports,smc->numMaterials,smc->pMesh->LODs[0].sectionCount);
			}
			smc->pMaterials = new PCCMaterial*[smc->pMesh->LODs[0].sectionCount];
			for(int i=0; i<smc->pMesh->LODs[0].sectionCount; i++){
				if(i>=smc->numMaterials){
					smc->pMaterials[i] = 0;
					continue;
				}
				int mat = matIds[i];
				if(mat>0){
					smc->pMaterials[i]=GetMaterial(exports+mat-1);
				}else{
					//Log("GetStaticMeshComponent: export %d, mat %d is %d %s\n",exp-exports,i,mat,GetObjName(mat));
					smc->pMaterials[i] = 0;
				}
			}
			smc->pMesh->LoadMaterials(this,smc->pMaterials);
			for(int i=0; i<smc->pMesh->LODs[0].sectionCount; i++){
				if(i>=smc->numMaterials){
					smc->pMaterials[i] = smc->pMesh->materials[i];
				}else{
					int mat = matIds[i];
					if(mat>0){
						//smc->pMaterials[i]=GetMaterial(exports+mat-1);
					}else{
						//if(smc->pMesh->materials[i]==0)
						//	Log("GetSMC: export %d, mat %d is %d %s\n",exp-exports,i,mat,GetObjName(mat));
						smc->pMaterials[i] = smc->pMesh->materials[i];
					}
				}
			}
		}
	}else{
		//Log("GetStaticMeshComponent: mesh %d %s, primitive %d %s\n",meshId,GetObjName(meshId),primId,GetObjName(primId));
	}
	delete[] matIds;
	exp->pData = smc;
	return smc;
}

std::string format(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	std::vector<char> v(1024);
	while (true)
	{
		va_list args2;
		va_copy(args2, args);
		int res = vsnprintf(v.data(), v.size(), fmt, args2);
		if ((res >= 0) && (res < static_cast<int>(v.size())))
		{
			va_end(args);
			va_end(args2);
			return std::string(v.data());
		}
		size_t size;
		if (res < 0)
			size = v.size() * 2;
		else
			size = static_cast<size_t>(res) + 1;
		v.clear();
		v.resize(size);
		va_end(args2);
	}
}

bool PCCFile::PrintProperties(pccExport_t *exp)
{
	if(exp<exports||exp>exports+header.exportCount){
		Log("PrintProperties: export %P out of bounds\n",exp);
		return false;
	}
	
	if(!in->good()){
		Log("PrintProperties: in stream not good (%X)\n",in->rdstate());
		return false;
	}
	
	in->seekg(exp->dataOffset);
	
	if(!in->good()){
		Log("PrintProperties: after seekg in stream not good (%X)\n",in->rdstate());
		return false;
	}

	bool ret=true;
	
	if(exp->objectFlags&0x0200000000000000){
		Log("exp has stack (flags %llX)\n",exp->objectFlags);
		in->seekg(30,ios::cur);
	}else{
		in->seekg(4,ios::cur);//unk
		int test = 0;
		in->read((char*)(&test),4);
		//Log("test %d\n",test);
		if(test>0&&test<header.nameCount){
			in->read((char*)(&test),4);
			if(test==0)
				in->seekg(-8,ios::cur);
			else
				in->seekg(-4,ios::cur);
		}
	}
	int i=0;
	for(i=0; i<32; i++)
	{
		int namei = 0;
		in->read((char*)(&namei),4);
		in->seekg(4,ios::cur);
		if(namei<0||namei>header.nameCount){
			Log("PrintProperties: name idx out of bounds %d\n",namei);
			ret=false;
			break;
		}
		string &propName = GetNameS(namei);
		if(propName=="None"){
			//Log("None prop\n");
			break;
		}
		int typei = 0;
		in->read((char*)(&typei),4);
		in->seekg(4,ios::cur);
		int size = 0;
		in->read((char*)(&size),4);
		in->seekg(4,ios::cur);//unk
		//24 byte readed
		string &type = GetNameS(typei);
		string value="(?)";
		if(type == "ArrayProperty"){
			int count=0;
			in->read((char*)(&count),4);
			//Log("Count %d\n",count);

			value = format("(Count %d, item len %d)",count,((size-4)/count));
			in->seekg(size-4,ios::cur);
			/*for(int j=0;j<count;j++){
				int val=0;
				in->read((char*)(&val),4);
				Log("val %d: %d\n",j,val);
			}*/
		}else if(type == "IntProperty"){
			int val=0;
			in->read((char*)(&val),4);
			
			//Log("Value: %d\n",val);
			value = format("(%d)",val);
		}else if(type == "FloatProperty"){
			float fval=0;
			in->read((char*)(&fval),4);

			//Log("Value: %f\n",fval);
			value = format("(%f)",fval);
		}
		else if(type == "StructProperty"){
			int snamei = 0;
			in->read((char*)(&snamei),4);
			in->seekg(4,ios::cur);
			//Log("Struct name %s\n",GetName(snamei));
			value = format("(Struct name %s)",GetName(snamei));

			//TODO: Read struct
			in->seekg(size,ios::cur);
		}
		else if(type == "ByteProperty"){
			int snamei = 0;
			in->read((char*)(&snamei),4);
			in->seekg(4,ios::cur);
			//LOG("Byte name %s\n",GetName(snamei));

			int val = 0;
			in->read((char*)&val,4);
			//TODO: size
			in->seekg(size-4,ios::cur);
			value = format("(%s, %s)",GetName(snamei),GetName(val));
		}
		else if(type == "BoolProperty"){
			bool val=0;
			in->read((char*)&val,1);
			//LOG("Bool val %d\n",val);
			value = format("(%s)",val?"true":"false");
		}
		else if(type=="NameProperty"){
			int val = 0;
			in->read((char*)(&val),4);
			in->seekg(4,ios::cur);
			//Log("Value: %s\n",GetName(val));
			value = format("(%s)",GetName(val));
		}
		else if(type=="ObjectProperty"){
			int val = 0;
			in->read((char*)(&val),4);
			//Log("Value: %d %s\n", val, GetObjName(val));
			value = format("(id %d name %s class %s)",val,GetObjName(val),GetClassName(val));
		}
		else{
			Log("Unknown type property (%s)\n",type.c_str());
			ret=false;
		}
		Log("prop %d: name %s, type %s, size %d, val %s\n",i,propName.c_str(),type.c_str(),size,value.c_str());
	}
	Log("Readed %d properties, %lld bytes left\n",i,exp->dataSize-(in->tellg()-(int64_t)exp->dataOffset));

	return ret;
}

int PCCFile::ReadInt(){
	int val=0;
	in->read((char*)&val,4);
	return val;
}

void PCCFile::Skip(int l){
	in->seekg(l,ios::cur);
}

std::string &PCCFile::GetNameS(int i)
{
	return nameTable[i];
}

std::string *PCCFile::GetNameSP(int i)
{
	return nameTable+i;
}

const char *PCCFile::GetName(int i)
{
	if(i<0||i>=header.nameCount)
		return 0;
	return nameTable[i].c_str();
}

std::string &PCCFile::GetObjNameS(int i)
{
	if(i>0){
		return nameTable[exports[i-1].objectName];
	}else if(i<0){
		return nameTable[imports[-i-1].importName];
	}
	Log("Error: PCCObject::GetObjNameS(0)\n");
	static std::string emptyString;
	return emptyString;
}

std::string *PCCFile::GetObjNameSP(int i)
{
	if(i>0){
		return nameTable+exports[i-1].objectName;
	}else if(i<0){
		return nameTable+imports[-i-1].importName;
	}
	Log("Error: PCCObject::GetObjNameSP(0)\n");
	return 0;
}

const char *PCCFile::GetObjName(int i)
{
	if(i>0&&i<header.exportCount){
		return nameTable[exports[i-1].objectName].c_str();
	}else if(i<0&&i>=-header.importCount){
		return nameTable[imports[-i-1].importName].c_str();
		//return nameTable[imports[-i].importName].c_str();
	}
	//Log("Error: PCCObject::GetObjName(0)\n");
	return 0;
}

const char *PCCFile::GetClassName(int i)
{
	if(i>0){
		return GetObjName(exports[i-1].class_);
	}else if(i<0){
		return GetName(imports[-i-1].className);
	}
	Log("Error: PCCObject::GetClassName(0)\n");
	return 0;
}

int LoadCacheTex(const char *arc,int offs,int cprSize,char *data,int uncSize)
{
	string filePath = string(arc)+".tfc";
	char path[256];
	g_fs.GetFilePath(filePath.c_str(), path);
	ifstream input(path, ios::binary);

	if(!input)
		return 0;
	
	//char *cprData = new char[cprSize];
	//input.seekg(offs);
	//input.read(cprData,cprSize);
	
	if(cprSize!=uncSize){//compressed
		if(ReadChunk(&input,offs,data)){
			Log("Error: chunk decompression failed\n");
		}
	}else{
		input.seekg(offs);
		input.read(data,uncSize);
	}
	
	input.close();
	//delete[] cprData;
	return 1;
}

bool ReadChunk(istream *pIn,int offs,char *dst)
{
	pccChunk_t chunk;
	pIn->seekg(offs);
	pIn->read((char *)&chunk,sizeof(pccChunk_t));
	if(chunk.magic!=PCC_CHUNK_MAGIC){
		Log("Error: chunk %X invalid magic %X\n",offs,chunk.magic);
		return 1;
	}
	int numBlocks = (int)ceil(chunk.uncompressedSize/(float)chunk.blockSize);
	//Log("chunk %X: magic %X, block size %d, compressed size %d, uncompressed size %d, num blocks %d\n",offs,chunk.magic,chunk.blockSize,chunk.compressedSize,chunk.uncompressedSize,numBlocks);

	pccBlock_t *blocks = new pccBlock_t[numBlocks];
	pIn->read((char *)blocks,sizeof(pccBlock_t)*numBlocks);
	int cprBlockSize = chunk.blockSize;
	char *blockData = new char[cprBlockSize];

	char *cdst = dst;
	for(int i = 0; i<numBlocks; i++)
	{
		//Log("block %d: compressed size %d, uncompressed size %d\n",i,blocks[i].compressedSize,blocks[i].uncompressedSize);
		if(blocks[i].compressedSize>cprBlockSize){
			Log("chunk 0x%X block %d: compressed size(%d) > uncompressed(%d)\n",offs,i,blocks[i].compressedSize,blocks[i].uncompressedSize);
			delete[] blockData;
			cprBlockSize = blocks[i].compressedSize;
			blockData = new char[cprBlockSize];
		}
		pIn->read(blockData,blocks[i].compressedSize);
		DecompressBlock(blockData,blocks[i].compressedSize,cdst,blocks[i].uncompressedSize);
		cdst += blocks[i].uncompressedSize;
	}

	delete[] blockData;
	delete[] blocks;
	
	return 0;
}

int DecompressBlock(char *src, int srcSize, char *dst, int dstSize)
{
	int ret;
	z_stream strm;

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	//decompress
	strm.avail_in = srcSize;
	strm.next_in = (uint8_t*)src;
	strm.avail_out = dstSize;
	strm.next_out = (uint8_t*)dst;


	ret = inflate(&strm, Z_SYNC_FLUSH);//Z_NO_FLUSH);
	if(ret!=1) LOG("inflate ret %d\n", ret);

	(void)inflateEnd(&strm);

	return ret;
}

