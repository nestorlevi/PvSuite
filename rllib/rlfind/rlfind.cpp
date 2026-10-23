/***************************************************************************
                          rlsvgcat.cpp  -  description
                             -------------------
    begin                : Tue Jul 30 2015
    copyright            : (C) 2015 by pvbrowser
    email                : lehrig@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
int printPath(const char *path)
{
  if(*path == '\0') return -1;
  if(*path == '\n') return -1;
  if(*path != '.' && *path != '\\' && *path != '/') putchar('/');
  while(*path != '\0') 
  {
    if(*path == '\\') putchar('/');
    else              putchar(*path);
    path++;
  }  
  return 0;
}

int find(const char *szDir, const char *szFunction, const char *szPattern)
{
   WIN32_FIND_DATAA ffd;
   HANDLE hFind = INVALID_HANDLE_VALUE;
   std::vector<char> bufv(strlen(szDir) + 4); char *buf = bufv.data(); // Nestor: sin VLA (MSVC)
   if(szDir[0] == '/') { strcpy(buf,&szDir[1]); strcat(buf,"\\*"); }
   else                { strcpy(buf,szDir);     strcat(buf,"\\*"); }
   
   hFind = FindFirstFileA(buf, &ffd);
   if(INVALID_HANDLE_VALUE == hFind) return -1;
   do
   {
      if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        if(ffd.cFileName[0] != '.')
        {  
          std::vector<char> pathv(strlen(szDir) + strlen(ffd.cFileName) + 2); char *path = pathv.data(); // Nestor: sin VLA (MSVC); +2 = separador + terminador
          strcpy(path, szDir);
          strcat(path,"\\");
          strcat(path,ffd.cFileName);
          if(strstr(ffd.cFileName,szPattern) != NULL)
          {  
            printPath(path);
            printf(" [d]\n");
          }  
          if(strcmp(szFunction,"-only") != 0) find(path,szFunction,szPattern);
        }  
      }
      else if(*szPattern=='\0' || strstr(ffd.cFileName,szPattern) != NULL)
      {
        std::vector<char> pathv(strlen(szDir) + strlen(ffd.cFileName) + 2); char *path = pathv.data(); // Nestor: sin VLA (MSVC); +2 = separador + terminador
        strcpy(path, szDir);
        strcat(path,"\\");
        strcat(path,ffd.cFileName);
        printPath(path);
        printf(" [f]\n");
      }
   }
   while(FindNextFileA(hFind, &ffd) != 0);
   FindClose(hFind);
   return 0;
}
#else

#include <sys/types.h>
#include <dirent.h>
int find(const char *szDir, const char *szFunction, const char *szPattern)
{
   std::vector<char> bufv(strlen(szDir) + 4); char *buf = bufv.data(); // Nestor: sin VLA (MSVC)
   strcpy(buf,szDir); strcat(buf,"\\*");

   DIR *dir = opendir(szDir);
   if(dir == NULL) return -1;   
   while(1)
   {
      struct dirent *de = readdir(dir);
      if(de == NULL) break;
      if(de->d_type == DT_DIR)
      {
	      if(de->d_name[0] != '.')
	      {
          std::vector<char> pathv(strlen(szDir) + strlen(de->d_name) + 2); char *path = pathv.data(); // Nestor: +2 = separador + terminador
          strcpy(path, szDir);
          strcat(path,"/");
          strcat(path,de->d_name);
	        printf("%s [d]\n", path);
          if(strcmp(szFunction,"-only") != 0) find(path,szFunction,szPattern);
	      }  
      }
      else
      {
        std::vector<char> pathv(strlen(szDir) + strlen(de->d_name) + 2); char *path = pathv.data(); // Nestor: +2 = separador + terminador
        strcpy(path, szDir);
        strcat(path,"/");
        strcat(path,de->d_name);
	      if(szPattern[0] == '\0')                      printf("%s [f]\n", path);
	      else if(strstr(de->d_name,szPattern) != NULL) printf("%s [f]\n", path);
      }
      
   }
   closedir(dir);
   return 0;
}
#endif

int main(int argc, char **argv)
{
   if(argc == 2)
   {
//#ifdef _WIN32   
     find(argv[1], "-only", "");
#ifdef _WIN32
     printf("#EOF#\n");
#endif     
     return 0;   
   }
   else if(argc != 4)
   {
      printf("usage: %s <start_directory> <-name|-only> <pattern>\n", argv[0]);
      return (-1);
   }

   std::vector<char> mypatternv(strlen(argv[3]) + 1); char *mypattern = mypatternv.data(); // Nestor: sin VLA (MSVC)
   char       *dest = &mypattern[0];
   const char *cptr = argv[3];
   while(*cptr != '\0')
   {
     if(*cptr == '*') cptr++;
     else             *dest++ = *cptr++;
   }
   *dest = '\0';
   find(argv[1], argv[2], mypattern);
#ifdef _WIN32
   printf("#EOF#\n");
#endif     
   
   return 0;
}
