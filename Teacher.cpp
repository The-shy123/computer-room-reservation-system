#include "Teacher.h"

Teacher::Teacher()
{

}

Teacher::Teacher(int empId, string name, string password)
{
    this->empId = empId;
    this->m_Name = name;
    this->m_Password = password;
}

void Teacher::OperMenu()
{
    cout << "欢迎教师:" << this->m_Name << "登录!" << endl;
    cout << "\t\t ----------------------------------\n";
    cout << "\t\t|                                  |\n";
    cout << "\t\t|          1.查看所有预约            |\n";
    cout << "\t\t|                                  |\n";
    cout << "\t\t|          2.审 核 预 约            |\n";
    cout << "\t\t|                                  |\n";
    cout << "\t\t|          3.退 出 登 录            |\n";
    cout << "\t\t|                                  |\n";
    cout << "\t\t ----------------------------------\n";
    cout << "请选择您的操作:";
}

void Teacher::ShowAll()
{
    OrderFile of;
    if (of.m_Size == 0)
    {
        cout << "无预约记录!" << endl;
        cout << "按任意键继续..." << endl;
        getchar();
        getchar();
        system("clear");
        return;
    }

    for(int i = 0; i < of.m_Size; i++)
    {
        cout << "预约编号:" << i + 1 << "\t";
        cout << "学生编号:" << of.m_Data[i]["studentId"] << "\t";
        cout << "学生姓名:" << of.m_Data[i]["studentName"] << "\t";
        cout << "机房编号:" << of.m_Data[i]["roomId"] << "\t";
        cout << "预约时间:" << of.m_Data[i]["date"] << "\t";
        cout << "预约时段:" << (of.m_Data[i]["interval"] == "1" ? "上午" : "下午") << "\t";
        string status = "状态未知";
        if (of.m_Data[i]["status"] == "0")
        {
            status = "审核中";
        }
        else if (of.m_Data[i]["status"] == "1")
        {
            status = "预约成功";
        }
        else if (of.m_Data[i]["status"] == "-1")
        {
            status = "预约失败";
        }
        cout << "预约状态:" << status << endl;
    }
    cout << "按任意键继续..." << endl;
    getchar();
    getchar();
    system("clear");
}

void Teacher::ValidOrder()
{
    OrderFile of;
    if(of.m_Size == 0)
    {
        cout << "无预约记录!" << endl;
        cout << "按任意键继续..." << endl;
        getchar();
        getchar();
        system("clear");
        return;
    }

    cout << "待审核的预约记录如下:" << endl;

    vector<int> v; //存放待审核的预约记录编号
    int index = 0; //记录序号
    for(int i = 0; i < of.m_Size; i++)
    {
        if(of.m_Data[i]["status"] == "1")
        {
            v.push_back(i);
            cout << ++index << ".";
            cout << "预约日期:" << of.m_Data[i]["date"];
            cout << " 时段:" << (of.m_Data[i]["interval"] == "1" ? "上午" : "下午");
            cout << " 机房:" << of.m_Data[i]["roomId"];

            string status = "状态未知";
            if (of.m_Data[i]["status"] == "1")
            {
                status = "审核中";
            }

            cout << " 状态:" << status << endl;
        }
    }

    cout << "请输入审核的预约记录,0代表返回:";
    int select = 0;
    int ret = 0;
    while(true)
    {
        cin >> select;
        if(select >= 0 && select <= v.size())
        {
            if(select == 0)
            {
                break;
            }
            else
            {
                cout << "请输入审核结果,1通过,2不通过:";
                cin >> ret;

                if(ret == 1) //通过
                {
                    of.m_Data[v[select - 1]]["status"] = "2";
                }
                else //不通过
                {
                    of.m_Data[v[select - 1]]["status"] = "-1";
                }
                of.updateOrder(); //更新预约信息
                cout << "审核完毕!" << endl;
                break;
            }
               
        }
        else
        {
            cout << "输入有误,请重新输入:";
        }
    }
}