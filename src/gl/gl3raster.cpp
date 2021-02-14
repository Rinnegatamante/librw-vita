#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwgl3.h"
#include "rwgl3shader.h"

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace gl3 {

int32 nativeRasterOffset;

#define RW_GLES

#ifdef RW_OPENGL
static Raster*
rasterCreateTexture(Raster *raster)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(raster->format & 0xF00){
	case Raster::C8888:
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		natras->bpp = 4;
		raster->depth = 32;
		break;
	case Raster::C888:
		natras->internalFormat = GL_RGB;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		natras->bpp = 3;
		raster->depth = 24;
		break;
	case Raster::C1555:
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_5_5_5_1;
		natras->hasAlpha = 1;
		natras->bpp = 2;
		raster->depth = 16;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

#ifdef RW_GLES
	// glReadPixels only supports GL_RGBA
	natras->internalFormat = GL_RGBA;
	natras->format = GL_RGBA;
	natras->type = GL_UNSIGNED_BYTE;
	natras->bpp = 4;
#endif

#ifndef PSP2_NO_DXT_TEXTURES
	if (raster->width >= 4 && raster->height >= 4)
		natras->internalFormat = natras->hasAlpha ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
#endif

	raster->stride = raster->width*natras->bpp;

	glGenTextures(1, &natras->texid);
	uint32 prev = bindTexture(natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, nil);
	natras->filterMode = 0;
	natras->addressU = 0;
	natras->addressV = 0;

	bindTexture(prev);
	return raster;
}

static Raster*
rasterCreateCameraTexture(Raster *raster)
{
	if(raster->format & (Raster::PAL4 | Raster::PAL8)){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}

	// TODO: figure out what the backbuffer is and use that as a default
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(raster->format & 0xF00){
	case Raster::C8888:
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		natras->bpp = 4;
		break;
	case Raster::C888:
	default:
		natras->internalFormat = GL_RGB;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		natras->bpp = 3;
		break;
	case Raster::C1555:
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_5_5_5_1;
		natras->hasAlpha = 1;
		natras->bpp = 2;
		break;
	}
	
#ifdef RW_GLES
	natras->internalFormat = GL_RGB;
	natras->format = GL_RGB;
	natras->type = GL_UNSIGNED_BYTE;
	natras->bpp = 3;
#endif

	raster->stride = raster->width*natras->bpp;

	glGenTextures(1, &natras->texid);
	uint32 prev = bindTexture(natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, nil);
	natras->filterMode = 0;
	natras->addressU = 0;
	natras->addressV = 0;

	bindTexture(prev);


	/*glGenFramebuffers(1, &natras->fbo);
	bindFramebuffer(natras->fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, natras->texid, 0);
	bindFramebuffer(0);*/
	natras->fboMate = nil;

	return raster;
}

static Raster*
rasterCreateCamera(Raster *raster)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	// TODO: set/check width, height, depth, format?
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->stride = 0;
	raster->pixels = nil;

	natras->texid = 0;
	natras->fbo = 0;
	natras->fboMate = nil;

	return raster;
}

static Raster*
rasterCreateZbuffer(Raster *raster)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	// TODO: set/check width, height, depth, format?
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->stride = 0;
	raster->pixels = nil;

	/*natras->internalFormat = GL_DEPTH_COMPONENT;
	natras->format = GL_DEPTH_COMPONENT;
	natras->type = GL_UNSIGNED_BYTE;

	glGenTextures(1, &natras->texid);
	uint32 prev = bindTexture(natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, nil);
	natras->filterMode = 0;
	natras->addressU = 0;
	natras->addressV = 0;

	bindTexture(prev);

	natras->fbo = 0;
	natras->fboMate = nil; */

	return raster;
}

#endif

/*
{ 0, 0, 0 },
{ 16, 4, GL_RGBA },	// 1555
{ 16, 3, GL_RGB },	// 565
{ 16, 4, GL_RGBA },	// 4444
{ 0, 0, 0 },	// LUM8
{ 32, 4, GL_RGBA },	// 8888
{ 24, 3, GL_RGB },	// 888
{ 16, 3, GL_RGB },	// D16
{ 24, 3, GL_RGB },	// D24
{ 32, 4, GL_RGBA },	// D32
{ 16, 3, GL_RGB },	// 555

0,
GL_RGB5_A1,
GL_RGB5,
GL_RGBA4,
0,
GL_RGBA8,
GL_RGB8,
GL_RGB5,
GL_RGB8,
GL_RGBA8,
GL_RGB5
*/

Raster*
rasterCreate(Raster *raster)
{
	if(raster->width == 0 || raster->height == 0){
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		return raster;
	}
	if(raster->flags & Raster::DONTALLOCATE)
		return raster;

	switch(raster->type){
#ifdef RW_OPENGL
	case Raster::NORMAL:
	case Raster::TEXTURE:
		return rasterCreateTexture(raster);
	case Raster::CAMERATEXTURE:
		return rasterCreateCameraTexture(raster);
	case Raster::ZBUFFER:
		return rasterCreateZbuffer(raster);
	case Raster::CAMERA:
		return rasterCreateCamera(raster);
#endif

	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}
}

#define MEM_ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
#ifdef RW_OPENGL
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	uint8 *px;

	assert(raster->privateFlags == 0);

	switch(raster->type & 0xF00){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		px = (uint8*)rwMalloc(raster->stride*raster->height, MEMDUR_EVENT | ID_DRIVER);
		memset(px, 0, raster->stride*raster->height);
		assert(raster->pixels == nil);
		raster->pixels = px;

		if(lockMode & Raster::LOCKREAD || !(lockMode & Raster::LOCKNOFETCH)){
			uint32 prev = bindTexture(natras->texid);
			int y = 0;
			uint8_t *p = (uint8_t*)vglGetTexDataPointer(GL_TEXTURE_2D);
#ifdef PSP2_NO_DXT_TEXTURES
			uint32_t internal_stride = MEM_ALIGN(raster->width, 8) * natras->bpp;
			while (y < raster->height) {
				memcpy_neon(px, p, raster->stride);
				p += internal_stride;
				y++;
			}		
#else
			if (raster->width < 4 || raster->height < 4) {
				uint32_t internal_stride = MEM_ALIGN(raster->width, 8) * natras->bpp;
				while (y < raster->height) {
					memcpy_neon(px, p, raster->stride);
					p += internal_stride;
					y++;
				}	
			} else memcpy_neon(px, p, natras->hasAlpha ? (raster->width * raster->height) : (raster->width * raster->height / 2));
#endif
			bindTexture(prev);
		}

		raster->privateFlags = lockMode;
		break;

	default:
		assert(0 && "cannot lock this type of raster yet");
		return nil;
	}

	return px;
#else
	return nil;
#endif
}

void
rasterUnlock(Raster *raster, int32 level)
{
#ifdef RW_OPENGL
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	assert(raster->pixels);

	if(raster->privateFlags & Raster::LOCKWRITE){
		uint32 prev = bindTexture(natras->texid);
		if (raster->privateFlags & Raster::LOCKRAW) {
			void *p = vglGetTexDataPointer(GL_TEXTURE_2D);
			memcpy_neon(p, raster->pixels, natras->hasAlpha ? (raster->width * raster->height) : (raster->width * raster->height / 2));
		} else {	
			glTexImage2D(GL_TEXTURE_2D, level, natras->internalFormat,
					raster->width, raster->height,
					0, natras->format, natras->type, raster->pixels);
#ifdef PSP2_MIPMAPS
			glGenerateMipmap(GL_TEXTURE_2D);
#endif
		}
		bindTexture(prev);
	}

	rwFree(raster->pixels);
	raster->pixels = nil;
	raster->privateFlags = 0;
#endif
}

int32
rasterNumLevels(Raster*)
{
	// TODO
	return 1;
}

// Almost the same as d3d9 and ps2 function
bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *pWidth, int32 *pHeight, int32 *pDepth, int32 *pFormat)
{
	int32 width, height, depth, format;

	assert((type&0xF) == Raster::TEXTURE);

//	for(width = 1; width < img->width; width <<= 1);
//	for(height = 1; height < img->height; height <<= 1);
	// Perhaps non-power-of-2 textures are acceptable?
	width = img->width;
	height = img->height;

	depth = img->depth;

	if(depth <= 8)
		depth = 32;

	switch(depth){
	case 32:
		if(img->hasAlpha())
			format = Raster::C8888;
		else{
			format = Raster::C888;
			depth = 24;
		}
		break;
	case 24:
		format = Raster::C888;
		break;
	case 16:
		format = Raster::C1555;
		break;

	case 8:
	case 4:
	default:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	format |= type;

	*pWidth = width;
	*pHeight = height;
	*pDepth = depth;
	*pFormat = format;

	return 1;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if((raster->type&0xF) != Raster::TEXTURE)
		return 0;

	void (*conv)(uint8 *out, uint8 *in) = nil;

	// Unpalettize image if necessary but don't change original
	Image *truecolimg = nil;
	if(image->depth <= 8){
		truecolimg = Image::create(image->width, image->height, image->depth);
		truecolimg->pixels = image->pixels;
		truecolimg->stride = image->stride;
		truecolimg->palette = image->palette;
		truecolimg->unpalettize();
		image = truecolimg;
	}

	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(image->depth){
	case 32:
#ifdef RW_GLES
		conv = conv_RGBA8888_from_RGBA8888;
#else
		if(raster->format == Raster::C8888)
			conv = conv_RGBA8888_from_RGBA8888;
		else if(raster->format == Raster::C888)
			conv = conv_RGB888_from_RGB888;
		else
			goto err;
#endif
		break;
	case 24:
#ifdef RW_GLES
		conv = conv_RGBA8888_from_RGB888;
#else
		if(raster->format == Raster::C8888)
			conv = conv_RGBA8888_from_RGB888;
		else if(raster->format == Raster::C888)
			conv = conv_RGB888_from_RGB888;
		else
			goto err;
#endif
		break;
	case 16:
#ifdef RW_GLES
		conv = conv_RGBA8888_from_ARGB1555;
#else
		if(raster->format == Raster::C1555)
			conv = conv_RGBA5551_from_ARGB1555;
		else
			goto err;
#endif
		break;

	case 8:
	case 4:
	default:
	err:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	natras->hasAlpha = image->hasAlpha();

	uint8 *pixels = raster->lock(0, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
	assert(pixels);
	uint8 *imgpixels = image->pixels + (image->height-1)*image->stride;

	int x, y;
	assert(image->width == raster->width);
	assert(image->height == raster->height);
	for(y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(x = 0; x < image->width; x++){
			conv(rasrow, imgrow);
			imgrow += image->bpp;
			rasrow += natras->bpp;
		}
		imgpixels -= image->stride;
		pixels += raster->stride;
	}
	raster->unlock(0);

	if(truecolimg)
		truecolimg->destroy();

	return 1;
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	Gl3Raster *ras = PLUGINOFFSET(Gl3Raster, object, offset);
	ras->texid = 0;
	ras->fbo = 0;
	ras->fboMate = nil;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	Raster *raster = (Raster*)object;
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, object, offset);
#ifdef RW_OPENGL
	switch(raster->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
		glDeleteTextures(1, &natras->texid);
		break;

	case Raster::CAMERATEXTURE:
		if(natras->fboMate){
			// Break apart from currently associated zbuffer
			Gl3Raster *zras = PLUGINOFFSET(Gl3Raster, natras->fboMate, offset);
			zras->fboMate = nil;
			natras->fboMate = nil;
		}
		glDeleteFramebuffers(1, &natras->fbo);
		glDeleteTextures(1, &natras->texid);
		break;

	case Raster::ZBUFFER:
		if(natras->fboMate){
			// Detatch from FBO we may be attached to
			Gl3Raster *oldfb = PLUGINOFFSET(Gl3Raster, natras->fboMate, nativeRasterOffset);
			if(oldfb->fbo){
				bindFramebuffer(oldfb->fbo);
				//glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0);
			}
			oldfb->fboMate = nil;
		}
		glDeleteTextures(1, &natras->texid);
		break;

	case Raster::CAMERA:
		break;
	}
	natras->texid = 0;
	natras->fbo = 0;

#endif
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	Gl3Raster *d = PLUGINOFFSET(Gl3Raster, dst, offset);
	d->texid = 0;
	d->fbo = 0;
	d->fboMate = nil;
	return dst;
}

static uint32
getLevelSize(Raster *raster, int32 level)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
#ifdef PSP2_NO_DXT_TEXTURES
	uint32 size = raster->stride * raster->height;
#else
	uint32 size;
	if (raster->width < 4 || raster->height < 4) size = raster->stride * raster->height;
	else size = natras->hasAlpha ? (raster->width*raster->height) : (raster->width*raster->height / 2);
#endif
	while(level--)
		size /= 4;
	return size;
}

Texture*
readNativeTexture(Stream *stream)
{
	uint32 platform;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	platform = stream->readU32();
	if(platform != PLATFORM_GL3){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}
	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	// Texture
	tex->filterAddressing = stream->readU32();
	stream->read8(tex->name, 32);
	stream->read8(tex->mask, 32);

	// Raster
	uint32 format = stream->readU32();
	int32 width = stream->readI32();
	int32 height = stream->readI32();
	int32 depth = stream->readI32();
	int32 numLevels = stream->readI32();

	Raster *raster;
	Gl3Raster *natras;
	raster = Raster::create(width, height, depth, format | Raster::TEXTURE, PLATFORM_GL3);
	assert(raster);
	natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	tex->raster = raster;

	uint32 size;
	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		size = stream->readU32();
		if(i < raster->getNumLevels()){
#ifdef PSP2_NO_DXT_TEXTURES
			data = raster->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
#else
			if (raster->width < 4 || raster->height < 4) data = raster->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
			else data = raster->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH|Raster::LOCKRAW);
#endif
			stream->read8(data, size);
			raster->unlock(i);
		}else
			stream->seek(size);
	}
	return tex;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	int32 chunksize = getSizeNativeTexture(tex);
	writeChunkHeader(stream, ID_STRUCT, chunksize-12);
	stream->writeU32(PLATFORM_GL3);

	// Texture
	stream->writeU32(tex->filterAddressing);
	stream->write8(tex->name, 32);
	stream->write8(tex->mask, 32);

	// Raster
	int32 numLevels = raster->getNumLevels();
	stream->writeI32(raster->format);
	stream->writeI32(raster->width);
	stream->writeI32(raster->height);
	stream->writeI32(raster->depth);
	stream->writeI32(numLevels);
	// TODO: compression? auto mipmaps?

	uint32 size;
	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		size = getLevelSize(raster, i);
		stream->writeU32(size);
		data = raster->lock(i, Raster::LOCKREAD);
		stream->write8(data, size);
		raster->unlock(i);
	}
}

uint32
getSizeNativeTexture(Texture *tex)
{
	uint32 size = 12 + 72 + 20;
	int32 levels = tex->raster->getNumLevels();
	for(int32 i = 0; i < levels; i++)
		size += 4 + getLevelSize(tex->raster, i);
	return size;
}



void registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(Gl3Raster),
	                                            ID_RASTERGL3,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);
}

}
}
