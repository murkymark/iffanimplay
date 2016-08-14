//dictionary using a hash table
//key is string
//
//Entries with same hashes (collisions) are stored unsorted (for now) in a bucket (singly linked list) for each hash.
//Here we use simple buckets which have a complexity of O(n) to search while the first bucket element is accessed with O(1).
//

/*
	//dict test
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



#ifndef _dict_H
#define _dict_H


//table entry
struct nlist { 
    struct nlist *next; // next entry in chain (same hash value but different string; or NULL to mark end)
    char *key; // key string
    char *val; // value string 
};

//dictionary
struct dict {
	int hashsize;
	struct nlist **hashtab; //pointer table
};


//create dict
//bigger hash table size can increase performance (reduce bucket size)
struct dict *dict_create(int hashsize);

//free dict, deallocate
void dict_free(struct dict *dict);

//find and return entry or NULL if not found
struct nlist *dict_get(struct dict *d, char *key);

//put key,value into dict
//replace old if key already exists
struct nlist *dict_put(struct dict *d, char *key, char *val);




//create copy with different hash size or use same as old for identical copy
//note: If hashsize is changed, hash values of entries may change also and hash values you have in your vars to refer to entries could be invalid for the new dict
//the newly created dict must be freed later
//struct dict *dict_copy(struct dict *d, hashsize_new);

//return number of hash collisions in dict (lower is better for performance)
//int dict_hash_coll_count(struct dict *);

#endif
