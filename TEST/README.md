# vsftpd 功能测试使用说明

本文档说明 `TEST/test.sh` 的测试范围、环境准备、配置项以及运行方法，便于快速验证 **vsftpd / vsftpd_bsc** 的主要功能。

---

## 1. 测试覆盖内容

脚本基于 Bash + curl，默认执行的检查：

1) 端口连通性 / Banner 读取  
2) 匿名登录与列目录（可选）  
3) 匿名上传策略（默认期望拒绝，可配置允许）  
4) 本地用户登录（正确凭据）  
5) 错误密码应被拒绝  
6) PASV 模式列目录  
7) PORT 模式列目录（可跳过）  
8) 上传 / 下载 / 删除（PASV）  
9) 重命名（RNFR/RNTO）  
10) 断点续传下载（REST）  
11) 嵌套目录创建 / 删除  
12) ASCII 模式传输（TYPE A，含 CR/LF 规范化校验）  
13) chroot 约束验证（CWD .. 预期失败，可选）  
14) Banner 内容断言（可选匹配 `ftpd_banner` 子串）  

每项测试输出 `[OK]` / `[FAIL]`，便于定位问题。

---

## 2. 环境与依赖

- Linux / WSL / 类 Unix 环境，可运行 vsftpd 或 vsftpd_bsc  
- 依赖命令：`curl`、`mktemp`、`cmp`、`head`、`tr`  
- 可选：`sudo` 权限用于安装软件、启动服务

Ubuntu 安装示例：

```bash
sudo apt update
sudo apt install -y vsftpd curl diffutils coreutils
```

---

## 3. vsftpd 配置要点

`/etc/vsftpd.conf` 可参考（按需调整）：

```
anonymous_enable=YES        # 不测匿名可设 NO，并在脚本里 ANON_ENABLED=0
local_enable=YES
write_enable=YES
local_umask=022
dirmessage_enable=YES
xferlog_enable=YES
connect_from_port_20=YES
listen=YES                  # 或 listen_ipv6=YES
pasv_enable=YES
port_enable=YES
ftpd_banner=Welcome to BSC vsftpd test server.
# 如需 chroot 测试： chroot_local_user=YES
```

权限要求：

```bash
sudo chown root:root /etc/vsftpd.conf
sudo chmod 644 /etc/vsftpd.conf   # 或更严格 600
```

启动示例（择一）：

```bash
# systemd
sudo systemctl restart vsftpd

# 或手动启动自编译 vsftpd_bsc
sudo pkill vsftpd_bsc 2>/dev/null
sudo /usr/local/sbin/vsftpd_bsc /etc/vsftpd.conf &
```

---

## 4. 脚本配置项（运行前可用环境变量覆盖）

```bash
HOST=localhost          # FTP 主机
PORT=21                 # FTP 端口
ANON_ENABLED=0          # 是否执行匿名登录/列目录
ANON_UPLOAD_ALLOWED=0   # 匿名上传是否允许（默认期望拒绝）
LOCAL_USER=xpf          # 本地用户
LOCAL_PASS=po450        # 本地用户密码
TMP_DIR=/tmp            # 本地临时目录
USE_TLS=0               # 1 时要求显式 FTPS (--ssl-reqd)
TLS_INSECURE=1          # 1 时跳过证书校验（实验环境常用）
SKIP_ACTIVE=0           # 1 时跳过主动模式(PORT)测试
RUN_ASCII_TEST=1        # 1 时跑 ASCII 传输校验
EXPECT_CHROOT=0         # 1 时断言 CWD .. 失败（需 chroot_local_user=YES）
EXPECT_BANNER_SUBSTR="" # 若设置，检查 220 Banner 包含该子串
```

示例：关闭匿名测试，启用 TLS 且校验证书，断言 chroot 与 banner：

```bash
ANON_ENABLED=0 USE_TLS=1 TLS_INSECURE=0 \
EXPECT_CHROOT=1 EXPECT_BANNER_SUBSTR="BSC vsftpd" \
bash test.sh
```

---

## 5. 运行脚本

```bash
cd TEST
bash test.sh
```

典型输出（节选）：

```
==== vsftpd functional tests ====
Target: localhost:21
Anonymous: 1 (upload allowed: 0)
Local user: xpf
TLS required: 0 (insecure cert: 1)
Skip active mode: 0
Expect chroot: 0
Banner expect: <none>

======== 1. Port reachable ========
[OK] 1. Port reachable
...
======== 13. ASCII transfer (TYPE A) ========
[OK] 13. ASCII transfer (TYPE A)
======== 14. Chroot enforcement (CWD .. fails) ========
[OK] 14. Chroot enforcement (CWD .. fails)

==== Done ====
All tests passed.
```

出现 `[FAIL]` 时，会打印简要原因；可结合 vsftpd 日志/配置继续排查。

---

## 6. 常见问题

- 530 登录失败：检查 `local_enable`、账号密码、PAM/SELinux；确认 `/etc/vsftpd.conf` 权限正确。  
- 连接超时或 curl 7：服务未启动、端口被防火墙/NAT 拦截。  
- 匿名上传被允许但期望拒绝：确认 `anon_upload_enable`/`write_enable`，或设置脚本 `ANON_UPLOAD_ALLOWED=0`。  
- 主动模式失败：可能被 NAT/防火墙阻断，可先 `SKIP_ACTIVE=1`，或配置被动端口并放行。  
- ASCII 传输不一致：检查服务器对 CR/LF 的处理；脚本已做规范化，仍异常需看服务器过滤/改写。  
- chroot 断言失败：确认 `chroot_local_user=YES` 或自定义 chroot 配置，与 `EXPECT_CHROOT=1` 保持一致。  
- TLS 校验失败：自签名环境可临时 `TLS_INSECURE=1`，或部署受信 CA。  

---

## 7. 快速参考配置（本地测试示例）

```
anonymous_enable=YES
local_enable=YES
write_enable=YES
local_umask=022
dirmessage_enable=YES
xferlog_enable=YES
connect_from_port_20=YES
listen=YES
pasv_enable=YES
port_enable=YES
ftpd_banner=Welcome to BSC vsftpd test server.
```

调整完配置后重启服务，运行 `TEST/test.sh`，根据输出修正配置直到全部测试通过。
