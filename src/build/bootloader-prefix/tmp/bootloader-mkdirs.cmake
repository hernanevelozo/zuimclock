# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/aldebaran/Appdata/Esp/esp-idf/components/bootloader/subproject"
  "/home/aldebaran/display/zuim-clock/build/bootloader"
  "/home/aldebaran/display/zuim-clock/build/bootloader-prefix"
  "/home/aldebaran/display/zuim-clock/build/bootloader-prefix/tmp"
  "/home/aldebaran/display/zuim-clock/build/bootloader-prefix/src/bootloader-stamp"
  "/home/aldebaran/display/zuim-clock/build/bootloader-prefix/src"
  "/home/aldebaran/display/zuim-clock/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/aldebaran/display/zuim-clock/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/aldebaran/display/zuim-clock/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
