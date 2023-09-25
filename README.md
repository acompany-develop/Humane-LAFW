# Humane Intel SGX Local Attestation Framework (Humane-LAFW)
## 概要
本リポジトリは、Intel SGXにおけるLocal Attestation（以下、LA）を「人道的な（Humane）」難易度でプロセス内完結で手軽に実現する事の出来る、LAフレームワーク（LAFW）のコードを格納しています。

公式SGXSDKに同梱されているものに比べ、プロセス間ではなくプロセス内で完結している事、また必要最低限の処理を解説コメント付きで実装している事から、より様々な方にとって理解しやすく利用しやすいものとなっています。

Humane−LAFWでは、Enclave外のアプリケーション（App）が、自身の同一性を証明する証明側Enclave（Initiator）と、証明側Enclaveを検証する検証側Enclave（Responder）の間を取り持つ形でLAを進行させていきます。

## 導入
### 動作確認環境
* OS: Ubuntu 22.04.3 LTS
* SGXSDK: バージョン2.21

### SGX環境構築
Linux-SGXをクローンし、READMEに従ってSGXSDK及びSGXPSWを導入してください。

使用しているOSのLinuxカーネルが5.11以降である場合、SGXドライバがデフォルトで組み込まれている（in-kernelドライバ）ため、自前で導入する必要はありません。

5.11未満のLinuxカーネルを使用している場合は、

* linux-sgx-driver
* linux SGX DCAP driver

のいずれかを導入してください。

### Humane-LAFWの展開
任意のディレクトリにて、本リポジトリをクローンしてください。

## 準備
### Enclave署名鍵の設定
Enclaveの署名に使用する鍵は、デフォルトで`Initiator_Enclave/private_key.pem`及び`Responder_Enclave/private_key.pem`として格納しており、これを使用しています。

ただ、実運用時には自前で生成したものを使用するのが望ましいため、以下のコマンドにて両Enclave向けに別々に新規作成し、上記のパスに同名でその鍵を格納してください。

```
openssl genrsa -out private_key.pem -3 3072
```

### 証明側の同一性情報の準備
LAでは、証明側EnclaveのMRENCLAVEとMRSIGNERを検証側Enclaveにハードコーディングし、それを用いて同一性検証を行うため、一度Enclaveをビルドし、そこからMRENCLAVE等を抽出してあげる必要があります。  

また、証明側もMRSIGNERに限り検証側の同一性を検証できるため、検証側Enclaveについての同一性情報も出力できるようにしてあります。

以下の手順でこの2つの値を抽出し、ハードコーディングを行います。

* 以下のコマンドをリポジトリのルートディレクトリで実行し、各種アプリケーションやEnclaveをビルドする。
  ```
  make
  ```

* 同一性情報抽出用補助ツールである`mr-extract`が配置されているディレクトリに移動する。
  ```
  cd subtools/mr-extract
  ```
* SGXSDKや署名済みEnclaveイメージ名が以下の通りではない場合は、`mr-extract.cpp`を開き、適宜以下の部分を編集する。

  ``` cpp
  /* SGXSDKのフォルダパスはここで指定。自身の環境に合わせて変更する */
  std::string sdk_path = "/opt/intel/sgxsdk/";

  /* 署名済みEnclaveイメージファイル名はここで指定。
  * 自身の環境に合わせて変更する */
  std::string image_path = "../../enclave.signed.so";
  ```

* makeコマンドでビルドする。
  ```
  make
  ```

* ビルドにより生成された実行ファイルを実行する。
  ```
  ./mr-extract
  ```

* 以下のような標準出力が表示されるため、検証側Enclaveの同一性情報を表示する場合は0を、証明側Enclaveのものを表示する場合は1を入力しEnterを押す。
  ```
  Input 0 or 1 (0: responder, 1: initiator):
  ```

* 続いて、以下のような標準出力が表示される。
  ```
  -------- message from sgx_sign tool --------
  Succeed.
  --------------------------------------------

  Copy and paste following measurement values into enclave code.
  MRENCLAVE value -> 
  0x6c, 0xea, 0x2a, 0xf0, 0x97, 0x51, 0x62, 0x02, 
  0xa3, 0xb1, 0xfd, 0x59, 0x49, 0x4a, 0x29, 0x91, 
  0x14, 0x81, 0xea, 0x55, 0x32, 0x77, 0x6a, 0x91, 
  0x09, 0x06, 0xe7, 0x67, 0x28, 0x2e, 0x93, 0x0d

  MRSIGNER value  -> 
  0x4a, 0x94, 0xff, 0x27, 0x69, 0x36, 0x2a, 0xe6, 
  0x25, 0xc9, 0x0b, 0x38, 0x1f, 0x5a, 0xdb, 0xac, 
  0x03, 0x23, 0xa3, 0xb2, 0x47, 0x96, 0x65, 0x84, 
  0x36, 0xdc, 0x45, 0x89, 0xcd, 0xb4, 0x19, 0x19
  ```

* 検証側Enclaveの同一性情報を出力させた場合には`Initiator_Enclave/initiator_enclave.cpp`の
  ```cpp
  /* ResponderのMRENCLAVEは、LAの片方向性により検証はしない */
  sgx_measurement_t mr_signer = {
      0xfd, 0x9c, 0x50, 0x01, 0x42, 0x64, 0x13, 0x9a, 
      0x83, 0x01, 0xab, 0x5d, 0x9e, 0x78, 0x4e, 0x7d, 
      0x97, 0xa8, 0x64, 0x73, 0x33, 0x64, 0x4e, 0x81, 
      0x2a, 0x36, 0x11, 0x6f, 0x87, 0xd5, 0xcc, 0x99
  };
  ```
  の部分に`MRSIGNER value ->`の部分の4行を上書きする形でハードコーディングする。  
  証明側Enclaveの同一性情報を出力させた場合には、`Responder_Enclave/responder_enclave.cpp`の
  ```cpp
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
  ```
  の部分に`MRENCLAVE value ->`と`MRSIGNER value ->`部分のそれぞれ4行を上書きする形でハードコーディングする。

* リポジトリのルートディレクトリに戻り、再度アプリケーションやEnclaveのビルドを行う。
  ```
  make
  ```

## 実行
ビルドと設定が完了したら、以下のコマンドで実行バイナリを実行し、LAを実行します。

```
./app
```

その後はLAが実行され、LAにおける同一性検証に成功したら、LAに並行して実施した楕円曲線ディフィー・ヘルマン鍵共有にて取得したセッション共通鍵を用いて、両Enclave間で安全な暗号通信路を確立させます。

その後、検証側が秘密情報として、検証側Enclave内のサンプルの2つの整数を証明側に安全に送信し、証明側Enclaveがそれらを復号して2値の平均を算出し、結果の平均値をOCALLで標準出力する、簡単なサンプルの秘密計算が実行されます。