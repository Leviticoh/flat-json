#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<limits.h>

#define BUFSIZE 4096

#define NOFILE -1


#define EERRNO INT_MIN
#define EHALT  0
#define ENCOMP 1
#define ESYNT  2

#define FJNULL  0
#define OBJECT  1
#define ARRAY   2
#define STRING  3
#define NUMBER  4
#define BOOLEAN 5
#define ERR_T   6


extern int errno;
int FJ_errno;

typedef struct {
	uint_fast8_t type;
	union{
		json_object* object;
		json_array*  array;
		char*        string;
		double       number;
		bool         boolean;
		error_t      error;
	} value;
} json_value;

typedef struct {
	int code;
	int type;
} error_t;

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


/*buffer management functions*/



ssize_t eob_callback(char* buffer, ssize_t* offset, int fd)
{
	ssize_t ret;
	if(fd == NOFILE){
		FJ_errno = ENCOMP;
		return -1;
	}

	ret = read(fd, buffer, BUFSIZE);
	if(ret == 0){
		FJ_errno = ENCOMP;
		return -1;
	}

	FJ_errno = EERRNO;

	if(ret > -1 && ret < BUFSIZE){
		buffer[ret] = '\0';
	}

	*offset = 0;
	return ret;
}

json_value json_save_error()
{
	json_value ret;
	ret.type = ERR_T;
	if(FJ_errno == EERRNO){
		ret.value.error.code = errno;
		ret.value.error.type = 0;
	}
	else{
		ret.value.error.code = FJ_errno;
		ret.value.error.type = 1;
	}
	return ret;
}

/*freeing functions*/

void json_free_value(json_value target)
{
	switch(target.type){
		case OBJECT:
			json_free_object(target.value.object);
			break;
		case ARRAY:
			json_free_array(target.value.array);
			break;
		case STRING:
			free(target.value.string);
			break;
		default:
			return;
	}
}

void json_free_element(json_element target)
{
	free(target.name);
	json_free_value(target.content);
}

void json_free_object(json_object* target)
{
	json_free_element(target->elem);
	json_free_object(target->next);
	free(target);
}

void json_free_array(json_array* target)
{
	json_free_value(target->value);
	json_free_array(target->next);
	free(target);
}

/*trimming functions*/

ssize_t json_trim_whitespace(char* buffer, ssize_t* offset, int fd)
{
	if((*offset) == BUFSIZE || buffer[*offset] == '\0'){
		if(eob_callback(buffer, offset, fd) == -1){
			return -1;
		}
	}
	while(
		buffer[*offset] == ' ' ||
		buffer[*offset] == '\t' ||
		buffer[*offset] == '\n' ||
		buffer[*offset] == '\r'
		){

		(*offset) += 1;
		if((*offset) == BUFSIZE || buffer[*offset] == '\0'){
			if(eob_callback(buffer, offset, fd) == -1){
				return -1;
			}
		}
	}
	return *offset;
}

ssize_t json_trim_comma(char* buffer, ssize_t* offset, int fd)
{
	if(json_trim_whitespace(buffer, offset, fd) == -1)
		return -1;

	if(buffer[*offset] == ','){
		(*offset) += 1;
		if((*offset) == BUFSIZE || buffer[*offset] == '\0'){
			if(eob_callback(buffer, offset, fd) == -1){
				return -1;
			}
		}
	}

	if(json_trim_whitespace(buffer, offset, fd) == -1)
		return -1;
}

/*parsing functions*/

json_value json_parse_array(char* buffer, ssize_t* offset, int fd)
{
	json_value ret;
	ret.type = ARRAY;

	(*offset) += 1;
	if((*offset) == BUFSIZE || buffer[*offset] == 0){
		if(eob_callback(buffer, offset, fd) == -1){
			ret = json_save_error();
			return ret;
		}
	}
	if(json_trim_whitespace(buffer, offset, fd) == -1){
		ret = json_save_error();
		return ret;
	}

	json_array* arr = NULL;
	json_array* tmp = NULL;
	json_array* tip = NULL;

	while(buffer[*offset] != ']'){
		tmp = (json_array*)malloc(sizeof(json_array));
		tmp->value = json_parse_value();
		if(arr == NULL){
			arr = tmp;
			tip = tmp;
		}
		else{
			tip->next = tmp;
			tip = tmp;
		}
		tip->next = NULL;
		if(tmp->value.type == ERR_T){
			ret = tmp->value;
			json_free_array(arr);
			return ret;
		}
		if(json_trim_comma(buffer, offset, fd) == -1){
			ret = json_save_error();
			json_free_array(arr);
			return ret;
		}
	}

	(*offset) += 1;
	ret.value.array = arr;
	return ret;
}

json_value json_parse_object(char* buffer, ssize_t* offset, int fd)
{
	json_value ret;
	ret.type = OBJECT;

	(*offset) += 1;
	if((*offset) == BUFSIZE || buffer[*offset] == 0){
		if(eob_callback(buffer, offset, fd) == -1){
			ret = json_save_error();
			return ret;
		}
	}
	if(json_trim_whitespace(buffer, offset, fd) == -1){
		ret = json_save_error();
		return ret;
	}

	json_object* obj = NULL;
	json_object* tmp = NULL;

	while(buffer[*offset] == '"'){
		tmp = (json_object*)malloc(sizeof(json_object));
		tmp->elem = json_parse_element();
		tmp->next = obj;
		obj = tmp;
		if(tmp->elem.content.type == ERR_T){
			ret = tmp->elem.content;
			json_free_object(obj);
			return ret;
		}
		if(json_trim_comma(buffer, offset, fd) == -1){
			ret = json_save_error();
			json_free_object(obj);
			return ret;
		}
	}
	if (buffer[*offset] != '}'){
		FJ_errno = ESYNT;
		ret = json_save_error();
		json_free_object(obj);
		return ret;
	}

	(*offset) += 1;
	ret.value.object = obj;
	return ret;
}

json_value json_parse_string(char* buffer, ssize_t* offset, int fd)
{
}

json_element json_parse_element(char* buffer, ssize_t* offset, int fd)
{
	json_element ret;
	json_value name = json_parse_string(buffer, offset, fd);
	if(name.type == ERR_T){
		ret.content = name;
		return ret;
	}

	if(json_trim_whitespace(buffer, offset, fd) == -1){
		ret.content = json_save_error();
		return ret;
	}
	if(buffer[*offset] != ':'){
		FJ_errno = ESYNT;
		ret.content = json_save_error();
		return ret;
	}
	(*offset) += 1;
	if((*offset) == BUFSIZE || buffer[*offset] == 0){
		if(eob_callback(buffer, offset, fd) == -1){
			ret.content = json_save_error();
			return ret;
		}
	}
	if(json_trim_whitespace(buffer, offset, fd) == -1){
		ret.content = json_save_error();
		return ret;
	}
	ret.value = json_parse_value(buffer, offset, fd);
	return ret;
}

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
