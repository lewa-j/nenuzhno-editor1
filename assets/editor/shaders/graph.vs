#version 100
precision highp float;

attribute vec4 a_position;
attribute vec2 a_uv;

varying vec2 v_uv;

uniform mat4 u_mvpMtx;

float func(float x)
{
	//return pow(x,3.0);
	//return 2.0*x/(pow(x,2.0)-4.0);
	//return sin(x*0.5)*sin(x*8.0);
	//x+=0.012;
	//return sqrt(1.0-pow(x,2.0));
	//return sin(sin(x*3.14)+x);
	float a=2.0;
	float b=-1.0;
	float c=-1.0;
/*	if(mod(x*20.0,2.0)>0.9)
		return a*x*x+b*x+c;
	else
		return (x-1.0)/(2.0*x+1.0);
*/
	//return (x*x+2.0*x-3.0)/((x-7.0)*(x+5.0));
	//return ((x-2.0)*(x-2.0))/((x-1.0)*(x-3.0));
	//return 12.0/(8.0*x-3.0);
	//return 5.0/(4.0*x-8.0);
	//return (pow(3.0,x+1.0)+2.0*pow(4.0,x))/(pow(4.0,x+1.0)-5.0);
	//return (1.0/(x+2.0) - (12.0/(pow(x,3.0)+8.0)));
	//return pow(3.0,1.0/x);
	//return (pow(x,3.0)-1.0)*(pow(x,2.0)+x+1.0);
	//return (pow(x,2.0)+1.0)/(pow(x,2.0)-1.0);
	return pow(x,3.0)-3.0*pow(x,2.0);
}

void main()
{
	vec4 pos = a_position;
	
	//pos.y = sin((pos.x-0.5) * 18.0)*0.17*0.5;
	//pos.y = func((pos.x-0.5)*18.0)*0.2;
	pos.y = func((pos.x-0.5)*17.0)*0.125;
	
	gl_Position = u_mvpMtx * pos;
	gl_PointSize = 4.0;
	v_uv = a_uv;
}
