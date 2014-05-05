/*
 *  help.h
 *  SMCP
 *
 *  Created by Robert Quattlebaum on 8/17/10.
 *  Copyright 2010 deepdarc. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>

typedef struct {
	char		shortarg;
	const char* longarg;
	const char* param;
	const char* desc;
} arg_list_item_t;

static inline void
print_arg_list_help(
	const arg_list_item_t	arg_list[],
	const char*				command_name,
	const char*				syntax
) {
	int i;
	const int desc_margin = 25;

	printf("Syntax:\n");
	printf("   %s %s\n", command_name, syntax);
	printf("Options:\n");
	for(i = 0; arg_list[i].desc; ++i) {
		int curr_desc_margin = desc_margin;

		if(arg_list[i].shortarg)
			printf("   -%c", arg_list[i].shortarg);
		else
			printf("     ");

		curr_desc_margin -= 5;

		if(arg_list[i].longarg) {
			if(arg_list[i].shortarg)
				printf("/");
			else
				printf(" ");

			printf( "--%s", arg_list[i].longarg);

			curr_desc_margin -= strlen(arg_list[i].longarg) + 4;
		} else {
			curr_desc_margin -= 1;
		}

		if(arg_list[i].param) {
			printf(" <%s>", arg_list[i].param);
			curr_desc_margin -= strlen(arg_list[i].param) + 3;
		}

		if(curr_desc_margin<=0) {
			printf("\n");
			curr_desc_margin = desc_margin;

			for(;curr_desc_margin>1;curr_desc_margin--)
				printf(" ");
		} else {
			if(curr_desc_margin-->0)
				printf(" ");

			for(;curr_desc_margin>0;curr_desc_margin--)
				printf(".");
		}

		printf(" %s\n", arg_list[i].desc);
	}
}


#define BEGIN_LONG_ARGUMENTS(ret)   \
    for(i = 1; i < argc; i++) { \
		if(argv[i][0] == '-' && argv[i][1] == '-') {    \
			if(false) { \
			}

#define HANDLE_LONG_ARGUMENT(str)   \
    else if(strcmp(argv[i] + 2, str) == 0)

#define BEGIN_SHORT_ARGUMENTS(ret)  \
    else { \
		fprintf(stderr, \
			"%s: error: Unknown command line argument \"%s\".\n", \
			argv[0], \
			argv[i]); \
		ret = ERRORCODE_BADARG; \
		goto bail; \
	} \
	} else if(argv[i][0] == '-') { \
		int j; \
		int ii = i; \
		for(j = 1; j && argv[ii][j]; j++) { \
			switch(argv[ii][j]) { \
			default: \
				fprintf(stderr, \
					"%s: error: Unknown command line argument \"-%c\".\n", \
					argv[0], \
					argv[ii][j]); \
				ret = ERRORCODE_BADARG; \
				goto bail;

#define HANDLE_SHORT_ARGUMENT(chr)  \
    break; case chr:

#define HANDLE_SHORT_ARGUMENT2(chr, chr2)    \
    break; case chr: case chr2:

#define HANDLE_OTHER_ARGUMENT() \
    break; \
	} \
	} \
	} else

#define END_ARGUMENTS \
	}
