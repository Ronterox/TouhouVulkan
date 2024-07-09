# CFLAGS = -std=c++17 -O2 -s -DNDEBUG
CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

SHADERS_VERT = $(wildcard */*.vert)
SHADERS_FRAG = $(wildcard */*.frag)
SPV = $(SHADERS_VERT:.vert=_vert.spv) $(SHADERS_FRAG:.frag=_frag.spv)

CPP_FILES = $(wildcard *.cpp)
OUT_FILES = $(CPP_FILES:.cpp=.out)

# .SILENT:
all: $(SPV) main.run clean

%_vert.spv: %.vert
	./shaderc/bin/glslc $^ -o $@

%_frag.spv: %.frag
	./shaderc/bin/glslc $^ -o $@

%.out: %.cpp
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.run: %.out $(SPV)
	./$<

.PHONY: clean
clean:
	rm -f $(OUT_FILES) $(SPV)
