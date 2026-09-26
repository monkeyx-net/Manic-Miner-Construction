#ifndef JSON_H
#define JSON_H

typedef struct JSON JSON;

JSON*       json_parse_file(const char *path);
JSON*       json_get(JSON *obj, const char *key);
JSON*       json_index(JSON *arr, int idx);
int         json_int(JSON *node);
const char* json_string(JSON *node);
int         json_length(JSON *node);
int         json_is_int(JSON *node);
int         json_is_array(JSON *node);
int         json_is_object(JSON *node);
int         json_is_null(JSON *node);
void        json_free(JSON *root);
int         json_error_line(void);

#endif
