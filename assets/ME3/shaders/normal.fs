#version 100
precision highp float;

varying vec3 v_normal;
varying vec2 v_uv;

uniform sampler2D u_tex;

void main()
{
	vec4 col = vec4(v_normal*0.5+0.5,1.0);
	//float p_light = max(0.1,dot(normalize(v_normal),normalize(vec3(0.3,0.4,1.0))));
	gl_FragColor = col;
}
