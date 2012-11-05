/***************************************************************************
*
*   File    : fileutils.h
*   Purpose : Implements librari for reading emls.
*
*
*   Original Author: David Ruano Ordás, Moncho Méndez Reboredo.
*
*
*   Date    : February  14, 2010
*
*****************************************************************************
*   LICENSING
*****************************************************************************
*
* WB4Spam: An ANSI C is an open source, highly extensible, high performance and
* multithread spam filtering platform. It takes concepts from SpamAssassin project
* improving distinct issues.
*
* Copyright (C) 2010, by Sing Research Group (http://sing.ei.uvigo.es)
*
* This file is part of WireBrush for Spam project.
*
* Wirebrush for Spam is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* Wirebrush for Spam is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***********************************************************************/

#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

#define EML_INVALID -4
#define OPEN_ERROR -3
#define READ_FAIL -2
#define MAGIC_ERROR -1
#define MAGIC_OK 0



int ae_load_eml_to_memory(const char *filename, char **result);

int get_mime_type(const char *path, char **mime);

#endif
