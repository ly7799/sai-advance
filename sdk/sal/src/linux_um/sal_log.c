#include "sal.h"

extern sal_loglevel_t debug_level_flag;

static const char* ll2str[] =
{
    "FATAL",
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG"
};

char sal_print_buf[1024];
char sal_print_line[1024];
char sal_log_buf[1024];
char sal_log_line[1024];

int
ctc_sal_log(int log_level,
        const char* file,
        int line,
        const char* fmt,
        ...)
{
    int i = 0;
    int j = 0;
    va_list ap;

    if (log_level < SAL_LL_FATAL)
    {
        log_level = SAL_LL_FATAL;
    }
    else if (log_level > SAL_LL_DEBUG)
    {
        log_level = SAL_LL_DEBUG;
    }

    if (log_level < SAL_LL_INFO)
    {
        CTC_SAL_DEBUG_OUT("%s %s:%d: ", ll2str[log_level], file, line);
    }

    va_start(ap, fmt);
    vsnprintf(sal_log_buf, 1023, fmt, ap);
    va_end(ap);

    i = 0;

    while (i < 1023 && j < 512 && sal_log_buf[i] != '\0')
    {
        switch (sal_log_buf[i])
        {
        case '\n':
            sal_log_line[j] = '\0';
            printf("%s\r\n", sal_log_line);
            j = 0;
            break;

        default:
            sal_log_line[j] = sal_log_buf[i];
            j++;
            break;
        }

        i++;
    }

    if (sal_log_buf[i] == '\0')
    {
        sal_log_line[j] = '\0';
        printf("%s", sal_log_line);
    }

    if (log_level < SAL_LL_INFO)
    {
        CTC_SAL_DEBUG_OUT("\n");
    }

    return 0;
}

int
ctc_sal_print_func(const char* fmt,  ...)
{
    int i = 0;
    int j = 0;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(sal_print_buf, 1023, fmt, ap);
    va_end(ap);

    i = 0;

    while (i < 1023 && j < 512 && sal_print_buf[i] != '\0')
    {
        switch (sal_print_buf[i])
        {
        case '\n':
            sal_print_line[j] = '\0';
            printf("%s\r\n", sal_print_line);
            j = 0;
            break;

        default:
            sal_print_line[j] = sal_print_buf[i];
            j++;
            break;
        }

        i++;
    }

    if (sal_print_buf[i] == '\0')
    {
        sal_print_line[j] = '\0';
        printf("%s", sal_print_line);
    }

    return 0;
}
