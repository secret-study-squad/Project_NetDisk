#include "Path.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void pathAppendComponent(Path *path, const char *s) {
    PathComponent *newnode = (PathComponent *)malloc(sizeof(PathComponent));
    newnode->component = strdup(s);

    newnode->next = path->tail;
    newnode->prev = path->tail->prev;

    newnode->prev->next = newnode;
    newnode->next->prev = newnode;
}


bool pathIsEmpty(Path *path) {
    return path->head->next == path->tail;
}

void pathPopComponent(Path *path) {
    if(pathIsEmpty(path)) {
        return;
    }
    PathComponent *node = path->tail->prev;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    free(node->component);
    free(node);
}

void pathConcatenateStr(Path *path, const char *s) {
    char str[256];
    char *pathStr = strdup(s);
    const char *component = strtok(pathStr, "/");
    while(component) {
        if(strcmp(component, ".") == 0) {

        }
        else if(strcmp(component, "..") == 0) {
            pathPopComponent(path);
        }
        else {
            pathAppendComponent(path, component);
        }
        component = strtok(NULL, "/");
    }

    free(pathStr);
}

void pathInit(Path *path, const char *s) {
    PathComponent *dummy = (PathComponent *)malloc(sizeof(PathComponent));
    dummy->component = NULL;
    dummy->next = dummy;
    dummy->prev = dummy;
    path->head = dummy;
    path->tail = dummy;

    pathConcatenateStr(path, s);
}
void pathDestroy(Path *path) {
    PathComponent *curr = path->head->next;
    while(curr != path->tail) {
        PathComponent *next = curr->next;
        free(curr->component);
        free(curr);
        curr = next;
    }
    free(path->tail);
    path->head = NULL;
    path->tail = NULL;
}
void pathStr(const Path *path, char *str) {
    char *p = str;
    *p++ = '/';
    PathComponent *curr = path->head->next;
    while(curr != path->tail) {
        char *comp = curr->component;
        while(*comp) {
            *p++ = *comp++;
        }
        curr = curr->next;
        if(curr != path->tail) {
            *p++ = '/';
        }
    }
    *p = '\0';
}

Path pathCdAbsolutely(const char *s) {
    Path path;
    pathInit(&path, s);
    return path;
}
Path pathCdRelatively(const Path *whence, const char *s) {
    char str[256] = {0};

    pathStr(whence, str);
    Path path;
    pathInit(&path, str);
    pathConcatenateStr(&path, s);
    return path;
}
