#define shininess (u_fxparams.x)
#define disableFBA (u_fxparams.y)

float4 main(
	half4 v_color : COLOR0,
	half2 v_tex0 : TEXCOORD0,
	half2 v_tex1 : TEXCOORD1,
	fixed v_fog : FOG,
	uniform half4 u_fogColor,
	uniform half2 u_alphaRef,
	uniform half4 u_colorClamp,
	uniform half2 u_fxparams,
	uniform sampler2D tex0 : TEXUNIT0,
	uniform sampler2D tex1 : TEXUNIT1
) {
	half4 pass1 = v_color;
	half4 envColor = max(pass1, u_colorClamp);
	pass1 *= tex2D(tex0, half2(v_tex0.x, 1.0-v_tex0.y));

	half4 pass2 = envColor*shininess*tex2D(tex1, half2(v_tex1.x, 1.0-v_tex1.y));

	pass1.rgb = lerp(u_fogColor.rgb, pass1.rgb, v_fog);
	pass2.rgb = lerp(half3(0.0, 0.0, 0.0), pass2.rgb, v_fog);
	
	half fba = max(pass1.a, disableFBA);
	float4 color;
	color.rgb = pass1.rgb*pass1.a + pass2.rgb*fba;
	color.a = pass1.a;

	DoAlphaTest(color.a, u_alphaRef);
	
	return color;
}
