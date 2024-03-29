#! /bin/bash

HOST=127.0.0.1
PORT=12345
THREAD_NUM=8
CLIENT_COUNT=3
SERVER=echo_server
CLIENT=echo_client
SERVER_LOG=server.log

if [ !  -f "$SERVER" ] || [ ! -f "$CLIENT" ];then
    echo "$SERVER and $CLIENT is not found"
    exit 1
fi

function ax_exit()
{
    killall $SERVER &>/dev/null
    killall $CLIENT &>/dev/null
    exit 1
}

trap 'killall $SERVER &>/dev/null;killall $CLIENT &>/dev/null;exit 1' INT QUIT
killall $SERVER &>dev/null
killall $CLIENT &>dev/null

echo "./$SERVER $HOST $PORT $THREAD_NUM &>$SERVER_LOG &"
./$SERVER $HOST $PORT $THREAD_NUM &>$SERVER_LOG &
sleep 1
if
    grep "ERROR" $SERVER_LOG
then
    at_exit
fi

i=0
while [ $i -lt $CLIENT_COUNT ]
do
    CLIENT_LOG=client_$i.log
    echo "./$CLIENT $HOST $PORT 1 &>$CLIENT_LOG &"
    ./$CLIENT $HOST $PORT 1 &>$CLIENT_LOG &
    sleep 1
    i=`expr $i + 1`
done

echo "tail -f $SERVER_LOG"
tail -f $SERVER_LOG

at_exit