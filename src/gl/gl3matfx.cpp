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
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwgl3.h"
#include "rwgl3shader.h"
#include "rwgl3plg.h"

#include "rwgl3impl.h"

namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

#define U(i) currentShader->uniformLocations[i]

static Shader *envShader;
static int32 u_texMatrix;
static int32 u_fxparams;
static int32 u_colorClamp;

void
matfxDefaultRender(InstanceDataHeader *header, InstanceData *inst)
{
	Material *m = inst->material;

	defaultShader->use();
	
	rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

	flushCache();

	setMaterial(m->color, m->surfaceProps);

	setTexture(0, m->texture);

	drawInst(header, inst);
}

static Frame *lastEnvFrame;

static RawMatrix normal2texcoord = {
	{ 0.5f,  0.0f, 0.0f }, 0.0f,
	{ 0.0f, -0.5f, 0.0f }, 0.0f,
	{ 0.0f,  0.0f, 1.0f }, 0.0f,
	{ 0.5f,  0.5f, 0.0f }, 1.0f
};

void
uploadEnvMatrix(Frame *frame)
{
	Matrix invMat;
	if(frame == nil)
		frame = engine->currentCamera->getFrame();

	// cache the matrix across multiple meshes
	static RawMatrix envMtx;
	if(frame != lastEnvFrame){
		lastEnvFrame = frame;

		RawMatrix invMtx;
		Matrix::invert(&invMat, frame->getLTM());
		convMatrix(&invMtx, &invMat);
		invMtx.pos.set(0.0f, 0.0f, 0.0f);
		RawMatrix::mult(&envMtx, &invMtx, &normal2texcoord);
	}
	glUniformMatrix4fv(U(u_texMatrix), 1, GL_FALSE, (float*)&envMtx);
}

void
matfxEnvRender(InstanceDataHeader *header, InstanceData *inst, MatFX::Env *env)
{
	Material *m;
	m = inst->material;

	if(env->tex == nil || env->coefficient == 0.0f){
		matfxDefaultRender(header, inst);
		return;
	}

	envShader->use();
	
	rw::SetRenderState(VERTEXALPHA, 1);
	rw::SetRenderState(SRCBLEND, BLENDONE);
	
	flushCache();

	setTexture(0, m->texture);
	setTexture(1, env->tex);
	uploadEnvMatrix(env->frame);

	setMaterial(m->color, m->surfaceProps);

	float fxparams[2];
	fxparams[0] = env->coefficient;
	fxparams[1] = env->fbAlpha ? 0.0f : 1.0f;

	glUniform2fv(U(u_fxparams), 1, fxparams);
	static float zero[4];
	static float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	// This clamps the vertex color below. With it we can achieve both PC and PS2 style matfx
	if(MatFX::modulateEnvMap)
		glUniform4fv(U(u_colorClamp), 1, zero);
	else
		glUniform4fv(U(u_colorClamp), 1, one);

	drawInst(header, inst);

	rw::SetRenderState(SRCBLEND, BLENDSRCALPHA);
}

void
matfxRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	setAttribPointers(header->attribDesc, header->numAttribs);

	lastEnvFrame = nil;

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		MatFX *matfx = MatFX::get(inst->material);

		if(matfx == nil)
			matfxDefaultRender(header, inst);
		else switch(matfx->type){
		case MatFX::ENVMAP:
			matfxEnvRender(header, inst, &matfx->fx[0].env);
			break;
		default:
			matfxDefaultRender(header, inst);
			break;
		}
		inst++;
	}
#ifndef RW_GL_USE_VAOS
	disableAttribPointers(header->attribDesc, header->numAttribs);
#endif
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = matfxRenderCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

static void*
matfxOpen(void *o, int32, int32)
{
	matFXGlobals.pipelines[PLATFORM_GL3] = makeMatFXPipeline();

#ifdef RW_GLES2
#include "gl2_shaders/matfx_gl2.inc"
#else
#include "shaders/matfx_gl3.inc"
#endif
	const char *vs[] = { header_vert_src, matfx_env_vert_src, nil };
	const char *fs[] = { header_frag_src, matfx_env_frag_src, nil };
	envShader = Shader::create(vs, fs, false);
	assert(envShader);

	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
	((ObjPipeline*)matFXGlobals.pipelines[PLATFORM_GL3])->destroy();
	matFXGlobals.pipelines[PLATFORM_GL3] = nil;

	envShader->destroy();
	envShader = nil;

	return o;
}

void
initMatFX(void)
{
	u_texMatrix = registerUniform("u_texMatrix");
	u_fxparams = registerUniform("u_fxparams");
	u_colorClamp = registerUniform("u_colorClamp");

	Driver::registerPlugin(PLATFORM_GL3, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}

#else

void initMatFX(void) { }

#endif

}
}

