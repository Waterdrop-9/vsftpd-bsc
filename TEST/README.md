# vsftpd 功能测试说明

本文档介绍了一个基于 **Bash + curl** 的自动化脚本，用于测试本机上 **vsftpd / vsftpd_bsc** 的主要功能是否工作正常，包括：

- FTP 服务端口是否可连接  
- 匿名用户是否可以登录并列出目录  
- 本地系统用户是否可以登录  
- 本地用户是否可以上传 / 下载 / 删除文件  
- 被动模式（PASV）和主动模式（PORT）是否正常工作  

脚本适用于：

- 系统自带的 `vsftpd`
- 自行编译安装的 `vsftpd_bsc`（毕昇C 加固版本）

---

## 1. 测试脚本概览

项目中核心脚本为：

```bash
test.sh
```

脚本内部主要做了以下几类测试：

1. **端口连通性测试**
   - 使用 `curl` 对 `ftp://HOST:PORT/` 发起连接，验证 FTP 监听端口是否打开。
2. **匿名登录 + 列目录（可选）**
   - 使用用户名 `anonymous` 登录，测试 `anonymous_enable=YES` 场景下能否列出 FTP 根目录。
3. **本地用户登录**
   - 使用系统已有用户（如 `xpf`）登录，验证 `local_enable=YES` 是否生效。
4. **本地用户上传 / 下载 / 删除文件（被动模式）**
   - 以本地用户身份登录后：
     - 创建远程测试目录 `bsc_test_$$`
     - 上传一个本地生成的测试文件
     - 再下载回本地并比较内容是否一致
     - 删除远程测试文件和测试目录
   - 依赖 `write_enable=YES`、本地用户对目标目录有写权限。
5. **被动模式（PASV）目录列表**
   - 使用 `curl --ftp-pasv` 列出目录，验证被动模式是否可用。
6. **主动模式（PORT）目录列表**
   - 使用 `curl --ftp-port -` 列出目录，验证主动模式是否可用。

每一项测试都会输出：

```
[OK] xxx
[FAIL] xxx
```

便于快速判断 vsftpd 当前的配置和功能状态。

------

## 2. 从零开始环境配置

### 2.1 系统要求

- Linux 发行版（例如：Ubuntu 20.04 / 22.04）
- 拥有 `sudo` 权限的用户
- 需要可以安装软件包（联网或有本地源）

### 2.2 安装基础依赖

脚本依赖以下命令：

- `curl`
- `mktemp`
- `cmp`
- `ftp`（用于手动验证登录，可选）

在 Ubuntu 上可以使用：

```
sudo apt update
sudo apt install -y vsftpd curl diffutils ftp
```

> 若你使用的是自编译的 `vsftpd_bsc`，仅需保证系统中存在可执行的 `vsftpd_bsc` 二进制文件（见下一节）。

------

## 3. 安装 / 配置 vsftpd

### 3.1 使用系统自带 vsftpd

如果你只使用系统的 `vsftpd`：

```
# 安装
sudo apt install -y vsftpd
```

安装后，默认配置文件路径为：

```
/etc/vsftpd.conf
```

我们将在 3.3 小节给出一个适配测试脚本的示例配置。

### 3.2 使用自编译的 vsftpd_bsc

如果你在源代码目录下已经编译完成（例如当前目录有一个可执行文件 `vsftpd`），可以将其复制为一个新的二进制：

```
# 在源码目录下执行
sudo cp vsftpd /usr/local/sbin/vsftpd_bsc
sudo chmod +x /usr/local/sbin/vsftpd_bsc
```

之后你可以选择：

- 用系统 `vsftpd` 进行测试，或者
- 用 `/usr/local/sbin/vsftpd_bsc` 加载同一份 `/etc/vsftpd.conf` 进行测试

### 3.3 配置vsftpd.conf

确保至少包含以下内容（去掉行首的 `#` 注释）：

```
# 允许匿名 FTP
anonymous_enable=YES

# 允许本地系统用户登录（例如 xpf）
local_enable=YES

# 允许写操作（上传、删除等）
write_enable=YES

# 本地用户默认 umask
local_umask=022

# 激活目录消息（可选）
dirmessage_enable=YES

# 启用上传/下载日志
xferlog_enable=YES

# PORT 模式时使用端口 20
connect_from_port_20=YES

# 独立模式监听 IPv4（端口默认 21）
listen=YES

# 显式启用被动 / 主动模式（方便脚本测试）
pasv_enable=YES
port_enable=YES

# 自定义登录横幅（用于确认正确加载了这份配置）
ftpd_banner=Welcome to BSC vsftpd test server.
```

* **权限要求**： vsftpd 对配置文件有较严格要求，必须保证 `/etc/vsftpd.conf`：

 ```
sudo chown root:root /etc/vsftpd.conf
sudo chmod 644 /etc/vsftpd.conf   # 或者更严格：chmod 600
 ```

否则可能出现类似报错：

```
500 OOPS: config file not owned by correct user, or not a file
```

------

## 4. 启动 FTP 服务器

你可以按以下两种方式启动：

### 4.1 使用系统服务（apt 安装 vsftpd 时推荐）

```
# 启动服务
sudo systemctl start vsftpd

# 查看状态
sudo systemctl status vsftpd
```

若状态为 `active (running)`，说明 vsftpd 已启动；可以用以下命令确认进程：

```
ps aux | grep vsftpd
```

### 4.2 手动启动 vsftpd_bsc

如果你要测试自编译的 `vsftpd_bsc`：

```
# 确保没有旧的 vsftpd_bsc 残留
sudo pkill vsftpd_bsc 2>/dev/null

# 使用 /etc/vsftpd.conf 启动
sudo /usr/local/sbin/vsftpd_bsc /etc/vsftpd.conf &
```

查看是否在运行：

```
ps aux | grep vsftpd_bsc
```

------

## 5. 手动验证 FTP 登录

在使用脚本之前，建议先手工确认本地用户能否正常登录。

假设系统中已经存在用户 `xpf`，密码为 `po450`：

```
ftp localhost
```

交互示例：

```
Connected to localhost.
220 Welcome to BSC vsftpd test server.
Name (localhost:xpf): xpf
331 Please specify the password.
Password: po450
230 Login successful.
ftp>
```

只要看到 `230 Login successful.`，说明：

- `local_enable=YES` 已生效
- PAM 配置允许本地用户登录
- 用户名和密码均正确

------

## 6. 使用测试脚本进行自动化测试

### 6.1 脚本配置

打开脚本顶部的配置部分：

```
nano test.sh
```

检查 / 修改以下变量：

```
# FTP 服务器地址和端口
HOST="localhost"
PORT=21

# 是否测试匿名登录（需与 vsftpd.conf 中 anonymous_enable 保持一致）
ANON_ENABLED=1

# 用于测试的本地系统用户
LOCAL_USER="xpf"
LOCAL_PASS="po450"

# 存放临时测试文件的目录
TMP_DIR="/tmp"
```

请根据实际情况修改：

- 如果你关闭了匿名登录（`anonymous_enable=NO`），请将 `ANON_ENABLED=0`，脚本会跳过匿名相关测试。
- 确保 `LOCAL_USER` 是系统中真实存在的用户，并且有对目标目录的写权限（通常其 home 目录即可）。

### 6.2 赋予执行权限

```
chmod +x test_vsftpd.sh
```

### 6.3 运行测试

```
./test_vsftpd.sh
```

典型输出示例：

```
==== vsftpd 功能测试开始 ====
目标服务器：localhost:21
匿名测试：启用
本地用户：xpf

======== 测试：1. 端口连通性 ========
[OK] 1. 端口连通性

======== 测试：2. 匿名登录 + 列目录 ========
[OK] 2. 匿名登录 + 列目录

======== 测试：3. 本地用户登录 ========
[OK] 3. 本地用户登录

======== 测试：4. 本地用户上传 / 下载 / 删除（被动模式） ========
[OK] 4. 本地用户上传 / 下载 / 删除（被动模式）

======== 测试：5. 本地用户被动模式列目录 (PASV) ========
[OK] 5. 本地用户被动模式列目录 (PASV)

======== 测试：6. 本地用户主动模式列目录 (PORT) ========
[OK] 6. 本地用户主动模式列目录 (PORT)

==== 测试结束 ====
全部测试通过 ✅
```

如果某个测试失败，将显示 `[FAIL]`，并伴随 `curl` 的错误信息，例如：

- `curl: (67) Access denied: 530` → 说明本地用户登录被拒绝（常见原因：`local_enable` 未开启、密码错误、PAM 限制等）
- `curl: (7) Failed to connect to localhost port 21` → 说明 FTP 服务未启动或端口被防火墙拦截
- 上传/下载对比失败时会有提示：“本地上传文件与下载文件内容不一致！”

------

## 7. 常见问题排查

### 7.1 本地用户一直 530 拒绝登录

- 检查 `/etc/vsftpd.conf`：

  ```
  local_enable=YES
  write_enable=YES
  ```

- 检查 `/etc/vsftpd.conf` 权限：

  ```
  sudo chown root:root /etc/vsftpd.conf
  sudo chmod 644 /etc/vsftpd.conf
  ```

- 确认用户存在且密码正确：

  ```
  su - xpf    # 尝试切换用户验证密码
  ```

### 7.2 提示 “500 OOPS: config file not owned by correct user”

- 通常是 `/etc/vsftpd.conf` 被普通用户创建或软链接导致。执行：

  ```
  sudo chown root:root /etc/vsftpd.conf
  sudo chmod 644 /etc/vsftpd.conf
  ```

- 确保不是软链接指向用户目录：

  ```
  ls -l /etc/vsftpd.conf
  ```

  若看到 `vsftpd.conf -> /home/xpf/...`，请改为真正的普通文件。

### 7.3 某些模式（PASV / PORT）失败

- 确认配置中已经显式开启：

  ```
  pasv_enable=YES
  port_enable=YES
  ```

- 若涉及跨主机访问（不是 localhost），可能需要额外配置被动端口区间、防火墙、NAT 相关设置；本 README 以本机 `localhost` 测试为主。

------

## 8. 附：示例 vsftpd.conf（适配测试脚本）

这是一个简化版、适配测试脚本的 `vsftpd.conf` 示例，可作为参考：

```
# 允许匿名登录（脚本 ANON_ENABLED=1 时需要）
anonymous_enable=YES

# 允许本地系统用户登录（如 xpf）
local_enable=YES

# 允许写操作：上传 / 删除 / 重命名等
write_enable=YES

# 本地用户创建文件的默认 umask
local_umask=022

# 目录消息与日志
dirmessage_enable=YES
xferlog_enable=YES
connect_from_port_20=YES

# 独立模式监听 IPv4
listen=YES
#listen_ipv6=YES

# 显式启用被动 / 主动模式
pasv_enable=YES
port_enable=YES

# 自定义横幅，用于确认正确读取了配置
ftpd_banner=Welcome to BSC vsftpd test server.
```

保存后记得调整权限并重启服务或重新启动 `vsftpd_bsc`：

```
sudo chown root:root /etc/vsftpd.conf
sudo chmod 644 /etc/vsftpd.conf

# 若使用 systemd：
sudo systemctl restart vsftpd

# 若使用手动启动 vsftpd_bsc：
sudo pkill vsftpd_bsc 2>/dev/null
sudo /usr/local/sbin/vsftpd_bsc /etc/vsftpd.conf &
```