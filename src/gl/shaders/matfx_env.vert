void main(
	float3 in_pos,
	float3 in_normal,
	float4 in_color,
	float2 in_tex0,
	uniform float4x4 u_proj,
	uniform float4x4 u_world,
	uniform float4x4 u_view,
	uniform float4x4 u_texMatrix,
	uniform float4 u_ambLight,
	uniform float4 u_surfProps,
	uniform float4 u_fogData,
	uniform float4 u_matColor,
	uniform float4 u_lightParams[MAX_LIGHTS],
	uniform float4 u_lightDirection[MAX_LIGHTS],
	uniform float4 u_lightColor[MAX_LIGHTS],
	float4 out v_color : COLOR0,
	float2 out v_tex0 : TEXCOORD0,
	float2 out v_tex1 : TEXCOORD1,
	float out v_fog : FOG,
	float4 out gl_Position : POSITION
) {
	float4 Vertex = u_world * float4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	float3 Normal = float3x3(u_world) * in_normal;

	v_tex0 = in_tex0;
	v_tex1 = (u_texMatrix * float4(Normal, 1.0)).xy;

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	
	float3 color = float3(0.0, 0.0, 0.0);
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_lightParams[i].x == 0.0)
			break;
		if(u_lightParams[i].x == 1.0){
			// direct
			float l = max(0.0, dot(Normal, -u_lightDirection[i].xyz));
			color += l*u_lightColor[i].rgb;
		}
	}
	
	v_color.rgb += color*surfDiffuse;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color *= u_matColor;

	v_fog = DoFog(gl_Position.w, u_fogData);
}
