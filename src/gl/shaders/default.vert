void main(
	float3 in_pos,
	float3 in_normal,
	fixed4 in_color,
	half2 in_tex0,
	uniform float4x4 u_wvp,
	uniform float4x4 u_world,
	uniform half4 u_ambLight,
	uniform half4 u_surfProps,
	uniform half4 u_fogData,
	uniform half4 u_matColor,
	uniform half4 u_lightParams[MAX_LIGHTS],
	uniform half4 u_lightDirection[MAX_LIGHTS],
	uniform half4 u_lightColor[MAX_LIGHTS],
	half4 out v_color : COLOR0,
	half2 out v_tex0 : TEXCOORD0,
	fixed out v_fog : FOG,
	float4 out gl_Position : POSITION
) {
	gl_Position = mul(float4(in_pos, 1.0), u_wvp);
	float3 Normal = mul(in_normal, float3x3(u_world));

	v_tex0 = in_tex0;

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	
	half3 color = half3(0.0, 0.0, 0.0);
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_lightParams[i].x == 0.0)
			break;
		if(u_lightParams[i].x == 1.0){
			// direct
			fixed l = max(0.0, dot(Normal, -u_lightDirection[i].xyz));
			color += l*u_lightColor[i].rgb;
		}
	}
	
	v_color.rgb += color*surfDiffuse;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color *= u_matColor;

	v_fog = DoFog(gl_Position.w, u_fogData);
}
