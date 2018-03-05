// Main Program      (Main.cpp) 
// Description: Calling ASM procedures in C with different calling conventions
// Date:        01/19/2017
// Written by:  Zuoliu Ding
// (C) Copyright 2017- 2027, Zuoliu Ding. All Rights Reserved.  

// __cdecl: https://msdn.microsoft.com/en-us/library/zkwh89ks.aspx
// __stdcall: https://msdn.microsoft.com/en-us/library/zxk0tw93.aspx

#include <iostream>
using namespace std;

extern "C" {
   int ProcC_CallWithParameterList(int, int);
   int ProcC_CallWithStackFrame(int, int);

   int __stdcall ProcSTD_CallWithParameterList(int, int);
   int __stdcall ProcSTD_CallWithStackFrame(int, int);
}

extern "C" {
   int ReadFromConsole(unsigned char); // A C function to be called in assembly
   void DisplayToConsole(char*, int);  // A C function to be called in assembly
   void DoSubtraction();               // An assembly procedure to be called in C++ main()
}

int ReadFromConsole(unsigned char by)
{
   cout << "Enter " << by <<": ";
   int i;
   cin >> i;
   return i;
}

void DisplayToConsole(char* s, int n)
{
   cout << s << n <<endl <<endl;
}

int main()
{
   DoSubtraction();

   cout << "C-Call With Parameters:  10-3 = " << ProcC_CallWithParameterList(10, 3) << endl;
   cout << "C-Call With Stack Frame: 10-3 = " << ProcC_CallWithStackFrame(10, 3) << endl;
   cout << "STD-Call With Parameters:  10-3 = " << ProcSTD_CallWithParameterList(10, 3) << endl;
   cout << "STD-Call With Stack Frame: 10-3 = " << ProcSTD_CallWithStackFrame(10, 3) << endl;
   system("pause");
}






















//extern "C" int __stdcall addem(int, int, int);
