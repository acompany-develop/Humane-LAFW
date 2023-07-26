#pragma once

#include <iostream>
#include <string>

/* ログ種別定義用の列挙型 */
typedef enum
{
    DEBUG_LOG = 0,
    INFO,
    ERROR
} LOG_TYPE;

/* ログ種別に応じてログメッセージを標準出力 */
void print_log_message(std::string message, LOG_TYPE type);

/* ログ種別のタグのみを表示（改行無し）。
 * 他の標準出力ツールと組み合わせての使用を想定。 */
void print_log_type(LOG_TYPE type);