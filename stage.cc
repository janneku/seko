/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
#include "stage.h"

namespace {

const Plane graveyard_floor[] = {
	{vec3(0, 1, 0), 2.5},
};

const Plane coffin_walls[] = {
	{vec3(0, 1, 0), 10.5}, /* top */
	{vec3(-1, 0, 0), -5}, /* left and right */
	{vec3(1, 0, 0), 20},
	{vec3(0, 0, -1), -11}, /* top and bottom */
	{vec3(0, 0, 1), 48},
};

const Plane tombstone_walls[] = {
	{vec3(0, 1, 0), 19}, /* top */
	{vec3(-1, 0, 0), -7}, /* left and right */
	{vec3(1, 0, 0), 19},
	{vec3(0, 0, -1), -2}, /* top and bottom */
	{vec3(0, 0, 1), 8},
};

const Plane tombstone2_walls[] = {
	{vec3(0, 1, 0), 11}, /* top */
	{vec3(-1, 0, 0), 25}, /* left and right */
	{vec3(1, 0, 0), -13},
	{vec3(0, 0, -1), -2}, /* top and bottom */
	{vec3(0, 0, 1), 8},
};

const Plane saha_floor[] = {
	{vec3(0, 1, 0), 0},
};

const Plane saw_walls[] = {
	{vec3(0, 1, 0), 4.5}, /* top */
	{vec3(-1, 0, 0), 6}, /* left and right */
	{vec3(1, 0, 0), 28},
	{vec3(0, 0, -1), 4}, /* top and bottom */
	{vec3(0, 0, 1), 4},
};

const Plane roof_walls[] = {
	{vec3(0.199, 0.98, 0), 16}, /* top */
	{vec3(-1, 0, 0), 63}, /* left and right */
	{vec3(1, 0, 0), -30},
	{vec3(0, 0, -1), 18}, /* top and bottom */
	{vec3(0, 0, 1), 38},
};

const Plane roof2_walls[] = {
	{vec3(0, 0.98, 0.199), 18}, /* top */
	{vec3(-1, 0, 0), 20}, /* left and right */
	{vec3(1, 0, 0), 35},
	{vec3(0, 0, -1), 50}, /* top and bottom */
	{vec3(0, 0, 1), -20},
};

}

const Stage graveyard = {
	"graveyard.obj", 7, vec3(13, 17, 30), 80, 4,
	{
		{1, graveyard_floor},
		{5, coffin_walls},
		{5, tombstone_walls},
		{5, tombstone2_walls},
	}
};

const Stage saha = {
	"sahastage.obj", 50, vec3(0, 11, 0), 100, 4,
	{
		{1, saha_floor},
		{5, saw_walls},
		{5, roof_walls},
		{5, roof2_walls},
	}
};

