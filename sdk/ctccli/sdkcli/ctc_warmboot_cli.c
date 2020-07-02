#include "sal.h"
#include "ctc_cli.h"
#include "ctc_api.h"
#include "ctc_cmd.h"
#include "ctc_cli_common.h"


extern int32 ctc_wb_sync(uint8 lchip);
extern int32 ctc_wb_sync_done(uint8 lchip, int32 result);

extern int32 ctc_wb_show_status(uint8 lchip);


CTC_CLI(ctc_cli_common_wb_sync,
        ctc_cli_common_wb_sync_cmd,
        "warmboot sync ",
        "Warmboot module",
        "Sync up all data")
{
    int32 ret = 0;
    uint8 lchip = 0;

    ret = ctc_wb_sync(lchip);

    if (ret < 0)
    {
        ctc_cli_out("%% %s\n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}


CTC_CLI(ctc_cli_common_wb_sync_done,
        ctc_cli_common_wb_sync_done_cmd,
        "warmboot sync-done",
        "Warmboot module",
        "Sync up done")
{
    int32 ret = 0;
    uint8 lchip = 0;

    ret = ctc_wb_sync_done(lchip, 0);

    if (ret < 0)
    {
        ctc_cli_out("%% %s\n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_common_wb_set_cpu_rx_en,
        ctc_cli_common_wb_set_cpu_rx_en_cmd,
        "warmboot cpu-rx (enable|disable)",
        "Warmboot module",
        "Cpu rx",
        "Enable",
        "Disable")
{
    int32 ret = CLI_SUCCESS;
    bool enable = FALSE;
    uint8 index = 0;
    uint8 lchip = 0;

    index = CTC_CLI_GET_ARGC_INDEX("enable");
    if (INDEX_VALID(index))
    {
        enable = TRUE;
    }

    ret = ctc_wb_set_cpu_rx_en(lchip, enable);

    if (ret < 0)
    {
        ctc_cli_out("%% %s\n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_common_wb_set_interval,
        ctc_cli_common_wb_set_interval_cmd,
        "warmboot interval INTERVAL",
        "Warmboot module",
        "Interval sync",
        "Interval timer, unit ms")
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 interval = 0;

    CTC_CLI_GET_UINT32("interval timer", interval, argv[0]);

    ret = ctc_wb_set_interval(lchip, interval);
    if (ret < 0)
    {
        ctc_cli_out("%% %s\n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_common_wb_show_status,
        ctc_cli_common_wb_show_status_cmd,
        "show warmboot status",
        "Show",
        "Warmboot module",
        "Status")
{
    int32 ret = CLI_SUCCESS;
    bool enable = FALSE;
    uint8 lchip = 0;
    uint32 interval = 0;

    ctc_cli_out("warmboot  : %s \n", CTC_WB_ENABLE?"eanble":"disable");
    if (CTC_WB_STATUS(lchip) == CTC_WB_STATUS_DONE)
    {
        ctc_cli_out("Status    : Done\n");
    }
    else if (CTC_WB_STATUS(lchip) == CTC_WB_STATUS_RELOADING)
    {
        ctc_cli_out("Status    : Reloading\n");
    }
    else if (CTC_WB_STATUS(lchip) == CTC_WB_STATUS_SYNC)
    {
        ctc_cli_out("Status    : Sync\n");
    }

    ret = ctc_wb_get_cpu_rx_en(lchip, &enable);

    if (ret < 0)
    {
        ctc_cli_out("%% %s\n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    ctc_cli_out("Cpu-rx enable : %d\n", enable?1:0);

    ret = ctc_wb_get_interval(lchip, &interval);

    if (ret < 0)
    {
        ctc_cli_out("%% %s\n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    ctc_cli_out("warmboot interval: %d ms\n", interval);

    ctc_wb_show_status(lchip);
    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_common_wb_debug_on,
        ctc_cli_common_wb_debug_on_cmd,
        "debug warmboot (debug-level {func|param|info|error} |)",
        CTC_CLI_DEBUG_STR,
        "Warmboot module",
        CTC_CLI_DEBUG_LEVEL_STR,
        CTC_CLI_DEBUG_LEVEL_FUNC,
        CTC_CLI_DEBUG_LEVEL_PARAM,
        CTC_CLI_DEBUG_LEVEL_INFO,
        CTC_CLI_DEBUG_LEVEL_ERROR)
{

    uint8 level = CTC_DEBUG_LEVEL_INFO;
    uint8 index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("debug-level");
    if (index != 0xFF)
    {
        level = CTC_DEBUG_LEVEL_NONE;
        index = CTC_CLI_GET_ARGC_INDEX("func");
        if (index != 0xFF)
        {
            level |= CTC_DEBUG_LEVEL_FUNC;
        }

        index = CTC_CLI_GET_ARGC_INDEX("param");
        if (index != 0xFF)
        {
            level |= CTC_DEBUG_LEVEL_PARAM;
        }

        index = CTC_CLI_GET_ARGC_INDEX("info");
        if (index != 0xFF)
        {
            level |= CTC_DEBUG_LEVEL_INFO;
        }

        index = CTC_CLI_GET_ARGC_INDEX("error");
        if (index != 0xFF)
        {
            level |= CTC_DEBUG_LEVEL_ERROR;
        }
    }

    ctc_debug_set_flag("wb", "wb", WB_CTC, level, TRUE);

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_common_wb_debug_off,
        ctc_cli_common_wb_debug_off_cmd,
        "no debug warmboot",
        CTC_CLI_NO_STR,
        CTC_CLI_DEBUG_STR,
        "Warmboot moudle")
{
    uint8 level = 0;

    ctc_debug_set_flag("wb", "wb", WB_CTC, level, FALSE);

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_common_wb_dump_memory,
        ctc_cli_common_wb_dump_memory_cmd,
        "warmboot dump-memory file FILE_NAME",
        "Warmboot module",
        "dump all warmboot memory data",
        "into file",
        "file name")
{
    uint8 lchip=0;
    char file_name[32];
    int32 ret = CTC_E_NONE;
    sal_file_t p_file = NULL;
    uint8 mod_id, sub_id;
    ctc_wb_appid_t * p_new_node, lkup_node;
    char cfg_file[40];
    uint32 cnt = 0,lines = 1;
    uint32 buf_idx = 0;
    uint8 i,remainder = 0;
    uint32 loop_line = 0;

    sal_memcpy(file_name, argv[argc - 1], sal_strlen(argv[argc - 1]));

    if(!CTC_WB_DM_MODE)
    {
        return ret;
    }

    if(sal_strlen(file_name) != 0)
    {
        sal_sprintf(cfg_file, "%s%s", file_name, ".txt");
        p_file = sal_fopen(cfg_file, "wb+");

        if (NULL == p_file)
        {
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR," Store file: %s failed!\n\n", cfg_file);
            goto done;
        }
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP," Store file: %s starting...\n", cfg_file);
    }

    for (mod_id = 0; mod_id < CTC_FEATURE_MAX; mod_id++)
    {
        for (sub_id = 0; sub_id < 32; sub_id++)
        {
            lkup_node.app_id = lchip << 16 | CTC_WB_APPID(mod_id, sub_id);
            p_new_node = ctc_hash_lookup(g_wb_master->appid_hash, &lkup_node);
            if (!p_new_node)
            {
                continue;
            }
            cnt = 0;
            buf_idx = 0;
            lines = 1;
            sal_fprintf(p_file, "\n------------------------------------------------------------------\n");
            sal_fprintf(p_file,"app_id:%X, entry_num:%d, entry_size:%d, valid_cnt:%d.\n", p_new_node->app_id, p_new_node->entry_num, p_new_node->entry_size, p_new_node->valid_cnt);
            if(p_new_node->entry_size > 36)
            {
                lines = (p_new_node->entry_size + 35) / 36;
                remainder = p_new_node->entry_size % 36;
            }
            while(cnt++ < p_new_node->valid_cnt)
            {
                loop_line = lines;
                sal_fprintf(p_file,"entry[%d]:",cnt);
                while(loop_line-- > 0)
                {
                    i = p_new_node->entry_size > 36 ? (loop_line ? 36 : remainder) : p_new_node->entry_size;
                    while(i-- > 0 )
                    {
                       sal_fprintf(p_file,"%02x ",((uint8 *)p_new_node->address)[buf_idx++]);
                    }
                    sal_fprintf(p_file,"\n");
                }
            }
            sal_fprintf(p_file, "\n------------------------------------------------------------------\n");
        }
    }

done:
    if (NULL != p_file)
    {
        sal_fclose(p_file);
    }

    return CLI_SUCCESS;
}



int32
ctc_warmboot_cli_init(void)
{
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_sync_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_sync_done_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_set_cpu_rx_en_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_show_status_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_debug_on_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_debug_off_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_dump_memory_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_common_wb_set_interval_cmd);

    return CTC_E_NONE;
}

