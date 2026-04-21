#include "separate_cmd_path.h"

/*
v1_15:41_未考虑前导0与缩进\t(补充：末尾的\n也未考虑)
 * */

void get_cmd(char* s, CmdType* cmd_type){
    int idx = 0;
    while(s[idx]){
        if(s[idx] == ' ' || s[idx] == '\n'){
            s[idx] = '\0';
            break; // 提前跳出
        }
        idx++;
    }
    
    if (strcmp(s, "register") == 0) {
    *cmd_type = CMD_REGISTER;
    } else if (strcmp(s, "login") == 0) {
    *cmd_type = CMD_LOGIN;
    } else if (strcmp(s, "cd") == 0) {
        *cmd_type = CMD_CD;
    } else if (strcmp(s, "ls") == 0) {
        *cmd_type = CMD_LS;
    } else if (strcmp(s, "puts") == 0) {
        *cmd_type = CMD_PUTS;
    } else if (strcmp(s, "gets") == 0) {
        *cmd_type = CMD_GETS;
    } else if (strcmp(s, "remove") == 0) {
        *cmd_type = CMD_REMOVE;
    } else if (strcmp(s, "pwd") == 0) {
        *cmd_type = CMD_PWD;
    } else if (strcmp(s, "mkdir") == 0) {
        *cmd_type = CMD_MKDIR;
    } else if (strcmp(s, "rmdir") == 0) {
        *cmd_type = CMD_RMDIR;
    } else {
        *cmd_type = CMD_UNKNOW;
    }

    s[idx] = ' '; // 还原字符串(提前跳出时idx指向修改成的'\0')
}

void get_path(char* s, char* path){
    // 跳过命令字符
    while(*s != ' ')
        s++;
    // 跳过空格
    while(*s == ' ')
        s++;
    // 处理末尾的\n
    char* p = s;
    while(*p != '\n' && *p != '\0')
        p++;
    if(*p == '\n')
        *p = '\0';

    strcpy(path, s);
}
