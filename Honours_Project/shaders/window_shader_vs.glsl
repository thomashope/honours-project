#version 410
in vec3 vPosition;
in vec2 vUV;
out vec2 fUV;
void main()
{
	fUV = vUV;
	gl_Position = vec4(vPosition, 1.0);
}