REM - Copy Resources to own build folders

xcopy .\Resources\*.* .\Unicode_Debug\Resources\   /E /EXCLUDE:.\Resources\Exclude.txt /Y /D
xcopy .\Resources\*.* .\Unicode_Release\Resources\ /E /EXCLUDE:.\Resources\Exclude.txt /Y /D

REM - Copy Resources to latest release

xcopy .\Resources\*.* D:\ToDoList_Source_Versions\ToDoList_src.%1\ToDoList\Resources\                 /E /EXCLUDE:.\Resources\Exclude.txt /Y /D
xcopy .\Resources\*.* D:\ToDoList_Source_Versions\ToDoList_src.%1\ToDoList\Unicode_Debug\Resources\   /E /EXCLUDE:.\Resources\Exclude.txt /Y /D
xcopy .\Resources\*.* D:\ToDoList_Source_Versions\ToDoList_src.%1\ToDoList\Unicode_Release\Resources\ /E /EXCLUDE:.\Resources\Exclude.txt /Y /D
