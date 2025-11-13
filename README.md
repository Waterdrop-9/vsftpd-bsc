# vsftpd-bsc

**vsftpd-bsc** is an experimental repository that applies **BiShengC** (the BiSheng C language) to the classic FTP server **vsftpd** in order to enhance its memory safety. The original repo of vsftpd is [HERE](https://security.appspot.com/vsftpd.html).

The goal is to introduce BiShengC features while preserving the original vsftpd behavior and performance as much as possible:

* `safe / unsafe` regions for controlled safety boundaries  

* `owned` / `borrow` for ownership and borrowing  

Safe APIs such as `safe_malloc` / `safe_free`  to expose traditional C time- and space-related memory safety issues (UAF, double free, memory leaks, null pointer dereference, etc.) as early as possible **at compile time**.

⚠️ This repository is primarily intended for **research and educational** purposes and is not recommended for direct production use.

