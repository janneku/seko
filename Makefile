CXXFLAGS = `sdl-config --cflags` `freetype-config --cflags` -W -Wall -g -O2 
OBJS = main.o utils.o vec.o gl.o skeleton.o sound.o menu.o effects.o game.o zombie.o stage.o system.o
CXX = g++

seko-linux: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) `sdl-config --libs` `freetype-config --libs` -lGL -lGLU -lvorbisfile -lpng -lGLEW -lcurl -lcrypto -o $@
