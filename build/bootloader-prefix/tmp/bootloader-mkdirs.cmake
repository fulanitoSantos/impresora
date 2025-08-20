# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/ESP-IDF/components/bootloader/subproject"
  "D:/Documents/Tesis/Codigos/impresora/build/bootloader"
  "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix"
  "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix/tmp"
  "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix/src"
  "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Documents/Tesis/Codigos/impresora/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
