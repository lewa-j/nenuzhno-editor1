
#pragma once

class Camera;
class PCCLevel;

class LevelRenderer
{
public:
	LevelRenderer();
	
	void SetLevel(PCCLevel *lvl);
	void Draw(Camera *cam);
	
	PCCLevel *level;
};

