//dictionary using hash table
//key is string
//

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



#ifndef _dict_H
#define _dict_H

//table entry
struct nlist { 
    struct nlist *next; // next entry in chain (same hash value but different string; or NULL - end)
    char *key; // key string to search
    char *val; // value string 
};

//dictionary
struct dict {
	int hashsize;
	struct nlist **hashtab; //pointer table
};

//create dict
//bigger hash table size can increase performance
struct dict *dict_create(int hashsize);

//free all allocated space and dict
void dict_free(struct dict *dict);

//find and return entry or NULL if not found
struct nlist *dict_get(struct dict *d, char *key);

//put key,value into dict
//replace if key already exists
struct nlist *dict_put(struct dict *d, char *key, char *val);

#endif
