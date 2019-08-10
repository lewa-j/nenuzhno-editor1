#version 100
precision highp float;

varying vec2 v_uv;
varying vec4 v_uv2;
varying vec3 v_normal;
varying vec3 v_lmDir;
varying vec3 v_viewDir;

uniform sampler2D u_texture;

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	vec3 lmNorm = vec3(0.577,0.577,0.577);
	//float diffuse = dot(v_normal*0.5+0.5, v_lmDir.xyz);
	//float diffuse = dot(v_lmDir,lmNorm*lmNorm);
	float diffuse = dot(v_lmDir,vec3(1.0));
	gl_FragColor = col * v_uv2 * diffuse;
	//gl_FragColor = col * lm * dot(v_normal, lmDir*2.0-1.0);
	//gl_FragColor = mix(col,v_lmDir,0.99);
	//gl_FragColor = mix(col,vec4(v_normal,1.0),0.99);
	//gl_FragColor = mix(col,v_uv2,0.99);
	//gl_FragColor = mix(col,v_uv2 * diffuse,0.99);
}
