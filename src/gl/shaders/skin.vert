void main(
	float3 in_pos,
	float3 in_normal,
	fixed4 in_color,
	half2 in_tex0,
	float4 in_weights,
	float4 in_indices,
	uniform float4x4 u_wvp,
	uniform float4x4 u_world,
	uniform half4 u_ambLight,
	uniform half4 u_surfProps,
	uniform half4 u_fogData,
	uniform half4 u_matColor,
	uniform half4 u_lightParams[MAX_LIGHTS],
	uniform half4 u_lightDirection[MAX_LIGHTS],
	uniform half4 u_lightColor[MAX_LIGHTS],
	uniform float4x4 u_boneMatrices[64],
	half4 out v_color : COLOR0,
	half2 out v_tex0 : TEXCOORD0,
	fixed out v_fog : FOG,
	float4 out gl_Position : POSITION
) {
	float4x4 Skin = u_boneMatrices[(int)(in_indices.x * 255)] * in_weights.x
		+ u_boneMatrices[(int)(in_indices.y * 255)] * in_weights.y
		+ u_boneMatrices[(int)(in_indices.z * 255)] * in_weights.z
		+ u_boneMatrices[(int)(in_indices.w * 255)] * in_weights.w;
		
	float3 SkinVertex = (mul(float4(in_pos, 1.f), Skin)).xyz;
	float3 SkinNormal = (mul(float4(in_normal, 0.f), Skin)).xyz;
	
	gl_Position = mul(float4(SkinVertex, 1.0), u_wvp);
	float3 Normal = mul(SkinNormal, float3x3(u_world));

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

	v_fog = DoFog(gl_Position.z, u_fogData);
}
