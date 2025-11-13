#ifndef PARSON_H
#define PARSON_H

#define PARSON_VERSION_MAJOR 1
#define PARSON_VERSION_MINOR 5
#define PARSON_VERSION_PATCH 3

#define PARSON_VERSION_STRING "1.5.3"

#include <stddef.h>

/* Types and enums */
typedef struct json_object_t    JSON_Object;
typedef struct json_array_t  	JSON_Array;
typedef struct json_value_t 	JSON_Value;

enum json_value_type {
	JSONError   = -1,
	JSONNull    = 1,
    JSONString  = 2,
	JSONNumber  = 3,
	JSONObject  = 4,
	JSONArray   = 5,
	JSONBoolean = 6
};
typedef int JSON_Value_Type;

enum json_result_t {
	JSONSuccess = 0,
	JSONFailure = -1
};
typedef int JSON_Status;

JSON_Value* json_parse_file(const char *filename);
JSON_Value* json_parse_string(const char *string);

int  json_value_equals(const JSON_Value *a, const JSON_Value *b);

JSON_Status json_validate(const JSON_Value *schema, const JSON_Value *value);

/*------------------------------- JSON Object --------------------------------*/
JSON_Value  * json_object_get_value  (const JSON_Object *object, const char *name);
const char  * json_object_get_string (const JSON_Object *object, const char *name);
size_t        json_object_get_string_len(const JSON_Object *object, const char *name); /* doesn't account for last null character */
JSON_Object * json_object_get_object (const JSON_Object *object, const char *name);
JSON_Array  * json_object_get_array  (const JSON_Object *object, const char *name);
double        json_object_get_number (const JSON_Object *object, const char *name); /* returns 0 on fail */
int           json_object_get_boolean(const JSON_Object *object, const char *name); /* returns -1 on fail */

JSON_Value  * json_object_dotget_value  (const JSON_Object *object, const char *name);
const char  * json_object_dotget_string (const JSON_Object *object, const char *name);
size_t        json_object_dotget_string_len(const JSON_Object *object, const char *name); /* doesn't account for last null character */
JSON_Object * json_object_dotget_object (const JSON_Object *object, const char *name);
JSON_Array  * json_object_dotget_array  (const JSON_Object *object, const char *name);
double        json_object_dotget_number (const JSON_Object *object, const char *name); /* returns 0 on fail */
int           json_object_dotget_boolean(const JSON_Object *object, const char *name); /* returns -1 on fail */

size_t        json_object_get_count   (const JSON_Object *object);
const char  * json_object_get_name    (const JSON_Object *object, size_t index);
JSON_Value  * json_object_get_value_at(const JSON_Object *object, size_t index);
JSON_Value  * json_object_get_wrapping_value(const JSON_Object *object);

int json_object_has_value        (const JSON_Object *object, const char *name);
int json_object_has_value_of_type(const JSON_Object *object, const char *name, JSON_Value_Type type);

int json_object_dothas_value        (const JSON_Object *object, const char *name);
int json_object_dothas_value_of_type(const JSON_Object *object, const char *name, JSON_Value_Type type);

JSON_Status json_object_set_value(JSON_Object *object, const char *name, JSON_Value *value);
JSON_Status json_object_set_string(JSON_Object *object, const char *name, const char *string);
JSON_Status json_object_set_string_with_len(JSON_Object *object, const char *name, const char *string, size_t len);  /* length shouldn't include last null character */
JSON_Status json_object_set_number(JSON_Object *object, const char *name, double number);
JSON_Status json_object_set_boolean(JSON_Object *object, const char *name, int boolean);
JSON_Status json_object_set_null(JSON_Object *object, const char *name);

JSON_Status json_object_remove(JSON_Object *object, const char *name);

JSON_Status json_object_clear(JSON_Object *object);

/*------------------------------- JSON Array --------------------------------*/
JSON_Value  * json_array_get_value  (const JSON_Array *array, size_t index);
const char  * json_array_get_string (const JSON_Array *array, size_t index);
size_t        json_array_get_string_len(const JSON_Array *array, size_t index); /* doesn't account for last null character */
JSON_Object * json_array_get_object (const JSON_Array *array, size_t index);
JSON_Array  * json_array_get_array  (const JSON_Array *array, size_t index);
double        json_array_get_number (const JSON_Array *array, size_t index); /* returns 0 on fail */
int           json_array_get_boolean(const JSON_Array *array, size_t index); /* returns -1 on fail */
size_t        json_array_get_count  (const JSON_Array *array);
JSON_Value  * json_array_get_wrapping_value(const JSON_Array *array);

JSON_Status json_array_remove(JSON_Array *array, size_t i);

JSON_Status json_array_replace_value(JSON_Array *array, size_t i, JSON_Value *value);
JSON_Status json_array_replace_string(JSON_Array *array, size_t i, const char* string);
JSON_Status json_array_replace_string_with_len(JSON_Array *array, size_t i, const char *string, size_t len); /* length shouldn't include last null character */
JSON_Status json_array_replace_number(JSON_Array *array, size_t i, double number);
JSON_Status json_array_replace_boolean(JSON_Array *array, size_t i, int boolean);
JSON_Status json_array_replace_null(JSON_Array *array, size_t i);

JSON_Status json_array_clear(JSON_Array *array);

JSON_Status json_array_append_value(JSON_Array *array, JSON_Value *value);
JSON_Status json_array_append_string(JSON_Array *array, const char *string);
JSON_Status json_array_append_number(JSON_Array *array, double number);
JSON_Status json_array_append_boolean(JSON_Array *array, int boolean);
JSON_Status json_array_append_null(JSON_Array *array);

/*------------------------------- JSON Value --------------------------------*/
JSON_Value * json_value_init_object (void);
JSON_Value * json_value_init_array  (void);
JSON_Value * json_value_init_string (const char *string); /* copies passed string */
JSON_Value * json_value_init_string_with_len(const char *string, size_t length); /* copies passed string, length shouldn't include last null character */
JSON_Value * json_value_init_number (double number);
JSON_Value * json_value_init_boolean(int boolean);
JSON_Value * json_value_init_null   (void);
void         json_value_free        (JSON_Value *value);

JSON_Value_Type json_value_get_type   (const JSON_Value *value);
JSON_Object *   json_value_get_object (const JSON_Value *value);
JSON_Array  *   json_value_get_array  (const JSON_Value *value);
const char  *   json_value_get_string (const JSON_Value *value);
size_t          json_value_get_string_len(const JSON_Value *value); /* doesn't account for last null character */
double          json_value_get_number (const JSON_Value *value);
int             json_value_get_boolean(const JSON_Value *value);
JSON_Value  *   json_value_get_parent (const JSON_Value *value);

void json_dump(JSON_Value* value);


#endif
