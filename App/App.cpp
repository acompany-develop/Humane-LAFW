#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sgx_urts.h>
#include <sgx_uswitchless.h>
#include <sgx_dh.h>
#include "error_print.h"
#include "log_print.hpp"
#include "initiator_enclave_u.h"
#include "responder_enclave_u.h"



/* Enclave内の値の出力を行うOCALL（主にデバッグやログ用） */
void ocall_print(const char *str, int log_type)
{
    LOG_TYPE type;
    if(log_type == 0) type = DEBUG_LOG;
    else if(log_type == 1) type = INFO;
    else type = ERROR;
 
    print_log_message("OCALL output-> ", type);
    print_log_message(str, type);

    return;
}


/* SGXステータスを識別し具体的な内容表示する */
void ocall_print_status(sgx_status_t st)
{
	print_sgx_status(st);
	return;
}


/* Enclaveの初期化 */
int initialize_enclave(sgx_enclave_id_t &eid, std::string enclave_name)
{
    /* LEはDeprecatedになったので、起動トークンはダミーで代用する */
    sgx_launch_token_t token = {0};

    /* 起動トークンが更新されているかのフラグ。Deprecated。 */
    int updated = 0;

    sgx_status_t status;

    sgx_uswitchless_config_t us_config = SGX_USWITCHLESS_CONFIG_INITIALIZER;
	void* enclave_ex_p[32] = {0};

	enclave_ex_p[SGX_CREATE_ENCLAVE_EX_SWITCHLESS_BIT_IDX] = &us_config;

    /* 
     * Switchless Callが有効化されたEnclaveの作成。
     * NULLの部分はEnclaveの属性（sgx_misc_attribute_t）が入る部分であるが、
     * 不要かつ省略可能なのでNULLで省略している。
     */
    status = sgx_create_enclave_ex(enclave_name.c_str(), SGX_DEBUG_FLAG,
                &token, &updated, &eid, NULL, SGX_CREATE_ENCLAVE_EX_SWITCHLESS, 
                    (const void**)enclave_ex_p);

    if(status != SGX_SUCCESS)
	{
		/* error_print.cppで定義 */
		print_sgx_status(status);
		return -1;
	}

    return 0;
}


/* Initiator側がResponder側にLAで自身の正当性を証明する */
int do_LA(sgx_enclave_id_t initiator_enclave_id,
    sgx_enclave_id_t responder_enclave_id)
{
    print_log_message("==============================================", INFO);
    print_log_message("Execute Local Attestation", INFO);
    print_log_message("==============================================", INFO);
    print_log_message("", INFO);
    int ret_init, ret_resp;
    sgx_status_t status_init, status_resp;

    /* 両Enclave共にLA（DHセッション）を初期化する */
    status_init = ecall_initiator_init_LA(initiator_enclave_id, &ret_init);
    status_resp = ecall_responder_init_LA(responder_enclave_id, &ret_resp);

    if(status_init != SGX_SUCCESS)
    {
        print_sgx_status(status_init);
        return -1;
    }

    if(status_resp != SGX_SUCCESS)
    {
        print_sgx_status(status_resp);
        return -1;
    }

    if(ret_init != 0 || ret_resp != 0) return -1;

    print_log_message("Initialized LA of Initiator and Responder successfully.", INFO);
    print_log_message("", INFO);

    /* 検証側は自身についての同一性情報（Target Info）と楕円曲線暗号キーペアを
     * 導出し、それら（キーペアは公開鍵のみ）をmsg1として取得する。
     * より厳密にLAに則るのであれば、証明側（Initiator）からリクエストを
     * 送信し、Responderがそれに応える形となる */
    sgx_dh_msg1_t msg1;
    memset(&msg1, 0, sizeof(sgx_dh_msg1_t));

    status_resp = ecall_responder_get_msg1(
        responder_enclave_id, &ret_resp, &msg1);
    
    if(status_resp != SGX_SUCCESS)
    {
        print_sgx_status(status_resp);
        return -1;
    }

    if(ret_resp) return -1;

    print_log_message("Generated msg1 successfully.", INFO);
    print_log_message("", INFO);

    /* msg1をInitiatorに渡し、ResponderのTarget Infoを材料に使った
     * レポートキーでREPORTを生成する。同時に楕円曲線暗号キーペアも生成し、
     * それら（キーペアは公開鍵のみ）をmsg2として取得する */
    sgx_dh_msg2_t msg2;
    memset(&msg2, 0, sizeof(sgx_dh_msg2_t));

    status_init = ecall_initiator_proc_msg1(
        initiator_enclave_id, &ret_init, &msg1, &msg2);

    memset(&msg1, 0, sizeof(sgx_dh_msg1_t));
    
    if(status_init != SGX_SUCCESS)
    {
        print_sgx_status(status_init);
        return -1;
    }

    if(ret_init) return -1;

    print_log_message("Processed msg1 and generated msg2 successfully.", INFO);
    print_log_message("", INFO);

    /* msg2をResponderに渡し、InitiatorのREPORTの検証を行う。
     * その後、InitiatorもResponderのREPORTを検証できるように、Initiatorの
     * REPORTからInitiatorのTarget Infoを抽出し、Responder側のREPORTを生成し
     * msg3を作成・取得する。Responder側でのセッション共通鍵生成もここで行う */
    sgx_dh_msg3_t msg3;
    memset(&msg3, 0, sizeof(sgx_dh_msg3_t));

    status_resp = ecall_responder_proc_msg2(
        responder_enclave_id, &ret_resp, &msg2, &msg3);
    
    memset(&msg2, 0, sizeof(sgx_dh_msg2_t));
    
    if(status_resp != SGX_SUCCESS)
    {
        print_sgx_status(status_resp);
        return -1;
    }

    if(ret_resp) return -1;

    print_log_message("Processed msg2 and generated msg3 successfully.", INFO);
    print_log_message("", INFO);

    /* msg3をInitiatorに渡し、Responder側のREPORTの検証を実施する。
     * Initiator側でのセッション共通鍵の生成もここで行う */
    status_init = ecall_initiator_proc_msg3(
        initiator_enclave_id, &ret_init, &msg3);
    
    if(status_init != SGX_SUCCESS)
    {
        print_sgx_status(status_init);
        return -1;
    }

    if(ret_init) return -1;

    memset(&msg3, 0, sizeof(sgx_dh_msg3_t));

    return 0;
}


int main()
{
    sgx_enclave_id_t initiator_enclave_id = -1;
    sgx_enclave_id_t responder_enclave_id = -1;

    std::string enclave_name = "initiator_enclave.signed.so";

    /* Initiator Enclaveの起動。自身の正当性を検証側（Responder）に
     * 示す側の「証明側Enclave」である */
    if(initialize_enclave(initiator_enclave_id, enclave_name) < 0)
	{
		std::cerr << "App: fatal error: Failed to initialize Initiator Enclave.";
		std::cerr << std::endl;
		return -1;
	}

    /* Responder Enclaveの起動。証明側Enclaveの正当性を検証する側の
     * 「検証側Enclave」である */
    enclave_name = "responder_enclave.signed.so";

    if(initialize_enclave(responder_enclave_id, enclave_name) < 0)
	{
		std::cerr << "App: fatal error: Failed to initialize Responder Enclave.";
		std::cerr << std::endl;
		return -1;
	}

    /* LAの実行。Responderとの通信が必要になった場合LAを実行する（例：データを保有する
     * Enclaveからのデータフェッチ時）に初めてLAをOCALL等経由して実施するケースが
     * 多いかも知れないが、今回は初めからLAで検証しておく */
    int ret = do_LA(initiator_enclave_id, responder_enclave_id);

    if(!ret)
    {
        print_log_message("LA accepted.", INFO);
        print_log_message("", INFO);
    }
    else
    {
        print_log_message("LA Rejected.", INFO);
        print_log_message("", INFO);

        exit(1);
    }


    /* サンプル用計算。Responderの持つ2値をAEKで暗号化して取得する。
     * ここでは数字が4桁であると仮定する前提で実装する点に注意 */
    print_log_message("==============================================", INFO);
    print_log_message("Sample computation", INFO);
    print_log_message("==============================================", INFO);
    print_log_message("", INFO);

    uint8_t *value1 = new uint8_t[4]();
    uint8_t *value2 = new uint8_t[4]();

    uint8_t *value1_iv = new uint8_t[12]();
    uint8_t *value2_iv = new uint8_t[12]();
    uint8_t *value1_tag = new uint8_t[16]();
    uint8_t *value2_tag = new uint8_t[16]();

    sgx_status_t status = ecall_responder_get_values(
        responder_enclave_id, &ret, value1, value2, 
        value1_iv, value2_iv, value1_tag, value2_tag);

    if(status != SGX_SUCCESS)
    {
        print_sgx_status(status);
        return -1;
    }

    /* Initiatorに暗号文を渡して平均値を計算し出力する */
    status = ecall_initiator_calc_average(
        initiator_enclave_id, &ret, value1, value2,
        value1_iv, value2_iv, value1_tag, value2_tag);

    if(status != SGX_SUCCESS)
    {
        print_sgx_status(status);
        return -1;
    }

    delete[] value1;
    delete[] value2;
    delete[] value1_iv;
    delete[] value2_iv;
    delete[] value1_tag;
    delete[] value2_tag;

    /* Enclaveをデストラクト */
    print_log_message("", INFO);
    print_log_message("==============================================", INFO);
    print_log_message("Destruct Enclaves", INFO);
    print_log_message("==============================================", INFO);
    print_log_message("", INFO);

    sgx_destroy_enclave(initiator_enclave_id);
    sgx_destroy_enclave(responder_enclave_id);

    print_log_message("Destructed enclaves successfully.", INFO);
    print_log_message("Exit program.", INFO);
    print_log_message("", INFO);

    return 0;
}