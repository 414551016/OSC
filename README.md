# OSC
【114下】535504作業系統總整與實作

## 在 Windows 11 安裝 WSL（Windows Subsystem for Linux）
- 右鍵開始選單 → Windows Terminal（系統管理員）
- 輸入安裝指令 wsl --install
- 執行完會提示你重開機 → 一定要重開
- 確認是否成功 wsl -l -v
- 查看可用版本：wsl --list --online
- 安裝 Ubuntu 22.04：wsl --install -d Ubuntu-22.04
- OS Lab
  - sudo apt update
  - sudo apt upgrade -y
  - sudo apt install build-essential git


