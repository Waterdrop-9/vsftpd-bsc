#!/usr/bin/env bash
# More complete vsftpd functional test suite (curl based).
# Coverage:
#   1) Port reachability / banner
#   2) Anonymous login + list (optional)
#   3) Anonymous upload policy (expect deny by default)
#   4) Local user login
#   5) PASV/PORT directory listing
#   6) Upload / download / delete
#   7) Rename (RNFR/RNTO)
#   8) Resume download (REST)
#   9) Nested directory create/remove
#  10) Bad password is rejected

set -o pipefail

########################
# Config (override via env)
########################
HOST="${HOST:-localhost}"
PORT="${PORT:-21}"
ANON_ENABLED="${ANON_ENABLED:-0}"
# If your server intentionally allows anonymous upload, set to 1.
ANON_UPLOAD_ALLOWED="${ANON_UPLOAD_ALLOWED:-0}"
LOCAL_USER="${LOCAL_USER:-xpf}"
LOCAL_PASS="${LOCAL_PASS:-po450}"
TMP_DIR="${TMP_DIR:-/tmp}"
# Force TLS (explicit FTPS); set to 1 to require, TLS_INSECURE=1 to skip cert check.
USE_TLS="${USE_TLS:-0}"
TLS_INSECURE="${TLS_INSECURE:-1}"
# In NATed lab environments you may want to skip active mode.
SKIP_ACTIVE="${SKIP_ACTIVE:-0}"

FTP_URL="ftp://${HOST}:${PORT}"
FAILED=0
declare -a CLEANUP_DIRS=()

########################
# Helpers
########################
check_dep() {
  for cmd in curl mktemp cmp head; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      echo "[FATAL] missing dependency: $cmd"
      exit 1
    fi
  done
}

curl_tls_args() {
  if [ "$USE_TLS" -eq 1 ]; then
    if [ "$TLS_INSECURE" -eq 1 ]; then
      printf -- "--ssl-reqd --insecure"
    else
      printf -- "--ssl-reqd"
    fi
  fi
}

run_test() {
  local name="$1"
  shift
  echo "======== $name ========"
  if "$@"; then
    echo "[OK] $name"
  else
    echo "[FAIL] $name"
    FAILED=$((FAILED + 1))
  fi
  echo
}

register_cleanup_dir() {
  CLEANUP_DIRS+=("$1")
}

cleanup_remote_dirs() {
  for dir in "${CLEANUP_DIRS[@]}"; do
    curl -sS --fail $(curl_tls_args) \
      --user "${LOCAL_USER}:${LOCAL_PASS}" \
      --ftp-pasv \
      --quote "RMD $dir" \
      "$FTP_URL/" >/dev/null 2>&1 || true
  done
}

trap cleanup_remote_dirs EXIT

mk_payload() {
  local path="$1"
  # Create 16 KiB predictable payload so resume/compare is reliable.
  head -c 16384 </dev/zero | tr '\0' 'X' >"$path"
}

########################
# Tests
########################

test_connect_port() {
  # Basic reachability; expect greeting.
  curl -sS --fail --connect-timeout 5 $(curl_tls_args) "$FTP_URL/" >/dev/null
}

test_anon_login_list() {
  curl -sS --fail $(curl_tls_args) \
    --user "anonymous:anonymous@" \
    --ftp-pasv \
    "$FTP_URL/" >/dev/null
}

test_anon_upload_policy() {
  local tmp_file target
  tmp_file="$(mktemp "$TMP_DIR/vsftpd_anon_XXXXXX")"
  target="anon_upload_test_$$.txt"
  echo "anon upload test $(date)" >"$tmp_file"

  if [ "$ANON_UPLOAD_ALLOWED" -eq 1 ]; then
    curl -sS --fail $(curl_tls_args) \
      --user "anonymous:anonymous@" \
      --ftp-pasv \
      -T "$tmp_file" \
      "$FTP_URL/$target" >/dev/null
    curl -sS --fail $(curl_tls_args) \
      --user "anonymous:anonymous@" \
      --ftp-pasv \
      --quote "DELE $target" \
      "$FTP_URL/" >/dev/null
    rm -f "$tmp_file"
    return 0
  fi

  if curl -sS $(curl_tls_args) \
    --user "anonymous:anonymous@" \
    --ftp-pasv \
    -T "$tmp_file" \
    "$FTP_URL/$target" >/dev/null 2>&1; then
    echo "Anonymous upload unexpectedly succeeded (set ANON_UPLOAD_ALLOWED=1 if this is intentional)."
    curl -sS $(curl_tls_args) \
      --user "anonymous:anonymous@" \
      --ftp-pasv \
      --quote "DELE $target" \
      "$FTP_URL/" >/dev/null 2>&1 || true
    rm -f "$tmp_file"
    return 1
  fi

  rm -f "$tmp_file"
  return 0
}

test_local_login() {
  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "$FTP_URL/" >/dev/null
}

test_bad_password_rejected() {
  if curl -sS $(curl_tls_args) \
    --user "${LOCAL_USER}:wrong-password" \
    --ftp-pasv \
    "$FTP_URL/" >/dev/null 2>&1; then
    echo "Login with bad password unexpectedly succeeded"
    return 1
  fi
  return 0
}

test_pasv_list() {
  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "$FTP_URL/" >/dev/null
}

test_active_list() {
  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-port - \
    "$FTP_URL/" >/dev/null
}

test_local_upload_download_delete_pasv() {
  local test_dir="bsc_test_$$"
  local remote_file="hello_vsftpd.txt"
  local tmp_file dl_file

  tmp_file="$(mktemp "$TMP_DIR/vsftpd_test_XXXXXX")"
  dl_file="$(mktemp "$TMP_DIR/vsftpd_dl_XXXXXX")"

  echo "This is a test file for vsftpd at $(date)" >"$tmp_file"

  register_cleanup_dir "$test_dir"
  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "MKD $test_dir" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    -T "$tmp_file" \
    "$FTP_URL/$test_dir/$remote_file" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "$FTP_URL/$test_dir/$remote_file" \
    -o "$dl_file"

  if ! cmp -s "$tmp_file" "$dl_file"; then
    echo "Uploaded and downloaded file differ"
    rm -f "$tmp_file" "$dl_file"
    return 1
  fi

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "DELE $test_dir/$remote_file" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "RMD $test_dir" \
    "$FTP_URL/" >/dev/null

  rm -f "$tmp_file" "$dl_file"
  return 0
}

test_local_rename() {
  local test_dir="bsc_rename_$$"
  local src_file="rename_src.txt"
  local dst_file="rename_dst.txt"
  local tmp_file dl_file

  tmp_file="$(mktemp "$TMP_DIR/vsftpd_rename_XXXXXX")"
  dl_file="$(mktemp "$TMP_DIR/vsftpd_rename_dl_XXXXXX")"
  echo "rename check $(date)" >"$tmp_file"
  register_cleanup_dir "$test_dir"

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "MKD $test_dir" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    -T "$tmp_file" \
    "$FTP_URL/$test_dir/$src_file" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "RNFR $test_dir/$src_file" \
    --quote "RNTO $test_dir/$dst_file" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "$FTP_URL/$test_dir/$dst_file" \
    -o "$dl_file"

  if ! cmp -s "$tmp_file" "$dl_file"; then
    echo "Renamed file content mismatch"
    rm -f "$tmp_file" "$dl_file"
    return 1
  fi

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "DELE $test_dir/$dst_file" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "RMD $test_dir" \
    "$FTP_URL/" >/dev/null

  rm -f "$tmp_file" "$dl_file"
  return 0
}

test_local_resume_download() {
  local test_dir="bsc_resume_$$"
  local remote_file="resume_test.bin"
  local src_file partial_file full_file
  src_file="$(mktemp "$TMP_DIR/vsftpd_resume_src_XXXXXX")"
  partial_file="$(mktemp "$TMP_DIR/vsftpd_resume_part_XXXXXX")"
  full_file="$(mktemp "$TMP_DIR/vsftpd_resume_full_XXXXXX")"

  mk_payload "$src_file"
  register_cleanup_dir "$test_dir"

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "MKD $test_dir" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    -T "$src_file" \
    "$FTP_URL/$test_dir/$remote_file" >/dev/null

  # Download first 4 KiB then resume to full file.
  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --range 0-4095 \
    "$FTP_URL/$test_dir/$remote_file" \
    -o "$partial_file"

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    -C - \
    "$FTP_URL/$test_dir/$remote_file" \
    -o "$partial_file"

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "$FTP_URL/$test_dir/$remote_file" \
    -o "$full_file"

  if ! cmp -s "$src_file" "$partial_file"; then
    echo "Resume download produced different file"
    rm -f "$src_file" "$partial_file" "$full_file"
    return 1
  fi
  if ! cmp -s "$src_file" "$full_file"; then
    echo "Full download mismatch"
    rm -f "$src_file" "$partial_file" "$full_file"
    return 1
  fi

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "DELE $test_dir/$remote_file" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "RMD $test_dir" \
    "$FTP_URL/" >/dev/null

  rm -f "$src_file" "$partial_file" "$full_file"
  return 0
}

test_nested_dirs() {
  local test_dir="bsc_nested_$$"
  local nested="$test_dir/sub/child"
  register_cleanup_dir "$test_dir"

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "MKD $test_dir" \
    --quote "MKD $test_dir/sub" \
    --quote "MKD $nested" \
    "$FTP_URL/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    "$FTP_URL/$nested/" >/dev/null

  curl -sS --fail $(curl_tls_args) \
    --user "${LOCAL_USER}:${LOCAL_PASS}" \
    --ftp-pasv \
    --quote "RMD $nested" \
    --quote "RMD $test_dir/sub" \
    --quote "RMD $test_dir" \
    "$FTP_URL/" >/dev/null
}

########################
# Main
########################
main() {
  echo "==== vsftpd functional tests ===="
  echo "Target: $HOST:$PORT"
  echo "Anonymous: $ANON_ENABLED (upload allowed: $ANON_UPLOAD_ALLOWED)"
  echo "Local user: $LOCAL_USER"
  echo "TLS required: $USE_TLS (insecure cert: $TLS_INSECURE)"
  echo "Skip active mode: $SKIP_ACTIVE"
  echo

  check_dep

  run_test "1. Port reachable" test_connect_port

  if [ "$ANON_ENABLED" -eq 1 ]; then
    run_test "2. Anonymous login + list" test_anon_login_list
    run_test "3. Anonymous upload policy" test_anon_upload_policy
  else
    echo "Skip anonymous tests (ANON_ENABLED=0)"
    echo
  fi

  if [ -n "$LOCAL_USER" ]; then
    run_test "4. Local login (PASV)" test_local_login
    run_test "5. Bad password rejected" test_bad_password_rejected
    run_test "6. PASV listing" test_pasv_list
    if [ "$SKIP_ACTIVE" -eq 0 ]; then
      run_test "7. PORT listing" test_active_list
    else
      echo "Skip active mode test (SKIP_ACTIVE=1)"
      echo
    fi
    run_test "8. Upload / download / delete (PASV)" test_local_upload_download_delete_pasv
    run_test "9. Rename (RNFR/RNTO)" test_local_rename
    run_test "10. Resume download (REST)" test_local_resume_download
    run_test "11. Nested directory create/remove" test_nested_dirs
  else
    echo "No LOCAL_USER configured, skip local-user tests."
    echo
  fi

  echo "==== Done ===="
  if [ "$FAILED" -eq 0 ]; then
    echo "All tests passed."
  else
    echo "$FAILED test(s) failed. Check vsftpd config/logs."
  fi
}

main "$@"
