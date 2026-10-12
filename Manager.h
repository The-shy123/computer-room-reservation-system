#pragma once
#include <iostream>
#include "Identity.h"
#include <fstream>
#include "GlobalFile.h"
#include <vector>
#include "Student.h"
#include "Teacher.h"
#include <algorithm>
#include "ComputerRoom.h"
using namespace std;

class Manager : public Identity
{
public:

    Manager();

    Manager(string name, string password);

    virtual void OperMenu();

    //添加账号
    void AddPerson();

    //查看账号
    void ShowPerson();

    //查看机房信息
    void ShowComputer();

    //清空预约记录
    void CleanFile();

    //初始化容器
    void InitVector();

    //学生容器
    vector<Student> StuVector;

    //老师容器
    vector<Teacher> TeaVector;

    //检测重复
    bool CheckRepeat(int id, int type);

    vector<ComputerRoom> vCom;

};
