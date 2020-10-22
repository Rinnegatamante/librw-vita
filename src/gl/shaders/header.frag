#define u_fogStart (u_fogData.x)
#define u_fogEnd (u_fogData.y)
#define u_fogRange (u_fogData.z)
#define u_fogDisable (u_fogData.w)

void DoAlphaTest(float a, half2 u_alphaRef)
{
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
}
