CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

all: compile run clean

compile: main.cpp
	g++ $(CFLAGS) -o game main.cpp $(LDFLAGS)

.PHONY: test clean

run: game
	./game

clean:
	rm -f game
