#version 100
precision highp float;

varying vec2 v_uv;
varying vec2 v_uv2;
varying vec3 v_normal;
//varying vec3 v_viewDir;
//varying mat3 v_tangentMtx;

uniform sampler2D u_texture;
uniform sampler2D u_lightmap;
uniform sampler2D u_lightmapDir;

const mat3 LightMapBasis = mat3(
				vec3(	0.0,					-1.0 / sqrt(2.0),			+1.0 / sqrt(2.0)),
				vec3(	sqrt(6.0) / 3.0,		-1.0 / sqrt(6.0),			-1.0 / sqrt(6.0)),
				vec3(	1.0 / sqrt(3.0),		1.0 / sqrt(3.0),			1.0 / sqrt(3.0))
				);

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	vec4 lm = texture2D(u_lightmap, v_uv2);
	vec3 lmDir = texture2D(u_lightmapDir, v_uv2).rgb;
	//vec3 norm = v_normal;
	//vec3 norm = normalTex*v_tangentMtx;
	vec3 norm = vec3(0.0,0.0,1.0);
	//vec3 lmNorm = LightMapBasis*norm;
	vec3 lmNorm = vec3(0.577,0.577,0.577);//without normalmapping
	//float diffuse = dot(lmDir,lmNorm*lmNorm);
	float diffuse = dot(lmDir,vec3(1.0));
	//v_viewDir = normalize(v_viewDir);
	//vec3 reflectVec = -v_viewDir+norm*dot(norm,v_viewDir)*2.0;
	//float specular = 0.5*pow(max(dot(lmDir, normalize(reflectVec)), 0.0), 10.0);
	//float specular = 0.5*pow(max(0.0,dot(lmDir,LightMapBasis*reflectVec)),5.0);
	//gl_FragColor = col * lm * diffuse + lm * specular;
	gl_FragColor = col * lm * diffuse;
	//gl_FragColor = col * pow(lm*diffuse,vec4(0.5));
	//gl_FragColor = mix(col,vec4(norm*0.5+0.5,1.0),0.99);
	//gl_FragColor = mix(gl_FragColor,lm * specular,0.99);
	//gl_FragColor = mix(col,lm * diffuse,0.99);
}
