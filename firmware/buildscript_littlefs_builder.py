Import("env")
env.Replace(
    MKSPIFFSTOOL=env.get("PROJECT_DIR") + "/firmware/tools/mklittlefs/mklittlefs.exe"
)
