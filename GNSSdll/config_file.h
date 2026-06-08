/*
 * config_file.h —— 配置文件读写（动态库内部私有实现）
 *
 * 本头文件仅供动态库内部使用，不对外暴露。
 * config.txt 存放在可执行文件同目录，由 Qt 端通过 QFile 写入，库端读取。
 */

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 读取 ──────────────────────────────────────────── */

/**
 * @brief 从配置文件读取字符串值
 * @param configPath 配置文件路径（传 NULL 使用默认路径）
 * @param key        键名
 * @param value      输出缓冲区
 * @param valueSize  缓冲区大小
 * @return 0=成功, -1=文件不存在, -2=键未找到
 */
int config_read_str(const char *configPath, const char *key,
                    char *value, int valueSize);

/* ── 路径 ──────────────────────────────────────────── */

/** @brief 获取默认配置文件完整路径（可执行文件同目录/config.txt） */
const char *config_default_path(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_FILE_H */
