const char *header_frag_src =
"#define u_fogStart (u_fogData.x)\n"
"#define u_fogEnd (u_fogData.y)\n"
"#define u_fogRange (u_fogData.z)\n"
"#define u_fogDisable (u_fogData.w)\n"

"void DoAlphaTest(float a, float2 u_alphaRef)\n"
"{\n"
"	if(a < u_alphaRef.x || a >= u_alphaRef.y)\n"
"		discard;\n"
"}\n"
;
