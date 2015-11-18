//This code creates a C array for each resource file.
//This way data can be embedded right into the executeable instead of depending on external files.
//The portable alternative to compiler specific resource packaging.
//
//Compile and run this little helper tool before the main project.
//A *.c and *.h file is generated which you can link to your project.
//In the current directory a file "list" shall contain the following: 
//  line 1-end: a file path per line
//   empty lines are skipped
//   excess spaces are not omitted
//
//Files to embed should be small.
//Generated *.c file becomes 5 times as big because of hexadecimal coding.
//Normal block scope arrays are size limited by the stack size
//  There are ways around that (to implement)
//   by splitting single files into multiple arrays and block scopes and joining them in an allocated array by an init function
//     -> doubles total memory requirement
//
//Options:
// -h, --help        : Print usage
// -l, --list <path> : Path to list input file (CWD/list by default) (dir or direct path file)
// -o, --out <path>  : Path to output files + file base name ("foo/myresource" outputs as "foo/myresource.h" and "foo/myresource.c")


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dict.h"

#define VERSION "2015-11-15"




FILE *fin = NULL;
FILE *fout_c = NULL;
FILE *fout_h = NULL;


void print_help(char *argv0){
	printf("%s [Option]\n", argv0);
	printf(
		"Options:\n"
		" -h, --help        : Print help\n"
		" -l, --list <path> : Path to list input file (CWD/list by default)\n"
		" -o, --out <path>  : Path to output files + file base name\n"
	);
}

//open file
FILE *open_file(char *base_path, char *suffix, char *mode){
	char ts[1000] = ""; //temp string
	int left = 999;
	strncpy(ts, base_path, left);
	left -= strlen(base_path);
	strncat(ts, suffix, left);
	left -= strlen(suffix);
	printf("Opening file: \"%s\" (mode: \"%s\")\n", ts, mode);
	
	FILE *f = fopen(ts, mode);
	if(f == NULL)
		fprintf(stderr, "Failed to open file \"%s\"!\n", ts);
	else
		printf("File opened.\n");
	
	return f;
}

//change all '\\' to '/'
//s must be a terminated string
void streamline_path(char *s){
	int len = strlen(s);
	for(int i = 0; i < len; i++)
		if(s[i] =='\\')
			s[i] = '/';
}

//get base name of file
//"foo/myfile.c" => "myfile"
char basename[1000] = "";
char *get_file_base_name(char *s){
	char *r = strrchr(s, '/'); //last '/'
	if(r != NULL)
		r = r + 1;
	else
		r = s;
	strncpy(basename, r, 999);
	
	r = strrchr(basename, '.'); //last '.'
	if(r != NULL) {
		int len = r - basename;
		basename[len] = '\0';
	}

	//replace with underscore
	return basename;
}


//returns 1 if letter (upper/lower) or underscore, else 0
int is_letter(int v) {
	return (v == '_'  ||  (v >= 0x41  &&  v <= 0x5A)  ||  (v >= 0x61  &&  v <= 0x7A) );
}

//returns 1 if digit, else 0
int is_digit(int v){
	return (v >= 0x30  &&  v <= 0x39);
}


//return valid C variable/array name
//invalid chars are replaced with '_'
char varname[1000];
char *make_var_name(char *s){
	varname[0] = '\0';
	//if(is_digit(s[0]))
	//	strcat(varname, "_"); //prepend '_'
	strcat(varname, "res_"); //prepend 'res_'
	strncat(varname, s, 998);
	//check and replace if needed
	int len = strlen(varname);
	for(int i = 0; i < len; i++)
		if(!is_digit(varname[i])  &&  !is_letter(varname[i]))
			varname[i] = '_';
	return varname;
}





int main (int argc, char *argv[]){
	char *path_list = "";
	char *path_out = "";
	char h_guard[1000] = "";
	
	//check for parameters - file paths, else we assume CWD
	for(int i = 1; i < argc ; i ++) {
		if(strcmp(argv[i], "-l") == 0  ||  strcmp(argv[i], "--list") == 0 ) {
			i++;
			if(i >= argc) {
				fprintf(stderr, "Too few arguments!\n");
				exit(EXIT_FAILURE);
			}
			path_list = argv[i];
		}
		else if(strcmp(argv[i], "-o") == 0  ||  strcmp(argv[i], "--out") == 0) {
			i++;
			if(i >= argc) {
				fprintf(stderr, "Too few arguments!\n");
				exit(EXIT_FAILURE);
			}
			path_out = argv[i];
		}
		else if(strcmp(argv[i], "-h") == 0  ||  strcmp(argv[i], "--help") == 0) {
			print_help(argv[0]);
			exit(EXIT_SUCCESS);
		}
		else {
			fprintf(stderr, "Unknown argument: \"%s\"\n", argv[i]);
			exit(EXIT_FAILURE);
		}
	}
	
	
	//make sure we only use '/' as dir separator
	streamline_path(path_list);
	streamline_path(path_out);
	
	//a set would be enough but a dict/map works also - actually we only need sorted keys for faster access
	struct dict *dfiles = dict_create(100); //to skip files we already have (same path)
	struct dict *dvars = dict_create(100);  //to avoid duplicate array name, we add a number
	
	
	//open list file
	//first try direct path then path/list
	char ts[1000] = ""; //temp string
	FILE *fin = open_file(path_list, "", "r");
	if(fin == NULL) {
		int left = 999; //free string buffer, to prevent string buffer overflow
		if(strlen(path_list) == 0){
			strncat(ts, ".", left);
			left -= 1;
		}
		else {
			strncat(ts, path_list, left);
			left -= strlen(path_list);
		}
		strncat(ts, "/list", left);
		left -= 5;
		fin = open_file(ts, "", "r");
	}
	if(fin == NULL){
		fprintf(stderr, "Failed to open list file: \"%s\"\n", path_list);
		exit(EXIT_FAILURE);
	}
	printf("List file opened\n\n");
	
	if(strlen(path_out) == 0)
		printf("Warning, output base name is empty!\n");
	
	//generate array per file
	int count_file = 0;
	int count_line = 0;
	int count_size = 0;
	char *r;
	r = fgets(ts, 1000, fin);
	
	while(r != NULL) {
		count_line ++; //starts at 1
		
		//remove newline
		char *n;
		n = strchr(r, '\n');
		if(n != NULL)
			r[n-r] = '\0';
		n = strchr(r, '\r');
		if(n != NULL)
			r[n-r] = '\0';
		//ts now contains file path with file name
		

		if(strlen(ts) != 0) { //no empty line
				
			if(dict_get(dfiles, ts) != NULL) {
				printf("File \"%s\" in line %d already processed! Skipping.\n", ts, count_line);
			}
			else {
				dict_put(dfiles, ts, ""); //remember file path
				
				if(fout_h == NULL) { //if h file not opened yet
					fout_h = open_file(path_out, ".h", "w");
					if(fout_h == NULL)
						exit(EXIT_FAILURE);
					sprintf(h_guard, "_%s_H_", get_file_base_name(path_out));
					fputs ("//Automatically generated by file2array, do not edit!\n\n", fout_h);
					fputs ("//include guard\n", fout_h);
					fputs ("#ifndef ", fout_h);
					fputs (h_guard, fout_h);
					fputs ("\n#define ", fout_h);
					fputs (h_guard, fout_h);
					fputs ("\n\n", fout_h);
					fputs ("#define RESOURCE_IN_ARRAY\n\n", fout_h);
				}
				if(fout_c == NULL) { //if c file not opened yet
					fout_c = open_file(path_out, ".c", "w");
					if(fout_c == NULL)
						exit(EXIT_FAILURE);
					fputs ("//Automatically generated by file2array, do not edit!\n\n", fout_c);
					printf("\n");
				}
				
				
				
				printf("file %d: \"%s\"\n", count_file, ts);
				count_file ++;
				
				//open input file
				FILE *fdata;
				fdata = open_file(ts, "", "rb");
				if(fdata == NULL) {
					fprintf(stderr, "Can't open file! line: %d!\n", count_line);
					printf("Incomplete!\n");
					exit(EXIT_FAILURE);
				}
				
				char *var_name = make_var_name(ts);
				unsigned char buf[2];
				int c = 0;
				int buffered = fread(buf, 1, 2, fdata); //we try to prebuffer 1 byte, so we can say if data follows the currently processed byte
				int fsize = 0;
				
				if(dict_get(dvars, var_name) != NULL) { //if var name already used
					char var_name_new_buf[1000];
					int i = 0;
					do {
						sprintf(var_name_new_buf, "%s_%d", var_name, i); //compose new name
						i++;
					} while (dict_get(dvars, var_name_new_buf) != NULL);
					var_name = var_name_new_buf;
				}
				dict_put(dvars, var_name, "");
				
				fprintf(fout_c, "unsigned char %s[] = {\n", var_name);
				
				while(buffered){
					fprintf(fout_c, "0x%02X", buf[0]);
					buffered -= 1;
					fsize += 1;
					if(buffered > 0) { //if there is more data
						fputs (",", fout_c);
						c++;
						if(c >= 16) { //bytes per line in *.c file
							fputs ("\n", fout_c);
							c = 0;
						}
					}
					buf[0] = buf[1];
					buffered += fread(buf+1, 1, 1, fdata);
				}
				count_size += fsize;
				fputs ("\n}\n", fout_c);
				fclose(fdata);
				printf("size: %d\n", fsize);
				fprintf(fout_h, "//source: \"%s\"  size: %d\n", ts, fsize);
				fprintf(fout_h, "extern unsigned char %s[%d];\n\n", var_name, fsize);
				
			}
		}
		
		r = fgets(ts, 1000, fin);
	}
	
	
	/*
	//test
	struct dict *d = dict_create(100);
	dict_put(d, "hello", "welcome");
	dict_put(d, "hello2", "yippie");
	struct nlist *n;
	n = dict_get(d, "hello");
	printf("%s %s\n", n->key, n->val);
	n = dict_get(d, "hello2");
	printf("%s %s\n", n->key, n->val);
	dict_free(d);
	*/
	
	//test
	//printf("%s\n", make_var_name("a+-.:1234"));
	//printf("%s\n", get_file_base_name("foo/myfile.cpp"));
	
	
	
	fputs ("\n#endif\n", fout_h); //close include guard
	
	printf("\n");
	printf("Num. arrayfied files: %d\n", count_file);
	printf("Total bytes in generated arrays: %d\n", count_size);
	
	
	//done, clean up
	dict_free(dfiles);
	dict_free(dvars);
	
	if(fin != NULL)
		fclose(fin);
	if(fout_c != NULL)
		fclose(fout_c);
	if(fout_h != NULL)
		fclose(fout_h);
	
	printf("No errors. Done.\n");
}
