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

/*trimming functions*/

ssize_t json_trim_whitespace(char* buffer, ssize_t* offset, int fd)
{
	ssize_t start = *offset;
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
	return (*offset)-start;
}

/*parsing functions*/

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
