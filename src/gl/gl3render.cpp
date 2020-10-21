#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

namespace rw {
namespace gl3 {

#define MAX_LIGHTS 

void
drawInst_simple(InstanceDataHeader *header, InstanceData *inst)
{
	vglIndexPointerMapped((uint8_t*)header->indexBuffer + inst->offset);
	vglVertexAttribPointerMapped(0, header->vertexBuffer);
	vglDrawObjects(header->primType, inst->numIndex, GL_FALSE);
}

// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void
drawInst_GSemu(InstanceDataHeader *header, InstanceData *inst)
{
	uint32 hasAlpha;
	int alphafunc, alpharef, gsalpharef;
	int zwrite;
	hasAlpha = getAlphaBlend();
	if(hasAlpha){
		zwrite = rw::GetRenderState(rw::ZWRITEENABLE);
		alphafunc = rw::GetRenderState(rw::ALPHATESTFUNC);
		if(zwrite){
			alpharef = rw::GetRenderState(rw::ALPHATESTREF);
			gsalpharef = rw::GetRenderState(rw::GSALPHATESTREF);

			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAGREATEREQUAL);
			SetRenderState(rw::ALPHATESTREF, gsalpharef);
			drawInst_simple(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHALESS);
			SetRenderState(rw::ZWRITEENABLE, 0);
			drawInst_simple(header, inst);
			SetRenderState(rw::ZWRITEENABLE, 1);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
			SetRenderState(rw::ALPHATESTREF, alpharef);
		}else{
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAALWAYS);
			drawInst_simple(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
		}
	}else
		drawInst_simple(header, inst);
}

void
drawInst(InstanceDataHeader *header, InstanceData *inst)
{
	if(rw::GetRenderState(rw::GSALPHATEST))
		drawInst_GSemu(header, inst);
	else
		drawInst_simple(header, inst);
}


void
setAttribPointers(AttribDesc *attribDescs, int32 numAttribs)
{
	//vglVertexAttribPointerMapped(0, gVertexBuffer);
}

void
disableAttribPointers(AttribDesc *attribDescs, int32 numAttribs)
{
	/*AttribDesc *a;
	for(a = attribDescs; a != &attribDescs[numAttribs]; a++)
		glDisableVertexAttribArray(a->index);*/
}

int32
lightingCB(Atomic *atomic)
{
	WorldLights lightData;
	Light *directionals[8];
	Light *locals[8];
	lightData.directionals = directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;

	if(atomic->geometry->flags & rw::Geometry::LIGHT){
		((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);
		if((atomic->geometry->flags & rw::Geometry::NORMALS) == 0){
			// Get rid of lights that need normals when we don't have any
			lightData.numDirectionals = 0;
			lightData.numLocals = 0;
		}
		return setLights(&lightData);
	}else{
		memset(&lightData, 0, sizeof(lightData));
		return setLights(&lightData);
	}
}

#define U(i) currentShader->uniformLocations[i]

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	Material *m;

	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	setAttribPointers(header->attribDesc, header->numAttribs);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	defaultShader->use();

	while(n--){
		m = inst->material;

		rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);
		
		flushCache();
		
		setMaterial(m->color, m->surfaceProps);

		setTexture(0, m->texture);

		drawInst(header, inst);
		inst++;
	}
#ifndef RW_GL_USE_VAOS
	disableAttribPointers(header->attribDesc, header->numAttribs);
#endif
}


}
}

#endif

