#include<iostream>
#include<memory>
using namespace std;

int count = 0;
class Test
{
public:
    Test(int id = count++)
        : _id(id)
    {
        cout<<"in Test(): id = "<<_id<<endl;
    }
    ~Test()
    {
        cout<<"in ~Test(): id = "<<_id<<endl;
    }
private:
    int _id;
};


int main()
{
    shared_ptr<Test[]> ptr = shared_ptr<Test[]>(new Test[10], [](Test* data){delete[] data; });
    Test* data = ptr.get();
}