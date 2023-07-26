#include "initiator_enclave_t.h"
#include <sgx_trts.h>
#include <string.h>
#include <stdlib.h>
#include <string>

/* LAv2を使用。この指定により、sgx_dh_*系のLA関連のAPIが、
 * 内部でLAv2用のものに置き換えられ、透過的にLAv2を利用できる */
#define SGX_USE_LAv2_INITIATOR
#include <sgx_dh.h>


/* LAに関連する変数を格納 */
namespace InitiatorLAParams
{
    sgx_dh_session_t session;
    sgx_key_128bit_t aek;
}


/* Responderに求める同一性情報のハードコーディング */
namespace IdentityRequest
{
    /* ResponderのMRENCLAVEは、LAの片方向性により検証はしない */
    sgx_measurement_t mr_signer = {
        0xfd, 0x9c, 0x50, 0x01, 0x42, 0x64, 0x13, 0x9a, 
        0x83, 0x01, 0xab, 0x5d, 0x9e, 0x78, 0x4e, 0x7d, 
        0x97, 0xa8, 0x64, 0x73, 0x33, 0x64, 0x4e, 0x81, 
        0x2a, 0x36, 0x11, 0x6f, 0x87, 0xd5, 0xcc, 0x99
    };

    sgx_prod_id_t isv_prod_id = 0;
    sgx_isv_svn_t isv_svn = 0;
}


/* LAの初期化 */
int ecall_initiator_init_LA()
{
    sgx_status_t status = 
        sgx_dh_init_session(SGX_DH_SESSION_INITIATOR,
            &InitiatorLAParams::session);
    
    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to initialize initiator's LA.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}


/* msg1を処理しmsg2を生成 */
int ecall_initiator_proc_msg1(sgx_dh_msg1_t *msg1, sgx_dh_msg2_t *msg2)
{
    sgx_status_t status = sgx_dh_initiator_proc_msg1(
        msg1, msg2, &InitiatorLAParams::session);
    
    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to process msg1 and get msg2 in initiator.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}


/* msg3を処理 */
int ecall_initiator_proc_msg3(sgx_dh_msg3_t *msg3)
{
    /* Responder側の同一性情報を格納する変数 */
    sgx_dh_session_enclave_identity_t responder_identity;
    memset(&responder_identity, 0, sizeof(sgx_dh_session_enclave_identity_t));

    sgx_status_t status = sgx_dh_initiator_proc_msg3(msg3, 
        &InitiatorLAParams::session, &InitiatorLAParams::aek, &responder_identity);
    
    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to process msg3 in initiator.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    /* 同一性情報の検証 */
    int res = 0;

    /* MRSIGNER */
    res = memcmp(&responder_identity.mr_signer, &IdentityRequest::mr_signer, 32);

    if(res)
    {
        const char *message = "MRSIGNER mismatched.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    /* ISV ProdID */
    if(responder_identity.isv_prod_id != IdentityRequest::isv_prod_id)
    {
        const char *message = "ISV ProdID mismatched.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    /* ISVSVN */
    if(responder_identity.isv_svn < IdentityRequest::isv_svn)
    {
        const char *message = "Insufficient ISVSVN.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}


/* Responderから受け取った2値の平均を計算し標準出力 */
int ecall_initiator_calc_average(uint8_t *value1, 
    uint8_t *value2, uint8_t *value1_iv, uint8_t *value2_iv, 
    uint8_t *value1_tag, uint8_t *value2_tag)
{
    sgx_status_t status;

    /* ヌル終端分も確保 */
    char *plain1 = new char[5]();
    char *plain2 = new char[5]();

    status = sgx_rijndael128GCM_decrypt(&InitiatorLAParams::aek,
        value1, 4, (uint8_t*)plain1, value1_iv, 12, NULL, 0, 
        (sgx_aes_gcm_128bit_tag_t*)value1_tag);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to decrypt value1.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    status = sgx_rijndael128GCM_decrypt(&InitiatorLAParams::aek,
        value2, 4, (uint8_t*)plain2, value2_iv, 12, NULL, 0, 
        (sgx_aes_gcm_128bit_tag_t*)value2_tag);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to decrypt value2.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    int plain1_int = atoi(plain1);
    int plain2_int = atoi(plain2);

    ocall_print(std::to_string((plain1_int + plain2_int)/2).c_str(), 1);

    delete[] plain1;
    delete[] plain2;

    return 0;
}