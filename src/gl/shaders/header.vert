#define u_fogStart (u_fogData.x)
#define u_fogEnd (u_fogData.y)
#define u_fogRange (u_fogData.z)
#define u_fogDisable (u_fogData.w)

#define MAX_LIGHTS 8

#define surfAmbient (u_surfProps.x)
#define surfSpecular (u_surfProps.y)
#define surfDiffuse (u_surfProps.z)

fixed DoFog(float w, half4 u_fogData)
{
	return clamp((w - u_fogEnd)*u_fogRange, u_fogDisable, 1.0);
}
