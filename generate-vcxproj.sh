#!/bin/sh

source toolchain/generate-vcxproj.sh

write_command "_GENERATE PROJECTS" {F9614213-9806-4150-88BA-32666939BEE4} '"c:\Program Files\git\usr\bin\sh.exe"' --login '$(SolutionDir)\generate-vcxproj.sh'

write_exe kernel kernel {9273FB38-F89F-4E3A-A210-290DB0C2A33D} .
write_exe app app {727A018B-03BC-411B-BB6F-7D224A38137A} .

write_solution NeedleOS.sln