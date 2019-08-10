
#pragma once

#include <string>
#include "editor.h"
#include "sound/sound.h"
#include "sound/AudioClip.h"
#include "renderer/mesh.h"
#include "button.h"
#include "graphics/fbo.h"

class SoundEditor: public Editor
{
public:
	SoundEditor();
	~SoundEditor();
	void Resize(int w, int h);
	void Draw();
	void OnTouch(float x,float y,int a);
private:
	void ReadConfig();
	ISoundSystem *sound;
	Button bClose;
	Button bPlay;
	Button bStop;
	Button bSlider;
	float sliderPos;
	float zoom;
	bool is3D;
	bool loop;
	bool isPlaying;
	float playTime;

	int mode;
	AudioClip testClip;
	IAudioSource *asrc;
	glm::vec3 asrcPos;
	Mesh meshLoc;
	Mesh wave;
	std::string clipName;
	float genF;
	float genA;
	float genF2;
	float genA2;

	int oscSamples;
	FrameBufferObject *fbo;
	Texture *oscAccum[2];
	glslProg progOscLines;
};

