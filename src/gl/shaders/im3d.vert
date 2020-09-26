void main(
	float3 in_pos,
	float4 in_color,
	float2 in_tex0,
	uniform float4 u_fogData,
	uniform float4x4 u_proj,
	uniform float4x4 u_world,
	uniform float4x4 u_view,
	float4 out v_color : COLOR0,
	float2 out v_tex0 : TEXCOORD0,
	float out v_fog : FOG,
	float4 out gl_Position : POSITION
) {
	float4 Vertex = u_world * float4(in_pos, 1.0);
	float4 CamVertex = u_view * Vertex;
	gl_Position = u_proj * CamVertex;
	v_color = in_color;
	v_tex0 = in_tex0;
	v_fog = DoFog(gl_Position.w, u_fogData);
}
