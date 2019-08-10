
#pragma once

#include "editor.h"
#include "button.h"
#include "renderer/mesh.h"
#include "renderer/Model.h"

class ModelViewer: public Editor
{
public:
	ModelViewer();
	~ModelViewer();
	void Resize(int w, int h);
	void Draw();
	void OnTouch(float x,float y,int a, int tf);
private:
	void ReadConfig();
	Button bClose;
	Button bImportObj;
	Button bImportMsh;
	Button bExportMsh;
	Button bExportNMF;
	Button bExportSMD;
	std::string inName;
	std::string outName;
	float size;
	Mesh *mesh;
	Model *mdl;
	std::string meshMat;
	glm::mat4 mtx;
	Camera meshCamera;
	Button bMove;
	Scroll bScroll;
	glm::vec3 velocity;
	glm::vec2 meshRot;
};

