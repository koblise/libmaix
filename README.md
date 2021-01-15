libmaix
=========

[中文 README](README_ZH.md)

-----

A library for embeded AI model inference with hardware acceleration，let's build a better AIOT together~

Now support platform:

* V831


## Build


Current only test pass on Ubuntu18.04 and Ubuntu20.04

* Install dependence

```
apt install build-essential cmake python3 sshpass
```

* Check `cmake` version, **should >= `v3.9`**
Check `CMake` version by 

```
cmake --version
```

The `cmake` version should be at least `v3.9`, if not, please install latest `cmake` manually from [cmake website](https://cmake.org/download/)

* Setup toolchain

Download toolchain first: [toolchain-sunxi-musl-pack-2021-01-09.tar.xz](https://api.dl.sipeed.com/shareURL/MAIX/SDK_MaixII/Toolchain) (or download from [github](https://github.com/sipeed/libmaix/releases/download/v0.1.0/toolchain-sunxi-musl-pack-2021-01-09.tar.xz))

Unzip to `/opt/` directory

```shell
tar -Jxvf toolchain-sunxi-musl-pack-2021-01-09.tar.xz -C /opt
```

Then you can see `toolchain-sunxi-musl` folder under `/opt` directory by command `ls /opt`

* Get `libmaix` source code

```
git clone https://github.com/sipeed/libmaix --recursive
```
Note that the `--recursive` parameter is used here, because sub-modules are used in the project. The advantage of sub-modules is that each project is managed separately. For example, `Kconfiglib` is used as a sub-module to provide `menuconfig` with interface function configuration.

**If you did't update submodule, the compile will error!!!!**

If you forget to add this parameter when cloning, you can also use the following command to update the submodule:
```
git submodule update --init --recursive
```
In addition, when the remote repository is updated, the user also needs to use the following command to update the code (ie update the submodule code at the same time):
```shell
git pull --recursive
```
or:
```
git pull
git submodule update --init --recursive
```


* Config project

```
cd examples/hello-world
python3 project.py menuconfig
```

You can select the module you want here, and config target info like `IP` or `user` info for upload bin files etc.

![menuconfig](assets/image/menuconfig_1.jpg)
![menuconfig](assets/image/menuconfig_2.jpg)

For first time compile, you can just use the default configuration.

Then push `q` key to exit, and select `yes` to save configuration.


* Build `hello-world` example

```
python3 project.py build
```

Then you can see the bin file `hello-world` in `dist` directory

* Upload bin files to target

You can manually copy `dist` folder to your target file system, e.g. use scp.

You can also config target info in the menuconfig, and upload by:
```
python3 project.py upload
```
> This uses `sshpass` + `scp` for transmission,
> But `sshpass` will not let the user enter the password when it encounters the `ssh key` for identification, but will directly exit without reporting an error.
> You can execute `sudo sh -c "echo StrictHostKeyChecking no >>/etc/ssh/ssh_config"` on the computer to close the check,
> Or if you don’t fill in the password in `menuconfig`, you won’t use `sshpass`, or you can manually copy it once with `scp`

Or you can just assign target info in command arg:
```
python3 project.py upload --target root@192.168.0.123:/root/maix_dist --passwd 123
```

* Clean build temp files and result files

Clean temp build files:
```
python3 project.py clean
```

Clean all build and result, and this will clean all configuration configed by `python3 project.py menuconfig`:
```
python3 project.py distclean
```

* Run executable bin file

Bin files in the `dist` directory contains some library like `*.a` or `*.so`, copy all of then, then execute `./start_app.sh`


## Create your own project

There's two way to create project

### Way 1. Create project in libmaix SDK

* Just copy `examples/hello-world` to `examples/my-project`

* Or create a new directory, e.g. copy `examples/hello-world` to `projects/my-project`


### Way 2. Create project in anywhere of the filesystem

* Clone `libmaix` to a directory, such as `/home/sipeed/libmaix`

* Then export the variable `export LIBMAIX_SDK_PATH=/home/neucrack/my_SDK` in the terminal, which can be placed in the `~/.bashrc` or `~/.zshrc` file, so that this variable will be automatically added every time the terminal is started

* Then create a project anywhere, such as copy the entire content of the folder `example/hello-world` to `/home/sipeed/my_projects/my-project`

* Finally compile in `/home/sipeed/my_projects/my-project` directory by `python3 project.py build`, and maybe you need to execute `python3 project.py distclean` first to clean the old temp files


### Add source files

* For simple project, you just add files to your project's `main/src` and `main/include` directory.
* You can add your own component at you project root directory, just copy `main` folder to a new folder like `examples/my-project/mylib`, and edit `examples/my-project/mylib/CMakeLists.txt` to add source files, and also edit `examples/my-project/main/CMakeLlist` edit
```
list(APPEND ADD_REQUIREMENTS libmaix)
```
to
```
list(APPEND ADD_REQUIREMENTS libmaix mylib)
```

More compile framwork usage see [c_cpp_project_framework](https://github.com/Neutree/c_cpp_project_framework)



## License

**MIT**， see [LICENSE](./LICENSE)

