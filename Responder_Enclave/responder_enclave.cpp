#include "responder_enclave_t.h"
#include <sgx_trts.h>
#include <string.h>
#include <string>

/* LAv2を使用。この指定により、sgx_dh_*系のLA関連のAPIが、
 * 内部でLAv2用のものに置き換えられ、透過的にLAv2を利用できる */
#define SGX_USE_LAv2_INITIATOR
#include <sgx_dh.h>

#define MRENCLAVE_CHECK 1 //ここを0にするとMRENCLAVEの検証をスキップする


/* LAに関連する変数を格納 */
namespace ResponderLAParams
{
    sgx_dh_session_t session;
    sgx_key_128bit_t aek;
}


/* Initiatorに求める同一性情報のハードコーディング */
namespace IdentityRequest
{
    sgx_measurement_t mr_enclave = {
        0x7f, 0x64, 0x6d, 0x31, 0x88, 0x96, 0x9d, 0xab, 
        0xd2, 0x50, 0xd1, 0xb4, 0xfe, 0x8b, 0x0e, 0x11, 
        0x94, 0x29, 0x40, 0xe9, 0xb1, 0xe0, 0xfc, 0xbd, 
        0xf4, 0xf0, 0x5d, 0xa2, 0x29, 0x57, 0x38, 0xa8
    };

    sgx_measurement_t mr_signer = {
        0x4a, 0x94, 0xff, 0x27, 0x69, 0x36, 0x2a, 0xe6, 
        0x25, 0xc9, 0x0b, 0x38, 0x1f, 0x5a, 0xdb, 0xac, 
        0x03, 0x23, 0xa3, 0xb2, 0x47, 0x96, 0x65, 0x84, 
        0x36, 0xdc, 0x45, 0x89, 0xcd, 0xb4, 0x19, 0x19
    };

    sgx_prod_id_t isv_prod_id = 0;
    sgx_isv_svn_t isv_svn = 0;
}


/* LAの初期化 */
int ecall_responder_init_LA()
{
    sgx_status_t status = 
        sgx_dh_init_session(SGX_DH_SESSION_RESPONDER,
            &ResponderLAParams::session);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to initialize responder's LA.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}


/* msg1を生成 */
int ecall_responder_get_msg1(sgx_dh_msg1_t *msg1)
{
    sgx_status_t status = sgx_dh_responder_gen_msg1(
        msg1, &ResponderLAParams::session);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to get msg1 in responder.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}


/* msg2を処理しmsg3を生成 */
int ecall_responder_proc_msg2(sgx_dh_msg2_t *msg2, sgx_dh_msg3_t *msg3)
{
    /* Initiator側の同一性情報を格納する変数 */
    sgx_dh_session_enclave_identity_t initiator_identity;
    memset(&initiator_identity, 0, sizeof(sgx_dh_session_enclave_identity_t));

    sgx_status_t status = sgx_dh_responder_proc_msg2(msg2, msg3,
        &ResponderLAParams::session, &ResponderLAParams::aek, &initiator_identity);
    
    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to process msg2 and get msg3 in responder.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    /* 同一性情報の検証 */
    int res = 0;

    /* MRENCLAVE */
    if(MRENCLAVE_CHECK != 0)
    {
        res = memcmp(&initiator_identity.mr_enclave, &IdentityRequest::mr_enclave, 32);

        if(res)
        {
            const char *message = "MRENCLAVE mismatched.";
            ocall_print(message, 2); //2はエラーログである事を表す
            ocall_print_status(status);
            return -1;
        }
    }

    /* MRSIGNER */
    res = memcmp(&initiator_identity.mr_signer, &IdentityRequest::mr_signer, 32);

    if(res)
    {
        const char *message = "MRSIGNER mismatched.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    /* ISV ProdID */
    if(initiator_identity.isv_prod_id != IdentityRequest::isv_prod_id)
    {
        const char *message = "ISV ProdID mismatched.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    /* ISVSVN */
    if(initiator_identity.isv_svn < IdentityRequest::isv_svn)
    {
        const char *message = "Insufficient ISVSVN.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}


/* 2つの整数をAEKで暗号化してリターンするサンプル関数 */
int ecall_responder_get_values(uint8_t *value1, 
    uint8_t *value2, uint8_t *value1_iv, uint8_t *value2_iv,
    uint8_t *value1_tag, uint8_t *value2_tag)
{
    /* ハードコーディングしているため、これらの値は秘密でも何でも無いが、
     * あくまでサンプル用のものとしてこの形で用意している */
    int plain1 = 4000;
    int plain2 = 2000;

    sgx_status_t status;

    status = sgx_read_rand(value1_iv, 12);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to generate IV for value 1 in responder.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    status = sgx_read_rand(value2_iv, 12);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to generate IV for value 2 in responder.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    status = sgx_rijndael128GCM_encrypt(&ResponderLAParams::aek, 
        (uint8_t*)std::to_string(plain1).c_str(),4, value1, value1_iv, 12, 
        NULL, 0, (sgx_aes_gcm_128bit_tag_t*)value1_tag);

    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to encrypt value 1 in responder.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }
    
    status = sgx_rijndael128GCM_encrypt(&ResponderLAParams::aek, 
        (uint8_t*)std::to_string(plain2).c_str(),4, value2, value2_iv, 12, 
        NULL, 0, (sgx_aes_gcm_128bit_tag_t*)value2_tag);
    
    if(status != SGX_SUCCESS)
    {
        const char *message = "Failed to encrypt value 2 in responder.";
        ocall_print(message, 2); //2はエラーログである事を表す
        ocall_print_status(status);
        return -1;
    }

    return 0;
}