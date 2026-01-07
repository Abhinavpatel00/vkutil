
# Name of the final executable
TARGET := test

# List your C and C++ source files here (relative or absolute paths)
SRC_C   := test.c vk_cmd.c helpers.c vk_startup.c vk_sync.c vk_queue.c vk_descriptor.c vk_pipeline_layout.c vk_pipelines.c vk_shader_reflect.c vk_swapchain.c volk.c vk_resources.c
SRC_CPP := vma.cpp 

# Compiler flags
CFLAGS   := -std=c99 -ggdb
CXXFLAGS := -std=c++17 -w  -g -fno-common
LDFLAGS  := 
LIBS     := -lvulkan -lm -lglfw

# Derived object file list
OBJ := $(SRC_C:.c=.o) $(SRC_CPP:.cpp=.o)

# Default rule
all: $(TARGET)

$(TARGET): $(OBJ)
	@echo Linking $@
	$(CXX) $(OBJ) $(LDFLAGS) -o $@ $(LIBS)

%.o: %.c
	@echo Compiling $<
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	@echo Compiling $<
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
