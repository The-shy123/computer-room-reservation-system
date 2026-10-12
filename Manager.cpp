#include "Manager.h"

Manager::Manager()
{

}

Manager::Manager(string name, string password)
{
    this->m_Name = name;
    this->m_Password = password;

    //初始化容器
    this->InitVector();

    //初始化机房信息(后续最好封装成函数)
    ifstream ifs;

    ifs.open(COMPUTER_FILE, ios::in);

    ComputerRoom c;
    while(ifs >> c.m_ComId && ifs >> c.m_MaxNum)
    {
        vCom.push_back(c);
    }

    cout << "当前机房数量为：" << vCom.size() << endl;
    ifs.close();

}

void Manager::OperMenu()
{
    cout << "欢迎管理员" << this->m_Name << "登陆" << endl;
    cout << "\t\t----------------------\n";
    cout << "\t\t|                    |\n";
    cout << "\t\t|     1、 添加账号     |\n";
    cout << "\t\t|                    |\n";
    cout << "\t\t|     2、 查看账号     |\n";
    cout << "\t\t|                    |\n";
    cout << "\t\t|     3、 查看机房     |\n";
    cout << "\t\t|                    |\n";
    cout << "\t\t|     4、 清空预约     |\n";
    cout << "\t\t|                    |\n";
    cout << "\t\t|     0、 退出登录     |\n";
    cout << "\t\t|                    |\n";
    cout << "\t\t----------------------\n";
    cout << "请选择您的操作：" << endl;
}

void Manager::AddPerson()
{
    cout << "请输入添加账号类型：" << endl;
    cout << "1、添加老师账号" << endl;
    cout << "2、添加学生账号" << endl;

    string fileName;
    string tip;
    ofstream ofs;

    int select = 0;
    cin >> select;

    string errorTip;

    if (select == 1)
    {
        fileName = STUDENT_FILE;
        tip = "请输入学号：";
        errorTip = "学号已存在，请重新输入！";
    }
    else 
    {
        fileName = TEACHER_FILE;
        tip = "请输入职工号：";
        errorTip = "职工号已存在，请重新输入！";
    }
   
    ofs.open(fileName, ios::out | ios::app);
    int id;
    string name;
    string password;
    cout << tip << endl;

    while(true)
    {
        cin >> id;
        if (CheckRepeat(id, select))
        {
            cout << errorTip << endl;
        }
        else
        {
            break;
        }
    }

    cout << "请输入姓名：" << endl;
    cin >> name;
    cout << "请输入密码：" << endl;
    cin >> password;
    ofs << id << " " << name << " " << password << endl;
    ofs.close();
    cout << "账号添加成功" << endl;

    this->InitVector(); //重新初始化容器

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");

}

void PrintStudent(Student &stu)
{
    cout << "学号：" << stu.id << "姓名：" << stu.m_Name << "密码：" << stu.m_Password << endl;
}

void PrintTeacher(Teacher &tea)
{
    cout << "职工号：" << tea.empId << "姓名：" << tea.m_Name << "密码：" << tea.m_Password << endl;
}

void Manager::ShowPerson()
{
    cout << "请选择查看内容：" << endl;
    cout << "1、查看所有学生" << endl;
    cout << "2、查看所有老师" << endl;

    int select = 0;
    cin >> select;

    if(select == 1)
    {
        cout << "所有学生信息如下：" << endl;
        for_each(StuVector.begin(), StuVector.end(), PrintStudent);
    }
    else
    {
        cout << "所有老师信息如下：" << endl;
        for_each(TeaVector.begin(), TeaVector.end(), PrintTeacher);
    }

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}

void Manager::ShowComputer()
{
    cout << "机房信息如下：" << endl;
    for (vector<ComputerRoom>::iterator it = vCom.begin(); it != vCom.end(); it++)
    {
        cout << "机房编号：" << it->m_ComId << " 机房最大容量：" << it->m_MaxNum << endl;
    }

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}

void Manager::CleanFile()
{
    ofstream ofs(ORDER_FILE, ios::trunc);
    ofs.close();
    
    cout << "已清空预约记录" << endl;

    cout << "按任意键继续" << endl;
    cin.ignore();
    cin.get();
    system("clear");
}

void Manager::InitVector()
{
    //初始化学生容器
    ifstream ifsStu(STUDENT_FILE, ios::in);
    if (!ifsStu.is_open())
    {
        cout << "学生文件打开失败" << endl;
        return;
    }

    StuVector.clear(); // 清空容器
    TeaVector.clear(); // 清空容器
    
    Student stu;
    while (ifsStu >> stu.id >> stu.m_Name >> stu.m_Password)
    {
        StuVector.push_back(stu);
    }
    cout << "当前学生数量：" << StuVector.size() << endl;
    ifsStu.close();

    //初始化老师容器
    ifstream ifsTea(TEACHER_FILE, ios::in);
    if (!ifsTea.is_open())
    {
        cout << "老师文件打开失败" << endl;
        return;
    }

    Teacher tea;
    while (ifsTea >> tea.empId >> tea.m_Name >> tea.m_Password)
    {
        TeaVector.push_back(tea);
    }
    cout << "当前老师数量：" << TeaVector.size() << endl;
    ifsTea.close();
}

bool Manager::CheckRepeat(int id, int type)
{
    if (type == 1) //学生
    {
        for (vector<Student>::iterator it = StuVector.begin(); it != StuVector.end(); it++)
        {
            if (it->id == id)
            {
                return true;
            }
        }
    }
    else //老师
    {
        for (vector<Teacher>::iterator it = TeaVector.begin(); it != TeaVector.end(); it++)
        {
            if (it->empId == id)
            {
                return true;
            }
        }
    }
    return false;
}
