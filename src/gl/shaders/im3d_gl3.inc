const char *im3d_vert_src =
"void main(\n"
"	float3 in_pos,\n"
"	half4 in_color,\n"
"	half2 in_tex0,\n"
"	uniform half4 u_fogData,\n"
"	uniform float4x4 u_wvp,\n"
"	uniform float4x4 u_world,\n"
"	half4 out v_color : COLOR0,\n"
"	half2 out v_tex0 : TEXCOORD0,\n"
"	fixed out v_fog : FOG,\n"
"	float4 out gl_Position : POSITION\n"
") {\n"
"	gl_Position = mul(float4(in_pos, 1.0), u_wvp);\n"
"	v_color = in_color;\n"
"	v_tex0 = in_tex0;\n"
"	v_fog = DoFog(gl_Position.w, u_fogData);\n"
"}\n"
;
