#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "assets.h"

// The game needs all of its image, font and sound files. Loaders report a file that is
// missing or unusable here; start-up then lists every problem and stops, rather than the
// game running with pieces silently missing.
static char     problems[2048];

void asset_problem(const char *fmt, ...) {
    char    line[256];
    size_t  used = strlen(problems);
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    SDL_Log("%s", line);
    snprintf(problems + used, sizeof problems - used, "%s\n", line);
}

int asset_file_exists(const char *path) {
    SDL_RWops *f = SDL_RWFromFile(path, "rb");
    if (!f) return 0;
    SDL_RWclose(f);
    return 1;
}

// Every problem reported so far, one per line, or "" if there were none.
const char *asset_problems(void) {
    return problems;
}
