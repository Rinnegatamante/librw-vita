float4 main(
	float4 v_color : COLOR,
	float2 v_tex0 : TEXCOORD0,
	float v_fog : FOG,
	uniform float2 u_alpharef,
	uniform float4 u_fogColor,
	uniform sampler2D tex0
) {
	float4 color = v_color*tex2D(tex0, float2(v_tex0.x, 1.0-v_tex0.y));
	color.rgb = lerp(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a, u_alphaRef);
	
	return color;
}

