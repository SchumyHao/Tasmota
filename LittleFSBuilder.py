Import("env")
import platform
import os

print(platform.machine().lower())
print(platform.system().lower())

env.Replace(MKSPIFFSTOOL='' + env.get("PROJECT_DIR") + '/mklittlefs/' + platform.machine().lower() + '_' + platform.system().lower() + '/mklittlefs' )