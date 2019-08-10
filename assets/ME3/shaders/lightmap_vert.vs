#version 100
precision highp float;

attribute vec4 a_position;
attribute vec3 a_normal;
attribute vec2 a_uv;
attribute vec4 a_uv2;
attribute vec4 a_lmDir;

varying vec3 v_normal;
varying vec2 v_uv;
varying vec4 v_uv2;
varying vec3 v_lmDir;
//varying vec3 v_viewDir;

uniform mat4 u_mvpMtx;
uniform mat4 u_modelMtx;
//uniform vec3 u_viewPos;

void main()
{
	gl_Position = u_mvpMtx * a_position;
	gl_PointSize = 4.0;
	v_normal = normalize(mat3(u_modelMtx)*a_normal);
	v_uv = a_uv;
	v_uv2 = a_uv2.bgra;
	v_lmDir = a_lmDir.bgr;
	//v_viewDir = (u_modelMtx*a_position).xyz-u_viewPos;
}
