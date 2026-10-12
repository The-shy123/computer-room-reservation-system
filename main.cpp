#include <iostream>
#include <fstream>
#include "Student.h"
#include "GlobalFile.h"
#include "Identity.h"
#include "Teacher.h"
#include "Manager.h"
using namespace std;

//登录功能
void LoginIn(string filename, int type);
//进入管理员子菜单界面
void managerMenu(Identity * &manager);
//进入学生子菜单界面
void studentMenu(Identity * &student);
//进入教师子菜单界面
void teacherMenu(Identity * &teacher);

int main()
{
    int select = 0;

    while (true)
    {
        cout << "======= 欢迎使用你的涛哥机房预约系统 =======" << endl;
        cout << endl << "请输入您的身份" << endl;
        cout << "\t\t----------------------\n";
        cout << "\t\t|                    |\n";
        cout << "\t\t|     1、学    生     |\n";
        cout << "\t\t|                    |\n";
        cout << "\t\t|     2、老    师     |\n";
        cout << "\t\t|                    |\n";
        cout << "\t\t|     3、管 理 员     ｜\n";
        cout << "\t\t|                    |\n";
        cout << "\t\t|     0、退    出     |\n";
        cout << "\t\t|                    |\n";
        cout << "\t\t----------------------\n";

        cout << " 请输入您的选择：" << endl;
        cin >> select;
        switch (select)
        {
            case 1:
                LoginIn(STUDENT_FILE, 1);
                break;
            case 2:
                LoginIn(TEACHER_FILE, 2);
                break;
            case 3:
                LoginIn(ADMIN_FILE, 3);
                break;
            case 0:
                cout << "已退出，欢迎下次使用！" << endl;
                cout << "按任意键继续" << endl;
                cin.ignore();
                cin.get();
                system("clear");
                return 0;
            default:
                cout << "输入有误，请重新输入！" << endl;
                cout << "按任意键继续" << endl;
                cin.ignore();
                cin.get();
                system("clear");
                break;
        }
    }
}

void LoginIn(string filename, int type)
{
    Identity * person =nullptr;

    ifstream ifs;
    ifs.open(filename, ios::in);

    if (!ifs.is_open())
    {
        cout <<"文件打开失败" << endl;
        ifs.close();
        return;
    }

    //准备信息
    int id;
    string name;
    string password;

    //判断身份
    if (type == 1)
    {
        cout << "请输入您的学号：" << endl;
        cin >> id;
    }
    else if (type == 2)
    {
        cout << "请输入您的工号：" << endl;
        cin >> id;
    }

    cout << "请输入用户名" << endl;
    cin >> name;

    cout << "请输入密码" << endl;
    cin >> password;

    if (type == 1)
    {
        //学生身份验证
        int fId;
        string fName;
        string fPwd;

        while (ifs >> fId && ifs >> fName && ifs >> fPwd)
        {

            if (id == fId && name == fName && password == fPwd)
            {
                cout << "登录成功!" << endl;
                cout << "按任意键继续" << endl;
                cin.ignore();
                cin.get();
                system("clear");
                person = new Student(id, name, password);

                //进入学生身份的子菜单
                studentMenu(person);
                return;
            }
        }
    }
    else if (type == 2)
    {
        //教师身份验证
        int fId;
        string fName;
        string fPwd;

        while (ifs >> fId && ifs >> fName && ifs >> fPwd)
        {

            if (id == fId && name == fName && password == fPwd)
            {
                cout << "登录成功!" << endl;
                cout << "按任意键继续" << endl;
                cin.ignore();
                cin.get();
                system("clear");
                person = new Teacher(id, name, password);

                //进入教师身份的子菜单
                teacherMenu(person);
                return;
            }
        }
    }
    else if (type == 3)
    {
        //管理员身份验证
        string fName;
        string fPwd;

        while (ifs >> fName && ifs >> fPwd)
        {

            if (name == fName && password == fPwd)
            {
                cout << "登录成功!" << endl;
                cout << "按任意键继续" << endl;
                cin.ignore();
                cin.get();
                system("clear");
                person = new Manager(name, password);

                //进入管理员身份的子菜单
                managerMenu(person);
                return;
            }
        }
    }

    cout << "验证登录失败" << endl;
    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");

}

void managerMenu(Identity * &manager)
{
    while(true)
    {
        //调用菜单
        manager->OperMenu();

        //将父类指针转位子类指针
        Manager * man = (Manager*)manager;
        int select = 0;
        cin >> select;
        if (select == 1) //添加账号
        {
            man->AddPerson();
        }
        else if (select == 2) //查看账号
        {
            man->ShowPerson();
        }
        else if (select == 3) //查看机房信息
        {
            man->ShowComputer();
        }
        else if (select == 4) //清空预约记录
        {
            man->CleanFile();
        }
        else //退出登录
        {
            delete manager;
            cout << "退出成功" << endl;
            cout << "按任意键继续" << endl;
            cin.ignore();
            cin.get();
            system("clear");
            return;
        }
    }
}

void studentMenu(Identity * &student)
{
    while (true)
    {
        //调用菜单
        student->OperMenu();

        //将父类指针转位子类指针
        Student * stu = (Student*)student;

        int select = 0;
        cin >> select;

        if (select == 1) //预约申请
        {
            stu->ApplyOrder();
        }
        else if (select == 2) //查看我的预约
        {
            stu->ShowMine();
        }
        else if (select == 3) //查看所有预约
        {
            stu->ShowAll();
        }
        else if (select == 4) //取消预约
        {
            stu->CancelOrder();
        }
        else //退出登录
        {
            delete student;
            cout << "退出成功" << endl;
            cout << "按任意键继续" << endl;
            cin.ignore();
            cin.get();
            system("clear");
            return;
        }
    }
}

void teacherMenu(Identity * &teacher)
{
    while (true)
    {
        //调用菜单
        teacher->OperMenu();

        //将父类指针转位子类指针
        Teacher * tea = (Teacher*)teacher;

        int select = 0;
        cin >> select;

        if (select == 1) //查看所有预约
        {
            tea->ShowAll();
        }
        else if (select == 2) //审核预约
        {
            tea->ValidOrder();
        }
        else //退出登录
        {
            delete teacher;
            cout << "退出成功" << endl;
            cout << "按任意键继续" << endl;
            cin.ignore();
            cin.get();
            system("clear");
            return;
        }
    }
}
