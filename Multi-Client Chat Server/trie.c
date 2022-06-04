#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "trie.h"

//Create a new node for our trie. Set all variables to default values 
trie_node *trie_create(void) {
    
    trie_node *n = malloc(sizeof(trie_node));
    n -> child = NULL;
    n -> next = NULL;
    n -> leaf = false;

    return n;
} 

//Insert a new word into our trie. This method will match the prefix of any nodes already inserted 
int trie_insert(trie_node *root, char *word, unsigned int word_len) {

    if (root == NULL || word == NULL) {

        return 0;
    }

    trie_node *level = root;
    trie_node *curr;
    trie_node *match = NULL;

    //For the length of each word, we will determine if a match already exists for the node in the same prefix. 
    for (int i = 0; i < word_len; ++i) {

        /*
            Assume no match exists. We will scan each level of the current node, where level refers to the next letter (all child nodes) of the current node. 
        
                                         Root
                                        /    
                                       C
                                      /
                                     A
                                   /   \
                                  T     P
        
            From our example here, the letters T and P appear on the same level. If the current node (letter) of our trie finds a match, set match to the current node 
            and break. We will set the level node to the match. 
        */

        match = NULL;
        for (curr = level->child; curr != NULL; curr = curr->next) {

            if (curr->letter == word[i]) {

                match = curr;
                break;
            }
        }

        //If we do not find a match for all nodes on the same level of our current prefix, we will proceed to make a new node. 
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

//Method to search for a specific word in our trie. 
int trie_search(trie_node *root, char *word, unsigned int word_len) {

    trie_node *level = root->child;
    trie_node *curr, *match;

    //Follows the pattern for inserting a new node. We will look for a match on the same level as our current node. 
    for (int i = 0; i < word_len; ++i) {

        match = NULL;
        for (curr = level; curr != NULL; curr = curr->next) {

            if (curr->letter == word[i]) {

                match = curr;
                break;
            }
        }

        //If at any point we do not find a match for our current node, we will return zero. 
        if (match == NULL) {

            return 0;
        }

        level = match->child;
    }

    //If we have found a match, return one. Else, return zero. 
    if (match != NULL && match->leaf) {

        return 1;
    }

    return 0;
}

//This method will delete a word in our trie 
void trie_delete(trie_node *root, char *word, unsigned int word_len) {

    trie_node *level = root->child;
    trie_node *curr, *match;

    //Same as before. Scan all nodes on the current level until we find a match 
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

    //Set the match to not be a leaf. Since the node is not a leaf it will always return false from trie_search. 
    if (match != NULL && match->leaf) {
        
        match->leaf = false;
    }
}
