######## SGX SDKに関する設定 ########

# SGXSDKの場所
SGX_SDK ?= /opt/intel/sgxsdk
# 動作モード。ここではHWかSIM。make SGX_MODE=SIMのようにしてオプションで指定可能
SGX_MODE ?= HW
# マシンのアーキテクチャ。32bitか64bit
SGX_ARCH ?= x64
# Enclaveのデバッグモード。1ならDebug版、0なら製品版
SGX_DEBUG ?= 1


## マシンが32bitであればアーキテクチャの変数を更新する
ifeq ($(shell getconf LONG_BIT), 32)
	SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CXXFLAGS)), -m32)
	SGX_ARCH := x86
endif


## アーキテクチャに応じて使用するSGXSDKのツールを設定する
#  32bit版の場合
ifeq ($(SGX_ARCH), x86)
	SGX_COMMON_CFLAGS := -m32                         # コンパイル時の共通オプション
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib                # SGX関係のライブラリの場所
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign # SGX署名ツールの場所
	SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r     # Edger8r Toolの場所
#  64bit版の場合。それぞれの変数の内訳は同上
else
	SGX_COMMON_CFLAGS := -m64
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif


## DEBUGモードとPRERELEASEモードは同時に有効にできないので弾く
ifeq ($(SGX_DEBUG), 1)
ifeq ($(SGX_PRERELEASE), 1)
$(error Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!!)
endif
endif


## DEBUGモード有無に応じてコンパイル共通フラグに追記
ifeq ($(SGX_DEBUG), 1)
		SGX_COMMON_CFLAGS += -O0 -g # 最適化なし、コンパイル時デバック情報表示
else
		SGX_COMMON_CFLAGS += -O2    # 最適化あり
endif



######## Enclave外アプリケーション（App）に関する設定 ########

## シミュレーションモードの場合は専用のUntrusted用ライブラリを用いる
ifneq ($(SGX_MODE), HW)
	Urts_Library_Name := sgx_urts_sim
else
	Urts_Library_Name := sgx_urts
endif

## コンパイル時に使用するC/C++のソースを列挙
App_Cpp_Files := App/App.cpp App/error_print.cpp App/log_print.cpp

## 使用するincludeファイル（ヘッダ）がある場所を列挙
App_Include_Paths := -IApp -I$(SGX_SDK)/include

## Appのコンパイル時に使用するオプションを指定。
#  共通オプション、位置独立コード、不明なスコープ属性への警告を無視、Includeパス
App_C_Flags := $(SGX_COMMON_CFLAGS) -fPIC -Wno-attributes $(App_Include_Paths)


## EnclaveのDEBUGモードに応じてデバッグ可否のフラグをCコンパイルオプションに追加する。
#   Debug - DEBUGフラグを付与（デバッグ可）
#   Prerelease - NDEBUG（NO DEBUGの意）フラグとDEBUGフラグ双方を付与（デバッグ可らしい）
#   Release - NDEBUGフラグを付与（デバッグ不可）
#
#   これ必要？
ifeq ($(SGX_DEBUG), 1)
		App_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
		App_C_Flags += -DNDEBUG -DEDEBUG -UDEBUG
else
		App_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif


## 実際にはC++コンパイルするので、それ用の最終的なオプションを生成
App_Cpp_Flags := $(App_C_Flags) -std=c++11

## リンクオプション
App_Link_Flags := $(SGX_COMMON_CFLAGS) -L$(SGX_LIBRARY_PATH) \
                  -Wl,--whole-archive  -lsgx_uswitchless -Wl,--no-whole-archive \
				  -l$(Urts_Library_Name) -lpthread

## シミュレーションモードの場合は専用のライブラリを紐付ける
ifneq ($(SGX_MODE), HW)
	App_Link_Flags += -lsgx_uae_service_sim
else
	App_Link_Flags += -lsgx_uae_service
endif

## オブジェクトファイルを指定
App_Cpp_Objects := $(App_Cpp_Files:.cpp=.o)

## UntrustedのAppの実行バイナリ名を指定
App_Name := app



######## Initiator Enclaveに関する設定 ########
## シミュレーションモードの場合は専用のTrusted用ライブラリを用いる
ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif


## SGX用暗号ライブラリを指定（他にはIntel IPPなどが使えるはず）
Crypto_Library_Name := sgx_tcrypto

## コンパイル時に使用するC/C++のソースを列挙
Initiator_Enclave_Cpp_Files := Initiator_Enclave/initiator_enclave.cpp

## 使用するincludeファイル（ヘッダ）がある場所を列挙
Initiator_Enclave_Include_Paths := -IInitiator_Enclave -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc -I$(SGX_SDK)/include/stlport \
									-I$(SGX_SDK)/include/libcxx


## Enclaveのコンパイル時に使用するオプションを指定。
#  共通オプション、通常のincludeファイルを検索しない（SGX専用のを使う）、
#  シンボルの外部隠蔽、位置独立実行形式、スタック保護有効化、使用するIncludeファイルのパス
Initiator_Enclave_C_Flags := $(SGX_COMMON_CFLAGS) -nostdinc -fvisibility=hidden -fpie -fstack-protector \
							$(Initiator_Enclave_Include_Paths)

## 実際にはC++コンパイルするので、それ用の最終的なオプションを生成
Initiator_Enclave_Cpp_Flags := $(Initiator_Enclave_C_Flags) -std=c++11 -nostdinc++

## 多すぎるので詳細はDeveloper Reference参照。Switchless CallのIncludeを忘れない事。
Initiator_Enclave_Link_Flags := $(SGX_COMMON_CFLAGS) -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH) \
	-Wl,--whole-archive -l$(Trts_Library_Name) -lsgx_tswitchless -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Service_Library_Name) -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0
	# -Wl,--version-script=Enclave/Enclave.lds


## オブジェクトファイルを設定
Initiator_Enclave_Cpp_Objects := $(Initiator_Enclave_Cpp_Files:.cpp=.o)


## Enclaveイメージ名とEnclave設定ファイル名の設定
Initiator_Enclave_Name := initiator_enclave.so
Initiator_Signed_Enclave_Name := initiator_enclave.signed.so
Initiator_Enclave_Config_File := Initiator_Enclave/Enclave.config.xml


######## Responder Enclaveに関する設定 ########
## シミュレーションモードの場合は専用のTrusted用ライブラリを用いる
ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif


## SGX用暗号ライブラリを指定（他にはIntel IPPなどが使えるはず）
Crypto_Library_Name := sgx_tcrypto

## コンパイル時に使用するC/C++のソースを列挙
Responder_Enclave_Cpp_Files := Responder_Enclave/responder_enclave.cpp

## 使用するincludeファイル（ヘッダ）がある場所を列挙
Responder_Enclave_Include_Paths := -IResponder_Enclave -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc -I$(SGX_SDK)/include/stlport \
									-I$(SGX_SDK)/include/libcxx


## Enclaveのコンパイル時に使用するオプションを指定。
#  共通オプション、通常のincludeファイルを検索しない（SGX専用のを使う）、
#  シンボルの外部隠蔽、位置独立実行形式、スタック保護有効化、使用するIncludeファイルのパス
Responder_Enclave_C_Flags := $(SGX_COMMON_CFLAGS) -nostdinc -fvisibility=hidden -fpie -fstack-protector \
							$(Responder_Enclave_Include_Paths)

## 実際にはC++コンパイルするので、それ用の最終的なオプションを生成
Responder_Enclave_Cpp_Flags := $(Responder_Enclave_C_Flags) -std=c++11 -nostdinc++

## 多すぎるので詳細はDeveloper Reference参照。Switchless CallのIncludeを忘れない事。
Responder_Enclave_Link_Flags := $(SGX_COMMON_CFLAGS) -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH) \
	-Wl,--whole-archive -l$(Trts_Library_Name) -lsgx_tswitchless -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Service_Library_Name) -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0
	# -Wl,--version-script=Enclave/Enclave.lds


## オブジェクトファイルを設定
Responder_Enclave_Cpp_Objects := $(Responder_Enclave_Cpp_Files:.cpp=.o)


## Enclaveイメージ名とEnclave設定ファイル名の設定
Responder_Enclave_Name := responder_enclave.so
Responder_Signed_Enclave_Name := responder_enclave.signed.so
Responder_Enclave_Config_File := Responder_Enclave/Enclave.config.xml


## HWモードかつRELEASEモードの際は専用のフラグを設定
ifeq ($(SGX_MODE), HW)
ifneq ($(SGX_DEBUG), 1)
ifneq ($(SGX_PRERELEASE), 1)
Build_Mode = HW_RELEASE
endif
endif
endif


## makeコマンド向け設定
#  make時にallコマンドとrunコマンドに対応（例：make all）
.PHONY: all run

## ややこしいが、Makefileはその場で依存関係が解決できない場合は後続の行を見に行くため、
## allやrunの内容はMakefileのこの行までの記述で実現はできない（Makeが後ろの方を勝手に見てくれる）

## RELEASEモードの場合のみ署名に関するメッセージを表示
ifeq ($(Build_Mode), HW_RELEASE)
all: $(App_Name) $(Initiator_Enclave_Name)
	@echo "The project has been built in release hardware mode."
	@echo "Please sign the $(Initiator_Enclave_Name) first with your signing key before you run the $(App_Name) to launch and access the enclave."
	@echo "To sign the enclave use the command:"
	@echo "   $(SGX_ENCLAVE_SIGNER) sign -key <your key> -enclave $(Initiator_Enclave_Name) -out <$(Initiator_Signed_Enclave_Name)> -config $(Enclave_Config_File)"
	@echo "You can also sign the enclave using an external signing tool. See User's Guide for more details."
	@echo "To build the project in simulation mode set SGX_MODE=SIM. To build the project in prerelease mode set SGX_PRERELEASE=1 and SGX_MODE=HW."
else
## RELEASEでない場合はビルドのみ実行（その際後続の処理を参照する）
all: $(App_Name) $(Initiator_Signed_Enclave_Name) $(Responder_Signed_Enclave_Name)
endif

run: all # runはallの結果に依存
ifneq ($(Build_Mode), HW_RELEASE)
	@$(CURDIR)/$(App_Name)
	@echo "RUN  =>  $(App_Name) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif


######## Appオブジェクト関する設定（つまりビルド設定） ########

## Edger8rによりUntrusted向けエッジ関数のソースを生成

App/initiator_enclave_u.c: $(SGX_EDGER8R) Initiator_Enclave/initiator_enclave.edl
	@cd App && $(SGX_EDGER8R) --untrusted ../Initiator_Enclave/initiator_enclave.edl --search-path ../Initiator_Enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

App/responder_enclave_u.c: $(SGX_EDGER8R) Responder_Enclave/responder_enclave.edl
	@cd App && $(SGX_EDGER8R) --untrusted ../Responder_Enclave/responder_enclave.edl --search-path ../Responder_Enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

## ソースによりエッジ関数のオブジェクトファイルを生成。$(CC)は暗黙のルールにより、デフォルトでccコマンド。
App/initiator_enclave_u.o: App/initiator_enclave_u.c
	@$(CC) $(App_C_Flags) -c $^ -o $@
	@echo "CC   <=  $<"

App/responder_enclave_u.o: App/responder_enclave_u.c
	@$(CC) $(App_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

## Appのオブジェクトファイルを生成。$(CC)は暗黙のルールにより、デフォルトでg++コマンド。
App/%.o: App/%.cpp
	@$(CXX) $(App_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

## リンクによりAppの実行ファイルを生成
$(App_Name): App/initiator_enclave_u.o App/responder_enclave_u.o $(App_Cpp_Objects)
	@$(CXX) $^ -o $@ $(App_Link_Flags)
	@echo "LINK =>  $@"


######## Initiator Enclaveオブジェクト関する設定（つまりビルド設定） ########

## Edger8rによりTrusted向けエッジ関数のソースを生成
Initiator_Enclave/initiator_enclave_t.c: $(SGX_EDGER8R) Initiator_Enclave/initiator_enclave.edl
	@cd Initiator_Enclave && $(SGX_EDGER8R) --trusted ../Initiator_Enclave/initiator_enclave.edl --search-path ../Initiator_Enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

## ソースによりエッジ関数のオブジェクトファイルを生成
Initiator_Enclave/initiator_enclave_t.o: Initiator_Enclave/initiator_enclave_t.c
	@$(CC) $(SGX_COMMON_CFLAGS) $(Initiator_Enclave_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

## Enclaveのオブジェクトファイルを生成
Initiator_Enclave/%.o: Initiator_Enclave/%.cpp
	@$(CXX) $(Initiator_Enclave_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

## Enclaveの未署名イメージ（共有ライブラリ）の生成
$(Initiator_Enclave_Name): Initiator_Enclave/initiator_enclave_t.o $(Initiator_Enclave_Cpp_Objects)
	@$(CXX) $^ -o $@ $(Initiator_Enclave_Link_Flags)
	@echo "LINK =>  $@"

## Enclave未署名イメージに対しsgx_signで署名を実施
$(Initiator_Signed_Enclave_Name): $(Initiator_Enclave_Name)
	@$(SGX_ENCLAVE_SIGNER) sign -key Initiator_Enclave/private_key.pem -enclave $(Initiator_Enclave_Name) -out $@ -config $(Initiator_Enclave_Config_File)
	@echo "SIGN =>  $@"


######## Responder Enclaveオブジェクト関する設定（つまりビルド設定） ########

## Edger8rによりTrusted向けエッジ関数のソースを生成
Responder_Enclave/responder_enclave_t.c: $(SGX_EDGER8R) Responder_Enclave/responder_enclave.edl
	@cd Responder_Enclave && $(SGX_EDGER8R) --trusted ../Responder_Enclave/responder_enclave.edl --search-path ../Responder_Enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

## ソースによりエッジ関数のオブジェクトファイルを生成
Responder_Enclave/responder_enclave_t.o: Responder_Enclave/responder_enclave_t.c
	@$(CC) $(Responder_Enclave_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

## Enclaveのオブジェクトファイルを生成
Responder_Enclave/%.o: Responder_Enclave/%.cpp
	@$(CXX) $(Responder_Enclave_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

## Enclaveの未署名イメージ（共有ライブラリ）の生成
$(Responder_Enclave_Name): Responder_Enclave/responder_enclave_t.o $(Responder_Enclave_Cpp_Objects)
	@$(CXX) $^ -o $@ $(Responder_Enclave_Link_Flags)
	@echo "LINK =>  $@"

## Enclave未署名イメージに対しsgx_signで署名を実施
$(Responder_Signed_Enclave_Name): $(Responder_Enclave_Name)
	@$(SGX_ENCLAVE_SIGNER) sign -key Responder_Enclave/private_key.pem -enclave $(Responder_Enclave_Name) -out $@ -config $(Responder_Enclave_Config_File)
	@echo "SIGN =>  $@"



## クリーンアップ用サブコマンドの定義
.PHONY: clean

clean:
	@rm -f $(App_Name) $(Initiator_Enclave_Name) $(Initiator_Signed_Enclave_Name) $(Responder_Enclave_Name) $(Responder_Signed_Enclave_Name) $(App_Cpp_Objects) App/initiator_enclave_u.* App/responder_enclave_u.* $(Initiator_Enclave_Cpp_Objects) $(Responder_Enclave_Cpp_Objects) Initiator_Enclave/initiator_enclave_t.* Responder_Enclave/responder_enclave_t.*