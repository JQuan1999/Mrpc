#include <mrpc/common/buffer.h>
#include <iostream>
#include <time.h>
#include <string>
using namespace mrpc;
using namespace std;


char* RandStr(char* str, int len)
{
    for(int i = 0; i < len; i++)
    {
        str[i] = 'a' + rand() % 26;
    }
    return str;
}


int main()
{
    srand(time(0));
    // set data大于一个block
    WriteBufferPtr ptr(new WriteBuffer());
    void* data;
    int size;
    ptr->Next(&data, &size);
    ptr->BackUp(size/2);
    RandStr((char*)data, size / 2);
    std::string s1 = ptr->ToString();

    int head = ptr->Reserve(size);
    cout<<"head: "<<head<<endl;
    char cs[size];
    RandStr(cs, size);
    std::string s2(cs, size);
    ptr->SetData(head, cs, size);
    cout<<"write count: "<<ptr->ByteCount()<<endl;

    auto iter = ptr->GetCurrentIter();
    cout<<"iter getsize(): "<<iter->GetSize()<<endl;
    cout<<"ptr tostring(): "<<ptr->ToString()<<endl;
}