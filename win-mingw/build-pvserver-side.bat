@echo off
                                                                           
cd pvserver                                                                
qmake pvsmt.pro -o Makefile                                  
mingw32-make.exe                                                                
cd ..                                                                      
pause                                                                  
                                                                           
cd rllib                                                                   
qmake lib.pro -o Makefile                                    
mingw32-make.exe                                                                
cd ..                                                                      
copy rllib\release\librllib.a bin\librllib.a                                       
pause                                                                  
                                                                           
cd rlsvg                                                                   
qmake rlsvgcat.pro -o Makefile                               
mingw32-make.exe                                                                
cd ..                                                                      
pause                                                                  
                                                                           
cd rlfind                                                                   
qmake rlfind.pro -o Makefile                               
mingw32-make.exe                                                                
cd ..                                                                      
pause                                                                  
                                                                           
cd rlhistory                                                               
qmake rlhistory.pro -o Makefile                              
mingw32-make.exe                                                                
cd ..                                                                      
pause                                                                  
                                                                           
rem cd rlhtml2pdf                                                               
rem %QTDIR%\bin\qmake rlhtml2pdf.pro -o Makefile                              
rem mingw32-make.exe                                                                
rem cd ..                                                                      
rem pause                                                                  
                                                                           
cd fake_qmake                                                               
qmake fake_qmake.pro -o Makefile                              
mingw32-make.exe                                                                
cd ..                                                                      
pause                                                                  
                                                                           
cd ..\start_pvbapp                                                         
qmake start_pvbapp.pro -o Makefile                           
mingw32-make.exe                                                                
cd ..\win-mingw                                                            
pause                                                                  
                                                                           
cd pvmore                                                              
qmake pvmore.pro -o Makefile                           
mingw32-make.exe                                                                
cd ..                                                                      

%mingwdir%\bin\gcc.exe nodave.c -o nodave_mingw.o -c -D_WIN32 -DBCCWIN -I%mingwdir%\include
                                                                           
copy pvserver\release\libserverlib.a                             bin\libserverlib.a                              
copy rlsvg\release\rlsvgcat.exe                                  bin\rlsvgcat.exe                                
copy rlfind\release\rlfind.exe                                   bin\rlfind.exe                                
copy rlhistory\release\rlhistory.exe                             bin\rlhistory.exe                               
rem copy rlhtml2pdf\release\rlhtml2pdf.exe                           bin\rlhtml2pdf.exe                               
copy ..\start_pvbapp\release\\start_pvbapp.exe                   bin\start_pvbapp.exe                            
copy fake_qmake\release\fake_qmake.exe                           bin\fake_qmake.exe                              

echo ##############################################################################
echo # run me again in order to verify that no errors occured                     #
echo # If you_want a Lua binding then run: build_language_bindings.bat            #
echo # Bulding the language binding will produce a lot of WARNINGS !!!            #    
echo ##############################################################################

