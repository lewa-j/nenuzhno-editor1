
#include <fstream>
using namespace std;

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "system/FileSystem.h"
#include "system/Config.h"
#include "graphics/platform_gl.h"
#include "renderer/mesh.h"
#include "sound/sound.h"
#include "log.h"

#include "SoundEditor.h"

Editor *GetSoundEditor(){return new SoundEditor();}

typedef unsigned short ushort;

Mesh CreateWave(AudioClip *ac,float zoom)
{
	int nv = ac->length/ac->channels/2;//16bit
	float *v = new float[nv*2];
	for(int i=0; i<nv; i++)
	{
		v[i*2] = (float)i/nv*zoom;
		v[i*2+1] = 0.5f+((short*)ac->buffer)[i*ac->channels]/65535.0f;//(32767.0f);
	}
	Mesh m(v, nv, GL_LINE_STRIP);
	return m;
}

void CreateClip(AudioClip *ac)
{
	ac->freq = 44100;
	ac->channels = 1;
	ac->length = ac->freq*2;//44100hz 16bit 1s
	ac->buffer = new char[ac->length];
}

void GenerateClip(AudioClip *ac, float f, float a,float f2, float a2)
{
	for(int i=0; i<ac->freq; i++)
	{
		//sin
		((short*)ac->buffer)[i]=glm::sin(i*glm::pi<float>()*f)*a*32767.0f;
		((short*)ac->buffer)[i]+=glm::sin(i*glm::pi<float>()*f2)*a2;//*32767.0f;
		
		//((short*)ac->buffer)[i]*=(((int)(i*f))%((int)f2)/f2);
		//saw
		//((short*)ac->buffer)[i]=((((int)(i*f))%((int)f2)/f2)-0.5f)*a*32767.0f;
		
		//float m = pow(glm::sin(i*glm::pi<float>()*f2)*a2,2.0f);
		//float m = (glm::pow(((int)i)%((int)f2)/f2,2.0f)*2.0-1.0f)*a2;
		
		//((short*)ac->buffer)[i]=glm::sin(i*glm::pi<float>()*f+m)*a*32767.0f;
		//((short*)ac->buffer)[i] = m*a*32767.0f;
	}
}

void SoundEditor::ReadConfig()
{
	ConfigFile config;
	config.Load("sound_editor.txt");
	mode = atoi(config.values["mode"].c_str());
	genF = atof(config.values["genF"].c_str());
	genA = atof(config.values["genA"].c_str());
	genF2 = atof(config.values["genF2"].c_str());
	genA2 = atof(config.values["genA2"].c_str());
	clipName = config.values["clipName"];
	is3D = config.values["is3D"] != "0";
	loop = config.values["loop"] != "0";
	zoom = atof(config.values["zoom"].c_str());
	oscSamples = atoi(config.values["oscSamples"].c_str());
}

SoundEditor::SoundEditor()
{
	bClose = Button(0,0.1,0.4,0.1, "Close");
	bPlay = Button(0.1,0.9,0.4,0.1, "Play");
	bStop = Button(0.5,0.9,0.4,0.1, "Stop");
	bSlider = Button(0.1,0.8,0.8,0.1);
	bSlider.active=1;
	sliderPos=0;
	isPlaying = false;
	sound = CreateSoundSystem();
	sound->Init();
	ReadConfig();
	if(mode==0){
		CreateClip(&testClip);
		GenerateClip(&testClip,genF,genA,genF2,genA2);
	}
	else
	{
		testClip.LoadFromFile(clipName.c_str());
	}
	
	wave = CreateWave(&testClip,zoom);
	
	asrc = sound->CreateSource(&testClip);
}

SoundEditor::~SoundEditor()
{
	sound->Destroy();
	delete sound;
	delete[] ((float *)wave.verts);
}

void SoundEditor::Resize(int w, int h)
{
	
}

void SoundEditor::Draw()
{
	if(bClose.pressed)
	{
		bClose.pressed=0;
		CloseEditor();
		return;
	}
	if(bPlay.pressed)
	{
		bPlay.pressed = 0;
		isPlaying = 1;
		asrc->Play(true);
	}
	if(bStop.pressed)
	{
		bStop.pressed = 0;
		isPlaying = 0;
		asrc->Stop();
	}
	
	DrawButton(&bClose);
	DrawButton(&bPlay);
	DrawButton(&bStop);
	
	char str[32];
	snprintf(str,20,"pos: %.3f",sliderPos);
	DrawText(str,0,0.8,0.4);
	
	glm::mat4 mtx(1.0);
	mtx = glm::translate(mtx,glm::vec3(-sliderPos*(zoom-1),0,0));
	SetModelMtx(mtx);
	whiteTex.Bind();
	glDisableVertexAttribArray(2);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, wave.verts);
	glDrawArrays(wave.mode, 0, wave.numVerts);
	glEnableVertexAttribArray(2);
}

void SoundEditor::OnTouch(float x, float y, int a)
{
	//Log("SoundEditor::OnTouch %f %f %d\n", x, y, a);
	float tx = x/scrWidth;
	float ty = y/scrHeight;
	if(a==0)
	{
		bClose.Hit(tx,ty);
		bPlay.Hit(tx,ty);
		bStop.Hit(tx,ty);
		bSlider.Hit(tx,ty);
	}
	if(a==1)
	{
		if(bSlider.pressed)
		{
			bSlider.pressed=0;
			sliderPos = glm::clamp((tx-bSlider.x)/bSlider.w,0.0f,1.0f);
		}
	}
	if(a==2)
	{
		if(bSlider.pressed)
		{
			sliderPos = glm::clamp((tx-bSlider.x)/bSlider.w,0.0f,1.0f);
		}
	}
}
