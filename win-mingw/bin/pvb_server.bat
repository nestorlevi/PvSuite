@echo off    
taskkill /F /IM pvs.exe
rem system("pvb_server.bat " + name);                                      
CALL "%PVBDIR%\win-mingw\bin\pvenv.bat"                              
fake_qmake %1.pro -o Makefile.win                                              
mingw32-make.exe -j -f Makefile.win                                                
release\%1  
taskkill /F /FI "WINDOWTITLE eq cmd.exe"
exit