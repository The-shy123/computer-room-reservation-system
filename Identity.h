#pragma once
#include<iostream>
using namespace std;

class Identity
{
public:

    virtual void OperMenu() = 0;

    string m_Name;
    string m_Password;

    virtual ~Identity() = default;
};