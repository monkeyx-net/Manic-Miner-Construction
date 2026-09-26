#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "json.h"

typedef enum { J_NULL, J_BOOL, J_INT, J_STRING, J_ARRAY, J_OBJECT } JTYPE;

struct JSON {
    JTYPE   type;
    int     ival;
    char   *sval;
    JSON  **items;
    char  **keys;
    int     len, cap;
};

static const char *p;
static const char *text;
static const char *error_at;    // where parsing first failed, or NULL

// Records the first syntax error; the parse still unwinds normally and the tree is discarded.
static int error_line;

// Line of the syntax error that made the last json_parse_file fail, or 0 if the file was unreadable.
int json_error_line(void) {
    return error_line;
}

static JSON *syntax_error(void) {
    if (!error_at) error_at = p;
    return NULL;
}

static JSON *jnew(JTYPE t) {
    JSON *j = calloc(1, sizeof *j);
    if (j) j->type = t;
    return j;
}

static void jskip(void) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
}

// Parses a JSON string literal including backslash escape sequences, advancing the global parser cursor.
static char *parse_str(void) {
    if (*p != '"') return (char *)syntax_error();
    p++;
    const char *s = p;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    size_t n = (size_t)(p - s);
    char *buf = malloc(n + 1);
    if (!buf) return NULL;
    char *d = buf;
    while (s < p) {
        if (*s == '\\') {
            s++;
            switch (*s) {
                case '"':  *d++ = '"';  break;
                case '\\': *d++ = '\\'; break;
                case 'n':  *d++ = '\n'; break;
                case 't':  *d++ = '\t'; break;
                default:   *d++ = *s;   break;
            }
        } else {
            *d++ = *s;
        }
        s++;
    }
    *d = '\0';
    if (*p == '"') p++;
    else syntax_error();
    return buf;
}

static JSON *parse_value(void);

// Appends a value (and optional key for objects) to a JSON node, doubling the backing array on overflow.
static int jpush(JSON *arr, JSON *val, char *key) {
    if (arr->len == arr->cap) {
        int nc = arr->cap ? arr->cap * 2 : 8;
        JSON **ni = realloc(arr->items, nc * sizeof *ni);
        if (!ni) return 0;
        arr->items = ni;
        if (arr->type == J_OBJECT) {
            char **nk = realloc(arr->keys, nc * sizeof *nk);
            if (!nk) return 0;
            arr->keys = nk;
        }
        arr->cap = nc;
    }
    arr->items[arr->len] = val;
    if (arr->type == J_OBJECT) arr->keys[arr->len] = key;
    arr->len++;
    return 1;
}

static JSON *parse_object(void) {
    JSON *obj = jnew(J_OBJECT);
    if (!obj || *p++ != '{') { free(obj); return NULL; }
    jskip();
    if (*p == '}') { p++; return obj; }
    for (;;) {
        jskip();
        char *key = parse_str();
        if (!key) break;
        jskip();
        if (*p != ':') { free(key); syntax_error(); break; }
        p++;
        jskip();
        JSON *val = parse_value();
        jpush(obj, val, key);
        jskip();
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        syntax_error();
        break;
    }
    return obj;
}

static JSON *parse_array(void) {
    JSON *arr = jnew(J_ARRAY);
    if (!arr || *p++ != '[') { free(arr); return NULL; }
    jskip();
    if (*p == ']') { p++; return arr; }
    for (;;) {
        jskip();
        JSON *val = parse_value();
        jpush(arr, val, NULL);
        jskip();
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; break; }
        syntax_error();
        break;
    }
    return arr;
}

// Dispatches to the appropriate parser based on the next character, handling strings,
// objects, arrays, booleans, null, and integers.
static JSON *parse_value(void) {
    jskip();
    if (*p == '"') {
        JSON *j = jnew(J_STRING);
        if (j) j->sval = parse_str();
        return j;
    }
    if (*p == '{') return parse_object();
    if (*p == '[') return parse_array();
    if (strncmp(p, "null", 4) == 0) { p += 4; return jnew(J_NULL); }
    if (strncmp(p, "true", 4) == 0) { JSON *j = jnew(J_BOOL); if (j) j->ival = 1; p += 4; return j; }
    if (strncmp(p, "false", 5) == 0) { p += 5; return jnew(J_BOOL); }
    if (*p == '-' || isdigit((unsigned char)*p)) {
        JSON *j = jnew(J_INT);
        int neg = (*p == '-') ? (p++, 1) : 0;
        if (!isdigit((unsigned char)*p)) { free(j); return syntax_error(); }
        if (j) {
            j->ival = 0;
            while (isdigit((unsigned char)*p)) j->ival = j->ival * 10 + (*p++ - '0');
            if (neg) j->ival = -j->ival;
        }
        // Only whole numbers are supported.
        if (*p == '.' || *p == 'e' || *p == 'E') { json_free(j); return syntax_error(); }
        return j;
    }
    return syntax_error();
}

JSON *json_parse_file(const char *path) {
    error_line = 0;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    sz = (long)fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    p = text = buf;
    error_at = NULL;
    jskip();
    JSON *root = parse_value();
    jskip();
    if (*p != '\0') syntax_error();
    if (error_at) {
        const char *c;
        error_line = 1;
        for (c = text; c < error_at; c++) if (*c == '\n') error_line++;
        json_free(root);
        root = NULL;
    }
    free(buf);
    return root;
}

JSON *json_get(JSON *obj, const char *key) {
    if (!obj || obj->type != J_OBJECT) return NULL;
    for (int i = 0; i < obj->len; i++)
        if (obj->keys[i] && strcmp(obj->keys[i], key) == 0)
            return obj->items[i];
    return NULL;
}

JSON *json_index(JSON *arr, int idx) {
    if (!arr) return NULL;
    if (arr->type != J_ARRAY && arr->type != J_OBJECT) return NULL;
    if (idx < 0 || idx >= arr->len) return NULL;
    return arr->items[idx];
}

int json_int(JSON *node) {
    if (!node) return 0;
    if (node->type == J_INT || node->type == J_BOOL) return node->ival;
    return 0;
}

const char *json_string(JSON *node) {
    if (!node || node->type != J_STRING) return "";
    return node->sval ? node->sval : "";
}

int json_is_int(JSON *node)    { return node && node->type == J_INT; }
int json_is_array(JSON *node)  { return node && node->type == J_ARRAY; }
int json_is_object(JSON *node) { return node && node->type == J_OBJECT; }
int json_is_null(JSON *node)   { return node && node->type == J_NULL; }

int json_length(JSON *node) {
    if (!node) return 0;
    if (node->type == J_ARRAY || node->type == J_OBJECT) return node->len;
    return 0;
}

// Recursively frees the entire JSON tree including string values, object keys, and all child nodes.
void json_free(JSON *root) {
    if (!root) return;
    if (root->type == J_STRING) free(root->sval);
    if (root->type == J_ARRAY || root->type == J_OBJECT) {
        for (int i = 0; i < root->len; i++) {
            if (root->type == J_OBJECT) free(root->keys[i]);
            json_free(root->items[i]);
        }
        free(root->items);
        if (root->type == J_OBJECT) free(root->keys);
    }
    free(root);
}
