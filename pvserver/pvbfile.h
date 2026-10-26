/***************************************************************************
                          pvbfile.h  -  description
                             -------------------
    begin                : 2026
    copyright            : (C) Nestor Palomeque, pvbrowser community
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
/****************************************************************************
** Nestor: archivos con nombre UTF-8 (acentos tambien en Windows) sin std::filesystem:
** el de MinGW 8.1 (Qt 5.15) no compila en Windows. Las usan pvbusers.h y pvbrecipes.h;
** estan implementadas en util.cpp (serverlib), asi windows.h no llega al codigo del usuario.
****************************************************************************/
#ifndef PVBFILE_H
#define PVBFILE_H

#include <stdio.h>
#include <string>
#include <vector>

FILE *pvbFileOpen(const std::string &utf8, const char *mode);         //!< fopen (Windows: _wfopen)
bool  pvbFileReplace(const std::string &from, const std::string &to); //!< reemplaza de una vez (Windows: MoveFileEx); si falla queda el anterior
bool  pvbFileRemove(const std::string &utf8);
void  pvbMakeDirs(const std::string &utf8);                           //!< crea la carpeta y las que falten ("a/b/c")
std::vector<std::string> pvbDirFiles(const std::string &dir, const std::string &ext); //!< archivos con esa extension (".rcp"), sin ella y ordenados
bool  pvbReadLines(const std::string &utf8, std::vector<std::string> &lines);         //!< lineas sin \r\n; false si no se puede abrir

#endif
