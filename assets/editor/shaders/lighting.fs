#version 100
precision highp float;

varying vec3 v_normal;
varying vec2 v_uv;

uniform sampler2D u_tex;
const vec3 v_viewDir=vec3(0.0,0.06,0.95);

void main()
{
	vec4 col = texture2D(u_tex, v_uv);
	//float p_light = max(0.1,dot(normalize(v_normal),normalize(vec3(0.3,0.4,0.7))));
	//gl_FragColor = col*vec4(vec3(p_light),1.0);
	vec3 ld = normalize(vec3(0.3,0.4,0.7));
	vec3 normal = normalize(v_normal);
	vec3 reflectVec = reflect(-ld, normal);
	float diffuse = max(0.0,dot(normal,ld));
	vec3 vd = normalize(v_viewDir);
	float specular = 0.5*pow(max(dot(vd, reflectVec), 0.0), 10.0);
	gl_FragColor = col;
	gl_FragColor.rgb*=diffuse+0.1;
	gl_FragColor.rgb+=specular;
	//gl_FragColor += vec4(1.0);
}
