CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

cpp_files = $(wildcard *.cpp)
out_files = $(cpp_files:.cpp=.out)

GLSLC = glslc/bin/glslc
SHADER_DIR = shaders

shader_frags = $(wildcard $(SHADER_DIR)/*.frag)
shader_verts = $(wildcard $(SHADER_DIR)/*.vert)
spv_files = $(shader_frags:.frag=.spv) $(shader_verts:.vert=.spv)

.SILENT:
all: main.run clean

%.out: %.cpp
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.spv: %.frag
	$(GLSLC) $^ -o $@

%.spv: %.vert
	$(GLSLC) $^ -o $@

%.run: %.out $(spv_files)
	./$(@:.run=.out)

.PHONY: all clean test
clean:
	rm -f $(out_files) $(spv_files)
