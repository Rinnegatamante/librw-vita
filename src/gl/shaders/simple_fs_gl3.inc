const char *simple_frag_src =
"float4 main(\n"
"	float4 v_color : COLOR,\n"
"	float2 v_tex0 : TEXCOORD0,\n"
"	float v_fog : FOG,\n"
"	uniform float2 u_alpharef,\n"
"	uniform float4 u_fogColor,\n"
"	uniform sampler2D tex0\n"
") {\n"
"	float4 color = v_color*tex2D(tex0, float2(v_tex0.x, 1.0-v_tex0.y));\n"
"	color.rgb = lerp(u_fogColor.rgb, color.rgb, v_fog);\n"
"	DoAlphaTest(color.a, u_alphaRef);\n"
"	\n"
"	return color;\n"
"}\n"

;
