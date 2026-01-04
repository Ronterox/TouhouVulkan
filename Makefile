LIBS_PATH = ./libs

# CFLAGS: add -g for debug, -O2 for optimization
CFLAGS = -std=c++20 -I$(LIBS_PATH) -g # -Oz -flto # -g
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
STRICTFLAGS = -Wall -Wextra -Wpedantic -Werror

SHADERS_VERT = $(wildcard */*.vert)
SHADERS_FRAG = $(wildcard */*.frag)
SPV = $(SHADERS_VERT:.vert=_vert.spv) $(SHADERS_FRAG:.frag=_frag.spv)

CPP_FILES = $(wildcard *.cpp)
OUT_FILES = $(CPP_FILES:.cpp=.out)

GLSLC = ./glslc

# Change compiler to clang++
CXX = clang++

all: run clean
run: $(SPV) main.run

build: $(SPV) $(OUT_FILES)

# TODO: Add precompilation for  headers and shaders

# Shader compilation
%_vert.spv: %.vert
	$(GLSLC) $^ -o $@

%_frag.spv: %.frag
	$(GLSLC) $^ -o $@

# Compile C++ files with clang++
%.out: %.cpp
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(STRICTFLAGS) -fuse-ld=mold

# Run the program
%.run: %.out $(SPV)
	./$<

.PHONY: clean
clean:
	rm -f $(OUT_FILES) $(SPV)

