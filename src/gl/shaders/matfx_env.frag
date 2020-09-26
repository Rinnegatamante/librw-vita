#define shininess (u_fxparams.x)
#define disableFBA (u_fxparams.y)

float4 main(
	float4 v_color : COLOR,
	float2 v_tex0 : TEXCOORD0,
	float2 v_tex1 : TEXCOORD1,
	float v_fog : FOG,
	uniform float2 u_alpharef,
	uniform float4 u_colorClamp,
	uniform float2 u_fxparams,
	uniform sampler2D tex0 : TEXUNIT0,
	uniform sampler2D tex1 : TEXUNIT1
) {
	float4 pass1 = v_color;
	float4 envColor = max(pass1, u_colorClamp);
	pass1 *= tex2D(tex0, float2(v_tex0.x, 1.0-v_tex0.y));

	vec4 pass2 = envColor*shininess*tex2D(tex1, float2(v_tex1.x, 1.0-v_tex1.y));

	pass1.rgb = lerp(u_fogColor.rgb, pass1.rgb, v_fog);
	pass2.rgb = lerp(float3(0.0, 0.0, 0.0), pass2.rgb, v_fog);
	
	float fba = max(pass1.a, disableFBA);
	float4 color;
	color.rgb = pass1.rgb*pass1.a + pass2.rgb*fba;
	color.a = pass1.a;

	DoAlphaTest(color.a, u_alphaRef);
	
	return color;
}
