# CFLAGS = -std=c++17 -O2
CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

.SILENT:

all: test clean

main.out: main.cpp
	g++ $(CFLAGS) -o "$@" main.cpp $(LDFLAGS)

.PHONY: test clean

test: main.out
	./main.out

clean:
	rm -f main.out
