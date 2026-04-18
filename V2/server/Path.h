#ifndef __PATH_H__
#define __PATH_H__

typedef struct path_component {
    char *component;
    struct path_component *prev;
    struct path_component *next;
}PathComponent;

typedef struct {
    PathComponent *head;
    PathComponent *tail;
}Path;

void pathInit(Path *path, const char *s);
void pathDestroy(Path *path);
void pathStr(const Path *path, char *str);

Path pathCdAbsolutely(const char *s);
Path pathCdRelatively(const Path *whence, const char *s);

#endif
