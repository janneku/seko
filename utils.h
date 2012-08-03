#ifndef __utils_h
#define __utils_h

#include <stdio.h>
#include <map>
#include <assert.h>
#include <string>
#include <math.h>

extern bool debug_enabled;

#define warning(...)	fprintf(stderr, "WARNING: " __VA_ARGS__)

#define debug(fmt...) \
	do { \
		if (debug_enabled) fprintf(stderr, fmt); \
	} while(0)

#define UNUSED(x)	((void) (x))

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

#define FOR_EACH(type, i, cont)		\
	for (type::iterator i = (cont).begin(); i != (cont).end(); ++i)

#define FOR_EACH_CONST(type, i, cont)		\
	for (type::const_iterator i = (cont).begin(); i != (cont).end(); ++i)

#define FOR_EACH_SAFE(type, i, cont)		\
	for (type::iterator i = (cont).begin(), i##_next = (cont).begin(); \
		i != (cont).end() && ++i##_next != i; \
		i = i##_next)

#define DISABLE_COPY_AND_ASSIGN(x)	\
	private: \
		x(const x &from); \
		void operator =(const x &from)

template<class Key, class Value>
void ins(std::map<Key, Value> &map, const Key &key, const Value &value)
{
	map.insert(std::pair<Key, Value>(key, value));
}

template<class Key, class Value>
void del(std::map<Key, Value> &map, const Key &key)
{
	typename std::map<Key, Value>::iterator i = map.find(key);
	assert(i != map.end());
	map.erase(i);
}

template<class Key, class Value>
Value get(const std::map<Key, Value> &map, const Key &key)
{
	typename std::map<Key, Value>::const_iterator i = map.find(key);
	assert(i != map.end());
	return i->second;
}

template<class Key, class Value>
Value &get(std::map<Key, Value> &map, const Key &key)
{
	typename std::map<Key, Value>::iterator i = map.find(key);
	assert(i != map.end());
	return i->second;
}

std::string strf(const char *fmt, ...);
double uniform();

template<class T>
class Animator {
public:
	Animator(const T &v, const T &vel) :
		m_value(v), m_vel(vel), m_target(v)
	{}

	operator T() const { return m_value; }

	T value() const { return m_value; }

	void set_value(const T &v, const T &vel)
	{
		m_value = v;
		m_vel = vel;
		m_target = v;
	}
	void set_target(const T &v)
	{
		m_target = v;
	}

	void update(double full_dt, double force)
	{
		const double MAX_STEP = 0.02;
		double friction = sqrt(force) * 2;

		while (full_dt > 0) {
			double dt = std::min(full_dt, MAX_STEP);

			T accel = (m_target - m_value) * force - m_vel * friction;
			m_value += m_vel * dt;
			m_vel += accel * dt;
			full_dt -= MAX_STEP;
		}
	}

private:
	T m_value;
	T m_vel;
	T m_target;
};

#endif
