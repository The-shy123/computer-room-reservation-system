#pragma once
#include <iostream>
#include <map>
#include <fstream>
#include "Identity.h"
#include "GlobalFile.h"
using namespace std;

class OrderFile
{
public:

    //构造函数
    OrderFile();

    //更新预约信息
    void updateOrder();

    //预约信息容器
    map<int, map<string, string>> m_Data;

    //预约记录条数
    int m_Size;
};
