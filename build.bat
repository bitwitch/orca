
if not exist bin mkdir bin

set INCLUDES=/I src /I src/util /I src/platform /I ext /I ext/angle_headers
cl /we4013 /Zi /Zc:preprocessor /std:c11 %INCLUDES% /c /Fo:bin/milepost.obj src/milepost.c
lib bin/milepost.obj /OUT:bin/milepost.lib
