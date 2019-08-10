
#include <stdlib.h>
#include <fstream>
using namespace std;

#include "engine.h"
#include "BinkPlayer.h"
#include "system/FileSystem.h"
#include "system/Config.h"
#include "log.h"

Editor *GetBinkPlayer(){return new BinkPlayer();}

void BinkPlayer::ReadConfig()
{
	ConfigFile config;
	config.Load("bink.txt");
	fileName = config.values["file"];
	upload = config.values["upload"]!="0";
	decode = config.values["decode"]!="0";
	skipping = config.values["skipping"]!="0";
	scaling = atof(config.values["scaling"].c_str());
	
	scaling = min(scaling,1.0f);
}

BinkPlayer::BinkPlayer()
{
	bClose = Button(0.02,0.02,0.4,0.08, "Close");
	bNext = Button(0.5,0.02,0.4,0.08, "Next");
	bPlayPause = Button(0.6,0.12,0.4,0.08,"Play/Pause");
	bink = 0;
	buff = 0;
	zoom = 1.0f;
	playing=0;
	lastFrame=0;
	
	ReadConfig();
}

BinkPlayer::~BinkPlayer()
{
	if(bink)
		delete bink;
	if(buff)
		delete buff;
}

void BinkPlayer::Resize(int w, int h)
{
	if(!bink)
		Open();
}

void BinkPlayer::Draw()
{
	if(bClose.pressed){
		bClose.pressed = 0;
		CloseEditor();
		return;
	}
	if(bNext.pressed){
		bNext.pressed=0;
		UpdateTex();
		lastFrame=bink->curFrame/bink->header.fps;
	}
	if(bPlayPause.pressed){
		bPlayPause.pressed=0;
		playing=!playing;
	}
	if(playing){
		lastFrame+=deltaTime;
		int frameId = lastFrame*bink->header.fps;
		//if(lastFrame>=1.0f/bink->header.fps)
		if(bink->curFrame<frameId)
		{
			if(skipping){
			if(frameId-bink->curFrame>1){
				if(!bink->SkipFrames(frameId-1))
					lastFrame = 0;
				//while(bink->curFrame<frameId-1)
				//	bink->GetNextFrame(buff,scaling);
			}
			//
			}
			UpdateTex();
			if(bink->curFrame==0)
				lastFrame = 0;
		}
	}
	
	//if(frameTex.id)
	{
		float h=0.9*aspect/((float)frameTex.width/frameTex.height);
		DrawRect(0.02,0.1,0.96*zoom,h*zoom,false,&frameTex);
	}
	char tstr[256];
	snprintf(tstr,256,"frame: %f",deltaTime);
	DrawText(tstr,0.02,0.95,0.3);
	DrawButton(&bClose);
	DrawButton(&bNext);
	DrawButton(&bPlayPause);
}

void BinkPlayer::OnTouch(float x,float y,int a)
{
	float tx = x/scrWidth;
	float ty = y/scrHeight;
	if(a==0)
	{
		bClose.Hit(tx,ty);
		bNext.Hit(tx,ty);
		bPlayPause.Hit(tx,ty);
		
		if(tx>0.8)
		{
			zoom=ty*8.0f;
		}
		else
		{
			zoom=1.0f;
		}
	}
}

void BinkPlayer::Open()
{
	bink = new BinkFile();
	char path[256];
	g_fs.GetFilePath(fileName.c_str(),path);
	if(!bink->Open(path)){
		delete bink;
		bink=0;
		Log("Error: can't load bink\n");
		CloseEditor();
		return;
	}
	int sw = bink->header.width*scaling;
	int sh = bink->header.height*scaling;
	frameTex.Create(sw,sh);
#ifdef BINK_16BIT
	buff = new char[sw*sh*2];
	frameTex.Upload(0,GL_RGB565,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,0);
#else
	buff = new char[sw*sh*3];
	frameTex.Upload(0,GL_RGB,0);
#endif
	UpdateTex();
}

void BinkPlayer::UpdateTex()
{
	if(decode || bink->curFrame==0)
		bink->GetNextFrame(buff,scaling);
	if(upload || bink->curFrame==0){
		frameTex.Bind();
		//frameTex.Upload(0,GL_RGB,(GLubyte*)buff);
		glTexSubImage2D(frameTex.target,0,0,0,frameTex.width,frameTex.height,frameTex.fmt,frameTex.type,(GLubyte*)buff);
	}
	//frameTex.Upload(0,GL_LUMINANCE,(GLubyte*)buff);
}
