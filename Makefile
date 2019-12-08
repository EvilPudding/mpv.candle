CC = cc
LD = cc

emscripten: CC = emcc
emscripten: LD = emcc
emscripten: AR = emar

DIR = build

SRCS = mpv.c

DEPS = $(shell pkg-config mpv --libs) mpv.candle/mpv/build/libmpv.so

DEPS_EMS =

SAUCES =

SAUCES_SRC = $(patsubst %, $(DIR)/%.sauce.c, $(SAUCES))
SAUCES_OBJ = $(patsubst $(DIR)/%.c, $(DIR)/%.o, $(SAUCES_SRC))

SAUCES_EMS_SRC  = $(patsubst %, $(DIR)/%.emscripten_sauce.c, $(SAUCES))
SAUCES_EMS_OBJ = $(patsubst $(DIR)/%.c, $(DIR)/%.o, $(SAUCES_EMS_SRC))

OBJS_REL = $(SAUCES_OBJ) $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(SAUCES_OBJ) $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))
OBJS_EMS = $(SAUCES_EMS_OBJ) $(patsubst %.c, $(DIR)/%.emscripten.o, $(SRCS))

CFLAGS = $(shell pkg-config sdl2 --cflags) -I../candle -Wuninitialized $(PARENTCFLAGS) \

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3

CFLAGS_EMS = -Iinclude $(CFLAGS) -O2 \
			 -s EMULATE_FUNCTION_POINTER_CASTS=1 \
			 -s FULL_ES3=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
			 -s SAFE_HEAP=1 -s WASM=1

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
	./waf --enable-libmpv-shared configure && \
	./waf --enable-libmpv-shared

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
	rm -fr $(DIR)

# vim:ft=make
