/** @file hashtbl.h
  * @brief Interface for the hashtbl library.
  *
  * This is a chained hashtable implementation, internally utilizing the
  * linklist library.  Note that for all key comparisons (in ht_put(), ht_get(),
  * ht_delete(), etc.) strcmp() is used for comparing the keys.  So while there's
  * nothing to stop you from using some other data type as a lookup key, that 
  * data item cannot contain any NULL bytes before the end if the key comparison 
  * is going to behave as expected.
  *
  */

#include <stdlib.h>
#include <string.h>	/* strcmp(), strlen() */
#include <linklist.h>
#include <hashfuncs.h>


#ifndef HASHTBL_H
#define HASHTBL_H

/** @brief the hashtable data type. */
typedef struct _hashtbl {
	size_t nelems;				/**< number of elements in the hashtable */
	size_t arrsz;				/**< size of the array */
	llist_t **arr;				/**< array of linked lists */
	unsigned int (* hash)(unsigned char *);	/**< hash function to use - see hashfuncs.h */
	void (* free)(void *);			/**< memory freeing function to call against an entry's data */
} hashtbl_t;

/** @brief a key/value pair within the hashtable */
typedef struct _ht_elem {
	char *key;	/**< string lookup key for the element */
	void *data;	/**< data to store in this element */
} ht_elem_t;

/** @brief initializes a new hashtable.  the memfree function should be
  * specified iff the payload of a node will need to be deallocated when
  * the hashtable is destroyed.  if a NULL hash function is specified 
  * the BKDRHash function will be used.
  * 
  * @param tbl the table to be initialized.
  * @param sz size to make the table.
  * @param hash function for hashing data when inserting or retrieving.
  * @param memfree function to free memory when destroying the hashtable.
  *
  * @return 0 on success, -1 on memory error, 1 if table is NULL, required
  * function is NULL or size is 0
  */
int ht_init( hashtbl_t *tbl, size_t sz, unsigned int (* hash)(unsigned char *), void (* memfree)(void *) );


/** @brief deallocates memory for all elements and destroys a hashtable.
  * 
  * @param tbl the table to be deallocated.
  */
void ht_destroy( hashtbl_t *tbl );

/** @brief adds an entry to the hashtable.  if an entry with the specified key
  * already exists, the value is overwritten.
  * 
  * @param tbl hashtable in which the entry should be put
  * @param key string to use as the lookup key
  * @param data value to be stored.
  * 
  * @return -1 on memory or 0 on success
  */
int ht_put( hashtbl_t *tbl, char *key, void *data );

/** @brief retrieves an entry's data from a hashtable.
  * 
  * @param tbl table in which the data is stored
  * @param key string lookup key
  * 
  * @return NULL if an element with the specified key does not exist, else
  * the data in the entry.
  */
void * ht_get( hashtbl_t *tbl, char *key );

/** @brief removes an entry from a hashtable
  * 
  * @param tbl table in which the data is stored
  * @param key string lookup key
  */
void ht_delete( hashtbl_t *tbl, char *key );

/** @brief executes a function for the data in each hashtable entry
  * 
  * @param tbl table to be traversed
  * @param func function to call which takes the entry's data as its
  * only argument.
  */
void ht_call_for_each( hashtbl_t *tbl, int (* func)( void * ) ) ;

/** @brief prints some statistics for a hashtable useful for judging
  * hash algorithm performance.
  *
  * data is printed to sdterr and includes the allocated array size,
  * the number of empty cells, the average length of non-empty cells,
  * the maximum chain length, and the total number of elements in the
  * hashtable.
  * 
  * @param tbl a hashtable
  */
void ht_dump_stats( hashtbl_t *tbl );

#endif /* HASHTBL_H */
