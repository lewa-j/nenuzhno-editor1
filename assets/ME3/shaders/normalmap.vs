#version 100
precision highp float;

attribute vec4 a_position;
attribute vec4 a_normal;
attribute vec2 a_uv;
attribute vec3 a_tangent;

varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_uv;
varying vec4 v_tangent;
varying vec3 v_binormal;
varying vec3 v_viewDir;

uniform mat4 u_mvpMtx;
uniform mat4 u_modelMtx;
uniform vec3 u_viewPos;

void main()
{
	gl_Position = u_mvpMtx * a_position;
	gl_PointSize = 4.0;
	v_position = (u_modelMtx * a_position).xyz;
	v_viewDir=u_viewPos-v_position;
	v_normal = normalize(mat3(u_modelMtx) * a_normal.xyz);
	v_uv = a_uv;
	v_tangent = vec4(normalize(mat3(u_modelMtx) * a_tangent),a_normal.w);
	v_binormal = normalize(cross(v_normal,v_tangent.xyz)*a_normal.w);
	//v_binormal = normalize(mat3(u_modelMtx)*cross(a_normal.xyz,a_tangent)*a_normal.w);
}


//
/*
precision highp float;

attribute vec4 a_position;
attribute vec3 a_normal;
attribute vec2 a_uv;

varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_uv;
varying vec3 v_lightDir;
varying vec3 v_viewDir;

uniform mat4 u_mvpMtx;
uniform mat4 u_modelMtx;
uniform vec3 u_cameraPos;

const vec3 u_lightPos = vec3(5.0,4.0,-0.5);

mat4 inverse(mat4 m){return m;}

void main()
{
	gl_Position = u_mvpMtx * a_position;
	v_position = (u_modelMtx * a_position).xyz;
	v_normal = normalize(mat3(u_modelMtx) * a_normal);
	//v_lightDir = normalize(mat3(inverse(u_modelMtx)) * (u_lightPos - v_position));
	//v_viewDir = mat3(inverse(u_modelMtx)) * (u_cameraPos - v_position);
	v_lightDir = u_lightPos - v_position;
	v_viewDir = u_cameraPos - v_position;
	v_uv = a_uv;
	gl_PointSize = 4.0;
}
*/

//
/*
//#undef BUMP
precision highp float;

attribute vec4 a_position;
attribute vec3 a_normal;
attribute vec2 a_uv;
attribute vec4 a_lmuv;
attribute vec3 a_tangent;

varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_uv;
varying vec4 v_lmuv12;
varying vec2 v_lmuv3;
varying vec3 v_tangent;

uniform mat4 u_mvpMtx;
uniform mat4 u_modelMtx;

void main()
{
	gl_Position = u_mvpMtx * a_position;
	v_position = (u_modelMtx * a_position).xyz;
	v_normal = normalize(mat3(u_modelMtx) * a_normal);
	v_uv = a_uv;
#ifdef BUMP
	v_lmuv12.xy = a_lmuv.xy+a_lmuv.zw;
	v_lmuv12.zw = v_lmuv12.xy+a_lmuv.zw;
	v_lmuv3 = v_lmuv12.zw+a_lmuv.zw;
#else
	v_lmuv12.xy = a_lmuv.xy;
#endif
	v_tangent = normalize(mat3(u_modelMtx) * a_tangent);
	gl_PointSize = 4.0;
}
*/
