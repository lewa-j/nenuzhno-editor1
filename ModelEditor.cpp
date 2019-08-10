
#include <stdlib.h>
#include <fstream>
#include <string>
#include <vector>
using namespace std;
#include <gtc/matrix_transform.hpp>

#include "log.h"
#include "system/FileSystem.h"
#include "system/Config.h"
#include "ModelEditor.h"
#include "resource/nmf.h"

using glm::vec3;

class DynamicModel :public Model{
public:
	DynamicModel():nVerts(){
		vbo.Create();
		ibo.Create();
		verts = 0;
	}
	void AddVert(vec3 p){
		nVerts.push_back(p);
	}
	void Bake(){
		int oldVertCount = vertexCount;
		vertexCount = nVerts.size();
		int vertexStride = 8*4;
		
		vertexFormat.Resize(3);
		vertexFormat[0]={0,3,GL_FLOAT,false,vertexStride,0};//pos
		vertexFormat[1]={1,3,GL_FLOAT,false,vertexStride,12};//norm
		vertexFormat[2]={2,2,GL_FLOAT,false,vertexStride,24};//uv

		if(vertexCount!=oldVertCount){
			if(verts)
				delete[] verts;
			verts = new char[vertexCount*vertexStride];
		}
		indexCount = 0;
		indexType = GL_UNSIGNED_SHORT;
		indexSize = 2;
		inds = 0;
		
		for(int i=0;i<vertexCount;i++){
			vec3 *v = (vec3*)(verts+vertexStride*i);
			*v = nVerts[i];
		}

		submeshes.Resize(1);
		submeshes[0] = {0,indexCount ? indexCount : vertexCount,0};
		materials.Resize(1);
		materials[0].name = "mat1";
		
		vbo.Upload(vertexCount*vertexStride,verts);
		if(indexCount){
			ibo.Upload(indexCount*indexSize,inds);
		}
	}
	
	std::vector<vec3> nVerts;
};

Editor *GetModelEditor(){return new ModelEditor();}

void ModelEditor::ReadConfig(){
	ConfigFile config;
	config.Load("model_editor.txt");
	outName = config.values["out"];
	size = atof(config.values["size"].c_str());
	meshMat = config.values["meshMat"];
	
	if(size==0)
		size=1;
}

ModelEditor::ModelEditor(){
	mdl = 0;
	bClose = Button(0.02,0.02,0.5,0.07, "Close");
	bNew = Button(0.02,0.1,0.5,0.07, "New Model");
	bExportNMF = Button(0.02,0.2,0.5,0.07, "Export NMF");
	size = 1;
	ReadConfig();
	mtx = glm::mat4(1);
	//mtx = glm::translate(mtx,glm::vec3(0,0,-5));
	mtx = glm::scale(mtx,glm::vec3(size));
	bMove = Button(0.0,0.5,0.5,0.5);
	velocity = glm::vec3(0.0f);
	meshCamera.pos = glm::vec3(0,0,5);
	meshRot = glm::vec2(0);
}

ModelEditor::~ModelEditor(){
	if(mdl){
		delete mdl;
	}
}

void ModelEditor::Resize(int w, int h){
	meshCamera.UpdateProj(90.0f, aspect, 0.01f, 50.0f);
}

DynamicModel *CreateModel(){
	DynamicModel *m = new DynamicModel();
	
	m->AddVert(vec3(0,0,0));
	m->AddVert(vec3(1,0,0));
	m->AddVert(vec3(1,1,0));
	m->Bake();
	
	return m;
}

void ModelEditor::Draw(){
	if(bClose.pressed){
		bClose.pressed = 0;
		CloseEditor();
		return;
	}
	if(bNew.pressed){
		bNew.pressed = 0;
		mdl = CreateModel();
	}
	if(bExportNMF.pressed){
		bExportNMF.pressed = 0;
		ExportNMF(outName.c_str(), mdl);
	}

	if(mdl){
		meshCamera.UpdateView();
		SetCamera(&meshCamera);

		meshRot += glm::vec2(velocity.x,-velocity.z)*(float)deltaTime;
		
		mtx = glm::mat4(1.0);
		mtx = glm::scale(mtx,glm::vec3(size));
		mtx = glm::rotate(mtx,glm::radians(meshRot.y*90.0f),glm::vec3(1,0,0));
		mtx = glm::rotate(mtx,glm::radians(meshRot.x*90.0f),glm::vec3(0,1,0));
		//mtx = glm::scale(mtx,glm::vec3(glm::clamp(r+0.8f,0.1f,10.0f)));
		//mtx = glm::rotate(mtx,glm::radians(-90.0f),glm::vec3(1,0,0));
		//mtx = glm::translate(mtx,-pMesh->bounds.pos);
		
		SetModelMtx(mtx);
	//	SetLightingEnabled(true);
		whiteTex.Bind();
		glEnableVertexAttribArray(1);
		//glDisableVertexAttribArray(2)
		
		DrawModel(mdl);
		
		glDisableVertexAttribArray(1);
		//glEnableVertexAttribArray(2);
		SetLightingEnabled(false);
		SetCamera(0);
	}

	DrawButton(&bClose);
	DrawButton(&bNew);
	DrawButton(&bExportNMF);
}

static float starttx = 0;
static float startty = 0;
void ModelEditor::OnTouch(float x, float y, int a){
	float tx = x/scrWidth;
	float ty = y/scrHeight;
	if(a==0){
		starttx = tx;
		startty = ty;
		
		bClose.Hit(tx,ty);
		bNew.Hit(tx,ty);
		bExportNMF.Hit(tx,ty);
		bMove.Hit(tx,ty);
	}else if(a==1){
		velocity = glm::vec3(0);
		bMove.pressed = false;
	}else if(a==2){
		if(bMove.pressed){
			velocity.x = -(starttx-tx);
			velocity.z = -(startty-ty);
		}
	}
}

