# sandbox

## 运行代码

```shell
cmake -B build
make -C build -j$(nproc)

sudo ./build/sandbox/app/sandbox
```

## ubuntu-2404

编译

```shell
apt install cmake \
pkgconfig \
libx11-dev \
libatk1.0-dev \
libglib2.0-dev \
libxapp-dev \
libdconf-dev \
libudisks2-dev \
libvte-2.91-dev \
libgail-3-dev \
protobuf-c-compiler \
libprotobuf-c-dev \
libtracker-sparql-3.0-dev \
libcinnamon-desktop-dev

```