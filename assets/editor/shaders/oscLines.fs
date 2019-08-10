#version 100
precision highp float;

varying float v_brightness;
uniform vec4 u_color;

void main()
{
	gl_FragColor = u_color * v_brightness;
}
