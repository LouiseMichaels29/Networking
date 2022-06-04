#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct node {

    char letter;
    bool leaf;
    struct node *child;
    struct node *next;
} trie_node;

trie_node *trie_create(void);
int trie_insert(trie_node *trie, char *word, unsigned int word_len);
int trie_search(trie_node *trie, char *word, unsigned int word_len);
void trie_delete(trie_node *trie, char *word, unsigned int word_len);
