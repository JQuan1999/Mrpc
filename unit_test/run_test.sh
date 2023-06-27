#!/bin/bash
function run()
{
    echo "===========" $1 "==========="
    ./$1
    if [ $? -ne 0 ]; then
        echo "TEST FAILED!!!"
        exit 1
    fi
}
echo 'make'
make
file=(test_buffer test_endpoint test_service_pool test_threadgroup)
num=0
for test_case in ${file[@]}
do
    run $test_case
    num=$((num+1))
done

echo
echo "CASE NUM: $num"
echo "ALL CASE PASSED!!!"
echo 'make clean'
make clean