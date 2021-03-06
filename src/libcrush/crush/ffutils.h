/*****************************************
   Copyright 2008 Google Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *****************************************/

/** @file ffutils.h
  * @brief some utilities for working with flat files.
  * @author jeremy hinds
  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef FFUTILS_H
#define FFUTILS_H

/** @brief string comparison - equals */
#define str_eq(a, b) \
	( strcmp(a, b) == 0 )

/** @brief string comparison - less than */
#define str_lt(a, b) \
	( strcmp(a, b) < 0 )

/** @brief string comparison - greater than */
#define str_gt(a, b) \
	( strcmp(a, b) > 0 )

/** @brief MM-DD-YYYY string date comparison - equals */
#define date_eq(a, b) \
	( mdyhms_datecmp(a, b) == 0 )

/** @brief MM-DD-YYYY string date comparison - less than */
#define date_lt(a, b) \
	( mdyhms_datecmp(a, b) < 0 )

/** @brief MM-DD-YYYY string date comparison - greater than */
#define date_gt(a, b) \
	( mdyhms_datecmp(a, b) > 0 )


/** @brief a duff device for unrolling loops by a factor of 8.
  * @param n the number of times to execute the code.
  * @param c the code to be executed.
  */
#define DUFF_DEV_8( n, c ) \
        do {                                    \
                int dd8_i = (n)/8;              \
                if ( ! (n) )                    \
                        break;                  \
                switch((n)%8) {                 \
                        case 0: do {    (c);    \
                        case 7:         (c);    \
                        case 6:         (c);    \
                        case 5:         (c);    \
                        case 4:         (c);    \
                        case 3:         (c);    \
                        case 2:         (c);    \
                        case 1:         (c);    \
                                } while ( dd8_i-- > 0 ); \
                }                               \
        } while (0)

/** @brief the amount by which expand_nums() will increment the size of
  * the target array when reallocating memory.
  */
#define FFUTILS_RESIZE_AMT 16

/** @brief finds the number of fields in a delimited string.
  *
  * @param l a delimited string
  * @param d delimiter
  *
  * @return the number of fields in the string
  */
size_t fields_in_line(const char *l, const char *d);

/** @brief extract a field from a delimited string.
  *
  * copies at most <i>n</i> characters from field <i>i</i> of
  * <i>delim</i>-delimited string <i>ct</i> into <i>cs</i>.
  *
  * @param dest destination buffer
  * @param line delimited string
  * @param n max chars to copy into buffer
  * @param i field to be copied (0-based)
  * @param delim delimiter of ct
  *
  * @return number of chars copied into the dest buffer, or -1 if i is greater
  * than the number of fields in line.
  */
int get_line_field(char *cs, const char *ct, size_t n, int i,
                   const char *delim);

/** @brief returns a pointer into a delimited string where a particular
  * field begins.
  *
  * @param line a delimited string
  * @param fn the desired field number (1-based)
  * @param delim the delimiting string
  *
  * @return a pointer into cs where the fn-th field begins, or NULL if
  *  the field does not exist.
  */
char *field_start(const char * const line, size_t fn, const char *delim);

/** @brief compares two date strings in the format MM-DD-YYYY_HH24:MI:SS
  *
  * @param a first date
  * @param b second date
  *
  * @return -1 if a < b; 0 if a == b; 1 if a > b.
  */
int mdyhms_datecmp(const char *a, const char *b);

/** @brief compares two date strings in the format DD-MM-YYYY_HH24:MI:SS
  * 
  * @param a first date
  * @param b second date
  * 
  * @return -1 if a < b; 0 if a == b; 1 if a > b.
  */
int dmyhms_datecmp(const char *a, const char *b);

/** @brief removes linebreak characters from the end of a string.
  * @param s a null-terminated array of characters
  */
void chomp(char *s);

/** @brief replaces trailing white space in a string with null characters.
  *
  * @param str the string to be trimmed.
  */
void trim(char *str);

/** @brief goes through a list of file names, attempting to open them.
  * it loops through trying to open each file until one is successfully
  * opened or there are no more file names in the list.
  *
  * If the filename "-" is in the list, either stdin or stdout will be
  * returned.
  * 
  * @param argc the number of arguments from the commandline
  * @param argv the array of arguments from the commandline
  * @param optind the index if the next argument to be processed
  * @param mode the mode for opening the file
  * 
  * @return the next successfully opened file or NULL
  */
FILE *nextfile(int argc, char *argv[], int *optind, const char *mode);

/** @brief expands escaped special characters within a string, modifying
  * the original string.
  * 
  * the following are valid escape sequences and the ascii character
  * to which they expand:
  *
  *  - \\a - 0x07
  *  - \\b - 0x08
  *  - \\f - 0x0c
  *  - \\n - 0x0a
  *  - \\r - 0x0d
  *  - \\t - 0x09
  *  - \\v - 0x0b
  *  - \\\\ - 0x5c
  *
  * @param s string potentially containing escaped characters
  */
void expand_chars(char *s);

/** @brief splits a string of number lists/ranges into an array.
  *
  * The "arg" string should be dynamically allocated, since strtok()
  * doesn't like const char[];
  *
  * "array" should also be dynamically allocated, as it may be realloc()'d
  * to accomodate more elements.  If memory hasn't been allocated for array,
  * set the pointer to NULL and pass 0 as array_size.  Memory will be
  * allocated for you, but remember to call free() on array when finished
  * with it.
  *
  * All of the parameters may be modified during execution.
  *
  * @param arg string representation of numbers
  * @param array pointer to array of ints
  * @param array_size number of elements array can currently hold
  *
  * @return on success, the number of items in populated array,
  *     -1 on memory allocation error,
  * 	-2 if arg has invalid syntax
  */
ssize_t expand_nums(char *arg, int **array, size_t * array_size);

/** @brief populates an array with 1-based indexes of the location of each of
  * a list of labels.
  *
  * @param labels comma-separated list of labels
  * @param line the header line
  * @param delim field separator used in the line
  * @param array the array in which to store the indexes
  * @param array_sz the current size of the array
  *
  * @returns the number of indexes found on success,
  *          -1 if one or more labels were not found in the line, or
  *          -2 on error.
  */
ssize_t expand_label_list(const char *labels,
                          const char *line,
                          const char *delim,
                          int **array, size_t *array_sz);

/** @brief determines the position of the first and the last character
  * of the given field.
  *
  * @param line the line buffer
  * @param i the field index (0-based)
  * @param d the field delimiter
  * @param start the position of the start character, or -1 if the
  *              field does not exist.
  * @param end the position of the end character, or -1 if the field
  *              does not exist.
  *
  * @return the length of the field, or -1 if the field does not exist.
  */
int get_line_pos(const char * const line, int i, const char *d, int *start,
                 int *end);

/** @brief locates the index of the first field in a delimited string
  * having a specific value.
  *
  * @param value the string to be located
  * @param line  delimited string
  * @param delim line's field separator
  *
  * @return 0-based index of the first field having the specified value,
  * -1 if not found, or -2 on error.
  */
ssize_t field_str(const char *value, const char *line, const char *delim);

#if defined(HAVE_CONFIG_H) && \
    defined(HAVE_FGETLN) && \
  ! defined(HAVE_GETLINE)
ssize_t getline(char **outbuf, size_t * outsize, FILE * fp);
#endif

#ifndef __GNUC__
/* no __VA_ARGS__ in vc++, and the use of '##' is a GNU cpp extension.
   So these versions take a format string plus a single required argument. */

/** @brief emit an error message.
  *
  * the message will include the file and line number.
  *
  * @param s format string
  * @param a value to pass into format string
  */
#define WARN(s, a) \
	fprintf(stderr, "%s:%d: " s, __FILE__, __LINE__, a)

/** @brief emit an error message and exit.
  *
  * the message will include the file and line number.  the
  * exit code is -1.
  *
  * @param s format string
  * @param a value to pass into format string
  */
#define DIE(s, a) \
	do { \
		WARN((s), (a)); \
		exit(-1); \
	} while ( 0 )

#else

/** @brief emit an error message.
  *
  * the message will include the file and line number.
  *
  * @param s format string
  * @param ... optional values to pass into format string
  */
#define WARN(s, ...) \
	fprintf(stderr, "%s: (%s:%d): " s, \
			getenv("_"), __FILE__,__LINE__, ##__VA_ARGS__)

/** @brief emit an error message and exit.
  *
  * the message will include the file and line number.  the exit
  * code is -1.
  *
  * @param s format string
  * @param ... optional values to pass into format string
  */
#define DIE(s, ...) \
	do { \
		fprintf(stderr, "%s: (%s:%d): " s, \
			getenv("_"), __FILE__,__LINE__, ##__VA_ARGS__); \
		exit(-1); \
	} while ( 0 )

#endif /* ifdef WIN32 */


#endif /* FFUTILS_H */
