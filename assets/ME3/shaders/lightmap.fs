#version 100
precision highp float;

varying vec2 v_uv;
varying vec2 v_uv2;
varying vec3 v_normal;

uniform sampler2D u_texture;
uniform sampler2D u_lightmap;
uniform vec4 u_lmColScale;
uniform vec4 u_lmColBias;

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	vec4 lm = texture2D(u_lightmap, v_uv2);
	
	gl_FragColor = col * (lm * u_lmColScale + u_lmColBias);
}
