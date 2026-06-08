/*
 * config_file.c —— 配置文件读取实现（动态库内部私有）
 *
 * 仅提供 config_read_str() 和 config_default_path()，
 * 供 ZD_ServerCreate 内部读取 config.txt 中的串口参数。
 *
 * config_write_str 不在库中实现（写入由 Qt 端通过 QFile 完成）。
 */

#include "config_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 跨平台头文件：
 *   Windows: windows.h 提供 GetModuleFileNameA / DWORD
 *   Linux:   sys/types.h 提供 ssize_t（readlink 返回值类型）
 *            unistd.h   提供 readlink
 */
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* ═══════════════════════════════════════════════════════
 * 默认配置文件路径
 * ═══════════════════════════════════════════════════════ */

const char *config_default_path(void)
{
    static char path[512] = {0};
    if (path[0] != '\0') {
        return path;
    }

#if defined(_WIN32) || defined(_WIN64)
    DWORD len = GetModuleFileNameA(NULL, path, sizeof(path) - 1);
    if (len > 0 && len < sizeof(path)) {
        char *lastSep = strrchr(path, '\\');
        if (lastSep) {
            *(lastSep + 1) = '\0';
        }
        strncat(path, "config.txt", sizeof(path) - strlen(path) - 1);
    } else {
        strncpy(path, "config.txt", sizeof(path) - 1);
    }
#else
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0 && (size_t)len < sizeof(path)) {
        path[len] = '\0';
        char *lastSep = strrchr(path, '/');
        if (lastSep) {
            *(lastSep + 1) = '\0';
        }
        strncat(path, "config.txt", sizeof(path) - strlen(path) - 1);
    } else {
        strncpy(path, "./config.txt", sizeof(path) - 1);
    }
#endif
    return path;
}

/* ═══════════════════════════════════════════════════════
 * 配置文件读取
 * ═══════════════════════════════════════════════════════ */

int config_read_str(const char *configPath, const char *key,
                    char *value, int valueSize)
{
    if (!key || !value || valueSize <= 0) return -1;
    value[0] = '\0';

    const char *path = configPath ? configPath : config_default_path();

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    int found = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *kStart = line;
        while (*kStart == ' ' || *kStart == '\t') ++kStart;
        char *kEnd = eq - 1;
        while (kEnd > kStart && (*kEnd == ' ' || *kEnd == '\t' || *kEnd == '\r' || *kEnd == '\n')) --kEnd;
        *(kEnd + 1) = '\0';

        if (strcmp(kStart, key) != 0) continue;

        char *vStart = eq + 1;
        while (*vStart == ' ' || *vStart == '\t') ++vStart;

        char *vEnd = vStart + strlen(vStart) - 1;
        while (vEnd >= vStart && (*vEnd == ' ' || *vEnd == '\t' || *vEnd == '\r' || *vEnd == '\n')) {
            *vEnd = '\0';
            --vEnd;
        }

        strncpy(value, vStart, (size_t)(valueSize - 1));
        value[valueSize - 1] = '\0';
        found = 1;
        break;
    }

    fclose(fp);
    return found ? 0 : -2;
}
