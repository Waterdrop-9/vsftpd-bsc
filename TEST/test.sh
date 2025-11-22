#!/usr/bin/env bash
#
# 简单的 vsftpd 功能测试脚本（基于 curl）
#
# 测试内容：
# 1. 能否连接到 FTP 服务器端口
# 2. 匿名登录 + 列出根目录（如果允许匿名）
# 3. 本地用户登录
# 4. 本地用户上传 / 下载 / 删除文件
# 5. 被动模式 / 主动模式下的目录访问
#
# 使用方法：
#   chmod +x test_vsftpd.sh
#   修改下面的 HOST / PORT / LOCAL_USER / LOCAL_PASS 等变量后运行：
#   ./test_vsftpd.sh
#

########################
# 配置区域（请按需修改）
########################

# vsftpd 所在主机和端口
HOST="localhost"
PORT=21

# 是否测试匿名登录（vsftpd.conf 中 anonymous_enable=YES 时建议开）
ANON_ENABLED=1

# 测试用的本地用户（需要在系统中已经存在，并在 vsftpd.conf 中允许本地用户登录）
LOCAL_USER="xpf"
LOCAL_PASS="po450"

# 本地临时目录（用来放测试文件）
TMP_DIR="/tmp"

########################
# 工具函数
########################

FAILED=0

check_dep() {
  for cmd in curl mktemp cmp; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      echo "[FATAL] 依赖命令 '$cmd' 未找到，请先安装再运行。"
      exit 1
    fi
  done
}

run_test() {
  local name="$1"
  shift
  echo "======== 测试：$name ========"
  if "$@"; then
    echo "[OK] $name"
  else
    echo "[FAIL] $name"
    FAILED=$((FAILED + 1))
  fi
  echo
}

########################
# 具体测试项
########################

# 1. 测试端口是否能连接
test_connect_port() {
  # 尝试用 curl 获取根目录（不登录），检查是否有响应
  curl -sS --connect-timeout 5 "ftp://$HOST:$PORT/" >/dev/null 2>&1
}

# 2. 匿名登录 + 列出根目录
test_anon_login_list() {
  curl -sS --fail \
    --user "anonymous:anonymous@" \
    --ftp-pasv \
    "ftp://$HOST:$PORT/" >/dev/null
}

# 3. 本地用户登录（仅测试能否登录）
test_local_login() {
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "ftp://$HOST:$PORT/" >/dev/null
}

# 4. 本地用户上传 / 下载 / 删除文件（被动模式）
test_local_upload_download_delete_pasv() {
  local test_dir="bsc_test_$$"
  local remote_file="hello_vsftpd.txt"
  local tmp_file
  local dl_file

  tmp_file="$(mktemp "$TMP_DIR/vsftpd_test_XXXXXX")"
  dl_file="$(mktemp "$TMP_DIR/vsftpd_dl_XXXXXX")"

  echo "This is a test file for vsftpd at $(date)" >"$tmp_file"

  # 创建远程测试目录
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "MKD $test_dir" \
    "ftp://$HOST:$PORT/" >/dev/null

  # 上传文件
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    -T "$tmp_file" \
    "ftp://$HOST:$PORT/$test_dir/$remote_file" >/dev/null

  # 下载回来
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "ftp://$HOST:$PORT/$test_dir/$remote_file" \
    -o "$dl_file"

  # 比较文件内容是否一致
  if ! cmp -s "$tmp_file" "$dl_file"; then
    echo "本地上传文件与下载文件内容不一致！"
    rm -f "$tmp_file" "$dl_file"
    return 1
  fi

  # 删除远程文件
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "DELE $test_dir/$remote_file" \
    "ftp://$HOST:$PORT/" >/dev/null

  # 删除远程目录
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "RMD $test_dir" \
    "ftp://$HOST:$PORT/" >/dev/null

  rm -f "$tmp_file" "$dl_file"
}

# 5. 被动模式下列目录（确认 PASV 正常）
test_pasv_list() {
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "ftp://$HOST:$PORT/" >/dev/null
}

# 6. 主动模式下列目录（确认 PORT 模式正常）
test_active_list() {
  # --ftp-port - 表示让 curl 使用主动模式（PORT）
  curl -sS --fail \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-port - \
    "ftp://$HOST:$PORT/" >/dev/null
}

########################
# 主流程
########################

main() {
  echo "==== vsftpd 功能测试开始 ===="
  echo "目标服务器：$HOST:$PORT"
  echo "匿名测试：$([ "$ANON_ENABLED" -eq 1 ] && echo '启用' || echo '关闭')"
  echo "本地用户：$LOCAL_USER"
  echo

  check_dep

  run_test "1. 端口连通性" test_connect_port

  if [ "$ANON_ENABLED" -eq 1 ]; then
    run_test "2. 匿名登录 + 列目录" test_anon_login_list
  else
    echo "跳过匿名登录测试（ANON_ENABLED=0）"
    echo
  fi

  if [ -n "$LOCAL_USER" ]; then
    run_test "3. 本地用户登录" test_local_login
    run_test "4. 本地用户上传 / 下载 / 删除（被动模式）" test_local_upload_download_delete_pasv
    run_test "5. 本地用户被动模式列目录 (PASV)" test_pasv_list
    run_test "6. 本地用户主动模式列目录 (PORT)" test_active_list
  else
    echo "未配置本地用户，跳过本地用户相关测试。"
    echo
  fi

  echo "==== 测试结束 ===="
  if [ "$FAILED" -eq 0 ]; then
    echo "全部测试通过 ✅"
  else
    echo "有 $FAILED 项测试失败 ❌ ，请检查 vsftpd 配置和日志。"
  fi
}

main "$@"