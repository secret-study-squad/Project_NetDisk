#include <my_header.h>

/*
v1_16:24_未考虑前导分隔符与字符集合式分隔符
 * */

char* strtok(char* str, const char* division){
    static char* next_start = NULL; // 下一次继续调用strtok时被处理字符串部分的起始位置
    char* token_start;

    // 首次调用 或 "切换"字符串时，需初始化next_start
    if(str != NULL)
        next_start = str;
    // 如果新字符串为空 或 当前字符串已经处理到末尾，则直接返回NULL
    if(next_start == NULL || *next_start == '\0')
        return NULL;

    /*
    // 前导分隔符，字符集合式分隔符
    // 跳过当前分隔符（前导分隔符）
    while (*next && strchr(delim, *next))
        next++;
    // 如果跳过之后直接到结尾，说明没有更多 token
    if (*next == '\0')
        return NULL;
    */

    token_start = next_start;
    while(*next_start && *next_start != *division) // 当前字符不为分隔符 或 '\0'
        next_start++;
    
    if(*next_start){ // 即遇到的是分隔符导致while中断
        *next_start = '\0';
        next_start++; // 将分隔符替换为'\0'，并移动到下一个字符
    }

    return token_start;
}
