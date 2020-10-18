#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#ifdef RW_OPENGL
#include "rwgl3.h"
#include "rwgl3shader.h"
#include "rwgl3impl.h"

#define PLUGIN_ID 0

extern uint16_t *gIndices;
extern float *gVertexBuffer;
extern uint16_t *gIndicesPtr;
extern float *gVertexBufferPtr;

uint16_t *gConstIndices;
float *gVertexBufferIm2D;
uint16_t *gIndicesIm2D;

namespace rw {
namespace gl3 {
#ifndef LIBRW_SDL2
struct DisplayMode
{
	int32 depth;
	uint32 flags;
};
#endif

struct GlGlobals
{
#ifdef LIBRW_SDL2
	SDL_Window *window;
	SDL_GLContext glcontext;
#else
	int numMonitors;
	int currentMonitor;

	DisplayMode *modes;
	int numModes;
	int currentMode;
#endif
	int presentWidth, presentHeight;

	// for opening the window
	int winWidth, winHeight;
	const char *winTitle;
} glGlobals;

int32   alphaFunc;
float32 alphaRef;

struct UniformState
{
	float32 alphaRefLow;
	float32 alphaRefHigh;
	int32   pad[2];

	float32 fogStart;
	float32 fogEnd;
	float32 fogRange;
	float32 fogDisable;

	RGBAf   fogColor;
};

struct UniformScene
{
	float32 proj[16];
	float32 view[16];
};

#define MAX_LIGHTS 8

struct UniformObject
{
	RawMatrix    world;
	RGBAf        ambLight;
	struct {
		float type;
		float radius;
		float minusCosAngle;
		float hardSpot;
	} lightParams[MAX_LIGHTS];
	V4d lightPosition[MAX_LIGHTS];
	V4d lightDirection[MAX_LIGHTS];
	RGBAf lightColor[MAX_LIGHTS];
};

struct GLShaderState
{
	RGBA matColor;
	SurfaceProperties surfProps;
};

// this needs a define in the shaders as well!
//#define RW_GL_USE_UBOS

#ifndef RW_GLES2
static GLuint vao;
#endif
#ifdef RW_GL_USE_UBOS
static GLuint ubo_state, ubo_scene, ubo_object;
#endif
static GLuint whitetex;
static UniformState uniformState;
static UniformScene uniformScene;
static UniformObject uniformObject;
static GLShaderState shaderState;

#ifndef RW_GL_USE_UBOS
// State
int32 u_alphaRef;
int32 u_fogData;
//int32 u_fogStart;
//int32 u_fogEnd;
//int32 u_fogRange;
//int32 u_fogDisable;
int32 u_fogColor;

// Scene
int32 u_proj;
int32 u_view;

// Object
int32 u_world;
int32 u_ambLight;
int32 u_lightParams;
int32 u_lightPosition;
int32 u_lightDirection;
int32 u_lightColor;
#endif

int32 u_matColor;
int32 u_surfProps;

Shader *defaultShader;

static bool32 stateDirty = 1;
static bool32 sceneDirty = 1;
static bool32 objectDirty = 1;

struct RwRasterStateCache {
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

#define MAXNUMSTAGES 8

// cached RW render states
struct RwStateCache {
	bool32 vertexAlpha;
	uint32 alphaTestEnable;
	uint32 alphaFunc;
	bool32 textureAlpha;
	bool32 blendEnable;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	uint32 fogEnable;
	float32 fogStart;
	float32 fogEnd;

	// emulation of PS2 GS
	bool32 gsalpha;
	uint32 gsalpharef;

	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

enum
{
	// actual gl states
	RWGL_BLEND,
	RWGL_SRCBLEND,
	RWGL_DESTBLEND,
	RWGL_DEPTHTEST,
	RWGL_DEPTHFUNC,
	RWGL_DEPTHMASK,
	RWGL_CULL,
	RWGL_CULLFACE,

	// uniforms
	RWGL_ALPHAFUNC,
	RWGL_ALPHAREF,
	RWGL_FOG,
	RWGL_FOGSTART,
	RWGL_FOGEND,
	RWGL_FOGCOLOR,

	RWGL_NUM_STATES
};
static bool uniformStateDirty[RWGL_NUM_STATES];

struct GlState {
	bool32 blendEnable;
	uint32 srcblend, destblend;

	bool32 depthTest;
	uint32 depthFunc;

	uint32 depthMask;

	bool32 cullEnable;
	uint32 cullFace;
};
static GlState curGlState, oldGlState;

static int32 activeTexture;
static uint32 boundTexture[MAXNUMSTAGES];

static uint32 currentFramebuffer;

static uint32 blendMap[] = {
	GL_ZERO,	// actually invalid
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA_SATURATE,
};

/*
 * GL state cache
 */

void
setGlRenderState(uint32 state, uint32 value)
{
	switch(state){
	case RWGL_BLEND: curGlState.blendEnable = value; break;
	case RWGL_SRCBLEND: curGlState.srcblend = value; break;
	case RWGL_DESTBLEND: curGlState.destblend = value; break;
	case RWGL_DEPTHTEST: curGlState.depthTest = value; break;
	case RWGL_DEPTHFUNC: curGlState.depthFunc = value; break;
	case RWGL_DEPTHMASK: curGlState.depthMask = value; break;
	case RWGL_CULL: curGlState.cullEnable = value; break;
	case RWGL_CULLFACE: curGlState.cullFace = value; break;
	}
}

void
flushGlRenderState(void)
{
	if(oldGlState.blendEnable != curGlState.blendEnable){
		oldGlState.blendEnable = curGlState.blendEnable;
		(oldGlState.blendEnable ? glEnable : glDisable)(GL_BLEND);
	}

	if(oldGlState.srcblend != curGlState.srcblend ||
	   oldGlState.destblend != curGlState.destblend){
		oldGlState.srcblend = curGlState.srcblend;
		oldGlState.destblend = curGlState.destblend;
		glBlendFunc(oldGlState.srcblend, oldGlState.destblend);
	}

	if(oldGlState.depthTest != curGlState.depthTest){
		oldGlState.depthTest = curGlState.depthTest;
		(oldGlState.depthTest ? glEnable : glDisable)(GL_DEPTH_TEST);
	}
	if(oldGlState.depthFunc != curGlState.depthFunc){
		oldGlState.depthFunc = curGlState.depthFunc;
		glDepthFunc(oldGlState.depthFunc);
	}
	if(oldGlState.depthMask != curGlState.depthMask){
		oldGlState.depthMask = curGlState.depthMask;
		glDepthMask(oldGlState.depthMask);
	}

	if(oldGlState.cullEnable != curGlState.cullEnable){
		oldGlState.cullEnable = curGlState.cullEnable;
		(oldGlState.cullEnable ? glEnable : glDisable)(GL_CULL_FACE);
	}
	if(oldGlState.cullFace != curGlState.cullFace){
		oldGlState.cullFace = curGlState.cullFace;
		glCullFace(oldGlState.cullFace);
	}
}



void
setAlphaBlend(bool32 enable)
{
	if(rwStateCache.blendEnable != enable){
		rwStateCache.blendEnable = enable;
		setGlRenderState(RWGL_BLEND, enable);
	}
}

bool32
getAlphaBlend(void)
{
	return rwStateCache.blendEnable;
}

static void
setDepthTest(bool32 enable)
{
	if(rwStateCache.ztest != enable){
		rwStateCache.ztest = enable;
		if(rwStateCache.zwrite && !enable){
			// If we still want to write, enable but set mode to always
			setGlRenderState(RWGL_DEPTHTEST, true);
			setGlRenderState(RWGL_DEPTHFUNC, GL_ALWAYS);
		}else{
			setGlRenderState(RWGL_DEPTHTEST, rwStateCache.ztest);
			setGlRenderState(RWGL_DEPTHFUNC, GL_LEQUAL);
		}
	}
}

static void
setDepthWrite(bool32 enable)
{
	enable = enable ? GL_TRUE : GL_FALSE;
	if(rwStateCache.zwrite != enable){
		rwStateCache.zwrite = enable;
		if(enable && !rwStateCache.ztest){
			// Have to switch on ztest so writing can work
			setGlRenderState(RWGL_DEPTHTEST, true);
			setGlRenderState(RWGL_DEPTHFUNC, GL_ALWAYS);
		}
		setGlRenderState(RWGL_DEPTHMASK, rwStateCache.zwrite);
	}
}

static void
setAlphaTest(bool32 enable)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaTestEnable != enable){
		rwStateCache.alphaTestEnable = enable;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(alphaFunc != shaderfunc){
			alphaFunc = shaderfunc;
			uniformStateDirty[RWGL_ALPHAFUNC] = true;
			stateDirty = 1;
		}
	}
}

static void
setAlphaTestFunction(uint32 function)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaFunc != function){
		rwStateCache.alphaFunc = function;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(alphaFunc != shaderfunc){
			alphaFunc = shaderfunc;
			uniformStateDirty[RWGL_ALPHAFUNC] = true;
			stateDirty = 1;
		}
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		if(!rwStateCache.textureAlpha){
			setAlphaBlend(enable);
			setAlphaTest(enable);
		}
		rwStateCache.vertexAlpha = enable;
	}
}

static void
setActiveTexture(int32 n)
{
	if(activeTexture != n){
		activeTexture = n;
		glActiveTexture(GL_TEXTURE0+n);
	}
}

uint32
bindTexture(uint32 texid)
{
	uint32 prev = boundTexture[activeTexture];
	boundTexture[activeTexture] = texid;
	glBindTexture(GL_TEXTURE_2D, texid);
	return prev;
}

void
bindFramebuffer(uint32 fbo)
{
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	if(currentFramebuffer != fbo){
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		currentFramebuffer = fbo;
	}
}

// TODO: support mipmaps
static GLint filterConvMap_NoMIP[] = {
	0, GL_NEAREST, GL_LINEAR,
	   GL_NEAREST, GL_LINEAR,
	   GL_NEAREST, GL_LINEAR
};

static GLint addressConvMap[] = {
	0, GL_REPEAT, GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE
};

static void
setFilterMode(uint32 stage, int32 filter)
{
	if(rwStateCache.texstage[stage].filter != (Texture::FilterMode)filter){
		rwStateCache.texstage[stage].filter = (Texture::FilterMode)filter;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
			if(natras->filterMode != filter){
				setActiveTexture(stage);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterConvMap_NoMIP[filter]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterConvMap_NoMIP[filter]);
				natras->filterMode = filter;
			}
		}
	}
}

static void
setAddressU(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingU != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingU = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			if(natras->addressU == addressing){
				setActiveTexture(stage);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addressConvMap[addressing]);
				natras->addressU = addressing;
			}
		}
	}
}

static void
setAddressV(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingV != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingV = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
			if(natras->addressV == addressing){
				setActiveTexture(stage);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressConvMap[addressing]);
				natras->addressV = addressing;
			}
		}
	}
}

static void
setRasterStageOnly(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster){
		rwStateCache.texstage[stage].raster = raster;
		setActiveTexture(stage);
		if(raster){
			assert(raster->platform == PLATFORM_GL3);
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			bindTexture(natras->texid);

			rwStateCache.texstage[stage].filter = (rw::Texture::FilterMode)natras->filterMode;
			rwStateCache.texstage[stage].addressingU = (rw::Texture::Addressing)natras->addressU;
			rwStateCache.texstage[stage].addressingV = (rw::Texture::Addressing)natras->addressV;

			alpha = natras->hasAlpha;
		}else{
			bindTexture(whitetex);
			alpha = 0;
		}

		if(stage == 0){
			if(alpha != rwStateCache.textureAlpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					setAlphaBlend(alpha);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

static void
setRasterStage(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster){
		rwStateCache.texstage[stage].raster = raster;
		setActiveTexture(stage);
		if(raster){
			assert(raster->platform == PLATFORM_GL3);
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			bindTexture(natras->texid);
			uint32 filter = rwStateCache.texstage[stage].filter;
			uint32 addrU = rwStateCache.texstage[stage].addressingU;
			uint32 addrV = rwStateCache.texstage[stage].addressingV;
			if(natras->filterMode != filter){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterConvMap_NoMIP[filter]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterConvMap_NoMIP[filter]);
				natras->filterMode = filter;
			}
			if(natras->addressU != addrU){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addressConvMap[addrU]);
				natras->addressU = addrU;
			}
			if(natras->addressV != addrV){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressConvMap[addrV]);
				natras->addressV = addrV;
			}
			alpha = natras->hasAlpha;
		}else{
			bindTexture(whitetex);
			alpha = 0;
		}

		if(stage == 0){
			if(alpha != rwStateCache.textureAlpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					setAlphaBlend(alpha);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

void
setTexture(int32 stage, Texture *tex)
{
	if(tex == nil || tex->raster == nil){
		setRasterStage(stage, nil);
		return;
	}
	setRasterStageOnly(stage, tex->raster);
	setFilterMode(stage, tex->getFilter());
	setAddressU(stage, tex->getAddressU());
	setAddressV(stage, tex->getAddressV());
}

static void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER:
		setRasterStage(0, (Raster*)pvalue);
		break;
	case TEXTUREADDRESS:
		setAddressU(0, value);
		setAddressV(0, value);
		break;
	case TEXTUREADDRESSU:
		setAddressU(0, value);
		break;
	case TEXTUREADDRESSV:
		setAddressV(0, value);
		break;
	case TEXTUREFILTER:
		setFilterMode(0, value);
		break;
	case VERTEXALPHA:
		setVertexAlpha(value);
		break;
	case SRCBLEND:
		if(rwStateCache.srcblend != value){
			rwStateCache.srcblend = value;
			setGlRenderState(RWGL_SRCBLEND, blendMap[rwStateCache.srcblend]);
		}
		break;
	case DESTBLEND:
		if(rwStateCache.destblend != value){
			rwStateCache.destblend = value;
			setGlRenderState(RWGL_DESTBLEND, blendMap[rwStateCache.destblend]);
		}
		break;
	case ZTESTENABLE:
		setDepthTest(value);
		break;
	case ZWRITEENABLE:
		setDepthWrite(value);
		break;
	case FOGENABLE:
		if(rwStateCache.fogEnable != value){
			rwStateCache.fogEnable = value;
			uniformStateDirty[RWGL_FOG] = true;
			stateDirty = 1;
		}
		break;
	case FOGCOLOR:
		// no cache check here...too lazy
		RGBA c;
		c.red = value;
		c.green = value>>8;
		c.blue = value>>16;
		c.alpha = value>>24;
		convColor(&uniformState.fogColor, &c);
		uniformStateDirty[RWGL_FOGCOLOR] = true;
		stateDirty = 1;
		break;
	case CULLMODE:
		if(rwStateCache.cullmode != value){
			rwStateCache.cullmode = value;
			if(rwStateCache.cullmode == CULLNONE)
				setGlRenderState(RWGL_CULL, false);
			else{
				setGlRenderState(RWGL_CULL, true);
				setGlRenderState(RWGL_CULLFACE, rwStateCache.cullmode == CULLBACK ? GL_BACK : GL_FRONT);
			}
		}
		break;

	case ALPHATESTFUNC:
		setAlphaTestFunction(value);
		break;
	case ALPHATESTREF:
		if(alphaRef != value/255.0f){
			alphaRef = value/255.0f;
			uniformStateDirty[RWGL_ALPHAREF] = true;
			stateDirty = 1;
		}
		break;
	case GSALPHATEST:
		rwStateCache.gsalpha = value;
		break;
	case GSALPHATESTREF:
		rwStateCache.gsalpharef = value;
	}
}

static void*
getRenderState(int32 state)
{
	uint32 val;
	RGBA rgba;
	switch(state){
	case TEXTURERASTER:
		return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			val = rwStateCache.texstage[0].addressingU;
		else
			val = 0;	// invalid
		break;
	case TEXTUREADDRESSU:
		val = rwStateCache.texstage[0].addressingU;
		break;
	case TEXTUREADDRESSV:
		val = rwStateCache.texstage[0].addressingV;
		break;
	case TEXTUREFILTER:
		val = rwStateCache.texstage[0].filter;
		break;

	case VERTEXALPHA:
		val = rwStateCache.vertexAlpha;
		break;
	case SRCBLEND:
		val = rwStateCache.srcblend;
		break;
	case DESTBLEND:
		val = rwStateCache.destblend;
		break;
	case ZTESTENABLE:
		val = rwStateCache.ztest;
		break;
	case ZWRITEENABLE:
		val = rwStateCache.zwrite;
		break;
	case FOGENABLE:
		val = rwStateCache.fogEnable;
		break;
	case FOGCOLOR:
		convColor(&rgba, &uniformState.fogColor);
		val = RWRGBAINT(rgba.red, rgba.green, rgba.blue, rgba.alpha);
		break;
	case CULLMODE:
		val = rwStateCache.cullmode;
		break;

	case ALPHATESTFUNC:
		val = rwStateCache.alphaFunc;
		break;
	case ALPHATESTREF:
		val = (uint32)(alphaRef*255.0f);
		break;
	case GSALPHATEST:
		val = rwStateCache.gsalpha;
		break;
	case GSALPHATESTREF:
		val = rwStateCache.gsalpharef;
		break;
	default:
		val = 0;
	}
	return (void*)(uintptr)val;
}

static void
resetRenderState(void)
{	
	rwStateCache.alphaFunc = ALPHAGREATEREQUAL;
	alphaFunc = 0;
	alphaRef = 10.0f/255.0f;
	uniformState.fogDisable = 1.0f;
	uniformState.fogStart = 0.0f;
	uniformState.fogEnd = 0.0f;
	uniformState.fogRange = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	rwStateCache.gsalpha = 0;
	rwStateCache.gsalpharef = 128;
	stateDirty = 1;

	rwStateCache.vertexAlpha = 0;
	rwStateCache.textureAlpha = 0;
	rwStateCache.alphaTestEnable = 0;

	memset(&oldGlState, 0xFF, sizeof(oldGlState));

	rwStateCache.blendEnable = 0;
	setGlRenderState(RWGL_BLEND, false);
	rwStateCache.srcblend = BLENDSRCALPHA;
	rwStateCache.destblend = BLENDINVSRCALPHA;
	setGlRenderState(RWGL_SRCBLEND, blendMap[rwStateCache.srcblend]);
	setGlRenderState(RWGL_DESTBLEND, blendMap[rwStateCache.destblend]);

	rwStateCache.zwrite = GL_TRUE;
	setGlRenderState(RWGL_DEPTHMASK, rwStateCache.zwrite);

	rwStateCache.ztest = 1;
	setGlRenderState(RWGL_DEPTHTEST, true);
	setGlRenderState(RWGL_DEPTHFUNC, GL_LEQUAL);

	rwStateCache.cullmode = CULLNONE;
	setGlRenderState(RWGL_CULL, false);
	setGlRenderState(RWGL_CULLFACE, GL_BACK);

	activeTexture = -1;
	for(int i = 0; i < MAXNUMSTAGES; i++){
		setActiveTexture(i);
		bindTexture(whitetex);
	}
	setActiveTexture(0);
}

void
setWorldMatrix(Matrix *mat)
{
	convMatrix(&uniformObject.world, mat);
	objectDirty = 1;
}

int32
setLights(WorldLights *lightData)
{
	int i, n;
	Light *l;
	int32 bits;

	uniformObject.ambLight = lightData->ambient;

	bits = 0;

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	n = 0;
	for(i = 0; i < lightData->numDirectionals && i < 8; i++){
		l = lightData->directionals[i];
		uniformObject.lightParams[n].type = 1.0f;
		uniformObject.lightColor[n] = l->color;
		memcpy_neon(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
		bits |= VSLIGHT_POINT;
		n++;
		if(n >= MAX_LIGHTS)
			goto out;
	}

	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			uniformObject.lightParams[n].type = 2.0f;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy_neon(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			bits |= VSLIGHT_POINT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			uniformObject.lightParams[n].type = 3.0f;
			uniformObject.lightParams[n].minusCosAngle = l->minusCosAngle;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy_neon(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			memcpy_neon(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				uniformObject.lightParams[n].hardSpot = 0.0f;
			else
				uniformObject.lightParams[n].hardSpot = 1.0f;
			bits |= VSLIGHT_SPOT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		}
	}

	uniformObject.lightParams[n].type = 0.0f;
out:
	objectDirty = 1;
	return bits;
}

void
setProjectionMatrix(float32 *mat)
{
	memcpy_neon(&uniformScene.proj, mat, 64);
	sceneDirty = 1;
}

void
setViewMatrix(float32 *mat)
{
	memcpy_neon(&uniformScene.view, mat, 64);
	sceneDirty = 1;
}

Shader *lastShaderUploaded;

#define U(i) currentShader->uniformLocations[i]

void
setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops)
{
	bool force = lastShaderUploaded != currentShader;
	if(force || !equal(shaderState.matColor, color)){
		rw::RGBAf col;
		convColor(&col, &color);
		glUniform4fv(U(u_matColor), 1, (GLfloat*)&col);
		shaderState.matColor = color;
	}

	if(force ||
	   shaderState.surfProps.ambient != surfaceprops.ambient ||
	   shaderState.surfProps.specular != surfaceprops.specular ||
	   shaderState.surfProps.diffuse != surfaceprops.diffuse){
		float surfProps[4];
		surfProps[0] = surfaceprops.ambient;
		surfProps[1] = surfaceprops.specular;
		surfProps[2] = surfaceprops.diffuse;
		surfProps[3] = 0.0f;
		glUniform4fv(U(u_surfProps), 1, surfProps);
		shaderState.surfProps = surfaceprops;
	}
}

void
flushCache(void)
{
	flushGlRenderState();

	// TODO: this is probably a stupid way to do it without UBOs
	/*if(lastShaderUploaded != currentShader){
		lastShaderUploaded = currentShader;
		objectDirty = 1;
		sceneDirty = 1;
		stateDirty = 1;

		int i;
		for(i = 0; i < RWGL_NUM_STATES; i++)
			uniformStateDirty[i] = true;
	}*/

	//if(sceneDirty){
		glUniformMatrix4fv(U(u_proj), 1, 0, uniformScene.proj);
		glUniformMatrix4fv(U(u_view), 1, 0, uniformScene.view);
	//	sceneDirty = 0;
	//}

	//if(objectDirty){
		glUniformMatrix4fv(U(u_world), 1, 0, (float*)&uniformObject.world);
		glUniform4fv(U(u_ambLight), 1, (float*)&uniformObject.ambLight);
		glUniform4fv(U(u_lightParams), MAX_LIGHTS, (float*)uniformObject.lightParams);
		glUniform4fv(U(u_lightPosition), MAX_LIGHTS, (float*)uniformObject.lightPosition);
		glUniform4fv(U(u_lightDirection), MAX_LIGHTS, (float*)uniformObject.lightDirection);
		glUniform4fv(U(u_lightColor), MAX_LIGHTS, (float*)uniformObject.lightColor);
	//	objectDirty = 0;
	//}

//	if(stateDirty){

		uniformState.fogDisable = rwStateCache.fogEnable ? 0.0f : 1.0f;
		uniformState.fogStart = rwStateCache.fogStart;
		uniformState.fogEnd = rwStateCache.fogEnd;
		uniformState.fogRange = 1.0f/(rwStateCache.fogStart - rwStateCache.fogEnd);

		//if(uniformStateDirty[RWGL_ALPHAFUNC] || uniformStateDirty[RWGL_ALPHAREF]){
			switch(alphaFunc){
			case ALPHAALWAYS:
			default:
				glUniform2f(U(u_alphaRef), -1000.0f, 1000.0f);
				break;
			case ALPHAGREATEREQUAL:
				glUniform2f(U(u_alphaRef), alphaRef, 1000.0f);
				break;
			case ALPHALESS:
				glUniform2f(U(u_alphaRef), -1000.0f, alphaRef);
				break;
			}
			//uniformStateDirty[RWGL_ALPHAFUNC] = false;
			//uniformStateDirty[RWGL_ALPHAREF] = false;
		//}

		/*if(uniformStateDirty[RWGL_FOG] ||
		   uniformStateDirty[RWGL_FOGSTART] ||
		   uniformStateDirty[RWGL_FOGEND]){*/
			float fogData[4] = {
				uniformState.fogStart,
				uniformState.fogEnd,
				uniformState.fogRange,
				uniformState.fogDisable
			};
			glUniform4fv(U(u_fogData), 1, fogData);
			uniformStateDirty[RWGL_FOG] = false;
			uniformStateDirty[RWGL_FOGSTART] = false;
			uniformStateDirty[RWGL_FOGEND] = false;
		//}

		//if(uniformStateDirty[RWGL_FOGCOLOR]){
			glUniform4fv(U(u_fogColor), 1, (float*)&uniformState.fogColor);
			uniformStateDirty[RWGL_FOGCOLOR] = false;
		//}
}

static void
setFrameBuffer(Camera *cam)
{
	Raster *fbuf = cam->frameBuffer;
	Raster *zbuf = cam->zBuffer;
	assert(fbuf);

	Gl3Raster *natfb = PLUGINOFFSET(Gl3Raster, fbuf, nativeRasterOffset);
	Gl3Raster *natzb = PLUGINOFFSET(Gl3Raster, zbuf, nativeRasterOffset);
	assert(fbuf->type == Raster::CAMERA || fbuf->type == Raster::CAMERATEXTURE);

	// Have to make sure depth buffer is attached to FB's fbo
	bindFramebuffer(natfb->fbo);
	/*if(zbuf){
		if(natfb->fboMate == zbuf){
			// all good
			assert(natzb->fboMate == fbuf);
		}else{
			if(natzb->fboMate){
				// have to detatch from fbo first!
				Gl3Raster *oldfb = PLUGINOFFSET(Gl3Raster, natzb->fboMate, nativeRasterOffset);
				if(oldfb->fbo){
					bindFramebuffer(oldfb->fbo);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
					bindFramebuffer(natfb->fbo);
				}
				oldfb->fboMate = nil;
			}
			natfb->fboMate = zbuf;
			natzb->fboMate = fbuf;
			if(natfb->fbo)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, natzb->texid, 0);
		}
	}else{
		// remove z-buffer
		if(natfb->fboMate && natfb->fbo)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		natfb->fboMate = nil;
	}*/
}

static void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];
	// View Matrix
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  =  -inv.at.x;
	view[9]  =   inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy_neon(&cam->devView, &view, sizeof(RawMatrix));
	setViewMatrix(view);

	// Projection Matrix
	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = (cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 1.0f;

		proj[14] = -2.0f*cam->nearPlane*cam->farPlane*invz;
		proj[15] = 0.0f;
	}else{
		proj[10] = -(cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 0.0f;

		proj[14] = 2.0f*invz;
		proj[15] = 1.0f;
	}
	memcpy_neon(&cam->devProj, &proj, sizeof(RawMatrix));
	setProjectionMatrix(proj);

	if(rwStateCache.fogStart != cam->fogPlane){
		rwStateCache.fogStart = cam->fogPlane;
		uniformStateDirty[RWGL_FOGSTART] = true;
		stateDirty = 1;
	}
	if(rwStateCache.fogEnd != cam->farPlane){
		rwStateCache.fogEnd = cam->farPlane;
		uniformStateDirty[RWGL_FOGEND] = true;
		stateDirty = 1;
	}

	setFrameBuffer(cam);

	int w, h;
	if(cam->frameBuffer->type == Raster::CAMERA){
#ifdef LIBRW_SDL2
		SDL_GetWindowSize(glGlobals.window, &w, &h);
#else
		w = 960;
		h = 544;
#endif
	}else{
		w = cam->frameBuffer->width;
		h = cam->frameBuffer->height;
	}

	if(w != glGlobals.presentWidth || h != glGlobals.presentHeight){
		glViewport(0, 0, w, h);
		glGlobals.presentWidth = w;
		glGlobals.presentHeight = h;
	}
}

void log2file(const char *format, ...) {
	__gnuc_va_list arg;
	int done;
	va_start(arg, format);
	char msg[512];
	done = vsprintf(msg, format, arg);
	va_end(arg);
	int i;
	sprintf(msg, "%s\n", msg);
	FILE *log = fopen("ux0:/data/librw.log", "a+");
	if (log != NULL) {
		fwrite(msg, 1, strlen(msg), log);
		fclose(log);
	}
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	RGBAf colf;
	int mask;

	setFrameBuffer(cam);

	convColor(&colf, col);
	glClearColor(colf.red, colf.green, colf.blue, colf.alpha);
	mask = 0;
	if(mode & Camera::CLEARIMAGE)
		mask |= GL_COLOR_BUFFER_BIT;
	if(mode & Camera::CLEARZ)
		mask |= GL_DEPTH_BUFFER_BIT;
	glDepthMask(GL_TRUE);
	glClear((GLbitfield)mask);
	glDepthMask(rwStateCache.zwrite);
}

static void
showRaster(Raster *raster, uint32 flags)
{
	// TODO: do this properly!
#ifdef LIBRW_SDL2
	SDL_GL_SwapWindow(glGlobals.window);
#else
	if(flags & Raster::FLIPWAITVSYNCH)
		vglWaitVblankStart(GL_TRUE);
	else
		vglWaitVblankStart(GL_FALSE);
	vglStopRendering();
	vglStartRendering();
	gVertexBuffer = gVertexBufferPtr;
	gVertexBufferIm2D = gVertexBufferPtr + 0xC00000;
	gIndices = gIndicesPtr;
	gIndicesIm2D = gIndicesPtr + 0x300000;
#endif
	//log2file("showRaster called");
	
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	Gl3Raster *natdst = PLUGINOFFSET(Gl3Raster, dst, nativeRasterOffset);
	Gl3Raster *natsrc = PLUGINOFFSET(Gl3Raster, src, nativeRasterOffset);

	switch(dst->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		switch(src->type){
		case Raster::CAMERA:
			setActiveTexture(0);
			glBindTexture(GL_TEXTURE_2D, natdst->texid);
			/*glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, (dst->height-src->height)-y,
				0, 0, src->width, src->height);*/
			glBindTexture(GL_TEXTURE_2D, boundTexture[0]);
			return 1;
		}
		break;
	}
	return 0;
}

#ifdef LIBRW_SDL2
static int
openSDL2(EngineOpenParams *openparams)
{
	if (!openparams){
		RWERROR((ERR_GENERAL, "openparams invalid"));
		return 0;
	}

	GLenum status;
	SDL_Window *win;
	SDL_GLContext ctx;

	/* Init SDL */
	if(SDL_InitSubSystem(SDL_INIT_VIDEO)){
		RWERROR((ERR_ENGINEOPEN, SDL_GetError()));
		return 0;
	}
	SDL_ClearHints();
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
	if (openparams->fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;
	win = SDL_CreateWindow(openparams->windowtitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, openparams->width, openparams->height, flags);
	if(win == nil){
		RWERROR((ERR_ENGINEOPEN, SDL_GetError()));
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 0;
	}
	ctx = SDL_GL_CreateContext(win);

#ifndef LIBRW_GLAD
	/* Init GLEW */
	glewExperimental = GL_TRUE;
	status = glewInit();
	if(status != GLEW_OK){
        	RWERROR((ERR_ENGINEOPEN, glewGetErrorString(status)));
        	SDL_GL_DeleteContext(ctx);
        	SDL_DestroyWindow(win);
        	SDL_QuitSubSystem(SDL_INIT_VIDEO);
        	return 0;
	}
	if(!GLEW_VERSION_3_3){
		RWERROR((ERR_VERSION, "OpenGL 3.3 needed"));
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 0;
	}
#else
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        	RWERROR((ERR_ENGINEOPEN, "gladLoadGLLoader failed"));
        	SDL_GL_DeleteContext(ctx);
        	SDL_DestroyWindow(win);
        	SDL_QuitSubSystem(SDL_INIT_VIDEO);
        	return 0;
	}
	if(!GLAD_GL_VERSION_3_3) {
		RWERROR((ERR_VERSION, "OpenGL 3.3 needed"));
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 0;
	}
#endif
	glGlobals.window = win;
	glGlobals.glcontext = ctx;
	*openparams->window = win;
	return 1;
}

static int
closeSDL2(void)
{
	SDL_GL_DeleteContext(glGlobals.glcontext);
	SDL_DestroyWindow(glGlobals.window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 1;
}
#else

static void
addVideoMode(/*const GLFWvidmode *mode*/)
{
}

static void
makeVideoModeList(void)
#ifndef PSP2
{
	int i, num;
	const GLFWvidmode *modes;

	modes = glfwGetVideoModes(glGlobals.monitor, &num);
	rwFree(glGlobals.modes);
	glGlobals.modes = rwNewT(DisplayMode, num+1, ID_DRIVER | MEMDUR_EVENT);

	glGlobals.modes[0].mode = *glfwGetVideoMode(glGlobals.monitor);
	glGlobals.modes[0].flags = 0;
	glGlobals.numModes = 1;

	for(i = 0; i < num; i++)
		addVideoMode(&modes[i]);

	for(i = 0; i < glGlobals.numModes; i++){
		num = glGlobals.modes[i].mode.redBits +
			glGlobals.modes[i].mode.greenBits +
			glGlobals.modes[i].mode.blueBits;
		// set depth to power of two
		for(glGlobals.modes[i].depth = 1; glGlobals.modes[i].depth < num; glGlobals.modes[i].depth <<= 1);
	}
}
#else
// stub video modes manually: 544p
{
}
#endif

static int
openGLFW(EngineOpenParams *openparams)
{
	glGlobals.winWidth = openparams->width;
	glGlobals.winHeight = openparams->height;
	glGlobals.winTitle = openparams->windowtitle;

	vglInitExtended(0x1400000, 960, 544, 0x100000, SCE_GXM_MULTISAMPLE_4X);
	vglUseVram(GL_TRUE);
	vglStartRendering();

	makeVideoModeList();

	return 1;
}

static int
closeGLFW(void)
{
	return 1;
}

static void
glfwerr(int error, const char *desc)
{
	fprintf(stderr, "GLFW Error: %s\n", desc);
}

static int
startGLFW(void)
{
	GLenum status;

	printf("version %s\n", glGetString(GL_VERSION));

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	printf("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));

	glGlobals.presentWidth = 0;
	glGlobals.presentHeight = 0;
	return 1;
}

static int
stopGLFW(void)
{
	return 1;
}
#endif

static int
initOpenGL(void)
{
#ifndef RW_GL_USE_UBOS
	u_alphaRef = registerUniform("u_alphaRef");
	u_fogData = registerUniform("u_fogData");
//	u_fogStart = registerUniform("u_fogStart");
//	u_fogEnd = registerUniform("u_fogEnd");
//	u_fogRange = registerUniform("u_fogRange");
//	u_fogDisable = registerUniform("u_fogDisable");
	u_fogColor = registerUniform("u_fogColor");
	u_proj = registerUniform("u_proj");
	u_view = registerUniform("u_view");
	u_world = registerUniform("u_world");
	u_ambLight = registerUniform("u_ambLight");
	u_lightParams = registerUniform("u_lightParams");
	u_lightPosition = registerUniform("u_lightPosition");
	u_lightDirection = registerUniform("u_lightDirection");
	u_lightColor = registerUniform("u_lightColor");
	lastShaderUploaded = nil;
#else
	registerBlock("Scene");
	registerBlock("Object");
	registerBlock("State");
#endif
	u_matColor = registerUniform("u_matColor");
	u_surfProps = registerUniform("u_surfProps");

	glClearColor(0.25, 0.25, 0.25, 1.0);

	byte whitepixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};

	glGenTextures(1, &whitetex);
	glBindTexture(GL_TEXTURE_2D, whitetex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, &whitepixel);

	resetRenderState();

	gVertexBufferPtr = (float*)malloc(0x1800000);
	gIndicesPtr = (uint16_t*)malloc(0x600000);
	gConstIndices = (uint16_t*)malloc(sizeof(uint16_t) * 512);
	gVertexBuffer = gVertexBufferPtr;
	gVertexBufferIm2D = gVertexBufferPtr + 0xC00000;
	gIndices = gIndicesPtr;
	gIndicesIm2D = gIndicesPtr + 0x300000;
	
	for (uint16_t i = 0; i < 512; i++) {
		gConstIndices[i] = i;
	}
	
#ifdef RW_GLES2
#include "gl2_shaders/default_vs_gl2.inc"
#include "gl2_shaders/simple_fs_gl2.inc"
#else
#include "shaders/default_vs_gl3.inc"
#include "shaders/simple_fs_gl3.inc"
#endif
	const char *vs[] = { header_vert_src, default_vert_src, nil };
	const char *fs[] = { header_frag_src, simple_frag_src, nil };
	defaultShader = Shader::create(vs, fs, false);
	assert(defaultShader);

	openIm2D();
	openIm3D();

	return 1;
}

static int
termOpenGL(void)
{
	closeIm3D();
	closeIm2D();
	return 1;
}

static int
finalizeOpenGL(void)
{
	return 1;
}

#ifdef LIBRW_SDL2
static int
deviceSystemSDL2(DeviceReq req, void *arg, int32 n)
{
	switch(req){
	case DEVICEOPEN:
		return openSDL2((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeSDL2();

	case DEVICEINIT:
		return initOpenGL();
	case DEVICETERM:
		return termOpenGL();

	// TODO: implement subsystems and video modes

	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

#else

static int
deviceSystemGLFW(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		return openGLFW((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeGLFW();

	case DEVICEINIT:
		return startGLFW() && initOpenGL();
	case DEVICETERM:
		return termOpenGL() && stopGLFW();

	case DEVICEFINALIZE:
		return finalizeOpenGL();


	case DEVICEGETNUMSUBSYSTEMS:
		return 1;

	case DEVICEGETCURRENTSUBSYSTEM:
		return 0;

	case DEVICESETSUBSYSTEM:
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		strncpy(((SubSystemInfo*)arg)->name, "PSVita Screen Stub", sizeof(SubSystemInfo::name));
		return 1;


	case DEVICEGETNUMVIDEOMODES:
		return 1;

	case DEVICEGETCURRENTVIDEOMODE:
		return 0;

	case DEVICESETVIDEOMODE:
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		rwmode = (VideoMode*)arg;
		rwmode->width = 960;
		rwmode->height = 544;
		rwmode->depth = 32;
		rwmode->flags = VIDEOMODEEXCLUSIVE;
		return 1;

	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

#endif

Device renderdevice = {
	-1.0f, 1.0f,
	gl3::beginUpdate,
	null::endUpdate,
	gl3::clearCamera,
	gl3::showRaster,
	gl3::rasterRenderFast,
	gl3::setRenderState,
	gl3::getRenderState,
	gl3::im2DRenderLine,
	gl3::im2DRenderTriangle,
	gl3::im2DRenderPrimitive,
	gl3::im2DRenderIndexedPrimitive,
	gl3::im3DTransform,
	gl3::im3DRenderPrimitive,
	gl3::im3DRenderIndexedPrimitive,
	gl3::im3DEnd,
#ifdef LIBRW_SDL2
	gl3::deviceSystemSDL2
#else
	gl3::deviceSystemGLFW
#endif
};

}
}

#endif

