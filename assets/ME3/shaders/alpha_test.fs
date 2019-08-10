#version 100
precision highp float;

varying vec2 v_uv;
uniform sampler2D u_texture;

void main()
{
	vec4 col = texture2D(u_texture, v_uv);
	if(col.a<0.5)
		discard;
	gl_FragColor = col;
}
