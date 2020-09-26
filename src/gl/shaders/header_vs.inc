const char *header_vert_src =
"#define u_fogStart (u_fogData.x)\n"
"#define u_fogEnd (u_fogData.y)\n"
"#define u_fogRange (u_fogData.z)\n"
"#define u_fogDisable (u_fogData.w)\n"

"#define MAX_LIGHTS 8\n"

"#define surfAmbient (u_surfProps.x)\n"
"#define surfSpecular (u_surfProps.y)\n"
"#define surfDiffuse (u_surfProps.z)\n"

"float DoFog(float w, float4 u_fogData)\n"
"{\n"
"	return clamp((w - u_fogEnd)*u_fogRange, u_fogDisable, 1.0);\n"
"}\n"
;
