#include "OrderFile.h"

OrderFile::OrderFile()
{
    //初始化容器大小
    m_Size = 0;

    ifstream ifs;
    ifs.open(ORDER_FILE, ios::in);

    string date;      //日期
    string interval;  //时间段
    string stuId;     //学生学号
    string stuName;   //学生姓名
    string roomId;    //机房编号
    string status;    //预约状态

    while(ifs >> date && ifs >> interval && ifs >> stuId
          && ifs >> stuName && ifs >> roomId && ifs >> status)
    {
        string key; //键
        string value; //值
        map<string, string> m; //map容器

        int pos = date.find(":");
        if(pos != -1)
        {
            key = date.substr(0, pos);
            value = date.substr(pos + 1, date.size() - pos - 1);
            m.insert(make_pair(key, value));
        }

        pos = interval.find(":");
        if(pos != -1)
        {
            key = interval.substr(0, pos);
            value = interval.substr(pos + 1, interval.size() - pos - 1);
            m.insert(make_pair(key, value));
        }

        pos = stuId.find(":");
        if(pos != -1)
        {
            key = stuId.substr(0, pos);
            value = stuId.substr(pos + 1, stuId.size() - pos - 1);
            m.insert(make_pair(key, value));
        }

        pos = stuName.find(":");
        if(pos != -1)
        {
            key = stuName.substr(0, pos);
            value = stuName.substr(pos + 1, stuName.size() - pos - 1);
            m.insert(make_pair(key, value));
        }

        pos = roomId.find(":");
        if(pos != -1)
        {
            key = roomId.substr(0, pos);
            value = roomId.substr(pos + 1, roomId.size() - pos - 1);
            m.insert(make_pair(key, value));
        }

        pos = status.find(":");
        if(pos != -1)
        {
            key = status.substr(0, pos);
            value = status.substr(pos + 1, status.size() - pos - 1);
            m.insert(make_pair(key, value));
        }

        this->m_Data.insert(make_pair(this->m_Size, m));
        this->m_Size++;
    }
}

void OrderFile::updateOrder()
{
    if(this->m_Size == 0)
    {
        return;
    }

    ofstream ofs;
    ofs.open(ORDER_FILE, ios::out | ios::trunc);

    for(int i = 0; i < this->m_Size; i++)
    {
        ofs << "date:" << this->m_Data[i]["date"] << " "
            << "interval:" << this->m_Data[i]["interval"] << " "
            << "stuId:" << this->m_Data[i]["stuId"] << " "
            << "stuName:" << this->m_Data[i]["stuName"] << " "
            << "roomId:" << this->m_Data[i]["roomId"] << " "
            << "status:" << this->m_Data[i]["status"] << endl;
    }

    ofs.close();
}
