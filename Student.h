#pragma once
#include <iostream>
#include "Identity.h"
#include "ComputerRoom.h"
#include <vector>
#include <fstream>
#include "GlobalFile.h"
#include "OrderFile.h"

using namespace std;

class Student : public Identity
{
public:
    Student();

    Student(int id, string name, string password);

    //菜单界面  
    virtual void OperMenu();

    //预约申请
    void ApplyOrder();

    //显示我的预约
    void ShowMine();

    //显示所有预约
    void ShowAll();

    //取消预约
    void CancelOrder();

    //学号
    int id;

    vector<ComputerRoom> vCom;

};
