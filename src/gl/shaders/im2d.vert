void main(
	float4 in_pos,
	half4 in_color,
	half2 in_tex0,
	uniform half4 u_fogData,
	uniform float4 u_xform,
	half4 out v_color : COLOR0,
	half2 out v_tex0 : TEXCOORD0,
	fixed out v_fog : FOG,
	float4 out gl_Position : POSITION
) {
	gl_Position = in_pos;
	gl_Position.w = 1.0;
	gl_Position.xy = gl_Position.xy * u_xform.xy + u_xform.zw;
	v_fog = DoFog(gl_Position.z, u_fogData);
	gl_Position.xyz *= gl_Position.w;
	v_color = in_color;
	v_tex0 = in_tex0;
}
