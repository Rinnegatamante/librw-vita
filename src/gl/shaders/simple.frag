float4 main(
	half4 v_color : COLOR0,
	half2 v_tex0 : TEXCOORD0,
	fixed v_fog : FOG,
	uniform half4 u_fogColor,
	uniform half2 u_alphaRef,
	uniform sampler2D tex0
) {
	half4 color = v_color*tex2D(tex0, half2(v_tex0.x, 1.0-v_tex0.y));
	color.rgb = lerp(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a, u_alphaRef);
	
	return color;
}

