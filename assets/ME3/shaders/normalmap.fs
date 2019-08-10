#version 100
precision highp float;

varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_uv;
varying vec4 v_tangent;
varying vec3 v_binormal;

uniform sampler2D u_texture;
uniform sampler2D u_normalmap;
//uniform vec4 u_color;

varying vec3 v_viewDir;
const float specPower = 15.0;

vec3 calcLight(vec3 dir, vec3 norm, vec3 lcol, vec3 col){
	vec3 reflectVec = reflect(-dir, norm);
	float diffuse = max(0.0,dot(norm,dir));
	vec3 vd = normalize(v_viewDir);
	float specular = 0.3*pow(max(dot(vd, reflectVec), 0.0), specPower);

	return col*diffuse*lcol + specular*lcol;
}

vec3 calcPointLight(vec3 pos, vec3 norm, vec3 lcol, vec3 col){
	return calcLight(normalize(pos-v_position),norm,lcol,col);
}

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	vec3 norm = texture2D(u_normalmap, v_uv).rgb*2.0-1.0;
	/*if(norm.z<0.0){
		gl_FragColor = vec4(0.0,1.0,0.0,1.0);
		return;
	}*/
	norm = normalize(v_normal*norm.z+v_tangent.xyz*norm.x+-v_binormal*norm.y);
	//col.xyz = mix(col,v_tangent.www*0.5+0.5,0.99);
	//col.xyz = mix(col.xyz,norm*0.5+0.5,0.99);
	//col.xyz = mix(col,v_tangent*0.5+0.5,0.99);
	//col.xyz = mix(col.rgb,v_binormal*0.5+0.5,0.99);

/*	vec3 ld = normalize(vec3(0.3,0.4,0.7));
	vec3 reflectVec = reflect(-ld, norm);
	float diffuse = max(0.0,dot(norm,ld));
	vec3 vd = normalize(v_viewDir);
	float specular = 0.5*pow(max(dot(vd, reflectVec), 0.0), specPower);

	//diffuse = dot(normalize(v_normal),norm);
	//col.xyz=mix(col.xyz,vec3(diffuse),0.99);
	col.rgb*=diffuse+0.1;
	col.rgb+=specular;
*/
	vec4 outc=vec4(0.0,0.0,0.0,1.0);
	outc.rgb+=col.rgb*0.1;
	//outc.rgb+=calcLight(normalize(vec3(0.6,0.4,0.3)),norm,vec3(1.0,0.7,0.5),col.rgb);
	//outc.rgb+=calcPointLight(vec3(-0.8,0.5,1.5),norm,vec3(0.5,0.6,1.0),col.rgb);
	outc.rgb+=calcLight(normalize(vec3(0.6,0.4,0.3)),norm,vec3(1.0),col.rgb);

	//outc = vec4(dot(normalize(v_normal),norm));

	gl_FragColor = outc;
}

//
/*
precision highp float;

varying vec3 v_position;
varying vec2 v_uv;
varying vec3 v_lightDir;
varying vec3 v_viewDir;

uniform sampler2D u_texture;
uniform sampler2D u_normalmap;
uniform samplerCube u_cubemap;
uniform vec3 u_cameraPos;

void main1()
{
	vec3 coord = v_position-vec3(5.0,3.2,1.0);
	coord.y -= 1.0;
	coord.xy *= 3.0;
	if(coord.y>1.0)
		discard;
	if(coord.y<-1.0)
	{
		coord.z *= -1.0;
		coord.y += 2.0;
	}
	
	if(coord.x<-1.0)
	{
		coord.x += 2.0;
	}
	else if(coord.x>-1.0&&coord.x<1.0)
	{
		coord.xzy = coord;
	}
	else
	{
		coord.x -= 2.0;
		coord.zyx = coord;
	}
	
	vec4 col = textureCube(u_cubemap,coord);
	gl_FragColor = col*40.0;
}

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	vec3 normal = normalize(texture2D(u_normalmap, v_uv).xyz*2.0-1.0);
	//normal.z*=-1.0;
	//normal = normalize(mix(normal,vec3(0.0,1.0,0.0),0.9));
	//vec3 lightDir = normalize(u_lightPos - v_position);
	//vec3 lightDir = normalize(vec3(0.5,0.5,1.0));
	//vec3 viewDir = normalize(u_cameraPos - v_position);
	vec3 ld = normalize(v_lightDir);
	vec3 reflectVec = reflect(-ld, normal);
	float diffuse = max(0.0,dot(normal,ld));
	//diffuse = 1.0;
	vec3 vd = normalize(v_viewDir);
	float fresnel = mix(pow(1.0-dot(vd, normal),0.8),1.0,0.1);
	float specular = 0.4*pow(max(dot(vd, reflectVec), 0.0), 20.0);
	//specular = 0.0;
	//normal = normalize(mix(normal,vec3(0.0,0.0,1.0),0.5));
	normal = vec3(0.0,0.0,1.0);
	vec3 reflectLook = normalize(reflect(-vd, normal));
	//reflectLook.z*=-1.0;
	reflectLook = reflectLook.xzy;
	
	vec4 env = textureCube(u_cubemap,reflectLook);
	//gl_FragColor = mix(col*diffuse+specular,env,fresnel);
	gl_FragColor = col*diffuse+(specular+env)*fresnel;
	gl_FragColor = env*(v_position.x-4.0)*20.0;
}

void main3()
{
	vec4 col = texture2D(u_texture, v_uv);
	vec3 normal = normalize(texture2D(u_normalmap, v_uv).xyz*2.0-1.0);
	//normal.z*=-1.0;
	//normal = vec3(0.0, 0.0, 1.0);
	float diffuse = 0.8*max(0.0,dot(normal,normalize(v_lightDir)));
	
	vec3 reflectVec = reflect(-normalize(v_lightDir), normal);
	float specular = 0.4*pow(max(dot(normalize(v_viewDir), reflectVec), 0.0), 20.0);
	gl_FragColor = col*diffuse+specular;
	//gl_FragColor = vec4(specular);
	//gl_FragColor = vec4(diffuse);
	//gl_FragColor.rgb = reflectVec;
	//gl_FragColor.rgb = normal;
	//gl_FragColor.rgb = v_viewDir;
	
	//vec3 vd = normalize(u_cameraPos - v_position);
	//gl_FragColor.rgb = abs(normalize(v_viewDir)-vd);
}
*/
