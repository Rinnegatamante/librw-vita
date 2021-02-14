TARGET          := librw
SOURCES         := src src/d3d src/gl src/lodepng src/ps2

CFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.c))
ASMFILES := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.S))
CPPFILES :=	$(foreach dir,$(SOURCES), $(wildcard $(dir)/*.cpp))
OBJS     := $(CFILES:.c=.o) $(ASMFILES:.S=.o) $(CPPFILES:.cpp=.o)

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CXX		= $(PREFIX)-g++
AR      = $(PREFIX)-gcc-ar
CFLAGS  = -g -Wl,-q,--no-enum-size-warning -fno-short-enums -fno-optimize-sibling-calls -O3 -fsingle-precision-constant -mfloat-abi=hard -ffast-math \
	-DPSP2 -DNDEBUG -DRW_GL3 -DLIBRW_GLAD -Dmemcpy_neon=sceClibMemcpy
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

all: $(TARGET).a

$(TARGET).a: $(OBJS)
	$(AR) -rc $@ $^
	
clean:
	@rm -rf $(TARGET).a $(TARGET).elf $(OBJS)
	
install: $(TARGET).a
	@mkdir -p $(VITASDK)/$(PREFIX)/lib/
	cp $(TARGET).a $(VITASDK)/$(PREFIX)/lib/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/
	cp *.h $(VITASDK)/$(PREFIX)/include/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/src
	cp src/*.h $(VITASDK)/$(PREFIX)/include/src/
	cp src/base.err $(VITASDK)/$(PREFIX)/include/src/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/src/d3d
	@mkdir -p $(VITASDK)/$(PREFIX)/include/src/gl
	@mkdir -p $(VITASDK)/$(PREFIX)/include/src/lodepng
	@mkdir -p $(VITASDK)/$(PREFIX)/include/src/ps2
	cp src/d3d/*.h $(VITASDK)/$(PREFIX)/include/src/d3d/
	cp src/gl/*.h $(VITASDK)/$(PREFIX)/include/src/gl/
	cp src/lodepng/lodepng.h $(VITASDK)/$(PREFIX)/include/src/lodepng/
	cp src/ps2/*.h $(VITASDK)/$(PREFIX)/include/src/ps2/
