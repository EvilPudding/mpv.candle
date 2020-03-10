CC = cc
LD = cc

emscripten: CC = emcc
emscripten: LD = emcc
emscripten: AR = emar

DIR = build

SRCS = mpv.c

MPV_OPTS = --disable-lua --disable-javascript --disable-libass \
		   --disable-rubberband --disable-vapoursynth --disable-libarchive --enable-dvbin \
		   --disable-sdl2-gamepad --disable-sdl2-audio --enable-openal \
		   --disable-sdl2-video --disable-drm --disable-drmprime --disable-gbm \
		   --disable-wayland-scanner --disable-wayland-protocols --disable-wayland \
		   --disable-x11 --disable-xv --disable-gl-cocoa --disable-gl-x11 --disable-egl15 \
		   --disable-egl-x11 --disable-egl-drm --disable-gl-wayland --disable-gl-win32 \
		   --disable-gl-dxinterop --disable-egl-angle --disable-egl-angle-lib \
		   --disable-egl-angle-win32 --disable-vdpau --disable-vdpau-gl-x11 \
		   --disable-vaapi --disable-vaapi-x11 --disable-vaapi-wayland --disable-vaapi-drm \
		   --disable-vaapi-x-egl --disable-caca --disable-jpeg --disable-direct3d \
		   --disable-shaderc --disable-spirv-cross --disable-libplacebo --disable-vulkan \
		   --enable-libmpv-shared 

DEPS = mpv.candle/mpv/build/libmpv.so

DEPS_EMS =

SAUCES =

SAUCES_SRC = $(patsubst %, $(DIR)/%.sauce.c, $(SAUCES))
SAUCES_OBJ = $(patsubst $(DIR)/%.c, $(DIR)/%.o, $(SAUCES_SRC))

SAUCES_EMS_SRC  = $(patsubst %, $(DIR)/%.emscripten_sauce.c, $(SAUCES))
SAUCES_EMS_OBJ = $(patsubst $(DIR)/%.c, $(DIR)/%.o, $(SAUCES_EMS_SRC))

OBJS_REL = $(SAUCES_OBJ) $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(SAUCES_OBJ) $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))
OBJS_EMS = $(SAUCES_EMS_OBJ) $(patsubst %.c, $(DIR)/%.emscripten.o, $(SRCS))

CFLAGS = -I../candle -Wuninitialized $(PARENTCFLAGS)

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3

CFLAGS_EMS = -Iinclude $(CFLAGS) -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 \
		      -s FULL_ES3=1 -s EMULATE_FUNCTION_POINTER_CASTS=1 \
		      -s WASM=1 -s ASSERTIONS=1 -s SAFE_HEAP=1 \
		      -s WASM_MEM_MAX=2GB -O3


##############################################################################

all: $(DIR)/export.a mpv/build/libmpv.so
	echo -n $(DEPS) > $(DIR)/deps

$(DIR)/export.a: init $(OBJS_REL)
	$(AR) rs build/export.a $(OBJS_REL)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

mpv/build/libmpv.so:
	cd mpv && \
	./bootstrap.py && \
	./waf configure $(MPV_OPTS) && \
	./waf $(MPV_OPTS)

##############################################################################

$(DIR)/%.sauce.c: %
	xxd -i $< > $@

##############################################################################

debug: $(DIR)/export_debug.a
	echo -n $(DEPS) > $(DIR)/deps

$(DIR)/export_debug.a: init $(OBJS_DEB)
	$(AR) rs build/export_debug.a $(OBJS_DEB)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)

##############################################################################

emscripten: $(DIR)/export_emscripten.a
	echo -n $(DEPS_EMS) > $(DIR)/deps

$(DIR)/export_emscripten.a: init $(OBJS_EMS)
	emar rs build/export_emscripten.a $(OBJS_EMS)

$(DIR)/%.emscripten.o: %.c
	emcc -o $@ -c $< $(CFLAGS_EMS)

$(DIR)/%.emscripten_sauce.c: %
	xxd -i $< > $@

##############################################################################

init:
	mkdir -p $(DIR)

##############################################################################

clean:
	rm -fr mpv/build
	rm -fr $(DIR)

# vim:ft=make
