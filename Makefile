# CFLAGS = -std=c++17 -O2 -s -DNDEBUG
CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

SHADERS = $(wildcard */*.vert) $(wildcard */*.frag)
SPV = $(SHADERS:.vert=_vert.spv) $(SHADERS:.frag=_frag.spv)

OUT_FILES = $(*.cpp:.cpp=.out)

# .SILENT:
all: main.run clean

%_vert.spv: %.vert
	./shaderc/bin/glslc $^ -o $(notdir $@)

%_frag.spv: %.frag
	./shaderc/bin/glslc $^ -o $(notdir $@)

%.out: %.cpp
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.run: %.out $(SPV)
	./$<

.PHONY: clean
clean:
	rm -f $(OUT_FILES) $(SPV)
