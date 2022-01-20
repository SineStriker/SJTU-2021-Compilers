goto start

@rem 在lab所在路径启动终端，运行此批处理即可创建容器
@rem 在任意位置运行ssh -p2222 stu@127.0.0.1即可进入虚拟终端（密码123）
@rem 使用CLion开发时，在Build, Execution, Development - Toolchains中添加RemoteHost，按以下配置连接ssh即可

@rem Host: localhose
@rem Port: 2222
@rem Local port: 22
@rem Username: stu
@rem Password: 123

:start
docker run -it --privileged -p 2222:22 -v %cd%:/home/stu/tiger-compiler ipadsse302/tigerlabs_env:latest