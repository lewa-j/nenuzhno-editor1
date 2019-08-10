#version 100
precision highp float;

attribute vec4 a_position;
attribute vec4 a_normal;
//attribute vec3 a_tangent;
attribute vec2 a_uv;
attribute vec2 a_uv2;

varying vec3 v_normal;
varying vec2 v_uv;
varying vec2 v_uv2;
//varying vec3 v_viewDir;
//varying mat3 v_tangentMtx;

uniform mat4 u_mvpMtx;
uniform mat4 u_modelMtx;
uniform vec4 u_lmScaleBias;
//uniform vec3 u_viewPos;

void main()
{
	gl_Position = u_mvpMtx * a_position;
	gl_PointSize = 4.0;
	v_normal = normalize(mat3(u_modelMtx)*a_normal.xyz);
	v_uv = a_uv;
	v_uv2 = a_uv2*u_lmScaleBias.xy+u_lmScaleBias.zw;
	//vec3 tangent = normalize(mat3(u_modelMtx) * a_tangent);
	//vec3 binormal = -normalize(cross(v_normal,tangent)*a_normal.w);
	//vec3 binormal = -normalize(mat3(u_modelMtx)*cross(a_normal.xyz,a_tangent)*a_normal.w);
	//v_tangentMtx = mat3(tangent,binormal,v_normal);
	//v_viewDir = u_viewPos-(u_modelMtx*a_position).xyz;
	//v_viewDir = v_viewDir*v_tangentMtx;
}
