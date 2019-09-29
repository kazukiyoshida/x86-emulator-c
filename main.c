/*
 * x86 アーキテクチャ
 * https://ja.wikibooks.org/wiki/X86アセンブラ/x86アーキテクチャ
 */

// printf について
// %08x .. 8桁. 0を詰める. 16進数
// %s   .. 文字列
// %X   .. x は16進数を示す. x, X はアルファベットの大小.
// %lu  .. unsigned long 型の数値を10進数で表記
// %lx  .. unsigned long 型の数値を16進数で表記
// %#lx .. unsigned long 型の数値を16進数で表記、先頭に 0x 付与.
// %p   .. ポインタ変数が格納しているアドレスを表記する

// 10進数と16進数について
// 0x1     = 1       = 16^0
// 0x10    = 16      = 16^1
// 0x100   = 256     = 16^2
// 0x200   = 512     = 16^2 * 2
// 0x1000  = 4096    = 16^3
// 0x10000 = 1048576 = 16^4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// メモリのサイズ. 1000 kB ?
#define MEMORY_SIZE ( 1024 * 1024 )

// レジスタ Enum
enum Register {
  EAX,  // アキュムレータレジスタ. 算術演算操作の結果が格納される.
  ECX,  // カウンタレジスタ. ループ命令に使用される.
  EDX,  // データレジスタ. 算術演算操作と IO に使用される.
  EBX,  // ベースレジスタ.
  ESP,  // スタックポインタレジスタ. スタックのトップを指し示すポインタ.
  EBP,  // スタックベースポインタ. スタックのベースを指し示すポインタ.
  ESI,  // ソースレジスタ.
  EDI,  // デスティネーションレジスタ.
  REGISTERS_COUNT  // = 8. 8種類のレジスタがあることを示す.
};

/*
 * Emulator 構造体
 */
typedef struct {
  /*
   * 汎用レジスタ
   * 要素数8 の uint32 配列.
   */
  uint32_t registers[REGISTERS_COUNT];

  /*
   * EFLAGS レジスタ
   * CPU の特殊なレジスタ. 32bit のレジスタで、1つ1つの bit が操作の結果や
   * プロセッサの状態などを格納し、制御の判断材料とする.
   *   0 : CF : キャリーフラグ. 算術演算操作でレジスタの大きさを超えた場合にセット(1)される
   *   9 : IF : 割り込み可能フラグ. 割り込みを有効化したい場合にセットする.
   *   12: IOPL: IO特権レベルフィールド. 現在のプロセスのIO特権レベルを示す.
   *   ..
   */
  uint32_t eflags;

  /*
   * メモリ（バイト列）のポインタ.
   * ホストPCのメモリの一部を使用するイメージ. 1区画8bitでそれが大量に並んでいる.
   * uint8 なので「ポインタ変数 memory が保持するアドレスの先では、0b00000000 などの数字が
   * 保持されている」ということ.
   * x86アーキはリトルエンディアン. 0xb3b2b1b0 はメモリ上では下のようになる.
   *   .. | b0 | b1 | b2 | b3 | ..
   */
  uint8_t* memory;

  /*
   * 命令ポインタ.
   * CPU の特殊なレジスタ. プログラムカウンタとも呼ばれる.
   * 分岐がおきない前提で、次に実行される命令のアドレスが保持される.
   */
  uint32_t eip;
} Emulator;

/*
 * エミュレータを作成する
 */
Emulator* createEmu(
    size_t size,  // メモリサイズ
    uint32_t eip, // 命令ポインタ
    uint32_t esp  // スタックポインタレジスタ（スタックのトップを指し示すポインタ）
) {
  Emulator* emu = malloc(sizeof(Emulator));

  // ホストPC のメモリから枠を確保し、 emu->memory にその枠へのアドレスを保存.
  // emu構造体のmemoryポインタ変数が実際に格納している値は、mallocで確保した先頭アドレス
  // emu、memory どちらもポインタ変数なのでやや分かりづらい
  // これでsize分だけがっつりと領域を確保し、emu->memory はそこの先頭アドレスを取得.
  emu->memory = malloc(size);

  // 汎用レジスタの初期値を全て 0 にする
  // memset はメモリに指定バイト数分の値をセットする
  // 1.メモリのポインタ、2.セットする値、3.セットするサイズ
  memset(emu->registers, 0, sizeof(emu->registers));

  // レジスタの初期値を指定されたものにする
  emu->eip = eip;
  emu->registers[ESP] = esp;

  printf("============== createEmu ==============\n");
  printf("memory size    : %lu\n", size);
  printf("emu            : %#08x\n", emu->eip);
  printf("registers[ESP] : %#08x\n", emu->registers[ESP]);
  printf("確保した領域のアドレス : %p\n", emu->memory);
  printf("=======================================\n");

  return emu;
}

/*
 * エミュレータを破棄する
 */
void destroyEmu(Emulator* emu)
{
  free(emu->memory);
  free(emu);
}

/*
 * 汎用レジスタとプログラムカウンタの値を標準出力する
 */
static void dumpRegisters(Emulator* emu)
{
  // レジスタ名一覧
  char* registersName[] = {
    "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"
  };
  int i;
  printf(">>> registers\n");
  for (i = 0; i < REGISTERS_COUNT; i++) {
    printf("%s = %08x\n", registersName[i], emu->registers[i]);
  }
  printf("EIP = %08x\n\n", emu->eip);
}

/*
 *
 * 32bit の数値 = 4byte の数値 = 16進数2ケタ
 */
uint32_t getCode8(Emulator* emu, int index)
{
  printf("###\n");
  printf("###\n");
  printf("### emu->eip         :  %d\n", emu->eip);
  printf("### index            :  %d\n", index);
  printf("### emu->eip + index :  %d\n", emu->eip + index);
  printf("### return           :  %02x\n", emu->memory[emu->eip + index]);
  printf("### --------- \n");
  printf("### memory           :  %#02x\n", emu->memory[0]);
  printf("### memory           :  %#02x\n", emu->memory[1]);
  printf("### memory           :  %#02x\n", emu->memory[2]);
  printf("### memory           :  %#02x\n", emu->memory[3]);
  printf("### memory           :  %#02x\n", emu->memory[4]);
  printf("### memory           :  %#02x\n", emu->memory[5]);
  printf("### memory           :  %#02x\n", emu->memory[6]);
  printf("### memory           :  %#02x\n", emu->memory[7]);
  printf("### memory           :  %#02x\n", emu->memory[8]);
  printf("### memory           :  %#02x\n", emu->memory[9]);
  printf("###\n");
  printf("###\n");
  return emu->memory[emu->eip + index];
}

int32_t getSignCode8(Emulator* emu, int index)
{
  return (int8_t)emu->memory[emu->eip + index];
}

uint32_t getCode32(Emulator* emu, int index)
{
  int i;
  uint32_t ret = 0;

  // リトルエンディアンでメモリの値を取得する
  for (i = 0; i < 4; i++) {
    ret |= getCode8(emu, index + i) << (i * 8);
  }
  return ret;
}

void mov_r32Imm32(Emulator* emu) {
  uint8_t reg = getCode8(emu, 0) - 0xB8;
  uint32_t value = getCode32(emu, 1);
  emu->registers[reg] = value;
  emu->eip += 5;
}

void short_jump(Emulator* emu)
{
  int8_t diff = getSignCode8(emu, 1);
  emu->eip += (diff + 2);
}

typedef void instructionFunc_t(Emulator*);

/*
 * 関数ポインタテーブル instructions
 * 要素数 256 個、instructionFunc_t のポインタの配列.
 */
instructionFunc_t* instructions[256];

/*
 * 関数ポインタテーブルを初期化する.
 */
void initInstructions(void)
{

  // 関数ポインタテーブルを全て0で埋める.
  // memset はメモリに指定バイト数分の値をセットする
  // 1.メモリのポインタ、2.セットする値、3.セットするサイズ
  memset(instructions, 0, sizeof(instructions));

  // 関数ポインタテーブル
  // 要素1つ .. 8byte = 64bit
  // 配列全体 .. 8byte * 256 = 2048byte
  printf(">>> instructions[0] size: %lu\n", sizeof(instructions[0]));
  printf(">>> instructions size: %lu\n", sizeof(instructions));

  int i;
  for (i = 0; i < 8; i++) {
    instructions[0xB8 + i] = mov_r32Imm32;
  }
  instructions[0xEB] = short_jump;
}

/*
 * main
 * 引数として機械語プログラムが格納されたファイルを指定する.
 */
int main(int argc, char* argv[])
{
  FILE* binary;
  Emulator* emu;

  if(argc != 2) {
    printf("usage: px86 filename\n");
    return 1;
  }

  // 命令ポインタ EIP が 0、ESP が 0x7c00 の状態のエミュレータを作る
  emu = createEmu(MEMORY_SIZE, 0x0000, 0x7c00);

  dumpRegisters(emu);

  binary = fopen(argv[1], "rb");
  if (binary == NULL) {
    printf("cannot open %s file\n", argv[1]);
    return 1;
  }

  // 機械語ファイルを開きエミュレータのメモリに保存する.
  //（最大512バイト = 0x200 バイト）
  // arg1. 格納するバッファのアドレス.
  // arg2. 一つのまとまりとして読み込むデータのバイト数 1byte = 8bit ずつ.
  // arg3. 読み込むデータの個数
  // arg4. ファイルポインタ
  fread(emu->memory, 1, 0x200, binary);
  fclose(binary);

  printf("-------------------------------\n");
  printf("### memory           :  %#02x\n", emu->memory[0]);
  printf("### memory           :  %#02x\n", emu->memory[1]);
  printf("### memory           :  %#02x\n", emu->memory[2]);
  printf("### memory           :  %#02x\n", emu->memory[3]);
  printf("### memory           :  %#02x\n", emu->memory[4]);
  printf("### memory           :  %#02x\n", emu->memory[5]);
  printf("### memory           :  %#02x\n", emu->memory[6]);
  printf("### memory           :  %#02x\n", emu->memory[7]);
  printf("### memory           :  %#02x\n", emu->memory[8]);
  printf("### memory           :  %#02x\n", emu->memory[9]);
  printf("-------------------------------\n");

  initInstructions();

  // 1つの命令を実行するたびに EIP をチェックし、 0 ならばメインループを
  // 終了させる.（普通の CPU には終了機能はないが、エミュレーションを終了させる）
  while (emu->eip < MEMORY_SIZE) {
    uint8_t code = getCode8(emu, 0);

    printf("EIP = %x, Code = %02x\n", emu->eip, code);

    // 実装されていない命令がきたら VM を終了する
    if (instructions[code] == NULL) {
      printf("\n\nNot Implemented: %x\n", code);
      break;
    }

    // 命令の実行
    instructions[code](emu);
    if (emu->eip == 0x00) {
      printf("\n\nend of program. \n\n");
      break;
    }
  }

  dumpRegisters(emu);
  destroyEmu(emu);

  return 0;
}
