# Compiler and flags
CC = g++
CFLAGS = -Wall -O2 -I$(INCDIR) `pkg-config --cflags libavformat libavcodec libavutil libswscale sdl2 SDL2_ttf opencv4 libcurl libpng`
LDFLAGS = `pkg-config --libs libavformat libavcodec libavutil libswscale sdl2 SDL2_ttf opencv4 libcurl libpng` -lpthread

# Target executable
TARGET = generic-stream-yolov8-render
# Output
OUTPUT_RES = *.mp4 *.jpg *.png *.exe *.log *.dat *.txt
# Source and object files
SRCDIR = src
OBJDIR = obj
INCDIR = include

SRCS = $(wildcard $(SRCDIR)/*.cc)
OBJS = $(patsubst $(SRCDIR)/%.cc, $(OBJDIR)/%.o, $(SRCS))

# Rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cc | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET) $(OUTPUT_RES)

# Phony targets
.PHONY: all clean
