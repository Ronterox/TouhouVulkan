LIBS_PATH = ./libs

# CFLAGS = -std=c++20 -O2 -s -DNDEBUG -I$(STB_INCLUDE_PATH)
CFLAGS = -std=c++20 -g -I$(LIBS_PATH)
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
STRICTFLAGS = -Wall -Wextra -Wpedantic -Werror

SHADERS_VERT = $(wildcard */*.vert)
SHADERS_FRAG = $(wildcard */*.frag)
SPV = $(SHADERS_VERT:.vert=_vert.spv) $(SHADERS_FRAG:.frag=_frag.spv)

CPP_FILES = $(wildcard *.cpp)
OUT_FILES = $(CPP_FILES:.cpp=.out)

GLSLC = ./shaderc/bin/glslc

all: $(SPV) main.run clean
run: main.run

%_vert.spv: %.vert
	$(GLSLC) $^ -o $@

%_frag.spv: %.frag
	$(GLSLC) $^ -o $@

%.out: %.cpp
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS) $(STRICTFLAGS)

%.run: %.out $(SPV)
	./$<

.PHONY: clean
clean:
	rm -f $(OUT_FILES) $(SPV)
