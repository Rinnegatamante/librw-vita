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

extern float *gVertexBuffer;

namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

#define U(i) currentShader->uniformLocations[i]

static Shader *skinShader;
static int32 u_boneMatrices;

void
skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	AttribDesc *attribs, *a;

	bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	bool hasNormals = !!(geo->flags & Geometry::NORMALS);

	if(!reinstance){
		AttribDesc tmpAttribs[14];
		uint32 stride;

		//
		// Create attribute descriptions
		//
		a = tmpAttribs;
		stride = 0;

		// Positions
		a->index = ATTRIB_POS;
		a->size = 3;
		a->type = GL_FLOAT;
		a->normalized = GL_FALSE;
		a->offset = stride;
		stride += 12;
		a++;

		// Normals
		// TODO: compress
		if(hasNormals){
			a->index = ATTRIB_NORMAL;
			a->size = 3;
			a->type = GL_FLOAT;
			a->normalized = GL_FALSE;
			a->offset = stride;
			stride += 12;
			a++;
		}

		// Prelighting
		if(isPrelit){
			a->index = ATTRIB_COLOR;
			a->size = 4;
			a->type = GL_UNSIGNED_BYTE;
			a->normalized = GL_TRUE;
			a->offset = stride;
			stride += 4;
			a++;
		}

		// Texture coordinates
		for(int32 n = 0; n < geo->numTexCoordSets; n++){
			a->index = ATTRIB_TEXCOORDS0+n;
			a->size = 2;
			a->type = GL_FLOAT;
			a->normalized = GL_FALSE;
			a->offset = stride;
			stride += 8;
			a++;
		}

		// Weights
		a->index = ATTRIB_WEIGHTS;
		a->size = 4;
		a->type = GL_FLOAT;
		a->normalized = GL_FALSE;
		a->offset = stride;
		stride += 16;
		a++;

		// Indices
		a->index = ATTRIB_INDICES;
		a->size = 4;
		a->type = GL_UNSIGNED_BYTE;
		a->normalized = GL_FALSE;
		a->offset = stride;
		stride += 4;
		a++;

		header->numAttribs = a - tmpAttribs;
		for(a = tmpAttribs; a != &tmpAttribs[header->numAttribs]; a++)
			a->stride = stride;
		header->attribDesc = rwNewT(AttribDesc, header->numAttribs, MEMDUR_EVENT | ID_GEOMETRY);
		memcpy_neon(header->attribDesc, tmpAttribs,
		       header->numAttribs*sizeof(AttribDesc));

		//
		// Allocate vertex buffer
		//
		header->vertexBuffer = rwNewT(uint8, header->totalNumVertex*stride, MEMDUR_EVENT | ID_GEOMETRY);
		assert(header->vbo == 0);
		glGenBuffers(1, &header->vbo);
	}

	Skin *skin = Skin::get(geo);
	attribs = header->attribDesc;

	//
	// Fill vertex buffer
	//

	uint8 *verts = header->vertexBuffer;

	// Positions
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKVERTICES){
		for(a = attribs; a->index != ATTRIB_POS; a++)
			;
		instV3d(VERT_FLOAT3, verts + a->offset,
			geo->morphTargets[0].vertices,
			header->totalNumVertex, a->stride);
	}

	// Normals
	if(hasNormals && (!reinstance || geo->lockedSinceInst&Geometry::LOCKNORMALS)){
		for(a = attribs; a->index != ATTRIB_NORMAL; a++)
			;
		instV3d(VERT_FLOAT3, verts + a->offset,
			geo->morphTargets[0].normals,
			header->totalNumVertex, a->stride);
	}

	// Prelighting
	if(isPrelit && (!reinstance || geo->lockedSinceInst&Geometry::LOCKPRELIGHT)){
		for(a = attribs; a->index != ATTRIB_COLOR; a++)
			;
		instColor(VERT_RGBA, verts + a->offset,
			  geo->colors,
			  header->totalNumVertex, a->stride);
	}

	// Texture coordinates
	for(int32 n = 0; n < geo->numTexCoordSets; n++){
		if(!reinstance || geo->lockedSinceInst&(Geometry::LOCKTEXCOORDS<<n)){
			for(a = attribs; a->index != ATTRIB_TEXCOORDS0+n; a++)
				;
			instTexCoords(VERT_FLOAT2, verts + a->offset,
				geo->texCoords[n],
				header->totalNumVertex, a->stride);
		}
	}

	// Weights
	if(!reinstance){
		for(a = attribs; a->index != ATTRIB_WEIGHTS; a++)
			;
		float *w = skin->weights;
		instV4d(VERT_FLOAT4, verts + a->offset,
			(V4d*)w,
			header->totalNumVertex, a->stride);
	}

	// Indices
	if(!reinstance){
		for(a = attribs; a->index != ATTRIB_INDICES; a++)
			;
		// not really colors of course but what the heck
		instColor(VERT_RGBA, verts + a->offset,
			  (RGBA*)skin->indices,
			  header->totalNumVertex, a->stride);
	}

	memcpy_neon(gVertexBuffer, header->vertexBuffer, header->totalNumVertex*attribs[0].stride);
}

void
skinUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

static float skinMatrices[64*16];

void
uploadSkinMatrices(Atomic *a)
{
	int i;
	Skin *skin = Skin::get(a->geometry);
	Matrix *m = (Matrix*)skinMatrices;
	HAnimHierarchy *hier = Skin::getHierarchy(a);

	if(hier){
		Matrix *invMats = (Matrix*)skin->inverseMatrices;
		Matrix tmp;

		assert(skin->numBones == hier->numNodes);
		if(hier->flags & HAnimHierarchy::LOCALSPACEMATRICES){
			for(i = 0; i < hier->numNodes; i++){
				invMats[i].flags = 0;
				Matrix::mult(m, &invMats[i], &hier->matrices[i]);
				m++;
			}
		}else{
			Matrix invAtmMat;
			Matrix::invert(&invAtmMat, a->getFrame()->getLTM());
			for(i = 0; i < hier->numNodes; i++){
				invMats[i].flags = 0;
				Matrix::mult(&tmp, &hier->matrices[i], &invAtmMat);
				Matrix::mult(m, &invMats[i], &tmp);
				m++;
			}
		}
	}else{
		for(i = 0; i < skin->numBones; i++){
			m->setIdentity();
			m++;
		}
	}
	glUniformMatrix4fv(U(u_boneMatrices), 64, GL_FALSE,
	                   (GLfloat*)skinMatrices);
}

void
skinRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	Material *m;

	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	setAttribPointers(header->attribDesc, header->numAttribs);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	skinShader->use();

	uploadSkinMatrices(atomic);

	while(n--){
		m = inst->material;

		setMaterial(m->color, m->surfaceProps);

		setTexture(0, m->texture);

		rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

		drawInst(header, inst);
		inst++;
	}
#ifndef RW_GL_USE_VAOS
	disableAttribPointers(header->attribDesc, header->numAttribs);
#endif
}

static void*
skinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_GL3] = makeSkinPipeline();

#ifdef RW_GLES2
#include "gl2_shaders/simple_fs_gl2.inc"
#include "gl2_shaders/skin_gl2.inc"
#else
#include "shaders/simple_fs_gl3.inc"
#include "shaders/skin_gl3.inc"
#endif
	const char *vs[] = { header_vert_src, skin_vert_src, nil };
	const char *fs[] = { header_frag_src, simple_frag_src, nil };
	skinShader = Shader::create(vs, fs, false);
	assert(skinShader);

	return o;
}

static void*
skinClose(void *o, int32, int32)
{
	((ObjPipeline*)skinGlobals.pipelines[PLATFORM_GL3])->destroy();
	skinGlobals.pipelines[PLATFORM_GL3] = nil;

	skinShader->destroy();
	skinShader = nil;

	return o;
}

void
initSkin(void)
{
	u_boneMatrices = registerUniform("u_boneMatrices");

	Driver::registerPlugin(PLATFORM_GL3, 0, ID_SKIN,
	                       skinOpen, skinClose);
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = skinInstanceCB;
	pipe->uninstanceCB = skinUninstanceCB;
	pipe->renderCB = skinRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

#else

void initSkin(void) { }

#endif

}
}

