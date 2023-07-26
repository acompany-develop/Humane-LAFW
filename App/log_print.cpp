#include "log_print.hpp"

/* ログ種別に応じてログメッセージを標準出力 */
void print_log_message(std::string message, LOG_TYPE type)
{
    if(type == DEBUG_LOG)
    {
        std::cout << "\033[32mDEBUG: \033[m" << message << std::endl;
    }
    else if(type == ERROR)
    {
        std::cerr << "\033[31mERROR: \033[m" << message << std::endl;
    }
    else
    {
        std::cout << "\033[36m INFO: \033[m" << message << std::endl;
    }
}

/* ログ種別を表示（改行無し） */
void print_log_type(LOG_TYPE type)
{
    if(type == DEBUG_LOG)
    {
        std::cout << "\033[32mDEBUG: \033[m";
    }
    else if(type == ERROR)
    {
        std::cerr << "\033[31mERROR: \033[m";
    }
    else
    {
        std::cout << "\033[36m INFO: \033[m";
    }
}