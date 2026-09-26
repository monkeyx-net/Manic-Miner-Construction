# manic miner

TARGET = manicminer

DESKTOP = manicminer.desktop
ICON = manicminer.png

LOCAL = ~/.local

CC ?= gcc
SDL2_CFLAGS  := $(shell sdl2-config --cflags 2>/dev/null)
SDL2_LIBS    := $(shell sdl2-config --libs 2>/dev/null)
TTF_CFLAGS   := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
TTF_LIBS     := $(shell pkg-config --libs SDL2_ttf 2>/dev/null || echo "-lSDL2_ttf")

CFLAGS ?= -O -MMD $(SDL2_CFLAGS) $(TTF_CFLAGS)
LDFLAGS ?= $(SDL2_LIBS) $(TTF_LIBS) -lSDL2_image -lSDL2_mixer -lm

SRC = src
O ?= linux

OBJS = $(O)/main.o $(O)/video.o $(O)/loader.o $(O)/title.o $(O)/audio.o $(O)/miner.o $(O)/levels.o $(O)/game.o $(O)/portal.o $(O)/trans.o $(O)/gameover.o $(O)/npcs.o $(O)/victory.o $(O)/lives.o $(O)/solar_powered_generator.o $(O)/misc.o $(O)/savestate.o $(O)/replay.o $(O)/json.o $(O)/gamedata.o $(O)/hires.o $(O)/collision.o $(O)/assets.o

BUILD = -DVERSION_HEADER

# Emscripten web build
EMCC    ?= emcc
W       = web
WO      = webobj
WEBCFLAGS  = -O2 -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=2
WEBLDFLAGS = -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=2 \
             -sSDL2_IMAGE_FORMATS='["png"]' \
             -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sNO_EXIT_RUNTIME=1 \
             --shell-file shell.html \
             --preload-file gfx/fonts \
             --preload-file sfx/music --preload-file sfx/sounds --preload-file gfx/sprites
WEBOBJS = $(patsubst $(O)/%,$(WO)/%,$(OBJS))

all:	dir $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Headless regression harness: every module except main.c, plus tests/sim.c
SIM_OBJS = $(filter-out $(O)/main.o,$(OBJS))

sim:	dir $(SIM_OBJS)
	$(CC) $(CFLAGS) tests/sim.c $(SIM_OBJS) -o sim $(LDFLAGS)

sprites:
	python3 tools/gen_sprites.py
	python3 tools/gen_title.py

audio:
	python3 tools/gen_audio.py

$(O)/loader.o:
	$(CC) $(CFLAGS) $(BUILD) -c $(SRC)/loader.c -o $@

$(O)/%.o:	$(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

web:	webdir $(WEBOBJS)
	python3 tools/fmt_levels.py --check
	$(EMCC) $(WEBOBJS) -o $(W)/index.html $(WEBLDFLAGS)
	cp levels.json $(W)/levels.json
	cp gfx/sprites/tiles.png $(W)/tiles.png

$(WO)/loader.o:
	$(EMCC) $(WEBCFLAGS) $(BUILD) -c $(SRC)/loader.c -o $@

$(WO)/%.o:	$(SRC)/%.c
	$(EMCC) $(WEBCFLAGS) -c $< -o $@

clean:
	rm -rf $(O) $(WO) $(TARGET) sim
	rm -f $(W)/index.html $(W)/index.js $(W)/index.wasm $(W)/index.data $(W)/levels.json $(W)/tiles.png

fmt-levels:
	python3 tools/fmt_levels.py

serve:
	python3 -m http.server 8080 --directory $(W)

dir:
	@mkdir -p $(O)

webdir:
	@mkdir -p $(W) $(WO)

-include $(O)/*.d

# manic miner
