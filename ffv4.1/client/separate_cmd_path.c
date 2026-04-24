#include "separate_cmd_path.h"
#include "cmd_set.h"

// 跳过空白字符（空格、制表符）
static const char* skip_whitespace(const char *p) {
    if (!p) return NULL;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

// 获取一个单词（直到空白或结尾）
// 返回下一个扫描位置，若 buf 非空则复制单词
static const char* get_word(const char *p, char *buf, size_t bufsize) {
    if (!p) return NULL;
    const char *start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    if (buf && bufsize > 0) {
        size_t len = p - start;
        if (len >= bufsize) len = bufsize - 1;
        memcpy(buf, start, len);
        buf[len] = '\0';
    }
    return p;
}

void get_cmd(char *s, CmdType *cmd_type) {
    if (!s || !cmd_type) return;
    *cmd_type = CMD_UNKNOW;

    // 去除末尾换行符
    char *nl = strchr(s, '\n');
    if (nl) *nl = '\0';

    // 跳过前导空格
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return;  // 空命令

    // 找到命令单词结束位置
    char *end = p;
    while (*end && *end != ' ' && *end != '\t') end++;

    // 保存并截断
    char saved = *end;
    *end = '\0';

    // 识别命令（注意此时 p 已指向命令开头）
    if (strcmp(p, "register") == 0)      *cmd_type = CMD_REGISTER;
    else if (strcmp(p, "login") == 0)    *cmd_type = CMD_LOGIN;
    else if (strcmp(p, "cd") == 0)       *cmd_type = CMD_CD;
    else if (strcmp(p, "ls") == 0)       *cmd_type = CMD_LS;
    else if (strcmp(p, "puts") == 0)     *cmd_type = CMD_PUTS;
    else if (strcmp(p, "gets") == 0)     *cmd_type = CMD_GETS;
    else if (strcmp(p, "remove") == 0)   *cmd_type = CMD_REMOVE;
    else if (strcmp(p, "pwd") == 0)      *cmd_type = CMD_PWD;
    else if (strcmp(p, "mkdir") == 0)    *cmd_type = CMD_MKDIR;
    else if (strcmp(p, "rmdir") == 0)    *cmd_type = CMD_RMDIR;
    else if (strcmp(p, "tree") == 0)     *cmd_type = CMD_TREE;
    // 新增本地命令解析
    else if (strncmp(p, "lcd", 3) == 0) *cmd_type = CMD_LCD;
    else if (strncmp(p, "lpwd", 4) == 0) *cmd_type = CMD_LPWD;
    else if (strncmp(p, "lls", 3) == 0) *cmd_type = CMD_LLS;
    else if (strncmp(p, "ltree", 5) == 0) *cmd_type = CMD_LTREE;
    // 恢复原字符（保持字符串完整）
    *end = saved;
}

void get_path1(const char *s, char *path1, size_t size) {
    if (!s || !path1 || size == 0) return;
    path1[0] = '\0';

    const char *p = skip_whitespace(s);
    if (!p || *p == '\0') return;

    p = get_word(p, NULL, 0);           // 跳过命令
    p = skip_whitespace(p);
    if (!p || *p == '\0') return;

    get_word(p, path1, size);
}

void get_path2(const char *s, char *path2, size_t size) {
    if (!s || !path2 || size == 0) return;
    path2[0] = '\0';

    const char *p = skip_whitespace(s);
    if (!p || *p == '\0') return;

    p = get_word(p, NULL, 0);           // 跳过命令
    p = skip_whitespace(p);
    if (!p || *p == '\0') return;

    p = get_word(p, NULL, 0);           // 跳过第一个参数
    p = skip_whitespace(p);
    if (!p || *p == '\0') return;

    get_word(p, path2, size);
}
