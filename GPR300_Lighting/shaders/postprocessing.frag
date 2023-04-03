#version 450
out vec4 FragColor;

in vec2 uv;

uniform sampler2D _Texture1;

uniform float time;
uniform int effectIndex = 0;

void main()
{
	vec3 color = texture(_Texture1, uv).rgb;
	FragColor = vec4(color, 1);

	switch (effectIndex)
	{
		vec2 newUV;
		vec3 newColor;

		// Invert
		case 1:
			FragColor = vec4(1 - FragColor.r, 1 - FragColor.g, 1 - FragColor.b, FragColor.a);
			break;

		// Red Overlay
		case 2:
			FragColor = vec4(FragColor.r, 0, 0, FragColor.a);
			break;
		
		// Whatever this is
		case 3:
			newUV = vec2(sin(uv.x * time), cos(uv.y * time));
			newColor = texture(_Texture1, newUV).rgb;
			FragColor = vec4(newColor, 1);

			break;

		// Wave
		case 4:
			vec2 pulse = sin(time - 2f * uv);
			newUV = uv + 0.25 * vec2(pulse.x, -pulse.x);
			newUV.x = uv.x;
			newColor = texture(_Texture1, newUV).rgb;
			FragColor = vec4(newColor, 1);

			break;

		// None
		default:
			break;
	}
}