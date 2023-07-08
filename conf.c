#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <pth.h>

#include "conf.h"

#define MAX_LINE_LENGTH 1024
#define DEFAULT_PORT 25
#define INCOMING "/var/spool/virtual"

void *generic_accept(void *_arg);


bool file_exists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        fclose(file);
        return true;
    }
    return false;
}


void remove_square_brackets(char *str) {
	size_t len = strlen(str);
	if (len >= 2 && str[0] == '[' && str[len - 1] == ']') {
		memmove(str, str + 1, len - 2);
		str[len - 2] = '\0';
	}
}

int parse_conf_file(pth_attr_t attr, const char *filename) {
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		printf("Failed to open the configuration file %s.\n", filename);
		return 1;
	}

	char line[MAX_LINE_LENGTH];
	char *token;
	char *saveptr;

	while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
		// Remove leading/trailing whitespace and newlines
		// char *trimmed_line = strtok_r(line, " \t\n", &saveptr);
		char *trimmed_line = strtok_r(line, "\n", &saveptr);

		// Skip comment lines and empty lines
		if (trimmed_line == NULL || trimmed_line[0] == '#' || trimmed_line[0] == '\0') {
			/*printf("Skipping configuration line: %s\n", line);*/
			continue;
		}

		// Look for the 'Listen' directive
		if (strncmp(trimmed_line, "Listen", 6) == 0) {
			/*printf("Parsing configuration line: %s\n", trimmed_line);*/
			// Extract the address and port
			token = strtok_r(trimmed_line, " \t\n", &saveptr);  // Skip the 'Listen' keyword
			/*printf("token(1): %s\n", token);*/
			token = strtok_r(NULL, " \t\n", &saveptr);  // Get the address and port
			/*printf("token(2): %s\n", token);*/
			if (token != NULL) {
				char *address = token;
				struct Address addr;
				strncpy(addr.ip, address, INET6_ADDRSTRLEN);
				addr.port = DEFAULT_PORT;

				// Check if port is specified
				char *port_delimiter = strrchr(token, ':');
				if (port_delimiter != NULL && port_delimiter > token && port_delimiter[-1] != ':') {
					// Port is specified
					*port_delimiter = '\0';  // Null terminate the address
					char *port_str = port_delimiter + 1;
					addr.port = atoi(port_str);
					strncpy(addr.ip, address, INET6_ADDRSTRLEN);
				}

				remove_square_brackets(addr.ip);

				printf("Address: %s, Port: %d\n", addr.ip, addr.port);
				pth_spawn(attr, generic_accept, (void*)&addr);
				// Yield, because we're going to overwrite addr
				pth_yield(NULL);
			}
		}
		else {
			/*printf("Skipping (2) configuration line: %s\n", line);*/
		}
	}

	fclose(file);
	return 0;
}

char* get_absolute_path(const char* relative_path) {
	char* absolute_path = malloc(MAX_LINE_LENGTH * sizeof(char));
	if (absolute_path == NULL) {
		perror("Memory allocation failed");
		return NULL;
	}

	if (realpath(relative_path, absolute_path) == NULL) {
		perror("Failed to get absolute path");
		free(absolute_path);
		return NULL;
	}

	return absolute_path;
}



int do_parse_conf_file(pth_attr_t attr, const char* argv0) {

	char executable_path[MAX_LINE_LENGTH];
	strncpy(executable_path, argv0, MAX_LINE_LENGTH);
	char *last_separator = strrchr(executable_path, '/');
	if (last_separator != NULL) {
		*(last_separator + 1) = '\0';  // Null terminate after the last separator
	} else {
		strcpy(executable_path, "./");  // No directory path, use current directory
		/* char cwd[MAX_LINE_LENGTH]; */
		/* if (getcwd(cwd, sizeof(cwd)) != NULL) { */
		/* printf("Current working directory: %s\n", cwd); */
		/* } else { */
		/* perror("getcwd() error"); */
		/* return 1; */
		/* } */
		/* strncpy(executable_path, cwd, MAX_LINE_LENGTH); */
	}

	// Construct the path to the configuration file
	const char *conf_filename = "dummymail.conf";
	char conf_path[MAX_LINE_LENGTH];



	strncpy(conf_path, executable_path, MAX_LINE_LENGTH);
	strncat(conf_path, conf_filename, MAX_LINE_LENGTH - strlen(executable_path) - 1);

	char* absolute_path = get_absolute_path(conf_path);

	if (absolute_path != NULL) {
		printf("Absolute path: %s\n", absolute_path);
		strncpy(conf_path, absolute_path, MAX_LINE_LENGTH);
		free(absolute_path);
	}

    if (!file_exists(conf_path)) {
        strncpy(conf_path, "/etc/dummymail.conf", MAX_LINE_LENGTH);
        if (!file_exists(conf_path)) {
            fprintf(stderr, "%s not found\n", conf_path);
            exit(4);
        }
    }


	if (chdir(INCOMING)) {
		perror("chdir");
		exit(3);
	}

	// Parse the configuration file
	parse_conf_file(attr, conf_path);

	return 0;
}
/* vim: set ts=3 sts=0 sw=3 noet: */
