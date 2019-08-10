
#include "editor.h"
#include "renderer/mesh.h"
#include "graphics/glsl_prog.h"

class GraphUtil: public Editor{
public:
	GraphUtil();
	~GraphUtil();
	void Resize(int w, int h){}
	void Draw();
	void OnTouch(float x,float y,int a);
private:
	Button bClose;
	Mesh graph;
	glslProg graphProg;
};

Editor *GetGraphUtil(){return new GraphUtil();}

GraphUtil::GraphUtil()
{
	bClose = Button(0.02,0.02,0.24,0.07, "Close");

	//mesh
	int nv = 800;
	float *v = new float[nv*2];
	float zoom = 1.5f;
	for(int i=0; i<nv; i++)
	{
		v[i*2] = (float)i/nv*zoom;
		v[i*2+1] = 0.0f;
		//pow(glm::sin(i*glm::pi<float>()*f2)*a2,2.0f)
	}
	graph = Mesh(v, nv, GL_LINE_STRIP);

	//shader
	graphProg.CreateFromFile("graph", "col_tex");
	graphProg.u_mvpMtx = graphProg.GetUniformLoc("u_mvpMtx");
	graphProg.u_color = graphProg.GetUniformLoc("u_color");
	graphProg.Use();
	glUniform4f(graphProg.u_color,1,1,1,1);
}

GraphUtil::~GraphUtil()
{
	delete[] ((float *)graph.verts);
}

float gridVerts[]={
	0  ,0.5, 1.5,0.5,
	0.5,0  , 0.5,1
};

void GraphUtil::Draw()
{
	if(bClose.pressed)
	{
		bClose.pressed=0;
		CloseEditor();
		return;
	}

	//DrawText("Work in progress...",0.05,0.2,0.8);

	whiteTex.Bind();
	SetColor(0.4,0.4,0.4,1.0);
	glDisableVertexAttribArray(2);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, gridVerts);
	glDrawArrays(GL_LINES, 0, 4);

	SetColor(1.0,1.0,1.0,1.0);
	glm::mat4 mtx(1.0);
	//mtx = glm::translate(mtx,glm::vec3(-sliderPos*(zoom-1),0,0));
	mtx = glm::translate(mtx,glm::vec3(0,0.5f,0));
	SetProg(&graphProg);
	SetModelMtx(mtx);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, graph.verts);
	glDrawArrays(graph.mode, 0, graph.numVerts);
	glEnableVertexAttribArray(2);

	SetProg(0);
	SetModelMtx(glm::mat4(1.0));

	DrawButton(&bClose);
}

void GraphUtil::OnTouch(float x, float y, int a)
{
	float tx = x/scrWidth;
	float ty = y/scrHeight;
	if(a==0){
		bClose.Hit(tx,ty);
	}
}
