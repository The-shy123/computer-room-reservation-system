#include "Student.h"

Student::Student()
{
}

Student::Student(int id, string name, string password)
{
    this->id = id;
    this->m_Name = name;
    this->m_Password = password;

    ifstream ifs;
    ifs.open(COMPUTER_FILE, ios::in);

    ComputerRoom com;
    while (ifs >> com.m_ComId && ifs >> com.m_MaxNum)
    {
        vCom.push_back(com);
    }

    ifs.close();
}

void Student::OperMenu()
{
    cout << "欢迎学生：" << this->m_Name << "登录！" << endl;
    cout << "\t\t ----------------------------------\n";
    cout << "\t\t|                                 |\n";
    cout << "\t\t|          1. 预 约 申 请           |\n";
    cout << "\t\t|                                 |\n";
    cout << "\t\t|          2. 查看我的预约          |\n";
    cout << "\t\t|                                 |\n";
    cout << "\t\t|          3. 查看所有预约          |\n";
    cout << "\t\t|                                 |\n";
    cout << "\t\t|          4. 取 消 预 约           |\n";
    cout << "\t\t|                                 |\n";
    cout << "\t\t|          0. 退 出 登 录           |\n";
    cout << "\t\t|                                 |\n";
    cout << "\t\t ----------------------------------\n";
    cout << "请输入您的选择： " << endl;
}

void Student::ApplyOrder()
{
    cout << "机房开放时间为周一至周五" << endl;
    cout << "请输入您要预约的时间" << endl;
    cout << "1. 周一" << endl;
    cout << "2. 周二" << endl;
    cout << "3. 周三" << endl;
    cout << "4. 周四" << endl;
    cout << "5. 周五" << endl;

    int date = 0; //预约日期
    int time = 0; //预约时间段
    int room = 0; //预约机房编号

    while(true)
    {
        cin >> date;

        if(date >=1 || date <=5)
        {
            break;
        }

        cout << "输入有误，请重新输入" << endl;

    }

    cout << "请输入您要预约的时间段" << endl;
    cout << "1、上午" << endl;
    cout << "2、下午" << endl;

    while(true)
    {
        cin >> time;

        if(time >=1 || time <=2)
        {
            break;
        }

        cout << "输入有误，请重新输入" << endl;

    }

    cout << "请输入您要预约的机房编号" << endl;
    cout << "1号机房容量:" << vCom[0].m_MaxNum << endl;
    cout << "2号机房容量:" << vCom[1].m_MaxNum << endl;
    cout << "3号机房容量:" << vCom[2].m_MaxNum << endl;

    while(true)
    {
        cin >> room;

        if(room >=1 || room <=3)
        {
            break;
        }

        cout << "输入有误，请重新输入" << endl;

    }

    cout << "预约信息如下：" << endl;
    cout << "预约日期：周" << date << " " 
         << "预约时间段：" << (time == 1 ? "上午" : "下午")<< " " 
         << "预约机房：" << room << "号机房" << endl;

    cout << "预约审核中！" << endl;

    ofstream ofs(ORDER_FILE, ios::app);
    ofs << "date:" << date << " " 
        << "interval:" << time << " "
        << "studId:" << this->id << " " 
        << "stuName:" << this->m_Name << " " 
        << "roomId:" << room << " " 
        << "status:" << 1 << endl;
    ofs.close();

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}

void Student::ShowMine()
{
    OrderFile of;

    if (of.m_Size == 0)
    {
        cout << "无预约记录！" << endl;
        cout << "按任意键继续" << endl;
        cin.ignore();
        cin.get();
        system("clear");
        return;
    }

    for(int i = 0; i < of.m_Size; i++)
    {
        if (this->id == atoi(of.m_Data[i]["studId"].c_str()))
        {
            cout << "预约日期：周" << of.m_Data[i]["date"] << " "
                 << "预约时间段：" << (of.m_Data[i]["interval"] == "1" ? "上午" : "下午") << " "
                 << "预约机房：" << of.m_Data[i]["roomId"] << "号机房 "
                 << "预约状态：";

            // 0 取消预约 1 审核中 2 预约成功 -1 预约失败
            if (of.m_Data[i]["status"] == "1")
            {
                cout << "审核中" << endl;
            }
            else if (of.m_Data[i]["status"] == "2")
            {
                cout << "预约成功" << endl;
            }
            else if (of.m_Data[i]["status"] == "-1")
            {
                cout << "预约失败" << endl;
            }
            else
            {
                cout << "预约取消" << endl;
            }
        }
    }

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}

void Student::ShowAll()
{
    OrderFile of;

    if (of.m_Size == 0)
    {
        cout << "无预约记录！" << endl;
        cout << "按任意键继续" << endl;
        cin.ignore();
        cin.get();
        system("clear");
        return;
    }

    for(int i = 0; i < of.m_Size; i++)
    {
        cout << i + 1 << "、";

        cout << "预约日期：周" << of.m_Data[i]["date"] << " "
             << "预约时间段：" << (of.m_Data[i]["interval"] == "1" ? "上午" : "下午") << " "
             << "预约人学号：" << of.m_Data[i]["studId"] << " "
             << "预约人姓名：" << of.m_Data[i]["stuName"] << " "
             << "预约机房：" << of.m_Data[i]["roomId"] << "号机房 "
             << "预约状态：";

        // 0 取消预约 1 审核中 2 预约成功 -1 预约失败
        if (of.m_Data[i]["status"] == "1")
        {
            cout << "审核中" << endl;
        }
        else if (of.m_Data[i]["status"] == "2")
        {
            cout << "预约成功" << endl;
        }
        else if (of.m_Data[i]["status"] == "-1")
        {
            cout << "预约失败" << endl;
        }
        else
        {
            cout << "预约取消" << endl;
        }
    }

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}

void Student::CancelOrder()
{
    OrderFile of;

    if (of.m_Size == 0)
    {
        cout << "无预约记录！" << endl;
        cout << "按任意键继续" << endl;
        cin.ignore();
        cin.get();
        system("clear");
        return;
    }

    cout << "审核中的预约不能取消，请注意！" << endl;
    cout << "请输入取消的预约记录" << endl;

    vector<int> v; //存放学生自己的预约记录
    int index = 1; //记录序号

    for(int i = 0; i < of.m_Size; i++)
    {
        if (this->id == atoi(of.m_Data[i]["studId"].c_str()))
        {
            if(of.m_Data[i]["status"] == "1" || of.m_Data[i]["status"] == "2")
            {
                v.push_back(i);
                cout << index++ << "、";
            

                cout << "预约日期：周" << of.m_Data[i]["date"] << " "
                    << "预约时间段：" << (of.m_Data[i]["interval"] == "1" ? "上午" : "下午") << " "
                    << "预约机房：" << of.m_Data[i]["roomId"] << "号机房 "
                    << "预约状态：";

                // 0 取消预约 1 审核中 2 预约成功 -1 预约失败
                if (of.m_Data[i]["status"] == "1")
                {
                    cout << "审核中" << endl;
                }
                else if (of.m_Data[i]["status"] == "2")
                {
                    cout << "预约成功" << endl;
                }
                else if (of.m_Data[i]["status"] == "-1")
                {
                    cout << "预约失败" << endl;
                }
                else
                {
                    cout << "预约取消" << endl;
                }
            }
        }
    }

    cout << "请输入取消的预约记录编号：" << endl;
    int select = 0;

    while(true)
    {
        cin >> select;

        if(select >=0 && select <= v.size())
        {
            if(select == 0)
            {
                break;
            }
            else
            {
                of.m_Data[v[select - 1]]["status"] = "0";
                of.updateOrder();
                cout << "已取消该预约！" << endl;
                break;
            }
        }

        cout << "输入有误，请重新输入" << endl;
       
    }
        
    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}