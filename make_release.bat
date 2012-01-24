SET TARGETDIR=frogatto_msvc_bin

::libs
copy stage\glew32.dll %TARGETDIR%
copy libfreetype-6.dll %TARGETDIR%
copy libogg-0.dll %TARGETDIR%
copy libpng15-15.dll %TARGETDIR%
copy libvorbis-0.dll %TARGETDIR%
copy libvorbisfile-3.dll %TARGETDIR%
copy msvcp100.dll %TARGETDIR%
copy msvcr100.dll %TARGETDIR%
copy SDL.dll %TARGETDIR%
copy SDL_image.dll %TARGETDIR%
copy SDL_mixer.dll %TARGETDIR%
copy SDL_ttf.dll %TARGETDIR%
copy zlib1.dll %TARGETDIR%

::executables
copy stage\frogatto.exe %TARGETDIR%
copy frogatto_fullscreen.bat  %TARGETDIR%