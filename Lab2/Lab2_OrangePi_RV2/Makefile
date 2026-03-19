# Makefile for RISC-V Bootloader
# 這個 Makefile 用於編譯和鏈接 RISC-V 引導程式，生成可執行的二進位文件，並根據不同的目標板子設定相應的地址和參數。
# 主要包含以下部分：
# 1. 工具鏈與環境變數定義：定義交叉編譯器、編譯器命令、物件轉換工具和目標板子。
# 2. 根據目標板子設定引導程式的基地址和載入地址：根據選擇的板子設定引導程式在存儲設備上的位置和被加載到內存中的位置。
# 3. 編譯與連結參數 (Flags)：定義編譯器和連結器的參數，包括警告選項、優化選項、目標架構、ABI 和鏈接腳本等。
# 4. 定義 FIT_DTB 變數：定義用於生成 FIT 映像的設備樹二進位文件。
# 5. 定義 Makefile 的目標和規則：定義生成 bootloader.elf、bootloader.bin 和 kernel.fit 的規則，以及清理生成文件的 clean 目標。	

# 建置流程圖解：
# 1. 使用 start.S 和 bootloader.c 編譯並鏈接生成 bootloader.elf，使用定義的 CFLAGS 和 LDFLAGS。
# 2. 使用 objcopy 將 bootloader.elf 轉換為純二進位文件 bootloader.bin。
# 3. 使用 mkimage 根據 kernel.its 文件生成 kernel.fit，這是一個 FIT 映像，包含了引導程式、內核映像、設備樹和初始 RAM 磁盤.




# 1. 工具鏈與環境變數定義
# 定義交叉編譯器的前綴，若未指定則預設為 riscv64-unknown-elf [cite: 16]
CROSS   ?= riscv64-unknown-elf
# 定義 C 編譯器命令，使用交叉編譯器的 riscv64-unknown-elf-gcc
CC      := $(CROSS)-gcc
# 指定物件轉換工具，用於將 ELF 轉為純二進位檔 (bin)，定義 objcopy 命令，使用交叉編譯器的 riscv64-unknown-elf-objcopy
OBJCOPY := $(CROSS)-objcopy
# 定義目標板子，預設為 rv2 (make BOARD=rv2)，可以通過命令行參數覆蓋，例如 make BOARD=qemu
BOARD ?= rv2

# 2. 根據目標板子設定引導程式的基地址和載入地址
# 如果目標板是 Orange Pi RV2
ifeq ($(BOARD),rv2)			
BOOT_BASE := 0x00200000		# 定義引導程式的基地址為 0x00200000，這是引導程式在存儲設備上的位置
LOAD_ADDR := 0x20000000		# 定義引導程式的載入地址為 0x20000000，這是引導程式被加載到內存中的位置		
# 如果目標板是 QEMU 模擬器
else ifeq ($(BOARD),qemu)
BOOT_BASE := 0x80200000		# 定義引導程式的基地址為 0x80200000，這是引導程式在存儲設備上的位置
LOAD_ADDR := 0x82000000		# 定義引導程式的載入地址為 0x82000000，這是引導程式被加載到內存中的位置
# 如果目標板既不是 rv2 也不是 qemu，則報錯提示用戶必須指定正確的 BOARD 參數
else
$(error BOARD must be rv2 or qemu)	# 報錯提示用戶必須指定正確的 BOARD 參數，這是一個 Makefile 的內建函數，用於在條件不滿足時輸出錯誤信息並終止執行
endif

# 3. 編譯與連結參數 (Flags)
# 定義 C 編譯器的參數，包含警告選項、優化選項、目標架構和 ABI 等
CFLAGS := -Wall -Wextra -O0 -g \
          -ffreestanding -nostdlib -nostartfiles \
          -march=rv64gc -mabi=lp64 -mcmodel=medany -fno-pie \
          -DBOOT_BASE=$(BOOT_BASE) -DLOAD_ADDR=$(LOAD_ADDR)
# Wall 和 -Wextra 用於啟用所有警告，幫助開發者發現潛在的問題
# -O0 禁用優化，方便調試
# -g 生成調試信息，方便使用 gdb 等工具進行調試
# -ffreestanding 表示編譯器不應該假設有任何標準庫或運行時環境，適用於操作系統內核或引導程式的開發
# -nostdlib 和 -nostartfiles 告訴編譯器不要鏈接標準庫和啟動文件，這是因為我們正在編寫一個獨立的引導程式，不依賴於任何現有的運行時環境
# -march=rv64gc 指定目標架構為 RISC-V 64位，並啟用通用指令集擴展 (G) 和壓縮指令集擴展 (C)
# -mabi=lp64 指定使用 LP64 ABI，這意味著 long 和 pointer 類型都是 64 位的
# -mcmodel=medany 指定使用中等地址模型，允許程式碼和數據在 2GB 以內的任何位置
# -fno-pie 禁止生成位置獨立的可執行文件，這是因為我們的引導程式需要在固定地址運行
# -DBOOT_BASE=$(BOOT_BASE) 和 -DLOAD_ADDR=$(LOAD_ADDR) 定義了兩個預處理器宏，分別表示引導程式的基地址和載入地址，這些值會在編譯時被替換為具體的地址值

# 定義連結器的參數，告訴連結器不要鏈接標準庫，並使用 linker.ld 作為鏈接腳本
LDFLAGS := -nostdlib -no-pie -T linker.ld

# 4. 定義 FIT_DTB 變數，預設為 x1_orangepi-rv2.dtb，這是用於生成 FIT 映像的設備樹二進位文件
FIT_DTB ?= x1_orangepi-rv2.dtb

# 5. 定義 Makefile 的目標和規則
# 默認目標是生成 bootloader.bin
all: bootloader.bin

# 定義生成 bootloader.elf 的規則，依賴於 start.S、bootloader.c 和 linker.ld
# 使用 C 編譯器將 start.S 和 bootloader.c 編譯並鏈接成 bootloader.elf，使用之前定義的 CFLAGS 和 LDFLAGS
bootloader.elf: start.S bootloader.c linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) start.S bootloader.c -o $@

# 定義生成 bootloader.bin 的規則，依賴於 bootloader.elf
# 使用 objcopy 將 bootloader.elf 轉換為純二進位文件 bootloader.bin
bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

# 定義生成 kernel.fit 的規則，依賴於 bootloader.bin、initramfs.cpio、kernel.its 和 FIT_DTB
# 使用 mkimage 工具根據 kernel.its 文件生成 kernel.fit，這是一個 FIT 映像，包含了引導程式、內核映像、設備樹和初始 RAM 磁盤
kernel.fit: bootloader.bin initramfs.cpio kernel.its $(FIT_DTB)
	mkimage -f kernel.its kernel.fit

# 定義 clean 目標，用於清理生成的文件
# 使用 rm 命令刪除 bootloader.elf、bootloader.bin 和 kernel.fit 文件，保持工作目錄的整潔
clean:
	rm -f bootloader.elf bootloader.bin kernel.fit
