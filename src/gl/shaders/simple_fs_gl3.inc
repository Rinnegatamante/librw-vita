const char *simple_frag_src =
"float4 main(\n"
"	half4 v_color : COLOR0,\n"
"	half2 v_tex0 : TEXCOORD0,\n"
"	fixed v_fog : FOG,\n"
"	uniform half4 u_fogColor,\n"
"	uniform half2 u_alphaRef,\n"
"	uniform sampler2D tex0\n"
") {\n"
"	half4 color = v_color*tex2D(tex0, half2(v_tex0.x, 1.0-v_tex0.y));\n"
"	color.rgb = lerp(u_fogColor.rgb, color.rgb, v_fog);\n"
"	DoAlphaTest(color.a, u_alphaRef);\n"
"	\n"
"	return color;\n"
"}\n"

;
