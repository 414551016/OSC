# OSC
【114下】535504作業系統總整與實作

## 在 Windows 11 安裝 WSL（Windows Subsystem for Linux）
- 打開 PowerShell（系統管理員）
- 輸入安裝指令 wsl --install
- 執行完會提示你重開機 → 一定要重開
- 確認是否成功 wsl -l -v
- 查看可用版本：wsl --list --online
- 安裝 Ubuntu 22.04：wsl --install -d Ubuntu-22.04
- OS Lab
  - sudo apt update
  - sudo apt full-upgrade -y
  - sudo apt install build-essential git
- 常用指令
  - wsl            # 進入 Linux
  - exit           # 離開 Linux
  - wsl --shutdown # 關閉 WSL
- Orange Pi RV2 開發環境配置：
  - 給 裸機 / OS Lab：gcc-riscv64-unknown-elf
  - 給 RISC-V 模擬：qemu-system-misc
  - 給 dtb / dts：device-tree-compiler
    ```
    sudo apt update
    sudo apt full-upgrade -y
    sudo apt install -y \
      build-essential \
      git curl wget vim nano unzip zip xz-utils \
      cmake ninja-build pkg-config \
      python3 python3-pip python3-venv \
      bc bison flex libssl-dev libncurses-dev libelf-dev \
      device-tree-compiler \
      qemu-system-misc qemu-user-static \
      gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
      gdb-multiarch \
      libc6-dev-riscv64-cross gcc-riscv64-linux-gnu g++-riscv64-linux-gnu \
      file rsync cpio
    ```  
- 檢查工具有沒有裝好：
  ```
  riscv64-unknown-elf-gcc --version
  riscv64-linux-gnu-gcc --version
  qemu-system-riscv64 --version
  dtc --version
  ```
- 
