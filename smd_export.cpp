
#include <fstream>
#include "system/FileSystem.h"
#include "log.h"
#include "renderer/Model.h"

using std::string;
using std::ofstream;
using glm::vec3;
using glm::vec2;

bool ExportSMD(const char *name, const char *mdlPath, float scale,Model *model)
{
	Log("ExportSMD(%s, %s)\n",name, mdlPath);

	string folder = string("export/models/")+name;
	char path[1024];

	//QC
	g_fs.GetFilePath((folder+".qc").c_str(),path,true);
	ofstream qcFile(path);
	if(!qcFile){
		Log( "Can't open output file %s\n", path);
		return false;
	}
	qcFile << "//Exported by nenuzhno-editor/PCCViewer\n";
	qcFile << "$modelname \""<<mdlPath<<name<<".mdl\"\n";
	qcFile << "$scale "<<scale<<"\n";
	qcFile << "$staticprop\n";
	qcFile << "$body studio "<<name<<"_ref\n";
	qcFile << "$sequence idle "<<name<<"_ref\n";
	qcFile << "$cdmaterials models/"<<mdlPath<<"\n";
	//$surfaceprop
	//$collisionmodel
	//$keyvalues
	qcFile.close();

	//SMD
	g_fs.GetFilePath((folder+"_ref.smd").c_str(),path,true);
	ofstream smdFile(path);
	if(!smdFile){
		Log( "Can't open output file %s\n", path);
		return false;
	}
	//header
	smdFile << "version 1\n";
	//bones
	smdFile << "nodes\n";
	smdFile << "  0 \"root\"  -1\n";
	smdFile << "end\n";
	//anims
	smdFile << "skeleton\n";
	smdFile << "time 0\n";
	smdFile << "  0 0 0 0 0 0 0\n";
	smdFile << "end\n";
	//mesh

	Log("submeshes %d\n",model->submeshes.size);
	for(int i=0; i<model->submeshes.size;i++){
		Log("sm %d: %d %d %s\n",i,model->submeshes[i].offs,model->submeshes[i].count,model->materials[model->submeshes[i].mat].name.c_str());
	}

	smdFile << "triangles\n";
	if(model->indexCount){//indexed mesh
		for(int m=0;m<model->submeshes.size;m++){
			const char *materialName = model->materials[model->submeshes[m].mat].name.c_str();
			for(uint32_t i=model->submeshes[m].offs; i<model->submeshes[m].offs+model->submeshes[m].count;i+=3){
				smdFile << materialName <<"\n";
				//for(int v=0; v<3;v++)
				for(int v=2; v>=0;v--)//reverse
				{
					vec3 pos = model->GetPos(model->GetIndex(i+v));
					vec3 nrm = model->GetNorm(model->GetIndex(i+v));
					vec2 uv = model->GetUV(model->GetIndex(i+v));
					// bone pos norm uv [weights]
					smdFile << "0 "<<pos.x<<" "<<pos.y<<" "<<pos.z<<" "<<nrm.x<<" "<<nrm.y<<" "<<nrm.z<<" "<<uv.x<<" "<<uv.y<<"\n";
				}
			}
		}
	}else{//not indexed mesh
		//Log("ExportSMD: not indexed mesh\n");
		for(int i=0; i<model->vertexCount;i+=3){
			smdFile << model->materials[0].name.c_str()<<"\n";
			for(int v=0; v<3;v++){
				char *vertBase = model->verts+(i+v)*model->vertexFormat[0].stride;
				vec3 pos = *(vec3*)(vertBase+model->vertexFormat[0].offset);
				vec3 nrm = *(vec3*)(vertBase+model->vertexFormat[1].offset);
				vec2 uv = *(vec2*)(vertBase+model->vertexFormat[2].offset);
				// bone pos norm uv [weights]
				smdFile << "0 "<<pos.x<<" "<<pos.y<<" "<<pos.z<<" "<<nrm.x<<" "<<nrm.y<<" "<<nrm.z<<" "<<uv.x<<" "<<uv.y<<"\n";
			}
		}
	}
	smdFile << "end\n";

	smdFile.close();

	return true;
}
