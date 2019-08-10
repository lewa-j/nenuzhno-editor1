
#pragma once

#include "editor.h"
#include "button.h"
#include "BinkFile.h"
#include "graphics/texture.h"

class BinkPlayer : public Editor
{
public:
	BinkPlayer();
	~BinkPlayer();
	
	void Resize(int w, int h);
	void Draw();
	void OnTouch(float x,float y,int a);
private:
	Button bClose;
	Button bNext;
	Button bPlayPause;
	
	std::string fileName;
	bool upload;
	bool decode;
	bool skipping;
	float scaling;
	void ReadConfig();

	BinkFile *bink;
	char *buff;
	Texture frameTex;
	float zoom;
	bool playing;
	float lastFrame;
	void Open();
	void UpdateTex();
};

