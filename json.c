#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>

typedef struct {
	uint_fast8_t type;
	union{
		json_object* object;
		json_array*  array;
		char*        string;
		double       number;
		bool         boolean;
	} value;
} json_value;

typedef struct {
	char* name;
	json_value content;
} json_element;

typedef struct {
	json_element elem;
	json_object* next;
} json_object;

typedef struct {
	json_value value;
	json_array* next;
} json_array;

json_value json_parse_value(char* buffer, ssize_t* offset, int fd)
{
	switch(buffer[*offset]){
		case '{':
			return json_parse_object(buffer, offset, fd);
			break;
		case '[':
			return json_parse_array(buffer, offset, fd);
			break;
		case '"':
			return json_parse_string(buffer, offset, fd);
			break;
		case 't':
		case 'f':
		case 'n':
			return json_parse_atom(buffer, offset, fd);
			break;
		default:
			return json_parse_gen(buffer, offset, fd);
	}
}

json_value parse_json(char* buffer, ssize_t* offset, int fd)
{
	return json_parse_value(buffer, offset, fd);
}
