#ifndef _print_h_
#define _print_h_
#include<stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
extern int g_client_fd;
extern struct termios g_old_terminal;
extern int g_terminal_saved;
void printSueess();
void printClose();
int LongestLine(const char* message,int len,int *lineNum);
void printMessage(const char* message,int len);
void printProgress(long cur,long total);
void* monitor_thread(void *arg);
void getAndPrintPass(char *password,int len);
void printTitle(const char *cur_name,const char *cur_path,const char *local_path);
void printp(const char *str);
void printw(const char *str);
void printe(const char *str);
void printTree(const char *tree_str);
#endif  