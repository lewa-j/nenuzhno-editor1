precision highp float;
varying vec2 v_uv;
uniform sampler2D u_texture;

#define PI 3.14
#define PI2 6.28

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	col = vec4(col.r,col.a,0.0,1.0);
	//vec3 norm = vec3(col.rg*2.0-1.0,0.0);
	//norm.z = sqrt(1.0 - dot(norm.xy,norm.xy));
	vec3 norm = vec3(cos(col.y*PI2)*cos(col.x*PI2),sin(col.y*PI2)*cos(col.x*PI2),sin(col.x*PI2));
	norm=normalize(vec3(-norm.z,norm.y,norm.x));
	col.rgb = norm*0.5+0.5;
	gl_FragColor = col;
}
