#!/usr/bin/env bash
#orginal test
sudo /usr/local/sbin/vsftpd /etc/vsftpd.conf &
echo "C version of vsftpd"
./time_test.sh 10

sudo pkill -9 vsftpd

#bsc test
sudo /usr/local/sbin/vsftpd_bsc /etc/vsftpd.conf &
echo "safeC version of vsftpd"
./time_test.sh 10

sudo pkill -9 vsftpd_bsc

