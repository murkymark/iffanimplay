//based on http://stackoverflow.com/questions/4384359/quick-way-to-implement-dictionary-in-c
//license ?



#include <stdlib.h>
#include <string.h>

#include "dict.h"


//#define HASHSIZE 100
//static struct nlist *hashtab[HASHSIZE] = {NULL}; /* pointer table */


struct dict *dict_create(int hashsize){
	struct dict *d = malloc(sizeof(struct dict));
	if(d != NULL){
		d->hashsize = hashsize;
		d->hashtab = malloc(sizeof(struct nlist*) * hashsize);
		if(d->hashtab == NULL) //failed
			free(d);
		else
			memset(d->hashtab, 0, sizeof(struct nlist*) * hashsize); //init with NULL (0)
	}
	
	return d;
}


void dict_free(struct dict *d){
	if(d == NULL)
		return;
	
	for(int i = 0; i < d->hashsize; i++){
		struct nlist *e = d->hashtab[i];
		struct nlist *e_next;
		while(e != NULL){
			free(e->key);
			free(e->val);
			e_next = e->next;
			free(e);
			e = e_next;
		}
	}
	free(d->hashtab);
	free(d);
}


/* hash: form hash value for string s */
unsigned hash(char *s, int hashsize)
{
    unsigned hashval;
    for (hashval = 0; *s != '\0'; s++)
      hashval = *s + 31 * hashval;
    return hashval % hashsize;
}


/* get: look for s in hashtab */
struct nlist *dict_get(struct dict *d, char *s)
{
    struct nlist *np;
    for (np = d->hashtab[hash(s, d->hashsize)]; np != NULL; np = np->next)
        if (strcmp(s, np->key) == 0)
          return np; /* found */
    return NULL; /* not found */
}


char *dict_strdup(char *s) /* make a duplicate of s */
{
    char *p;
    p = (char *) malloc(strlen(s)+1); /* +1 for ’\0’ */
    if (p != NULL)
       strcpy(p, s);
    return p;
}

/* install: put (key, val) in hashtab */
struct nlist *dict_put(struct dict *d, char *key, char *val)
{
    struct nlist *np;
    unsigned hashval;
    if ((np = dict_get(d, key)) == NULL) { /* not found */
        np = (struct nlist *) malloc(sizeof(*np));
        if (np == NULL || (np->key = dict_strdup(key)) == NULL)
          return NULL;
        hashval = hash(key, d->hashsize);
        np->next = d->hashtab[hashval];
        d->hashtab[hashval] = np;
    } else /* already there */
        free((void *) np->val); /*free previous val */
    if ((np->val = dict_strdup(val)) == NULL)
       return NULL;
    return np;
}


