#ifndef __gl_h
#define __gl_h

#include <GL/glew.h>
#include "vec.h"
#include "utils.h"
#include <vector>
#include <list>

const GLuint INVALID_TEXTURE = -1;

void check_gl_errors();
void verify_state_disabled(GLenum state);

class GLState {
public:
	static const int MAX_STATES = 8;

	GLState() :
		m_num_states(0)
	{}
	~GLState()
	{
		if (debug_enabled) {
			check_gl_errors();
		}
		for (int i = 0; i < m_num_states; ++i) {
			glDisable(m_states[i]);
		}
	}

	void enable(GLenum state)
	{
		assert(m_num_states < MAX_STATES);
		if (debug_enabled) {
			verify_state_disabled(state);
		}
		glEnable(state);
		m_states[m_num_states++] = state;
	}

private:
	GLenum m_states[MAX_STATES];
	int m_num_states;
};

class GLClientState {
public:
	static const int MAX_STATES = 4;

	GLClientState() :
		m_num_states(0)
	{}
	~GLClientState()
	{
		for (int i = 0; i < m_num_states; ++i) {
			glDisableClientState(m_states[i]);
		}
	}

	void enable(GLenum state)
	{
		assert(m_num_states < MAX_STATES);
		glEnableClientState(state);
		m_states[m_num_states++] = state;
	}

private:
	GLenum m_states[MAX_STATES];
	int m_num_states;
};

class Color {
public:
	float r, g, b, a;

	Color() {}
	Color(float i_r, float i_g, float i_b, float i_a=1) :
		r(i_r), g(i_g), b(i_b), a(i_a)
	{
	}
};

struct Face {
	size_t vert[3], norm[3];
};

struct Group {
	Color diffuse;
	std::vector<Face> faces;
};

typedef std::map<std::string, Group> group_map_t;

struct Mesh {
	std::vector<vec3> vertices;
	std::vector<vec3> normals;
	group_map_t groups;
};

struct GLVertex {
	float pos[3], normal[3], texcoord[2];
};

class Model {
public:
	struct ModelGroup {
		Color diffuse;
		size_t count;
		GLuint buffer;
	};

	bool empty() const { return m_groups.empty(); }

	Model();
	~Model();
	void load(const char *fname, double scale=1,
		  const vec3 &origo=vec3(0, 0, 0));
	void draw(bool noise);

private:
	std::list<ModelGroup> m_groups;

	DISABLE_COPY_AND_ASSIGN(Model);
};

struct FBO {
	GLuint fbo;
	GLuint texture;
};

class GLFont {
public:
	GLFont();
	~GLFont();

	void open(const char *fname, int size);

	void draw_text(const vec2 &pos, const std::string &s, double scale=1) const;

private:
	static const size_t NUM_GLYPHS = 256;

	struct Glyph {
		vec2 tex_pos, tex_size, size;
		vec2 offset, advance;
	};
	GLuint m_texture;
	Glyph m_glyphs[NUM_GLYPHS];

	DISABLE_COPY_AND_ASSIGN(GLFont);
};

extern inline void glVertex(const vec3 &v)
{
	glVertex3d(v.x, v.y, v.z);
}

extern inline void glNormal(const vec3 &v)
{
	glNormal3d(v.x, v.y, v.z);
}

extern inline void glColor(const Color &c)
{
	glColor4f(c.r, c.g, c.b, c.a);
}

GLuint load_png(const char *fname);
void load_mesh(Mesh *mesh, const char *fname, double scale=1,
	       const vec3 &origo=vec3(0, 0, 0));
void draw_quad(const vec2 &pos, const vec2 &size);
void draw_block(const Plane *walls, size_t num_walls);
void create_fbo(FBO *fbo, int width, int height, bool bilinear);
GLuint load_program(const char *vs_source, const char *fs_source);

#endif
