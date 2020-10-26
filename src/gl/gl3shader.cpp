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
#ifdef RW_OPENGL
#include "rwgl3.h"
#include "rwgl3shader.h"

namespace rw {
namespace gl3 {

#ifdef PSP2_USE_SHADER_COMPILER
#ifdef RW_GLES2
#include "gl2_shaders/header_vs.inc"
#include "gl2_shaders/header_fs.inc"
#else
#include "shaders/header_vs.inc"
#include "shaders/header_fs.inc"
#endif
#endif

UniformRegistry uniformRegistry;

int32
registerUniform(const char *name)
{
	int i;
	i = findUniform(name);
	if(i >= 0) return i;
	// TODO: print error
	if(uniformRegistry.numUniforms+1 >= MAX_UNIFORMS){
		assert(0 && "no space for uniform");
		return -1;
	}
	uniformRegistry.uniformNames[uniformRegistry.numUniforms] = strdup(name);
	return uniformRegistry.numUniforms++;
}

int32
findUniform(const char *name)
{
	int i;
	for(i = 0; i < uniformRegistry.numUniforms; i++)
		if(strcmp(name, uniformRegistry.uniformNames[i]) == 0)
			return i;
	return -1;
}

int32
registerBlock(const char *name)
{
	int i;
	i = findBlock(name);
	if(i >= 0) return i;
	// TODO: print error
	if(uniformRegistry.numBlocks+1 >= MAX_BLOCKS)
		return -1;
	uniformRegistry.blockNames[uniformRegistry.numBlocks] = strdup(name);
	return uniformRegistry.numBlocks++;
}

int32
findBlock(const char *name)
{
	int i;
	for(i = 0; i < uniformRegistry.numBlocks; i++)
		if(strcmp(name, uniformRegistry.blockNames[i]) == 0)
			return i;
	return -1;
}

Shader *currentShader;

static void
printShaderSource(const char **src)
{
	int f;
	const char *file;
	bool printline;
	int line = 1;
	for(f = 0; src[f]; f++){
		int fileline = 1;
		char c;
		file = src[f];
		printline = true;
		while(c = *file++, c != '\0'){
			if(printline)
				printf("%.4d/%d:%.4d: ", line++, f, fileline++);
			putchar(c);
			printline = c == '\n';
		}
		putchar('\n');
	}
}

char shader_source_buffer[16 * 1024];

static int
compileshader(GLenum type, const char **src, GLuint *shader)
{
	GLint shdr = glCreateShader(type);
#ifdef PSP2_USE_SHADER_COMPILER	
	GLint n;
	Glint success;
	GLint len;
	char *log;
	
	shader_source_buffer[0] = 0;
	
	for(n = 0; src[n]; n++) {
		sprintf(shader_source_buffer, "%s%s", shader_source_buffer, src[n]);
	}
	
	const char *_src = (const char*)shader_source_buffer;
	
	glShaderSource(shdr, 1, &_src, nil);
	glCompileShader(shdr);
	glGetShaderiv(shdr, GL_COMPILE_STATUS, &success);
	if(!success){
		printShaderSource(src);
		fprintf(stderr, "Error in %s shader\n",
		  type == GL_VERTEX_SHADER ? "vertex" : "fragment");
		glGetShaderiv(shdr, GL_INFO_LOG_LENGTH, &len);
		log = (char*)rwMalloc(len, MEMDUR_FUNCTION);
		glGetShaderInfoLog(shdr, len, nil, log);
		fprintf(stderr, "%s\n", log);
		rwFree(log);
		return 1;
	}
	*shader = shdr;
	return 0;
#else
	unsigned int size = *((unsigned int*)src[1]);
	glShaderBinary(1, (const uint32_t*)&shdr, 0, src[0], size - 1);
	*shader = shdr;
	return 0;
#endif
}

static int
linkprogram(GLint vs, GLint fs, GLuint *program, bool is_2d)
{
	GLint prog, success;
	GLint len;
	char *log;

	prog = glCreateProgram();

	glAttachShader(prog, vs);
	glAttachShader(prog, fs);

	int stride = 0;
	int pos_size = is_2d ? 4 : 3;
	stride += vglBindPackedAttribLocation(prog, "in_pos"    , pos_size, GL_FLOAT,         stride, stride + sizeof(float) * pos_size) * (sizeof(float) * pos_size);
	stride += vglBindPackedAttribLocation(prog, "in_normal" ,        3, GL_FLOAT,         stride, stride + sizeof(float) * 3) * (sizeof(float) * 3);
	stride += vglBindPackedAttribLocation(prog, "in_color"  ,        4, GL_UNSIGNED_BYTE, stride, stride + 4) * 4;
	stride += vglBindPackedAttribLocation(prog, "in_tex0"   ,        2, GL_FLOAT,         stride, stride + sizeof(float) * 2) * (sizeof(float) * 2);
	stride += vglBindPackedAttribLocation(prog, "in_weights",        4, GL_FLOAT,         stride, stride + sizeof(float) * 4) * (sizeof(float) * 4);
	          vglBindPackedAttribLocation(prog, "in_indices",        4, GL_UNSIGNED_BYTE, stride, stride + 4);

	glLinkProgram(prog);

	*program = prog;
	return 0;
}

Shader*
Shader::create(const char **vsrc, const char **fsrc, bool is_2d)
{
	GLuint vs, fs, program;
	int i;
	int fail;

	fail = compileshader(GL_VERTEX_SHADER, vsrc, &vs);
	if(fail)
		return nil;

	fail = compileshader(GL_FRAGMENT_SHADER, fsrc, &fs);
	if(fail){
		glDeleteShader(vs);
		return nil;
	}

	fail = linkprogram(vs, fs, &program, is_2d);
	if(fail){
		glDeleteShader(fs);
		glDeleteShader(vs);
		return nil;
	}

	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);	 // or global?

#ifdef xxxRW_GLES2
	int numUniforms;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
	for(i = 0; i < numUniforms; i++){
		GLint size;
		GLenum type;
		char name[100];
		glGetActiveUniform(program, i, 100, nil, &size, &type, name);
		printf("%d %d %s\n", size, type, name);
	}
	printf("\n");
#endif

#ifdef xxxRW_GLES2
	int numAttribs;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttribs);
	for(i = 0; i < numAttribs; i++){
		GLint size;
		GLenum type;
		char name[100];
		glGetActiveAttrib(program, i, 100, nil, &size, &type, name);
		GLint bind = glGetAttribLocation(program, name);
		printf("%d %d %s. %d\n", size, type, name, bind);
	}
	printf("\n");
#endif

	// query uniform locations
	sh->program = program;
	sh->uniformLocations = rwNewT(GLint, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	for(i = 0; i < uniformRegistry.numUniforms; i++)
		sh->uniformLocations[i] = glGetUniformLocation(program,
			uniformRegistry.uniformNames[i]);

	// set samplers
	glUseProgram(program);

	// reset program
	if(currentShader)
		glUseProgram(currentShader->program);

	return sh;
}

void
Shader::use(void)
{
	//if(currentShader != this){
		glUseProgram(this->program);
		currentShader = this;
	//}
}

void
Shader::destroy(void)
{
	glDeleteProgram(this->program);
	rwFree(this->uniformLocations);
	rwFree(this);
}

}
}

#endif
