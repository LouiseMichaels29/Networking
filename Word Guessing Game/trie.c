#include "trie.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

//Create a new node for each word in our trie and initialize starting values 
trie_node *trie_create(void) {

    trie_node *n;
    n = malloc(sizeof(trie_node));
    n->child = NULL;
    n->next = NULL;
    n->leaf = false;

    return n;
} 

//Insert a word into our trie. 
int trie_insert(trie_node *root, char *word, unsigned int word_len) {

    if (root == NULL || word == NULL) {

        return 0;
    }

    trie_node *level = root;
    trie_node *curr;
    trie_node *match = NULL;

    //For the length of the word, increment our current node and check to see if we have a match for each letter in our trie. 
    for (int i = 0; i < word_len; ++i) {
        match = NULL;

        for (curr = level->child; curr != NULL; curr = curr->next) {

            if (curr->letter == word[i]) {

                match = curr;
                break;
            }
        }

        if (match == NULL) {
            
            trie_node *new = trie_create();
            new->letter = word[i];
            new->next = level->child;
            level->child = new;
            match = new;
        }

        level = match;
    }

    if (match == NULL) {

        return 0;
    }

    match->leaf = true;
    return 1;
}

int trie_search(trie_node *root, char *word, unsigned int word_len) {

    trie_node *level = root->child;
    trie_node *curr, *match;

    for (int i = 0; i < word_len; ++i) {

        match = NULL;
        for (curr = level; curr != NULL; curr = curr->next) {

            if (curr->letter == word[i]) {

                match = curr;
                break;
            }
        }

        if (match == NULL) {

            return 0;
        }

        level = match->child;
    }

    if (match != NULL && match->leaf) {

        return 1;
    }
    
    return 0;
}

void trie_delete(trie_node *root, char *word, unsigned int word_len) {

    trie_node *level = root->child;
    trie_node *curr, *match;

    for (int i = 0; i < word_len; ++i) {
        
        match = NULL;
        for (curr = level; curr != NULL; curr = curr->next) {

            if (curr->letter == word[i]) {

                match = curr;
                break;
            }
        }

        if (match == NULL) {
            
            return ;
        }

        level = match->child;
    }

    if (match != NULL && match->leaf) {

        match->leaf = false;
    }
}
