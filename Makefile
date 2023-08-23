CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

GLSLC = glslc/bin/glslc
SHADER_DIR = shaders

shader_frags = $(wildcard $(SHADER_DIR)/*.frag)
shader_verts = $(wildcard $(SHADER_DIR)/*.vert)
spv_files = $(shader_frags:.frag=.spv) $(shader_verts:.vert=.spv)

.SILENT:
all: compile run clean

test: $(spv_files)

compile: main.cpp
	g++ $(CFLAGS) main.cpp -o game $(LDFLAGS)

%.spv: %.frag
	$(GLSLC) $^ -o $@

%.spv: %.vert
	$(GLSLC) $^ -o $@

run: game $(spv_files)
	./game

.PHONY: all clean
clean:
	rm -f game $(spv_files)
