# vsftpd-bsc

## Introduction
**vsftpd-bsc** is an experimental repository that applies [BiShengC](https://gitee.com/bisheng_c_language_dep/llvm-project/blob/bishengc/15.0.4/clang/docs/BSC/BiShengCLanguageUserManual.md) (a version of enchaned C language)to the classic FTP server **vsftpd-3.0.5** in order to enhance its memory safety. The original project page of vsftpd is [HERE](https://security.appspot.com/vsftpd.html).


The goal is to introduce BiShengC features while preserving the original vsftpd behavior and performance as much as possible:

* `safe / unsafe` regions for controlled safety boundaries  

* `owned` / `borrow` for ownership and borrowing  

Safe APIs such as `safe_malloc` / `safe_free`  to expose traditional C time- and space-related memory safety issues (UAF, double free, memory leaks, null pointer dereference, etc.) as early as possible **at compile time**.

## How to Install?
Please follow the INSTALL document to install vsftpd-bsc, before that you may need to configure bsc-compiler by following [THIS](https://github.com/yingluosanqian/Bi-Sheng-C-AutoGen).

## Notice
⚠️ This repository is primarily intended for **research and educational** purposes and is not recommended for direct production use.

