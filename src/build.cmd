set msbuild=C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe

:: Main Routine ::
rd /s /q .\_build\
call:Clean
call:Build
call:Clean
GOTO:EOF
::   End Main   ::




:Build
call:BuildCpp .\Bootstrap\Bootstrap.vcxproj
call:BuildCpp .\Inject\Inject.vcxproj
call:BuildCs .\InjectGUI\InjectGUI.csproj
call:BuildCs .\InjectExample\InjectExample.csproj anycpu\
GOTO:EOF

:BuildCpp
%msbuild% %~1 /p:VisualStudioVersion=11.0 /p:Platform=x86 /p:Configuration=Debug /p:OutDir=.\..\_build\debug\x86\
%msbuild% %~1 /p:VisualStudioVersion=11.0 /p:Platform=x86 /p:Configuration=Release /p:OutDir=.\..\_build\release\x86\
%msbuild% %~1 /p:VisualStudioVersion=11.0 /p:Platform=x64 /p:Configuration=Debug /p:OutDir=.\..\_build\debug\x64\
%msbuild% %~1 /p:VisualStudioVersion=11.0 /p:Platform=x64 /p:Configuration=Release /p:OutDir=.\..\_build\release\x64\
GOTO:EOF

:BuildCs
%msbuild% %~1 /p:VisualStudioVersion=11.0 /p:Platform=AnyCPU /p:Configuration=Debug /p:OutDir=.\..\_build\debug\%~2
%msbuild% %~1 /p:VisualStudioVersion=11.0 /p:Platform=AnyCPU /p:Configuration=Release /p:OutDir=.\..\_build\release\%~2
GOTO:EOF

:Clean
call:DeleteBin .\
call:DeleteBin .\Bootstrap\
call:DeleteBin .\Inject\
call:DeleteBin .\InjectGUI\
call:DeleteBin .\InjectExample\
rd /s /q .\ipch\
del /f .\*.sdf
del /f /AH .\*.suo
del /f /s .\*.aps
del /f /s .\*.vcxproj.filters
del /f /s .\*.vcxproj.user
del /f /s .\_build\*.exp
del /f /s .\_build\*.lib
del /f /s .\_build\*.ilk
del /f /s .\_build\FirstFloor.ModernUI.XML
GOTO:EOF

:DeleteBin
rd /s /q %~1Debug\
rd /s /q %~1Release\
rd /s /q %~1x64\
rd /s /q %~1bin\
rd /s /q %~1obj\
GOTO:EOF
