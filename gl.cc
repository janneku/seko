/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 *
 * OpenGL stuff
 */
#include "gl.h"
#include "utils.h"
#include <SDL.h>
#include <png.h>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <sstream>

extern SDL_Surface *screen;

namespace {

const Color white(1, 1, 1);

const int PAD = 2;

int round_power2(int val)
{
	int i = 1;
	while (i < val)
		i <<= 1;
	return i;
}

void load_materials(group_map_t &groups, const char *fname)
{
	std::ifstream f(fname);
	if (!f) {
		throw std::runtime_error(strf("Can not open %s", fname));
	}
	std::string line;
	Group *group = NULL;
	while (std::getline(f, line)) {
		std::istringstream parser(line);
		std::string token;
		parser >> token;
		if (!parser || token[0] == '#')
			continue;

		if (token == "newmtl") {
			std::string name;
			parser >> name;
			std::pair<group_map_t::iterator, bool> res =
				groups.insert(std::make_pair(name, Group()));
			group = &res.first->second;
		} else if (token == "Kd") {
			parser >> group->diffuse.r >> group->diffuse.g
				>> group->diffuse.b;
			if (!parser) {
				throw std::runtime_error("Corrupted material file");
			}
			group->diffuse.a = 1;
		}
	}
}

void print_shader_log(GLuint obj)
{
	int len;
	if (glIsShader(obj)) {
		glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len);
	} else {
		glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
	}

	std::string buf(len, 0);
	int real_len = 0;
	if (glIsShader(obj)) {
		glGetShaderInfoLog(obj, len, &real_len, &buf[0]);
	} else {
		glGetProgramInfoLog(obj, len, &real_len, &buf[0]);
	}
	buf.resize(real_len);

	if (real_len > 0) {
		printf("---- SHADER LOG ----\n%s----------------\n",
			buf.c_str());
	}
}

}

void check_gl_errors()
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		throw std::runtime_error(strf("OpenGL error: %x", error));
	}
	glEnd();
	error = glGetError();
	if (error != GL_INVALID_OPERATION) {
		throw std::runtime_error(strf("OpenGL error: %x", error));
	}
	int depth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &depth);
	if (depth >= 8) {
		throw std::runtime_error("Matrix stack leak");
	}
}

void verify_state_disabled(GLenum state)
{
	if (glIsEnabled(state)) {
		throw std::runtime_error(strf("OpenGL state leaked: %x", state));
	}
}

GLuint load_png(const char *fname)
{
	debug("loading %s\n", fname);
	FILE *f = fopen(fname, "rb");
	if (f == NULL) {
		throw std::runtime_error(strf("Can not open: %s", fname));
	}

	png_byte header[8];
	fread(header, 1, 8, f);
	if (png_sig_cmp(header, 0, 8)) {
		throw std::runtime_error(strf("Not a PNG: %s", fname));
	}

	png_structp png_ptr =
		png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	int width = png_get_image_width(png_ptr, info_ptr);
	int height = png_get_image_height(png_ptr, info_ptr);

	if (round_power2(width) != width || round_power2(height) != height) {
		throw std::runtime_error(strf("Non-power-of-two: %s", fname));
	}

	int type = 0, components;
	switch (png_get_color_type(png_ptr, info_ptr)) {
	case PNG_COLOR_TYPE_GRAY:
		type = GL_ALPHA;
		components = 1;
		break;
	case PNG_COLOR_TYPE_RGB:
		type = GL_RGB;
		components = 3;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		type = GL_RGBA;
		components = 4;
		break;
	default:
		throw std::runtime_error(strf("Wrong color type: %s", fname));
	}

	std::vector<char> pixels(width * height * components, 0);
	std::vector<png_bytep> rows(height);
	for (int i = 0; i < height; ++i) {
		rows[i] = png_bytep(&pixels[width * components * i]);
	}
	png_read_image(png_ptr, &rows[0]);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(f);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0,
		     type, GL_UNSIGNED_BYTE, &pixels[0]);
	return texture;
}

void load_mesh(Mesh *mesh, const char *fname, double scale, const vec3 &origo)
{
	std::ifstream f(fname);
	if (!f) {
		throw std::runtime_error(strf("Can not open %s", fname));
	}
	std::string line;
	Group *group = NULL;
	while (std::getline(f, line)) {
		std::istringstream parser(line);
		std::string token;
		parser >> token;
		if (!parser || token[0] == '#')
			continue;

		if (token == "v") {
			vec3 p;
			parser >> p.x >> p.y >> p.z;
			mesh->vertices.push_back(p * scale + origo);
			if (!parser) {
				throw std::runtime_error("Corrupted mesh file");
			}
		} else if (token == "vn") {
			vec3 v;
			parser >> v.x >> v.y >> v.z;
			mesh->normals.push_back(v);
			if (!parser) {
				throw std::runtime_error("Corrupted mesh file");
			}
		} else if (token == "mtllib") {
			std::string name;
			parser >> name;
			load_materials(mesh->groups, name.c_str());
		} else if (token == "usemtl") {
			std::string name;
			parser >> name;
			std::pair<group_map_t::iterator, bool> res =
				mesh->groups.insert(std::make_pair(name, Group()));
			group = &res.first->second;
		} else if (token == "f") {
			Face f;
			for (int i = 0; i < 3; ++i) {
				size_t idx;
				parser >> idx;
				if (!parser) {
					break;
				}
				assert(idx > 0 && idx <= mesh->vertices.size());
				f.vert[i] = idx - 1;
				if (parser.peek() == '/') {
					parser.get();
					if (parser.peek() != '/') {
						parser >> idx;
					}
				}
				if (parser.peek() == '/') {
					parser.get();
					parser >> idx;
					assert(idx > 0 &&
					       idx <= mesh->normals.size());
					f.norm[i] = idx - 1;
				}
				if (i == 2) {
					group->faces.push_back(f);
				}
			}
		}
	}
}

Model::Model()
{
}

Model::~Model()
{
	FOR_EACH(std::list<ModelGroup>, group, m_groups) {
		glDeleteBuffers(1, &group->buffer);
	}
}

void Model::load(const char *fname, double scale, const vec3 &origo)
{
	debug("loading %s\n", fname);

	FOR_EACH(std::list<ModelGroup>, group, m_groups) {
		glDeleteBuffers(1, &group->buffer);
	}
	m_groups.clear();

	Mesh mesh;
	load_mesh(&mesh, fname, scale, origo);

	/* upload to GPU */
	FOR_EACH_CONST(group_map_t, iter, mesh.groups) {
		const Group *group = &iter->second;
		std::vector<GLVertex> buf(group->faces.size() * 3);
		for (size_t j = 0; j < group->faces.size(); ++j) {
			const Face *face = &group->faces[j];
			for (int i = 0; i < 3; ++i) {
				vec3 p = mesh.vertices[face->vert[i]];
				vec3 n = mesh.normals[face->norm[i]];
				GLVertex gv;
				gv.pos[0] = p.x;
				gv.pos[1] = p.y;
				gv.pos[2] = p.z;
				gv.normal[0] = n.x;
				gv.normal[1] = n.y;
				gv.normal[2] = n.z;
				gv.texcoord[0] = (p.x + p.y) * 0.3;
				gv.texcoord[1] = (p.z + p.y) * 0.3;
				buf[j * 3 + i] = gv;
			}
		}
		debug("%s: %zd\n", iter->first.c_str(), buf.size());
		ModelGroup mgroup;
		mgroup.diffuse = group->diffuse;
		mgroup.count = buf.size();
		glGenBuffers(1, &mgroup.buffer);
		glBindBuffer(GL_ARRAY_BUFFER, mgroup.buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLVertex) * buf.size(),
			     &buf[0], GL_STATIC_DRAW);
		m_groups.push_back(mgroup);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Model::draw(bool noise)
{
	static GLuint noise_tex = INVALID_TEXTURE;
	if (noise_tex == INVALID_TEXTURE) {
		noise_tex = load_png("noise.png");
	}
	GLState state;
	GLClientState cstate;

	if (noise) {
		glBindTexture(GL_TEXTURE_2D, noise_tex);
		state.enable(GL_TEXTURE_2D);
		cstate.enable(GL_TEXTURE_COORD_ARRAY);
	}
	cstate.enable(GL_VERTEX_ARRAY);
	cstate.enable(GL_NORMAL_ARRAY);

	FOR_EACH_CONST(std::list<ModelGroup>, group, m_groups) {
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
			     &group->diffuse.r);
		glBindBuffer(GL_ARRAY_BUFFER, group->buffer);
		const GLVertex *v = NULL;
		glVertexPointer(3, GL_FLOAT, sizeof(GLVertex), v->pos);
		glNormalPointer(GL_FLOAT, sizeof(GLVertex), v->normal);
		glTexCoordPointer(2, GL_FLOAT, sizeof(GLVertex), v->texcoord);

		glDrawArrays(GL_TRIANGLES, 0, group->count);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLFont::GLFont() :
	m_texture(INVALID_TEXTURE)
{
}

GLFont::~GLFont()
{
	if (m_texture != INVALID_TEXTURE) {
		glDeleteTextures(1, &m_texture);
	}
}

void GLFont::open(const char *fname, int size)
{
	FT_Library library;
	FT_Face face;
	if (FT_Init_FreeType(&library)) {
		throw std::runtime_error("Unable to initialize FreeType");
	}
	if (FT_New_Face(library, fname, 0, &face)) {
		throw std::runtime_error(std::string("Unable to open font: ") +
					 fname);
	}
	FT_Set_Char_Size(face, size * 64, size * 64, 96, 96);
	FT_GlyphSlot slot = face->glyph;

	/* calculate the total size */
	int w = 0;
	int h = 0;
	for (size_t i = 0; i < NUM_GLYPHS; ++i) {
		FT_Load_Glyph(face, FT_Get_Char_Index(face, i),
			      FT_LOAD_RENDER);

		w += slot->bitmap.width + 2 * PAD;
		h = std::max(h, slot->bitmap.rows + 2 * PAD);
	}

	w = round_power2(w);
	h = round_power2(h);
	std::vector<char> pixels(w * h, 0);

	int x = 0;
	for (size_t i = 0; i < NUM_GLYPHS; ++i) {
		Glyph *info = &m_glyphs[i];
		FT_Load_Glyph(face, FT_Get_Char_Index(face, i),
			      FT_LOAD_RENDER);

		info->tex_pos = vec2(x * 1.0 / w, 0);
		info->tex_size = vec2((slot->bitmap.width + 2 * PAD) * 1.0 / w,
				   (slot->bitmap.rows + 2 * PAD) * 1.0 / h);
		info->size = vec2(slot->bitmap.width + 2 * PAD,
				  slot->bitmap.rows + 2 * PAD);

		info->offset = vec2(slot->bitmap_left,
				    -slot->bitmap_top);
		info->advance = vec2(slot->advance.x / 64.0,
				     slot->advance.y / 64.0);

		for (int y = 0; y < slot->bitmap.rows; ++y) {
			memcpy(&pixels[(y + PAD) * w + (PAD + x)],
			       &slot->bitmap.buffer[y * slot->bitmap.width],
			       slot->bitmap.width);
		}
		x += slot->bitmap.width + 2 * PAD;
	}
	FT_Done_Face(face);
	FT_Done_FreeType(library);

	if (m_texture == INVALID_TEXTURE) {
		glGenTextures(1, &m_texture);
	}
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0,
		     GL_ALPHA, GL_UNSIGNED_BYTE, pixels.data());
}

void GLFont::draw_text(const vec2 &pos, const std::string &s, double scale) const
{
	GLState state;
	state.enable(GL_TEXTURE_2D);
	state.enable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glBegin(GL_QUADS);

	vec2 line_pos = pos;
	for (size_t i = 0; i < s.size(); ++i) {
		unsigned char c = s[i];
		if (c == '\n') {
			const Glyph *info = &m_glyphs['W'];
			line_pos.x = pos.x;
			line_pos.y += info->size.y*scale*1.5;
			continue;
		}
		if (c >= NUM_GLYPHS) {
			c = '?';
		}
		const Glyph *info = &m_glyphs[c];

		vec2 p = line_pos + info->offset * scale;
		vec2 s = info->size * scale;

		glTexCoord2f(info->tex_pos.x, info->tex_pos.y);
		glVertex2f(p.x, p.y);
		glTexCoord2f(info->tex_pos.x,
			     info->tex_pos.y + info->tex_size.y);
		glVertex2f(p.x, p.y + s.y);
		glTexCoord2f(info->tex_pos.x + info->tex_size.x,
			     info->tex_pos.y + info->tex_size.y);
		glVertex2f(p.x + s.x, p.y + s.y);
		glTexCoord2f(info->tex_pos.x + info->tex_size.x,
			     info->tex_pos.y);
		glVertex2f(p.x + s.x, p.y);

		line_pos += info->advance * scale;
	}
	glEnd();
}

void draw_quad(const vec2 &pos, const vec2 &size)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(pos.x, pos.y, 0);
	glTexCoord2f(0, 1);
	glVertex3f(pos.x, pos.y + size.y, 0);
	glTexCoord2f(1, 1);
	glVertex3f(pos.x + size.x, pos.y + size.y, 0);
	glTexCoord2f(1, 0);
	glVertex3f(pos.x + size.x, pos.y, 0);
	glEnd();
}

vec3 limit_walls(vec3 pos, const Plane *walls, size_t num_walls)
{
	for (int j = 0; j < 2; ++j) {
		for (size_t i = 0; i < num_walls; ++i) {
			const Plane *wall = &walls[i];
			double d = dot(pos, wall->normal) - wall->d;
			if (d > 1e-10) {
				pos -= wall->normal * d;
			}
		}
	}
	return pos;
}

void draw_block(const Plane *walls, size_t num_walls)
{
	for (size_t i = 0; i < num_walls; ++i) {
		const Plane *wall = &walls[i];
		vec3 p = wall->normal * wall->d;
		vec3 a = normalize(cross(wall->normal, vec3(1, 1, 0))) * 100;
		vec3 b = cross(a, wall->normal);

		glBegin(GL_TRIANGLE_FAN);
		glVertex(limit_walls(p, walls, num_walls));
		for (int i = 0; i <= 50; ++i) {
			double x = i * (M_PI * 2 / 50);
			glVertex(limit_walls(p + a * cos(x) - b * sin(x),
					     walls, num_walls));
		}
		glEnd();
	}
}

void create_fbo(FBO *fbo, int width, int height, bool bilinear)
{
	debug("creating FBO %d x %d\n", width, height);

	glGenFramebuffersEXT(1, &fbo->fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->fbo);

	GLuint depthbuffer;
	glGenRenderbuffersEXT(1, &depthbuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT,
				 width, height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
				GL_RENDERBUFFER_EXT, depthbuffer);

	GLenum filter = GL_NEAREST;
	if (bilinear) {
		filter = GL_LINEAR;
	}

	/* Create texture */
	glGenTextures(1, &fbo->texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fbo->texture);
	glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, filter);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGB, width, height, 0, GL_RGB,
		     GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_RECTANGLE_ARB, fbo->texture, 0);

	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)
	    != GL_FRAMEBUFFER_COMPLETE_EXT) {
		throw std::runtime_error("Creating OpenGL FBO failed");
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	check_gl_errors();
}

GLuint load_program(const char *vs_source, const char *fs_source)
{
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_source, NULL);
	glCompileShader(vs);
	if (debug_enabled) {
		print_shader_log(vs);
	}

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_source, NULL);
	glCompileShader(fs);
	if (debug_enabled) {
		print_shader_log(fs);
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	if (debug_enabled) {
		print_shader_log(program);
	}
	int status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		throw std::runtime_error("Loading OpenGL program failed");
	}
	return program;
}
