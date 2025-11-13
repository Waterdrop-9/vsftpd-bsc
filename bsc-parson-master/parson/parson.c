#include "parson.h"

#define PARSON_IMPL_VERSION_MAJOR 1
#define PARSON_IMPL_VERSION_MINOR 5
#define PARSON_IMPL_VERSION_PATCH 3

#if (PARSON_VERSION_MAJOR != PARSON_IMPL_VERSION_MAJOR)\
|| (PARSON_VERSION_MINOR != PARSON_IMPL_VERSION_MINOR)\
|| (PARSON_VERSION_PATCH != PARSON_IMPL_VERSION_PATCH)
#error "parson version mismatch between parson.c and parson.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#define STARTING_CAPACITY 16
#define MAX_NESTING       2048

#ifndef PARSON_INDENT_STR
#define PARSON_INDENT_STR "    "
#endif

#define SIZEOF_TOKEN(a)       (sizeof(a) - 1)
#define SKIP_CHAR(str)        ((*str)++)
#define SKIP_WHITESPACES(str) while (isspace((unsigned char)(**str))) { SKIP_CHAR(str); }
#define MAX(a, b)             ((a) > (b) ? (a) : (b))


#if defined(isnan) && defined(isinf)
#define IS_NUMBER_INVALID(x) (isnan((x)) || isinf((x)))
#else
#define IS_NUMBER_INVALID(x) (((x) * 0.0) != 0.0)
#endif

#define OBJECT_INVALID_IX ((size_t)-1)

static int parson_escape_slashes = 1;

#define IS_CONT(b) (((unsigned char)(b) & 0xC0) == 0x80) /* is utf-8 continuation byte */

typedef int parson_bool_t;

#define PARSON_TRUE 1
#define PARSON_FALSE 0

#define APPEND_STRING(str) do {\
                                written = SIZEOF_TOKEN((str));\
                                if (buf != NULL) {\
                                    memcpy(buf, (str), written);\
                                    buf[written] = '\0';\
                                    buf += written;\
                                }\
                                written_total += written;\
                            } while (0)

typedef struct json_string {
    char *chars;
    size_t length;
} JSON_String;

typedef union json_value_value {
    JSON_String  string;
    double       number;
    JSON_Object *object;
    JSON_Array  *array;
    int          boolean;
    int          null;
} JSON_Value_Value;

struct json_value_t {
    JSON_Value      *parent;
    JSON_Value_Type  type;
    JSON_Value_Value value;
};

struct json_object_t {
    JSON_Value    *wrapping_value;
    size_t        *cells;
    unsigned long *hashes;
    char         **names;
    JSON_Value   **values;
    size_t        *cell_ixs;
    size_t         count;
    size_t         item_capacity;
    size_t         cell_capacity;
};

/*
 * struct json array_t
 * @field: wrapping_value:  parent json node
 * @field: items:           elements of the array
 * @field: count:           number of elements in the array
 * @field: capacity:        capacity of the array
*/
struct json_array_t {
    JSON_Value  *wrapping_value;
    JSON_Value **items;
    size_t       count;
    size_t       capacity;
};

/* Various */
static char * read_file(const char *filename);
static char * parson_strndup(const char *string, size_t n);
static char * parson_strdup(const char *string);

static int    hex_char_to_int(char c);
static JSON_Status parse_utf16_hex(const char *string, unsigned int *result);
static int         num_bytes_in_utf8_sequence(unsigned char c);
static JSON_Status   verify_utf8_sequence(const unsigned char *string, int *len);
static parson_bool_t is_valid_utf8(const char *string, size_t string_len);
static parson_bool_t is_decimal(const char *string, size_t length);
static unsigned long hash_string(const char *string, size_t n);

/* JSON Object */
static JSON_Object * json_object_make(JSON_Value *wrapping_value);
static JSON_Status   json_object_init(JSON_Object *object, size_t capacity);
static void          json_object_deinit(JSON_Object *object, parson_bool_t free_keys, parson_bool_t free_values);
static JSON_Status   json_object_grow_and_rehash(JSON_Object *object);
static size_t        json_object_get_cell_ix(const JSON_Object *object, const char *key, size_t key_len, unsigned long hash, parson_bool_t *out_found);
static JSON_Status   json_object_add(JSON_Object *object, char *name, JSON_Value *value);
static JSON_Value  * json_object_getn_value(const JSON_Object *object, const char *name, size_t name_len);
static JSON_Status   json_object_remove_internal(JSON_Object *object, const char *name, parson_bool_t free_value);

/* JSON Array */
static JSON_Array * json_array_make(JSON_Value *wrapping_value);
static JSON_Status  json_array_add(JSON_Array *array, JSON_Value *value);
static JSON_Status  json_array_resize(JSON_Array *array, size_t new_capacity);
static void         json_array_free(JSON_Array *array);

/* JSON Value */
static JSON_Value * json_value_init_string_no_copy(char *string, size_t length);
static const JSON_String * json_value_get_string_desc(const JSON_Value *value);


/* Parser */
static JSON_Status   skip_quotes(const char **string);
static JSON_Status   parse_utf16(const char **unprocessed, char **processed);
static char *        process_string(const char *input, size_t input_len, size_t *output_len);
static char *        get_quoted_string(const char **string, size_t *output_string_len);
static JSON_Value *  parse_object_value(const char **string, size_t nesting);
static JSON_Value *  parse_array_value(const char **string, size_t nesting);
static JSON_Value *  parse_string_value(const char **string);
static JSON_Value *  parse_boolean_value(const char **string);
static JSON_Value *  parse_number_value(const char **string);
static JSON_Value *  parse_null_value(const char **string);
static JSON_Value *  parse_value(const char **string, size_t nesting);

/* Serialization */
static int json_serialize_string(const char *string, size_t len, char *buf);

/*--------------------------------- Various ---------------------------------*/

static char * read_file(const char * filename) {
    FILE *fp = fopen(filename, "r");
    size_t size_to_read = 0;
    size_t size_read = 0;
    long pos;
    char *file_contents;
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return NULL;
    }
    size_to_read = pos;
    rewind(fp);
    file_contents = (char*)malloc(sizeof(char) * (size_to_read + 1));
    if (!file_contents) {
        fclose(fp);
        return NULL;
    }
    size_read = fread(file_contents, 1, size_to_read, fp);
    if (size_read == 0 || ferror(fp)) {
        fclose(fp);
        free(file_contents);
        return NULL;
    }
    fclose(fp);
    file_contents[size_read] = '\0';
    return file_contents;
}

static char * parson_strndup(const char *string, size_t n) {
    /* We expect the caller has validated that 'n' fits within the input buffer. */
    char *output_string = (char*)malloc(n + 1);
    if (!output_string) {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

static char * parson_strdup(const char *string) {
    return parson_strndup(string, strlen(string));
}

static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static JSON_Status parse_utf16_hex(const char *s, unsigned int *result) {
    int x1, x2, x3, x4;
    if (s[0] == '\0' || s[1] == '\0' || s[2] == '\0' || s[3] == '\0') {
        return JSONFailure;
    }
    x1 = hex_char_to_int(s[0]);
    x2 = hex_char_to_int(s[1]);
    x3 = hex_char_to_int(s[2]);
    x4 = hex_char_to_int(s[3]);
    if (x1 == -1 || x2 == -1 || x3 == -1 || x4 == -1) {
        return JSONFailure;
    }
    *result = (unsigned int)((x1 << 12) | (x2 << 8) | (x3 << 4) | x4);
    return JSONSuccess;
}

static int is_valid_utf8(const char *string, size_t string_len) {
    int len = 0;
    const char *string_end =  string + string_len;
    while (string < string_end) {
        if (verify_utf8_sequence((const unsigned char*)string, &len) != JSONSuccess) {
            return PARSON_FALSE;
        }
        string += len;
    }
    return PARSON_TRUE;
}

static int num_bytes_in_utf8_sequence(unsigned char c) {
    if (c == 0xC0 || c == 0xC1 || c > 0xF4 || IS_CONT(c)) {
        return 0;
    } else if ((c & 0x80) == 0) {    /* 0xxxxxxx */
        return 1;
    } else if ((c & 0xE0) == 0xC0) { /* 110xxxxx */
        return 2;
    } else if ((c & 0xF0) == 0xE0) { /* 1110xxxx */
        return 3;
    } else if ((c & 0xF8) == 0xF0) { /* 11110xxx */
        return 4;
    }
    return 0; /* won't happen */
}

static JSON_Status verify_utf8_sequence(const unsigned char *string, int *len) {
    unsigned int cp = 0;
    *len = num_bytes_in_utf8_sequence(string[0]);

    if (*len == 1) {
        cp = string[0];
    } else if (*len == 2 && IS_CONT(string[1])) {
        cp = string[0] & 0x1F;
        cp = (cp << 6) | (string[1] & 0x3F);
    } else if (*len == 3 && IS_CONT(string[1]) && IS_CONT(string[2])) {
        cp = ((unsigned char)string[0]) & 0xF;
        cp = (cp << 6) | (string[1] & 0x3F);
        cp = (cp << 6) | (string[2] & 0x3F);
    } else if (*len == 4 && IS_CONT(string[1]) && IS_CONT(string[2]) && IS_CONT(string[3])) {
        cp = string[0] & 0x7;
        cp = (cp << 6) | (string[1] & 0x3F);
        cp = (cp << 6) | (string[2] & 0x3F);
        cp = (cp << 6) | (string[3] & 0x3F);
    } else {
        return JSONFailure;
    }

    /* overlong encodings */
    if ((cp < 0x80    && *len > 1) ||
        (cp < 0x800   && *len > 2) ||
        (cp < 0x10000 && *len > 3)) {
        return JSONFailure;
    }

    /* invalid unicode */
    if (cp > 0x10FFFF) {
        return JSONFailure;
    }

    /* surrogate halves */
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        return JSONFailure;
    }

    return JSONSuccess;
}

static parson_bool_t is_decimal(const char *string, size_t length) {
    if (length > 1 && string[0] == '0' && string[1] != '.') {
        return PARSON_FALSE;
    }
    if (length > 2 && !strncmp(string, "-0", 2) && string[2] != '.') {
        return PARSON_FALSE;
    }
    while (length--) {
        if (strchr("xX", string[length])) {
            return PARSON_FALSE;
        }
    }
    return PARSON_TRUE;
}

static unsigned long hash_string(const char *string, size_t n) {
#ifdef PARSON_FORCE_HASH_COLLISIONS
    (void)string;
    (void)n;
    return 0;
#else
    unsigned long hash = 5381;
    unsigned char c;
    size_t i = 0;
    for (i = 0; i < n; i++) {
        c = string[i];
        if (c == '\0') {
            break;
        }
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
#endif
}

/*-------------------------------- JSON Object --------------------------------*/

static JSON_Object * json_object_make(JSON_Value *wrapping_value) {
    JSON_Status res = JSONFailure;
    JSON_Object *new_obj = (JSON_Object*)malloc(sizeof(JSON_Object));
    if (new_obj == NULL) {
        return NULL;
    }
    new_obj->wrapping_value = wrapping_value;
    res = json_object_init(new_obj, 0);
    if (res != JSONSuccess) {
        free(new_obj);
        return NULL;
    }
    return new_obj;
}

static JSON_Status json_object_init(JSON_Object *object, size_t capacity) {
    unsigned int i = 0;

    object->cells = NULL;
    object->names = NULL;
    object->values = NULL;
    object->cell_ixs = NULL;
    object->hashes = NULL;

    object->count = 0;
    object->cell_capacity = capacity;
    object->item_capacity = (unsigned int)(capacity * 7/10);

    if (capacity == 0) {
        return JSONSuccess;
    }

    object->cells = (size_t*)malloc(object->cell_capacity * sizeof(*object->cells));
    object->names = (char**)malloc(object->item_capacity * sizeof(*object->names));
    object->values = (JSON_Value**)malloc(object->item_capacity * sizeof(*object->values));
    object->cell_ixs = (size_t*)malloc(object->item_capacity * sizeof(*object->cell_ixs));
    object->hashes = (unsigned long*)malloc(object->item_capacity * sizeof(*object->hashes));
    if (object->cells == NULL
        || object->names == NULL
        || object->values == NULL
        || object->cell_ixs == NULL
        || object->hashes == NULL) {
        goto error;
    }
    for (i = 0; i < object->cell_capacity; i++) {
        object->cells[i] = OBJECT_INVALID_IX;
    }
    return JSONSuccess;
error:
    free(object->cells);
    free(object->names);
    free(object->values);
    free(object->cell_ixs);
    free(object->hashes);
    return JSONFailure;
}

static void json_object_deinit(JSON_Object *object, parson_bool_t free_keys, parson_bool_t free_values) {
    unsigned int i = 0;
    for (i = 0; i < object->count; i++) {
        if (free_keys) {
            free(object->names[i]);
        }
        if (free_values) {
            json_value_free(object->values[i]);
        }
    }

    object->count = 0;
    object->item_capacity = 0;
    object->cell_capacity = 0;

    free(object->cells);
    free(object->names);
    free(object->values);
    free(object->cell_ixs);
    free(object->hashes);

    object->cells = NULL;
    object->names = NULL;
    object->values = NULL;
    object->cell_ixs = NULL;
    object->hashes = NULL;
}

static JSON_Status json_object_grow_and_rehash(JSON_Object *object) {
    JSON_Value *wrapping_value = NULL;
    JSON_Object new_object;
    char *key = NULL;
    JSON_Value *value = NULL;
    unsigned int i = 0;
    size_t new_capacity = MAX(object->cell_capacity * 2, STARTING_CAPACITY);
    JSON_Status res = json_object_init(&new_object, new_capacity);
    if (res != JSONSuccess) {
        return JSONFailure;
    }

    wrapping_value = json_object_get_wrapping_value(object);
    new_object.wrapping_value = wrapping_value;

    for (i = 0; i < object->count; i++) {
        key = object->names[i];
        value = object->values[i];
        res = json_object_add(&new_object, key, value);
        if (res != JSONSuccess) {
            json_object_deinit(&new_object, PARSON_FALSE, PARSON_FALSE);
            return JSONFailure;
        }
        value->parent = wrapping_value;
    }
    json_object_deinit(object, PARSON_FALSE, PARSON_FALSE);
    *object = new_object;
    return JSONSuccess;
}

static size_t json_object_get_cell_ix(const JSON_Object *object, const char *key, size_t key_len, unsigned long hash, parson_bool_t *out_found) {
    size_t cell_ix = hash & (object->cell_capacity - 1);
    size_t cell = 0;
    size_t ix = 0;
    unsigned int i = 0;
    unsigned long hash_to_check = 0;
    const char *key_to_check = NULL;
    size_t key_to_check_len = 0;

    *out_found = PARSON_FALSE;

    for (i = 0; i < object->cell_capacity; i++) {
        ix = (cell_ix + i) & (object->cell_capacity - 1);
        cell = object->cells[ix];
        if (cell == OBJECT_INVALID_IX) {
            return ix;
        }
        hash_to_check = object->hashes[cell];
        if (hash != hash_to_check) {
            continue;
        }
        key_to_check = object->names[cell];
        key_to_check_len = strlen(key_to_check);
        if (key_to_check_len == key_len && strncmp(key, key_to_check, key_len) == 0) {
            *out_found = PARSON_TRUE;
            return ix;
        }
    }
    return OBJECT_INVALID_IX;
}

static JSON_Status json_object_add(JSON_Object *object, char *name, JSON_Value *value) {
    unsigned long hash = 0;
    parson_bool_t found = PARSON_FALSE;
    size_t cell_ix = 0;
    JSON_Status res = JSONFailure;

    if (!object || !name || !value) {
        return JSONFailure;
    }

    hash = hash_string(name, strlen(name));
    found = PARSON_FALSE;
    cell_ix = json_object_get_cell_ix(object, name, strlen(name), hash, &found);
    if (found) {
        return JSONFailure;
    }

    if (object->count >= object->item_capacity) {
        res = json_object_grow_and_rehash(object);
        if (res != JSONSuccess) {
            return JSONFailure;
        }
        cell_ix = json_object_get_cell_ix(object, name, strlen(name), hash, &found);
    }

    object->names[object->count] = name;
    object->cells[cell_ix] = object->count;
    object->values[object->count] = value;
    object->cell_ixs[object->count] = cell_ix;
    object->hashes[object->count] = hash;
    object->count++;
    value->parent = json_object_get_wrapping_value(object);

    return JSONSuccess;
}

static JSON_Value * json_object_getn_value(const JSON_Object *object, const char *name, size_t name_len) {
    unsigned long hash = 0;
    parson_bool_t found = PARSON_FALSE;
    size_t cell_ix = 0;
    size_t item_ix = 0;
    if (!object || !name) {
        return NULL;
    }
    hash = hash_string(name, name_len);
    found = PARSON_FALSE;
    cell_ix = json_object_get_cell_ix(object, name, name_len, hash, &found);
    if (!found) {
        return NULL;
    }
    item_ix = object->cells[cell_ix];
    return object->values[item_ix];
}

static JSON_Status json_object_remove_internal(JSON_Object *object, const char *name, parson_bool_t free_value) {
    unsigned long hash = 0;
    parson_bool_t found = PARSON_FALSE;
    size_t cell = 0;
    size_t item_ix = 0;
    size_t last_item_ix = 0;
    size_t i = 0;
    size_t j = 0;
    size_t x = 0;
    size_t k = 0;
    JSON_Value *val = NULL;

    if (object == NULL) {
        return JSONFailure;
    }

    hash = hash_string(name, strlen(name));
    found = PARSON_FALSE;
    cell = json_object_get_cell_ix(object, name, strlen(name), hash, &found);
    if (!found) {
        return JSONFailure;
    }

    item_ix = object->cells[cell];
    if (free_value) {
        val = object->values[item_ix];
        json_value_free(val);
        val = NULL;
    }

    free(object->names[item_ix]);
    last_item_ix = object->count - 1;
    if (item_ix < last_item_ix) {
        object->names[item_ix] = object->names[last_item_ix];
        object->values[item_ix] = object->values[last_item_ix];
        object->cell_ixs[item_ix] = object->cell_ixs[last_item_ix];
        object->hashes[item_ix] = object->hashes[last_item_ix];
        object->cells[object->cell_ixs[item_ix]] = item_ix;
    }
    object->count--;

    i = cell;
    j = i;
    for (x = 0; x < (object->cell_capacity - 1); x++) {
        j = (j + 1) & (object->cell_capacity - 1);
        if (object->cells[j] == OBJECT_INVALID_IX) {
            break;
        }
        k = object->hashes[object->cells[j]] & (object->cell_capacity - 1);
        if ((j > i && (k <= i || k > j))
         || (j < i && (k <= i && k > j))) {
            object->cell_ixs[object->cells[j]] = i;
            object->cells[i] = object->cells[j];
            i = j;
        }
    }
    object->cells[i] = OBJECT_INVALID_IX;
    return JSONSuccess;
}

/*-------------------------------- JSON Array --------------------------------*/

static JSON_Array * json_array_make(JSON_Value *wrapping_value) {
    JSON_Array *new_array = (JSON_Array*)malloc(sizeof(JSON_Array));
    if (new_array == NULL) {
        return NULL;
    }
    new_array->wrapping_value = wrapping_value;
    new_array->items = (JSON_Value**)NULL;
    new_array->capacity = 0;
    new_array->count = 0;
    return new_array;
}

static JSON_Status json_array_add(JSON_Array *array, JSON_Value *value) {
    if (array->count >= array->capacity) {
        size_t new_capacity = MAX(array->capacity * 2, STARTING_CAPACITY);
        if (json_array_resize(array, new_capacity) != JSONSuccess) {
            return JSONFailure;
        }
    }
    value->parent = json_array_get_wrapping_value(array);
    array->items[array->count] = value;
    array->count++;
    return JSONSuccess;
}

static JSON_Status json_array_resize(JSON_Array *array, size_t new_capacity) {
    JSON_Value **new_items = NULL;
    if (new_capacity == 0) {
        return JSONFailure;
    }
    new_items = (JSON_Value**)malloc(new_capacity * sizeof(JSON_Value*));
    if (new_items == NULL) {
        return JSONFailure;
    }
    if (array->items != NULL && array->count > 0) {
        memcpy(new_items, array->items, array->count * sizeof(JSON_Value*));
    }
    free(array->items);
    array->items = new_items;
    array->capacity = new_capacity;
    return JSONSuccess;
}

static void json_array_free(JSON_Array *array) {
    size_t i;
    for (i = 0; i < array->count; i++) {
        json_value_free(array->items[i]);
    }
    free(array->items);
    free(array);
}


/*------------------------------- JSON Value --------------------------------*/
static JSON_Value * json_value_init_string_no_copy(char *string, size_t length) {
    JSON_Value *new_value = (JSON_Value*)malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONString;
    new_value->value.string.chars = string;
    new_value->value.string.length = length;
    return new_value;
}


/*---------------------------------- Parser ----------------------------------*/
static JSON_Status skip_quotes(const char **string) {
    if (**string != '\"') {
        return JSONFailure;
    }
    SKIP_CHAR(string);
    while (**string != '\"') {
        if (**string == '\0') {
            return JSONFailure;
        } else if (**string == '\\') {
            SKIP_CHAR(string);
            if (**string == '\0') {
                return JSONFailure;
            }
        }
        SKIP_CHAR(string);
    }
    SKIP_CHAR(string);
    return JSONSuccess;
}

static JSON_Value * parse_object_value(const char **string, size_t nesting) {
    JSON_Status status = JSONFailure;
    JSON_Value *output_value = NULL, *new_value = NULL;
    JSON_Object *output_object = NULL;
    char *new_key = NULL;

    output_value = json_value_init_object();
    if (output_value == NULL) {
        return NULL;
    }
    if (**string != '{') {
        json_value_free(output_value);
        return NULL;
    }
    output_object = json_value_get_object(output_value);
    SKIP_CHAR(string);
    SKIP_WHITESPACES(string);
    if (**string == '}') { /* empty object */
        SKIP_CHAR(string);
        return output_value;
    }
    while (**string != '\0') {
        size_t key_len = 0;
        new_key = get_quoted_string(string, &key_len);
        /* We do not support key names with embedded \0 chars */
        if (!new_key) {
            json_value_free(output_value);
            return NULL;
        }
        if (key_len != strlen(new_key)) {
            free(new_key);
            json_value_free(output_value);
            return NULL;
        }
        SKIP_WHITESPACES(string);
        if (**string != ':') {
            free(new_key);
            json_value_free(output_value);
            return NULL;
        }
        SKIP_CHAR(string);
        new_value = parse_value(string, nesting);
        if (new_value == NULL) {
            free(new_key);
            json_value_free(output_value);
            return NULL;
        }
        status = json_object_add(output_object, new_key, new_value);
        if (status != JSONSuccess) {
            free(new_key);
            json_value_free(new_value);
            json_value_free(output_value);
            return NULL;
        }
        SKIP_WHITESPACES(string);
        if (**string != ',') {
            break;
        }
        SKIP_CHAR(string);
        SKIP_WHITESPACES(string);
        if (**string == '}') {
            break;
        }
    }
    SKIP_WHITESPACES(string);
    if (**string != '}') {
        json_value_free(output_value);
        return NULL;
    }
    SKIP_CHAR(string);
    return output_value;
}

static JSON_Value *  parse_array_value(const char **string, size_t nesting) {
    JSON_Value *output_value = NULL, *new_array_value = NULL;
    JSON_Array *output_array = NULL;
    output_value = json_value_init_array();
    if (output_value == NULL) {
        return NULL;
    }
    if (**string != '[') {
        json_value_free(output_value);
        return NULL;
    }
    output_array = json_value_get_array(output_value);
    SKIP_CHAR(string);
    SKIP_WHITESPACES(string);
    if (**string == ']') { /* empty array */
        SKIP_CHAR(string);
        return output_value;
    }
    while (**string != '\0') {
        new_array_value = parse_value(string, nesting);
        if (new_array_value == NULL) {
            json_value_free(output_value);
            return NULL;
        }
        if (json_array_add(output_array, new_array_value) != JSONSuccess) {
            json_value_free(new_array_value);
            json_value_free(output_value);
            return NULL;
        }
        SKIP_WHITESPACES(string);
        if (**string != ',') {
            break;
        }
        SKIP_CHAR(string);
        SKIP_WHITESPACES(string);
        if (**string == ']') {
            break;
        }
    }
    SKIP_WHITESPACES(string);
    if (**string != ']' || /* Trim array after parsing is over */
        json_array_resize(output_array, json_array_get_count(output_array)) != JSONSuccess) {
            json_value_free(output_value);
            return NULL;
    }
    SKIP_CHAR(string);
    return output_value;
}

static JSON_Value * parse_string_value(const char **string) {
    JSON_Value *value = NULL;
    size_t new_string_len = 0;
    char *new_string = get_quoted_string(string, &new_string_len);
    if (new_string == NULL) {
        return NULL;
    }
    value = json_value_init_string_no_copy(new_string, new_string_len);
    if (value == NULL) {
        free(new_string);
        return NULL;
    }
    return value;
}

static JSON_Value * parse_boolean_value(const char **string) {
    size_t true_token_size = SIZEOF_TOKEN("true");
    size_t false_token_size = SIZEOF_TOKEN("false");
    if (strncmp("true", *string, true_token_size) == 0) {
        *string += true_token_size;
        return json_value_init_boolean(1);
    } else if (strncmp("false", *string, false_token_size) == 0) {
        *string += false_token_size;
        return json_value_init_boolean(0);
    }
    return NULL;
}

static JSON_Value * parse_number_value(const char **string) {
    char *end;
    double number = 0;
    errno = 0;
    number = strtod(*string, &end);
    if (errno == ERANGE && (number <= -HUGE_VAL || number >= HUGE_VAL)) {
        return NULL;
    }
    if ((errno && errno != ERANGE) || !is_decimal(*string, end - *string)) {
        return NULL;
    }
    *string = end;
    return json_value_init_number(number);
}

static JSON_Value * parse_null_value(const char **string) {
    size_t token_size = SIZEOF_TOKEN("null");
    if (strncmp("null", *string, token_size) == 0) {
        *string += token_size;
        return json_value_init_null();
    }
    return NULL;
}

static JSON_Status parse_utf16(const char **unprocessed, char **processed) {
    unsigned int cp, lead, trail;
    char *processed_ptr = *processed;
    const char *unprocessed_ptr = *unprocessed;
    JSON_Status status = JSONFailure;
    unprocessed_ptr++; /* skips u */
    status = parse_utf16_hex(unprocessed_ptr, &cp);
    if (status != JSONSuccess) {
        return JSONFailure;
    }
    if (cp < 0x80) {
        processed_ptr[0] = (char)cp; /* 0xxxxxxx */
    } else if (cp < 0x800) {
        processed_ptr[0] = ((cp >> 6) & 0x1F) | 0xC0; /* 110xxxxx */
        processed_ptr[1] = ((cp)      & 0x3F) | 0x80; /* 10xxxxxx */
        processed_ptr += 1;
    } else if (cp < 0xD800 || cp > 0xDFFF) {
        processed_ptr[0] = ((cp >> 12) & 0x0F) | 0xE0; /* 1110xxxx */
        processed_ptr[1] = ((cp >> 6)  & 0x3F) | 0x80; /* 10xxxxxx */
        processed_ptr[2] = ((cp)       & 0x3F) | 0x80; /* 10xxxxxx */
        processed_ptr += 2;
    } else if (cp >= 0xD800 && cp <= 0xDBFF) { /* lead surrogate (0xD800..0xDBFF) */
        lead = cp;
        unprocessed_ptr += 4; /* should always be within the buffer, otherwise previous sscanf would fail */
        if (*unprocessed_ptr++ != '\\' || *unprocessed_ptr++ != 'u') {
            return JSONFailure;
        }
        status = parse_utf16_hex(unprocessed_ptr, &trail);
        if (status != JSONSuccess || trail < 0xDC00 || trail > 0xDFFF) { /* valid trail surrogate? (0xDC00..0xDFFF) */
            return JSONFailure;
        }
        cp = ((((lead - 0xD800) & 0x3FF) << 10) | ((trail - 0xDC00) & 0x3FF)) + 0x010000;
        processed_ptr[0] = (((cp >> 18) & 0x07) | 0xF0); /* 11110xxx */
        processed_ptr[1] = (((cp >> 12) & 0x3F) | 0x80); /* 10xxxxxx */
        processed_ptr[2] = (((cp >> 6)  & 0x3F) | 0x80); /* 10xxxxxx */
        processed_ptr[3] = (((cp)       & 0x3F) | 0x80); /* 10xxxxxx */
        processed_ptr += 3;
    } else { /* trail surrogate before lead surrogate */
        return JSONFailure;
    }
    unprocessed_ptr += 3;
    *processed = processed_ptr;
    *unprocessed = unprocessed_ptr;
    return JSONSuccess;
}

static char* process_string(const char *input, size_t input_len, size_t *output_len) {
    const char *input_ptr = input;
    size_t initial_size = (input_len + 1) * sizeof(char);
    size_t final_size = 0;
    char *output = NULL, *output_ptr = NULL, *resized_output = NULL;
    output = (char*)malloc(initial_size);
    if (output == NULL) {
        goto error;
    }
    output_ptr = output;
    while ((*input_ptr != '\0') && (size_t)(input_ptr - input) < input_len) {
        if (*input_ptr == '\\') {
            input_ptr++;
            switch (*input_ptr) {
                case '\"': *output_ptr = '\"'; break;
                case '\\': *output_ptr = '\\'; break;
                case '/':  *output_ptr = '/';  break;
                case 'b':  *output_ptr = '\b'; break;
                case 'f':  *output_ptr = '\f'; break;
                case 'n':  *output_ptr = '\n'; break;
                case 'r':  *output_ptr = '\r'; break;
                case 't':  *output_ptr = '\t'; break;
                case 'u':
                    if (parse_utf16(&input_ptr, &output_ptr) != JSONSuccess) {
                        goto error;
                    }
                    break;
                default:
                    goto error;
            }
        } else if ((unsigned char)*input_ptr < 0x20) {
            goto error; /* 0x00-0x19 are invalid characters for json string (http://www.ietf.org/rfc/rfc4627.txt) */
        } else {
            *output_ptr = *input_ptr;
        }
        output_ptr++;
        input_ptr++;
    }
    *output_ptr = '\0';
    /* resize to new length */
    final_size = (size_t)(output_ptr-output) + 1;
    /* todo: don't resize if final_size == initial_size */
    resized_output = (char*)malloc(final_size);
    if (resized_output == NULL) {
        goto error;
    }
    memcpy(resized_output, output, final_size);
    *output_len = final_size - 1;
    free(output);
    return resized_output;
error:
    free(output);
    return NULL;
}

static char * get_quoted_string(const char **string, size_t *output_string_len) {
    const char *string_start = *string;
    size_t input_string_len = 0;
    JSON_Status status = skip_quotes(string);
    if (status != JSONSuccess) {
        return NULL;
    }
    input_string_len = *string - string_start - 2; /* length without quotes */
    return process_string(string_start + 1, input_string_len, output_string_len);
}

static JSON_Value * parse_value(const char **string, size_t nesting) {
    if (nesting > MAX_NESTING) {
        return NULL;
    }
    SKIP_WHITESPACES(string);
    switch (**string) {
        case '{':
            return parse_object_value(string, nesting + 1);
        case '[':
            return parse_array_value(string, nesting + 1);
        case '\"':
            return parse_string_value(string);
        case 'f': case 't':
            return parse_boolean_value(string);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number_value(string);
        case 'n':
            return parse_null_value(string);
        default:
            return NULL;
    }
}

static int json_serialize_string(const char *string, size_t len, char *buf) {
    size_t i = 0;
    char c = '\0';
    int written = -1, written_total = 0;
    APPEND_STRING("\"");
    for (i = 0; i < len; i++) {
        c = string[i];
        switch (c) {
            case '\"': APPEND_STRING("\\\""); break;
            case '\\': APPEND_STRING("\\\\"); break;
            case '\b': APPEND_STRING("\\b"); break;
            case '\f': APPEND_STRING("\\f"); break;
            case '\n': APPEND_STRING("\\n"); break;
            case '\r': APPEND_STRING("\\r"); break;
            case '\t': APPEND_STRING("\\t"); break;
            case '\x00': APPEND_STRING("\\u0000"); break;
            case '\x01': APPEND_STRING("\\u0001"); break;
            case '\x02': APPEND_STRING("\\u0002"); break;
            case '\x03': APPEND_STRING("\\u0003"); break;
            case '\x04': APPEND_STRING("\\u0004"); break;
            case '\x05': APPEND_STRING("\\u0005"); break;
            case '\x06': APPEND_STRING("\\u0006"); break;
            case '\x07': APPEND_STRING("\\u0007"); break;
            /* '\x08' duplicate: '\b' */
            /* '\x09' duplicate: '\t' */
            /* '\x0a' duplicate: '\n' */
            case '\x0b': APPEND_STRING("\\u000b"); break;
            /* '\x0c' duplicate: '\f' */
            /* '\x0d' duplicate: '\r' */
            case '\x0e': APPEND_STRING("\\u000e"); break;
            case '\x0f': APPEND_STRING("\\u000f"); break;
            case '\x10': APPEND_STRING("\\u0010"); break;
            case '\x11': APPEND_STRING("\\u0011"); break;
            case '\x12': APPEND_STRING("\\u0012"); break;
            case '\x13': APPEND_STRING("\\u0013"); break;
            case '\x14': APPEND_STRING("\\u0014"); break;
            case '\x15': APPEND_STRING("\\u0015"); break;
            case '\x16': APPEND_STRING("\\u0016"); break;
            case '\x17': APPEND_STRING("\\u0017"); break;
            case '\x18': APPEND_STRING("\\u0018"); break;
            case '\x19': APPEND_STRING("\\u0019"); break;
            case '\x1a': APPEND_STRING("\\u001a"); break;
            case '\x1b': APPEND_STRING("\\u001b"); break;
            case '\x1c': APPEND_STRING("\\u001c"); break;
            case '\x1d': APPEND_STRING("\\u001d"); break;
            case '\x1e': APPEND_STRING("\\u001e"); break;
            case '\x1f': APPEND_STRING("\\u001f"); break;
            case '/':
                if (parson_escape_slashes) {
                    APPEND_STRING("\\/");  /* to make json embeddable in xml\/html */
                } else {
                    APPEND_STRING("/");
                }
                break;
            default:
                if (buf != NULL) {
                    buf[0] = c;
                    buf += 1;
                }
                written_total += 1;
                break;
        }
    }
    APPEND_STRING("\"");
    return written_total;
}

/*----------------------------- JSON Parser API -----------------------------*/

/* entry point for parsing json file */
JSON_Value * json_parse_file(const char *filename) {
    char *file_contents = read_file(filename);
    JSON_Value *output_value = NULL;
    if (file_contents == NULL) {
        return NULL;
    }
    output_value = json_parse_string(file_contents);
    free(file_contents);
    return output_value;
}

JSON_Value * json_parse_string(const char *string) {
    if (string == NULL) {
        return NULL;
    }
    if (string[0] == '\xEF' && string[1] == '\xBB' && string[2] == '\xBF') {
        string = string + 3; /* Support for UTF-8 BOM */
    }
    return parse_value((const char**)&string, 0);
}

JSON_Status json_validate(const JSON_Value *schema, const JSON_Value *value) {
    JSON_Value *temp_schema_value = NULL, *temp_value = NULL;
    JSON_Array *schema_array = NULL, *value_array = NULL;
    JSON_Object *schema_object = NULL, *value_object = NULL;
    JSON_Value_Type schema_type = JSONError, value_type = JSONError;
    const char *key = NULL;
    size_t i = 0, count = 0;
    if (schema == NULL || value == NULL) {
        return JSONFailure;
    }
    schema_type = json_value_get_type(schema);
    value_type = json_value_get_type(value);
    if (schema_type != value_type && schema_type != JSONNull) { /* null represents all values */
        return JSONFailure;
    }
    switch (schema_type) {
        case JSONArray:
            schema_array = json_value_get_array(schema);
            value_array = json_value_get_array(value);
            count = json_array_get_count(schema_array);
            if (count == 0) {
                return JSONSuccess; /* Empty array allows all types */
            }
            /* Get first value from array, rest is ignored */
            temp_schema_value = json_array_get_value(schema_array, 0);
            for (i = 0; i < json_array_get_count(value_array); i++) {
                temp_value = json_array_get_value(value_array, i);
                if (json_validate(temp_schema_value, temp_value) != JSONSuccess) {
                    return JSONFailure;
                }
            }
            return JSONSuccess;
        case JSONObject:
            schema_object = json_value_get_object(schema);
            value_object = json_value_get_object(value);
            count = json_object_get_count(schema_object);
            if (count == 0) {
                return JSONSuccess; /* Empty object allows all objects */
            } else if (json_object_get_count(value_object) < count) {
                return JSONFailure; /* Tested object mustn't have less name-value pairs than schema */
            }
            for (i = 0; i < count; i++) {
                key = json_object_get_name(schema_object, i);
                temp_schema_value = json_object_get_value(schema_object, key);
                temp_value = json_object_get_value(value_object, key);
                if (temp_value == NULL) {
                    return JSONFailure;
                }
                if (json_validate(temp_schema_value, temp_value) != JSONSuccess) {
                    return JSONFailure;
                }
            }
            return JSONSuccess;
        case JSONString: case JSONNumber: case JSONBoolean: case JSONNull:
            return JSONSuccess; /* equality already tested before switch */
        case JSONError: default:
            return JSONFailure;
    }
}

int json_value_equals(const JSON_Value *a, const JSON_Value *b) {
    JSON_Object *a_object = NULL, *b_object = NULL;
    JSON_Array *a_array = NULL, *b_array = NULL;
    const JSON_String *a_string = NULL, *b_string = NULL;
    const char *key = NULL;
    size_t a_count = 0, b_count = 0, i = 0;
    JSON_Value_Type a_type, b_type;
    a_type = json_value_get_type(a);
    b_type = json_value_get_type(b);
    if (a_type != b_type) {
        return PARSON_FALSE;
    }
    switch (a_type) {
        case JSONArray:
            a_array = json_value_get_array(a);
            b_array = json_value_get_array(b);
            a_count = json_array_get_count(a_array);
            b_count = json_array_get_count(b_array);
            if (a_count != b_count) {
                return PARSON_FALSE;
            }
            for (i = 0; i < a_count; i++) {
                if (!json_value_equals(json_array_get_value(a_array, i),
                                       json_array_get_value(b_array, i))) {
                    return PARSON_FALSE;
                }
            }
            return PARSON_TRUE;
        case JSONObject:
            a_object = json_value_get_object(a);
            b_object = json_value_get_object(b);
            a_count = json_object_get_count(a_object);
            b_count = json_object_get_count(b_object);
            if (a_count != b_count) {
                return PARSON_FALSE;
            }
            for (i = 0; i < a_count; i++) {
                key = json_object_get_name(a_object, i);
                if (!json_value_equals(json_object_get_value(a_object, key),
                                       json_object_get_value(b_object, key))) {
                    return PARSON_FALSE;
                }
            }
            return PARSON_TRUE;
        case JSONString:
            a_string = json_value_get_string_desc(a);
            b_string = json_value_get_string_desc(b);
            if (a_string == NULL || b_string == NULL) {
                return PARSON_FALSE; /* shouldn't happen */
            }
            return a_string->length == b_string->length &&
                   memcmp(a_string->chars, b_string->chars, a_string->length) == 0;
        case JSONBoolean:
            return json_value_get_boolean(a) == json_value_get_boolean(b);
        case JSONNumber:
            return fabs(json_value_get_number(a) - json_value_get_number(b)) < 0.000001; /* EPSILON */
        case JSONError:
            return PARSON_TRUE;
        case JSONNull:
            return PARSON_TRUE;
        default:
            return PARSON_TRUE;
    }
}


/*------------------------------JSON Object API------------------------------*/

JSON_Value * json_object_get_value(const JSON_Object *object, const char *name) {
    if (object == NULL || name == NULL) {
        return NULL;
    }
    return json_object_getn_value(object, name, strlen(name));
}

const char * json_object_get_string(const JSON_Object *object, const char *name) {
    return json_value_get_string(json_object_get_value(object, name));
}

size_t json_object_get_string_len(const JSON_Object *object, const char *name) {
    return json_value_get_string_len(json_object_get_value(object, name));
}

JSON_Object * json_object_get_object(const JSON_Object *object, const char *name) {
    return json_value_get_object(json_object_get_value(object, name));
}

JSON_Array * json_object_get_array(const JSON_Object *object, const char *name) {
    return json_value_get_array(json_object_get_value(object, name));
}

double json_object_get_number(const JSON_Object *object, const char *name) {
    return json_value_get_number(json_object_get_value(object, name));
}

int json_object_get_boolean(const JSON_Object *object, const char *name) {
    return json_value_get_boolean(json_object_get_value(object, name));
}

JSON_Value * json_object_dotget_value(const JSON_Object *object, const char *name) {
    const char *dot_position = strchr(name, '.');
    if (!dot_position) {
        return json_object_get_value(object, name);
    }
    object = json_value_get_object(json_object_getn_value(object, name, dot_position - name));
    return json_object_dotget_value(object, dot_position + 1);
}

const char * json_object_dotget_string(const JSON_Object *object, const char *name) {
    return json_value_get_string(json_object_dotget_value(object, name));
}

size_t json_object_dotget_string_len(const JSON_Object *object, const char *name) {
    return json_value_get_string_len(json_object_dotget_value(object, name));
}

JSON_Object * json_object_dotget_object(const JSON_Object *object, const char *name) {
    return json_value_get_object(json_object_dotget_value(object, name));
}

JSON_Array * json_object_dotget_array(const JSON_Object *object, const char *name) {
    return json_value_get_array(json_object_dotget_value(object, name));
}

double json_object_dotget_number(const JSON_Object *object, const char *name) {
    return json_value_get_number(json_object_dotget_value(object, name));
}

int json_object_dotget_boolean(const JSON_Object *object, const char *name) {
    return json_value_get_boolean(json_object_dotget_value(object, name));
}

size_t json_object_get_count(const JSON_Object *object) {
    return object ? object->count : 0;
}

const char * json_object_get_name(const JSON_Object *object, size_t index) {
    if (object == NULL || index >= json_object_get_count(object)) {
        return NULL;
    }
    return object->names[index];
}

JSON_Value * json_object_get_value_at(const JSON_Object *object, size_t index) {
    if (object == NULL || index >= json_object_get_count(object)) {
        return NULL;
    }
    return object->values[index];
}

JSON_Value *json_object_get_wrapping_value(const JSON_Object *object) {
    if (!object) {
        return NULL;
    }
    return object->wrapping_value;
}

int json_object_has_value (const JSON_Object *object, const char *name) {
    return json_object_get_value(object, name) != NULL;
}

int json_object_has_value_of_type(const JSON_Object *object, const char *name, JSON_Value_Type type) {
    JSON_Value *val = json_object_get_value(object, name);
    return val != NULL && json_value_get_type(val) == type;
}

int json_object_dothas_value (const JSON_Object *object, const char *name) {
    return json_object_dotget_value(object, name) != NULL;
}

int json_object_dothas_value_of_type(const JSON_Object *object, const char *name, JSON_Value_Type type) {
    JSON_Value *val = json_object_dotget_value(object, name);
    return val != NULL && json_value_get_type(val) == type;
}

JSON_Status json_object_set_value(JSON_Object *object, const char *name, JSON_Value *value) {
    unsigned long hash = 0;
    parson_bool_t found = PARSON_FALSE;
    size_t cell_ix = 0;
    size_t item_ix = 0;
    JSON_Value *old_value = NULL;
    char *key_copy = NULL;

    if (!object || !name || !value || value->parent) {
        return JSONFailure;
    }
    hash = hash_string(name, strlen(name));
    found = PARSON_FALSE;
    cell_ix = json_object_get_cell_ix(object, name, strlen(name), hash, &found);
    if (found) {
        item_ix = object->cells[cell_ix];
        old_value = object->values[item_ix];
        json_value_free(old_value);
        object->values[item_ix] = value;
        value->parent = json_object_get_wrapping_value(object);
        return JSONSuccess;
    }
    if (object->count >= object->item_capacity) {
        JSON_Status res = json_object_grow_and_rehash(object);
        if (res != JSONSuccess) {
            return JSONFailure;
        }
        cell_ix = json_object_get_cell_ix(object, name, strlen(name), hash, &found);
    }
    key_copy = parson_strdup(name);
    if (!key_copy) {
        return JSONFailure;
    }
    object->names[object->count] = key_copy;
    object->cells[cell_ix] = object->count;
    object->values[object->count] = value;
    object->cell_ixs[object->count] = cell_ix;
    object->hashes[object->count] = hash;
    object->count++;
    value->parent = json_object_get_wrapping_value(object);
    return JSONSuccess;
}

JSON_Status json_object_set_string(JSON_Object *object, const char *name, const char *string) {
    JSON_Value *value = json_value_init_string(string);
    JSON_Status status = json_object_set_value(object, name, value);
    if (status != JSONSuccess) {
        json_value_free(value);
    }
    return status;
}

JSON_Status json_object_set_string_with_len(JSON_Object *object, const char *name, const char *string, size_t len) {
    JSON_Value *value = json_value_init_string_with_len(string, len);
    JSON_Status status = json_object_set_value(object, name, value);
    if (status != JSONSuccess) {
        json_value_free(value);
    }
    return status;
}

JSON_Status json_object_set_number(JSON_Object *object, const char *name, double number) {
    JSON_Value *value = json_value_init_number(number);
    JSON_Status status = json_object_set_value(object, name, value);
    if (status != JSONSuccess) {
        json_value_free(value);
    }
    return status;
}

JSON_Status json_object_set_boolean(JSON_Object *object, const char *name, int boolean) {
    JSON_Value *value = json_value_init_boolean(boolean);
    JSON_Status status = json_object_set_value(object, name, value);
    if (status != JSONSuccess) {
        json_value_free(value);
    }
    return status;
}

JSON_Status json_object_set_null(JSON_Object *object, const char *name) {
    JSON_Value *value = json_value_init_null();
    JSON_Status status = json_object_set_value(object, name, value);
    if (status != JSONSuccess) {
        json_value_free(value);
    }
    return status;
}

JSON_Status json_object_remove(JSON_Object *object, const char *name) {
    return json_object_remove_internal(object, name, PARSON_TRUE);
}

JSON_Status json_object_clear(JSON_Object *object) {
    size_t i = 0;
    if (object == NULL) {
        return JSONFailure;
    }
    for (i = 0; i < json_object_get_count(object); i++) {
        free(object->names[i]);
        object->names[i] = NULL;
        
        json_value_free(object->values[i]);
        object->values[i] = NULL;
    }
    object->count = 0;
    for (i = 0; i < object->cell_capacity; i++) {
        object->cells[i] = OBJECT_INVALID_IX;
    }
    return JSONSuccess;
}


/*-------------------------------JSON Array API-------------------------------*/

JSON_Value * json_array_get_value(const JSON_Array *array, size_t index) {
    if (array == NULL || index >= json_array_get_count(array)) {
        return NULL;
    }
    return array->items[index];
}

const char * json_array_get_string(const JSON_Array *array, size_t index) {
    return json_value_get_string(json_array_get_value(array, index));
}

size_t json_array_get_string_len(const JSON_Array *array, size_t index) {
    return json_value_get_string_len(json_array_get_value(array, index));
}

JSON_Object * json_array_get_object(const JSON_Array *array, size_t index) {
    return json_value_get_object(json_array_get_value(array, index));
}

double json_array_get_number(const JSON_Array *array, size_t index) {
    return json_value_get_number(json_array_get_value(array, index));
}

JSON_Array * json_array_get_array(const JSON_Array *array, size_t index) {
    return json_value_get_array(json_array_get_value(array, index));
}

int json_array_get_boolean(const JSON_Array *array, size_t index) {
    return json_value_get_boolean(json_array_get_value(array, index));
}

size_t json_array_get_count(const JSON_Array *array) {
    return array ? array->count : 0;
}

JSON_Value * json_array_get_wrapping_value(const JSON_Array *array) {
    if (!array) {
        return NULL;
    }
    return array->wrapping_value;
}

JSON_Status json_array_remove(JSON_Array *array, size_t ix) {
    size_t to_move_bytes = 0;
    if (array == NULL || ix >= json_array_get_count(array)) {
        return JSONFailure;
    }
    json_value_free(json_array_get_value(array, ix));
    to_move_bytes = (json_array_get_count(array) - 1 - ix) * sizeof(JSON_Value*);
    memmove(array->items + ix, array->items + ix + 1, to_move_bytes);
    array->count -= 1;
    return JSONSuccess;
}

JSON_Status json_array_replace_value(JSON_Array *array, size_t ix, JSON_Value *value) {
    if (array == NULL || value == NULL || value->parent != NULL || ix >= json_array_get_count(array)) {
        return JSONFailure;
    }
    json_value_free(json_array_get_value(array, ix));
    value->parent = json_array_get_wrapping_value(array);
    array->items[ix] = value;
    return JSONSuccess;
}

JSON_Status json_array_replace_string(JSON_Array *array, size_t i, const char* string) {
    JSON_Value *value = json_value_init_string(string);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_string_with_len(JSON_Array *array, size_t i, const char *string, size_t len) {
    JSON_Value *value = json_value_init_string_with_len(string, len);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_number(JSON_Array *array, size_t i, double number) {
    JSON_Value *value = json_value_init_number(number);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_boolean(JSON_Array *array, size_t i, int boolean) {
    JSON_Value *value = json_value_init_boolean(boolean);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_null(JSON_Array *array, size_t i) {
    JSON_Value *value = json_value_init_null();
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_clear(JSON_Array *array) {
    size_t i = 0;
    if (array == NULL) {
        return JSONFailure;
    }
    for (i = 0; i < json_array_get_count(array); i++) {
        json_value_free(json_array_get_value(array, i));
    }
    array->count = 0;
    return JSONSuccess;
}

JSON_Status json_array_append_value(JSON_Array *array, JSON_Value *value) {
    if (array == NULL || value == NULL || value->parent != NULL) {
        return JSONFailure;
    }
    return json_array_add(array, value);
}

JSON_Status json_array_append_string(JSON_Array *array, const char *string) {
    JSON_Value *value = json_value_init_string(string);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_append_number(JSON_Array *array, double number) {
    JSON_Value *value = json_value_init_number(number);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_append_boolean(JSON_Array *array, int boolean) {
    JSON_Value *value = json_value_init_boolean(boolean);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_append_null(JSON_Array *array) {
    JSON_Value *value = json_value_init_null();
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) != JSONSuccess) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

/*-------------------------------JSON Value API-------------------------------*/

JSON_Value * json_value_init_object(void) {
    JSON_Value *new_value = (JSON_Value*)malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONObject;
    new_value->value.object = json_object_make(new_value);
    if (!new_value->value.object) {
        free(new_value);
        return NULL;
    }
    return new_value;
}

JSON_Value * json_value_init_array(void) {
    JSON_Value *new_value = (JSON_Value*)malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONArray;
    new_value->value.array = json_array_make(new_value);
    if (!new_value->value.array) {
        free(new_value);
        return NULL;
    }
    return new_value;
}

JSON_Value * json_value_init_string(const char *string) {
    if (string == NULL) {
        return NULL;
    }
    return json_value_init_string_with_len(string, strlen(string));
}

JSON_Value * json_value_init_string_with_len(const char *string, size_t length) {
    char *copy = NULL;
    JSON_Value *value;
    if (string == NULL) {
        return NULL;
    }
    if (!is_valid_utf8(string, length)) {
        return NULL;
    }
    copy = parson_strndup(string, length);
    if (copy == NULL) {
        return NULL;
    }
    value = json_value_init_string_no_copy(copy, length);
    if (value == NULL) {
        free(copy);
    }
    return value;
}

JSON_Value * json_value_init_number(double number) {
    JSON_Value *new_value = NULL;
    if (IS_NUMBER_INVALID(number)) {
        return NULL;
    }
    new_value = (JSON_Value*)malloc(sizeof(JSON_Value));
    if (new_value == NULL) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONNumber;
    new_value->value.number = number;
    return new_value;
}

JSON_Value * json_value_init_boolean(int boolean) {
    JSON_Value *new_value = (JSON_Value*)malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONBoolean;
    new_value->value.boolean = boolean ? 1 : 0;
    return new_value;
}

JSON_Value * json_value_init_null(void) {
    JSON_Value *new_value = (JSON_Value*)malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONNull;
    return new_value;
}

void json_value_free(JSON_Value *value) {
    switch (json_value_get_type(value)) {
        case JSONArray:
            #if !NDEBUG
            printf("free the json array\n");
            #endif
            json_array_free(value->value.array);
            break;
        default:
            break;
    }
    free(value);
}


JSON_Value_Type json_value_get_type(const JSON_Value *value) {
    return value ? value->type : JSONError;
}

JSON_Object * json_value_get_object(const JSON_Value *value) {
    return json_value_get_type(value) == JSONObject ? value->value.object : NULL;
}

JSON_Array * json_value_get_array(const JSON_Value *value) {
    return json_value_get_type(value) == JSONArray ? value->value.array : NULL;
}

static const JSON_String * json_value_get_string_desc(const JSON_Value *value) {
    return json_value_get_type(value) == JSONString ? &value->value.string : NULL;
}

const char * json_value_get_string(const JSON_Value *value) {
    const JSON_String *str = json_value_get_string_desc(value);
    return str ? str->chars : NULL;
}

size_t json_value_get_string_len(const JSON_Value *value) {
    const JSON_String *str = json_value_get_string_desc(value);
    return str ? str->length : 0;
}

double json_value_get_number(const JSON_Value *value) {
    return json_value_get_type(value) == JSONNumber ? value->value.number : 0;
}

int json_value_get_boolean(const JSON_Value *value) {
    return json_value_get_type(value) == JSONBoolean ? value->value.boolean : -1;
}

JSON_Value * json_value_get_parent (const JSON_Value *value) {
    return value ? value->parent : NULL;
}

void json_dump(JSON_Value* value) {
    switch (json_value_get_type(value)) {
        case JSONString: {
            printf("%s", json_value_get_string(value));
            break;
        }
        case JSONNumber: {
            printf("%lf", json_value_get_number(value));
            break;
        }
        case JSONObject: {
            printf("{");
            JSON_Object* object = json_value_get_object(value);
            size_t count = json_object_get_count(object);
            for (size_t index = 0; index < count; index += 1) {
                const char* name = json_object_get_name(object, index);
                printf("%s", name);
                printf(" : ");
                JSON_Value* value = json_object_get_value_at(object, index);
                json_dump(value);
                if (index != count - 1) {
                    printf(", ");
                }
            }
            printf("}");
            break;
        }
        case JSONArray: {
            printf("[");
            JSON_Array* array = json_value_get_array(value);
            size_t count = json_array_get_count(array);
            for (size_t index = 0; index < count; index += 1) {
                JSON_Value* elem = json_array_get_value(array, index);
                json_dump(elem);
                if (index != count - 1) {
                    printf(", ");
                }
            }
            printf("]\n");
            break;
        }
        case JSONBoolean: {
            int boolean = json_value_get_boolean(value);
            if (boolean) {
                printf("true");
            } else {
                printf("false");
            }
            break;
        }
        case JSONNull: {
            printf("null");
            break;
        }
        default:
            printf("Error!\n");
            return;
    }
}