const char *matfx_env_vert_src =
"void main(\n"
"	float3 in_pos,\n"
"	float3 in_normal,\n"
"	fixed4 in_color,\n"
"	half2 in_tex0,\n"
"	uniform float4x4 u_wvp,\n"
"	uniform float4x4 u_world,\n"
"	uniform float4x4 u_texMatrix,\n"
"	uniform half4 u_ambLight,\n"
"	uniform half4 u_surfProps,\n"
"	uniform half4 u_fogData,\n"
"	uniform half4 u_matColor,\n"
"	uniform half4 u_lightParams[MAX_LIGHTS],\n"
"	uniform half4 u_lightDirection[MAX_LIGHTS],\n"
"	uniform half4 u_lightColor[MAX_LIGHTS],\n"
"	half4 out v_color : COLOR0,\n"
"	half2 out v_tex0 : TEXCOORD0,\n"
"	half2 out v_tex1 : TEXCOORD1,\n"
"	fixed out v_fog : FOG,\n"
"	float4 out gl_Position : POSITION\n"
") {\n"
"	gl_Position = mul(float4(in_pos, 1.0), u_wvp);\n"
"	float3 Normal = mul(in_normal, float3x3(u_world));\n"

"	v_tex0 = in_tex0;\n"
"	v_tex1 = (mul(float4(Normal, 1.0), u_texMatrix)).xy;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	\n"
"	half3 color = float3(0.0, 0.0, 0.0);\n"
"	for(int i = 0; i < MAX_LIGHTS; i++){\n"
"		if(u_lightParams[i].x == 0.0)\n"
"			break;\n"
"		if(u_lightParams[i].x == 1.0){\n"
"			// direct\n"
"			fixed l = max(0.0, dot(Normal, -u_lightDirection[i].xyz));\n"
"			color += l*u_lightColor[i].rgb;\n"
"		}\n"
"	}\n"
"	\n"
"	v_color.rgb += color*surfDiffuse;\n"
"	v_color = clamp(v_color, 0.0, 1.0);\n"
"	v_color *= u_matColor;\n"

"	v_fog = DoFog(gl_Position.w, u_fogData);\n"
"}\n"
;
const char *matfx_env_frag_src =
"#define shininess (u_fxparams.x)\n"
"#define disableFBA (u_fxparams.y)\n"

"float4 main(\n"
"	half4 v_color : COLOR0,\n"
"	half2 v_tex0 : TEXCOORD0,\n"
"	half2 v_tex1 : TEXCOORD1,\n"
"	fixed v_fog : FOG,\n"
"	uniform half4 u_fogColor,\n"
"	uniform half2 u_alphaRef,\n"
"	uniform half4 u_colorClamp,\n"
"	uniform half2 u_fxparams,\n"
"	uniform sampler2D tex0 : TEXUNIT0,\n"
"	uniform sampler2D tex1 : TEXUNIT1\n"
") {\n"
"	half4 pass1 = v_color;\n"
"	half4 envColor = max(pass1, u_colorClamp);\n"
"	pass1 *= tex2D(tex0, half2(v_tex0.x, 1.0-v_tex0.y));\n"

"	half4 pass2 = envColor*shininess*tex2D(tex1, half2(v_tex1.x, 1.0-v_tex1.y));\n"

"	pass1.rgb = lerp(u_fogColor.rgb, pass1.rgb, v_fog);\n"
"	pass2.rgb = lerp(half3(0.0, 0.0, 0.0), pass2.rgb, v_fog);\n"
"	\n"
"	half fba = max(pass1.a, disableFBA);\n"
"	float4 color;\n"
"	color.rgb = pass1.rgb*pass1.a + pass2.rgb*fba;\n"
"	color.a = pass1.a;\n"

"	DoAlphaTest(color.a, u_alphaRef);\n"
"	\n"
"	return color;\n"
"}\n"
;
