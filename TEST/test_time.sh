#!/usr/bin/env bash

# 用法： ./bench_test.sh N
# 会执行 N 次： ./test.sh

if [ $# -ne 1 ]; then
    echo "Usage: $0 N"
    exit 1
fi

N="$1"

# 检查 N 是否为正整数
if ! [[ "$N" =~ ^[0-9]+$ ]] || [ "$N" -le 0 ]; then
    echo "Error: N must be a positive integer."
    exit 1
fi

# 临时文件保存 time 输出
TMP_FILE=$(mktemp)

#echo "Running ./test.sh $N times ..."

for ((i=1; i<=N; i++)); do
    #echo "Run $i ..."
    # 丢弃脚本本身输出，只保留 time 的输出到 TMP_FILE
    /usr/bin/time -p ./test.sh > /dev/null 2>> "$TMP_FILE"
done

echo
echo "==== Result from $N runs ===="

awk -v n="$N" '
/^real/ { real += $2 }
/^user/ { user += $2 }
/^sys/  { sys  += $2 }
END {
    if (n > 0) {
        printf "Average real: %.4f s\n", real / n
        printf "Average user: %.4f s\n", user / n
        printf "Average sys : %.4f s\n", sys  / n
    }
}' "$TMP_FILE"

rm -f "$TMP_FILE"
