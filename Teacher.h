#pragma once
#include<iostream>
using namespace std;
#include"Identity.h"
#include"GlobalFile.h"
#include<fstream>
#include"ComputerRoom.h"
#include"OrderFile.h"

class Teacher:public Identity
{
public:
    Teacher();

    Teacher(int empId, string name, string password);

    //菜单
    virtual void OperMenu();

    //查看所有预约
    void ShowAll();

    //审核预约
    void ValidOrder();

    int empId;

};