# CFLAGS = -std=c++17 -O2
CFLAGS = -std=c++17
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

OUT_FILES = $(*.cpp:.cpp=.out)

.SILENT:
all: main.run clean

%.out: %.cpp
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.run: %.out
	./$(@:.run=.out)

.PHONY: clean
clean:
	rm -f $(OUT_FILES)
