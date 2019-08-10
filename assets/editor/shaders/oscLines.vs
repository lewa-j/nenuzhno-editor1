#version 100
precision highp float;

attribute vec4 a_position;

varying float v_brightness;

uniform mat4 u_mvpMtx;

void main()
{
	v_brightness = a_position.z;
	gl_Position = u_mvpMtx * vec4(a_position.x,a_position.y,0.0,a_position.w);
	gl_PointSize = 4.0;
}
